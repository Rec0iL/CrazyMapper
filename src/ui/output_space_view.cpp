#include "ui/output_space_view.hpp"
#include "math/homography.hpp"
#include <imgui.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace ui {

OutputSpaceView::OutputSpaceView()
    : viewSize_(800.0f, 600.0f),
      viewPosition_(0.0f),
      cornerHandleRadius_(8.0f),
      showGrid_(true),
      gridSize_(50.0f),
      perspectiveShader_(nullptr),
      layerMesh_(nullptr),
      previewFbo_(nullptr),
      quadVAO_(0),
      quadVBO_(0),
      shaderInitialized_(false),
      lastFboSize_(0.0f) {
}

OutputSpaceView::~OutputSpaceView() {
    if (quadVAO_) { glDeleteVertexArrays(1, &quadVAO_); quadVAO_ = 0; }
    if (quadVBO_) { glDeleteBuffers(1, &quadVBO_); quadVBO_ = 0; }
}

void OutputSpaceView::render(const std::vector<Shared<layers::Layer>>& layers,
                             int selectedIndex,
                             float canvasAspect,
                             bool* outCollapsed) {
    bool opened = ImGui::Begin("Output Space (Corner-Pin)", nullptr,
                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    if (outCollapsed) *outCollapsed = ImGui::IsWindowCollapsed();

    if (opened) {
        ImVec2 screenTL = ImGui::GetCursorScreenPos();
        viewPosition_ = Vec2(screenTL.x, screenTL.y);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 panelSize = ImGui::GetContentRegionAvail();
        viewSize_ = Vec2(panelSize.x, panelSize.y);

        // Background
        drawList->AddRectFilled(
            screenTL,
            ImVec2(screenTL.x + viewSize_.x, screenTL.y + viewSize_.y),
            IM_COL32(30, 30, 40, 255));

        if (showGrid_) renderGrid();

        // Compute and persist canvas rect
        Vec2 canvPos, canvSize;
        computeCanvasRect(canvasAspect, canvPos, canvSize);
        canvasLocalPos_  = canvPos;
        canvasLocalSize_ = canvSize;

        // Dim the area outside the canvas
        ImVec2 canvTL(screenTL.x + canvPos.x, screenTL.y + canvPos.y);
        ImVec2 canvBR(canvTL.x + canvSize.x,  canvTL.y + canvSize.y);
        drawList->AddRectFilled(screenTL,
            ImVec2(screenTL.x + viewSize_.x, screenTL.y + viewSize_.y),
            IM_COL32(0, 0, 0, 60));
        drawList->AddRectFilled(canvTL, canvBR, IM_COL32(0, 0, 0, 0));

        if (!shaderInitialized_) initPerspectiveShader();

        if (shaderInitialized_ && !layers.empty())
            renderQADWithPerspective(layers, canvPos, canvSize,
                                     Vec2(screenTL.x, screenTL.y));

        drawList->AddRect(canvTL, canvBR, IM_COL32(220, 220, 220, 200), 0.0f, 0, 2.0f);

        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(layers.size()))
            renderCornerHandles(layers[selectedIndex]);

        ImGui::InvisibleButton("OutputSpaceCanvas", panelSize,
                               ImGuiButtonFlags_MouseButtonLeft);
    }
    ImGui::End();
}

void OutputSpaceView::onCornerDragged(int /* cornerIndex */, Vec2 /* newPosition */) {
    // Update corner position (will be called from UI manager)
}

CornerHandle OutputSpaceView::getCornerAtPosition(Vec2 screenPos,
                                                  const Shared<layers::Layer>& layer) {
    if (!layer) return CornerHandle::NONE;

    auto corners = layer->getOutputCorners();
    for (int i = 0; i < 4; ++i) {
        Vec2 screenCorner = viewPosition_ + corners[i];
        float dist = glm::distance(screenPos, screenCorner);
        if (dist <= cornerHandleRadius_) {
            return static_cast<CornerHandle>(i);
        }
    }
    return CornerHandle::NONE;
}

Vec2 OutputSpaceView::screenToViewCoords(Vec2 screenPos) const {
    return screenPos - viewPosition_;
}

bool OutputSpaceView::isMouseInView(Vec2 screenMouse) const {
    return screenMouse.x >= viewPosition_.x &&
           screenMouse.x <= viewPosition_.x + viewSize_.x &&
           screenMouse.y >= viewPosition_.y &&
           screenMouse.y <= viewPosition_.y + viewSize_.y;
}

void OutputSpaceView::resetCorners(const Shared<layers::Layer>& layer) {
    if (layer) {
        layer->resetOutputCorners();
    }
}

bool OutputSpaceView::saveCornerPoints(const std::string& filepath,
                                       const Shared<layers::Layer>& layer) {
    if (!layer) return false;

    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    auto corners = layer->getOutputCorners();
    for (int i = 0; i < 4; ++i) {
        file << corners[i].x << " " << corners[i].y << "\n";
    }

    return true;
}

bool OutputSpaceView::loadCornerPoints(const std::string& filepath,
                                       Shared<layers::Layer>& layer) {
    if (!layer) return false;

    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    for (int i = 0; i < 4; ++i) {
        float x, y;
        if (!(file >> x >> y)) return false;
        layer->setOutputCorner(i, Vec2(x, y));
    }

    return true;
}

void OutputSpaceView::computeCanvasRect(float aspect, Vec2& outPos, Vec2& outSize) const {
    // Largest centered rect with the given aspect ratio, with 5% margins
    const float margin = 0.05f;
    float avW = viewSize_.x * (1.0f - 2.0f * margin);
    float avH = viewSize_.y * (1.0f - 2.0f * margin);
    float viewAspect = avW / std::max(avH, 1.0f);

    if (viewAspect > aspect) {
        outSize.y = avH;
        outSize.x = avH * aspect;
    } else {
        outSize.x = avW;
        outSize.y = avW / aspect;
    }
    outPos.x = (viewSize_.x - outSize.x) * 0.5f;
    outPos.y = (viewSize_.y - outSize.y) * 0.5f;
}

void OutputSpaceView::renderQADWithPerspective(
        const std::vector<Shared<layers::Layer>>& layers,
        Vec2 canvasPos, Vec2 canvasSize,
        Vec2 screenTL) {
    // Resize / create composite FBO to match canvas pixel size
    int fboW = std::max(1, (int)canvasSize.x);
    int fboH = std::max(1, (int)canvasSize.y);
    if (!previewFbo_ || (int)lastFboSize_.x != fboW || (int)lastFboSize_.y != fboH) {
        previewFbo_ = std::make_unique<gl::Framebuffer>(fboW, fboH);
        lastFboSize_ = Vec2((float)fboW, (float)fboH);
    }

    // ---- Save GL state ----
    GLint prevFBO = 0;     glGetIntegerv(GL_FRAMEBUFFER_BINDING,  &prevFBO);
    GLint prevVP[4];       glGetIntegerv(GL_VIEWPORT,             prevVP);
    GLint prevProg = 0;    glGetIntegerv(GL_CURRENT_PROGRAM,      &prevProg);
    GLint prevVAO = 0;     glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
    GLint prevTex = 0;     glGetIntegerv(GL_TEXTURE_BINDING_2D,   &prevTex);
    GLboolean prevBlend;   glGetBooleanv(GL_BLEND,                &prevBlend);

    // ---- Composite all layers into the FBO ----
    previewFbo_->bind();
    glViewport(0, 0, fboW, fboH);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);  // fully transparent background
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    perspectiveShader_->use();
    perspectiveShader_->setUniform1i("sourceTexture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(quadVAO_);

    GLint cornersLoc = glGetUniformLocation(perspectiveShader_->getHandle(),
                                            "outputCorners");

    for (auto& layer : layers) {
        if (!layer->isVisible()) continue;
        auto source = layer->getSource();
        if (!source || !source->isValid() || source->getTextureHandle() == 0) continue;

        // Output corners are stored as canvas UV [0..1] — use directly
        auto outCorners = layer->getOutputCorners();
        std::array<Vec2, 4> normOut;
        for (int i = 0; i < 4; ++i)
            normOut[i] = outCorners[i];

        auto inputCorners = layer->getInputCorners();
        bool isTriangle = (layer->getShapeType() == 4);

        glm::mat3 H;
        if (isTriangle) {
            std::array<Vec2, 3> triOut = {normOut[0], normOut[1], normOut[2]};
            std::array<Vec2, 3> triIn  = {inputCorners[0], inputCorners[1], inputCorners[2]};
            H = math::Homography::computeAffine(triOut, triIn);
        } else {
            H = math::Homography::compute(normOut, inputCorners);
        }

        glm::mat3 H_shape = glm::mat3(1.0f);
        if (!isTriangle) {
            std::array<Vec2, 4> unit = {Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1)};
            H_shape = math::Homography::compute(normOut, unit);
        }

        perspectiveShader_->setUniformMat3("homographyInverse", H);
        perspectiveShader_->setUniformMat3("shapeMaskHomography", H_shape);
        perspectiveShader_->setUniform1f("opacity", layer->getOpacity());
        perspectiveShader_->setUniform1f("feather", layer->getFeather());
        perspectiveShader_->setUniform1i("shapeType",   layer->getShapeType());
        auto cr = layer->getShapeCornerRadii();
        perspectiveShader_->setUniform4f("shapeCornerRadii", cr[0], cr[1], cr[2], cr[3]);
        perspectiveShader_->setUniform1i("shapeSides",  std::max(3, layer->getShapePolySides()));

        if (cornersLoc >= 0) {
            float cd[8] = {
                normOut[0].x, normOut[0].y,
                normOut[1].x, normOut[1].y,
                normOut[2].x, normOut[2].y,
                normOut[3].x, normOut[3].y,
            };
            glUniform2fv(cornersLoc, 4, cd);
        }

        glBindTexture(GL_TEXTURE_2D, source->getTextureHandle());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glBindVertexArray(0);

    // ---- Restore GL state ----
    glBindTexture(GL_TEXTURE_2D, prevTex);
    glUseProgram(prevProg);
    glBindVertexArray(prevVAO);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]);
    if (!prevBlend) glDisable(GL_BLEND);

    // ---- Display composite FBO via ImGui at the canvas position ----
    // V-flip: GL FBO y=0 is bottom → ImGui UV (0,1) at TL
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImTextureID fboTex = (ImTextureID)(intptr_t)previewFbo_->getColorTexture();
    ImVec2 canvScreen(screenTL.x + canvasPos.x, screenTL.y + canvasPos.y);
    drawList->AddImage(
        fboTex,
        canvScreen,
        ImVec2(canvScreen.x + canvasSize.x, canvScreen.y + canvasSize.y),
        ImVec2(0.f, 1.f),
        ImVec2(1.f, 0.f)
    );
}

void OutputSpaceView::renderCornerHandles(const Shared<layers::Layer>& layer) {
    if (!layer) return;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    int numCorners = layer->getActiveCornerCount();  // 3 for triangle, 4 others

    // Output corners are [0..1] canvas UV.
    // Convert to screen pixels for drawing: screenTL + canvasLocalPos_ + uv * canvasLocalSize_
    auto corners = layer->getOutputCorners();
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;

    // Helper: canvas UV -> screen pos
    auto uvToScreen = [&](Vec2 uv) -> ImVec2 {
        return ImVec2(canvasPos.x + canvasLocalPos_.x + uv.x * canvasLocalSize_.x,
                      canvasPos.y + canvasLocalPos_.y + uv.y * canvasLocalSize_.y);
    };

    // Draw outline for the selected layer
    {
        std::array<ImVec2, 4> pts;
        for (int i = 0; i < numCorners; ++i)
            pts[i] = uvToScreen(corners[i]);
        if (numCorners == 3) {
            drawList->AddTriangle(pts[0], pts[1], pts[2], IM_COL32(0, 220, 255, 200), 1.5f);
        } else {
            drawList->AddQuad(pts[0], pts[1], pts[2], pts[3],
                              IM_COL32(0, 220, 255, 200), 1.5f);
        }
    }

    // Release drag when mouse button is lifted
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        draggedCorner_ = -1;

    hoveredCorner_ = -1;
    for (int i = 0; i < numCorners; ++i) {
        ImVec2 cornerPos = uvToScreen(corners[i]);

        ImVec2 delta = ImVec2(mousePos.x - cornerPos.x, mousePos.y - cornerPos.y);
        float distSq = delta.x * delta.x + delta.y * delta.y;

        bool isHovered = distSq < (cornerHandleRadius_ * 3.0f) * (cornerHandleRadius_ * 3.0f);

        // Begin drag on click
        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            draggedCorner_ = i;

        bool isDragging = (draggedCorner_ == i);
        if (isHovered && !isDragging)
            hoveredCorner_ = i;
        ImU32 color = (isHovered || isDragging)
            ? IM_COL32(255, 255, 0, 255)
            : IM_COL32(255, 150, 0, 255);
        drawList->AddCircleFilled(cornerPos, cornerHandleRadius_, color);
        drawList->AddCircle(cornerPos, cornerHandleRadius_,
                           IM_COL32(255, 255, 255, 255), 0, 2.0f);

        if (isHovered || isDragging)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        // Move corner while dragging — clamp to canvas UV [0..1]
        if (isDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)
            && canvasLocalSize_.x > 0.0f && canvasLocalSize_.y > 0.0f) {
            float u = (mousePos.x - canvasPos.x - canvasLocalPos_.x) / canvasLocalSize_.x;
            float v = (mousePos.y - canvasPos.y - canvasLocalPos_.y) / canvasLocalSize_.y;
            layer->setOutputCorner(i, Vec2(u, v));
        }
    }

    // Arrow-key fine control for hovered (non-dragged) corner
    if (hoveredCorner_ >= 0 && !ImGui::GetIO().WantTextInput
        && canvasLocalSize_.x > 0.0f && canvasLocalSize_.y > 0.0f) {
        const float stepU = 1.0f / std::max(canvasLocalSize_.x, 1.0f);
        const float stepV = 1.0f / std::max(canvasLocalSize_.y, 1.0f);
        Vec2 pos = layer->getOutputCorners()[hoveredCorner_];
        bool moved = false;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow,  true)) { pos.x -= stepU; moved = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) { pos.x += stepU; moved = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow,    true)) { pos.y -= stepV; moved = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow,  true)) { pos.y += stepV; moved = true; }
        if (moved) {
            layer->setOutputCorner(hoveredCorner_, pos);
        }
        ImGui::BeginTooltip();
        Vec2 c = layer->getOutputCorners()[hoveredCorner_];
        ImGui::Text("Corner %d: (%.4f, %.4f)", hoveredCorner_, c.x, c.y);
        ImGui::TextDisabled("Arrow keys: fine-tune position");
        ImGui::EndTooltip();
    }
}

void OutputSpaceView::renderGrid() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImU32 gridColor = IM_COL32(60, 60, 80, 100);

    for (float x = 0; x < viewSize_.x; x += gridSize_) {
        drawList->AddLine(
            ImVec2(canvasPos.x + x, canvasPos.y),
            ImVec2(canvasPos.x + x, canvasPos.y + viewSize_.y),
            gridColor, 1.0f
        );
    }

    for (float y = 0; y < viewSize_.y; y += gridSize_) {
        drawList->AddLine(
            ImVec2(canvasPos.x, canvasPos.y + y),
            ImVec2(canvasPos.x + viewSize_.x, canvasPos.y + y),
            gridColor, 1.0f
        );
    }
}

bool OutputSpaceView::initPerspectiveShader() {
    // Vertex shader: passes NDC position + ImGui-UV texCoord to fragment stage.
    // texCoord (0,0)=top-left, (1,1)=bottom-right matches ImGui canvas y-down convention.
    // The quad vertices are ordered TL,TR,BL,BR for GL_TRIANGLE_STRIP.
    const char* vertSrc = R"glsl(
        #version 330 core
        layout(location = 0) in vec2 position;
        layout(location = 1) in vec2 texCoord;
        out VS_OUT { vec2 texCoord; } vs_out;
        void main() {
            gl_Position = vec4(position, 0.0, 1.0);
            vs_out.texCoord = texCoord;
        }
    )glsl";

    // Fragment shader embedded inline (mirrors shaders/perspective.frag)
    const char* fragSrc = R"glsl(
        #version 330 core
        in VS_OUT { vec2 texCoord; } fs_in;
        out vec4 FragColor;
        uniform sampler2D sourceTexture;
        uniform mat3 homographyInverse;
        uniform mat3 shapeMaskHomography;  // canvas UV -> output quad local UV [0,1]²
        uniform float opacity;
        uniform float feather;             // edge feather width in shape-local UV space
        uniform vec2 outputCorners[4];
        uniform int   shapeType;         // 0=rect 1=rounded_rect 2=ellipse 3=n_polygon 4=triangle
        uniform vec4  shapeCornerRadii;  // TL TR BR BL corner radii [0..0.5]
        uniform int   shapeSides;        // number of sides for n_polygon

        float cross2d(vec2 a, vec2 b) { return a.x*b.y - a.y*b.x; }

        bool insideConvexQuad(vec2 p) {
            float d0 = cross2d(outputCorners[1]-outputCorners[0], p-outputCorners[0]);
            float d1 = cross2d(outputCorners[2]-outputCorners[1], p-outputCorners[1]);
            float d2 = cross2d(outputCorners[3]-outputCorners[2], p-outputCorners[2]);
            float d3 = cross2d(outputCorners[0]-outputCorners[3], p-outputCorners[3]);
            bool allPos = (d0>=0.0)&&(d1>=0.0)&&(d2>=0.0)&&(d3>=0.0);
            bool allNeg = (d0<=0.0)&&(d1<=0.0)&&(d2<=0.0)&&(d3<=0.0);
            return allPos || allNeg;
        }

        // --- SDF functions (positive = inside, 0 = on edge, negative = outside) ---

        float rectSDF(vec2 uv) {
            return min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
        }

        float roundedRectSDF(vec2 p, vec4 r) {
            if (p.x < r.x && p.y < r.x) {
                return r.x - length(p - vec2(r.x, r.x));
            }
            if (p.x > 1.0-r.y && p.y < r.y) {
                return r.y - length(p - vec2(1.0-r.y, r.y));
            }
            if (p.x > 1.0-r.z && p.y > 1.0-r.z) {
                return r.z - length(p - vec2(1.0-r.z, 1.0-r.z));
            }
            if (p.x < r.w && p.y > 1.0-r.w) {
                return r.w - length(p - vec2(r.w, 1.0-r.w));
            }
            return min(min(p.x, 1.0-p.x), min(p.y, 1.0-p.y));
        }

        float ellipseSDF(vec2 uv) {
            return 0.5 - length(uv - vec2(0.5));
        }

        float polygonSDF(vec2 uv, int sides) {
            float n = float(sides);
            vec2 p = uv - vec2(0.5);
            float plen = length(p);
            if (plen < 0.001) return 0.5;
            float sector = 6.28318530 / n;
            float a = mod(atan(p.y, p.x), sector);
            if (a < 0.0) a += sector;
            float theta = abs(a - sector * 0.5);
            float apothem = 0.5 * cos(3.14159265 / n);
            return apothem - plen * cos(theta);
        }

        // Triangle SDF in canvas UV space. Positive = inside.
        float triangleSDF(vec2 p, vec2 a, vec2 b, vec2 c) {
            vec2 e0 = b - a, e1 = c - b, e2 = a - c;
            vec2 v0 = p - a, v1 = p - b, v2 = p - c;
            vec2 pq0 = v0 - e0 * clamp(dot(v0,e0)/dot(e0,e0), 0.0, 1.0);
            vec2 pq1 = v1 - e1 * clamp(dot(v1,e1)/dot(e1,e1), 0.0, 1.0);
            vec2 pq2 = v2 - e2 * clamp(dot(v2,e2)/dot(e2,e2), 0.0, 1.0);
            float s = sign(cross2d(e0, e2));
            vec2 d = min(min(vec2(dot(pq0,pq0), s*cross2d(v0,e0)),
                             vec2(dot(pq1,pq1), s*cross2d(v1,e1))),
                             vec2(dot(pq2,pq2), s*cross2d(v2,e2)));
            return sqrt(d.x) * sign(d.y);
        }

        float shapeSDF(vec2 uv) {
            if (shapeType == 0) return rectSDF(uv);
            if (shapeType == 1) return roundedRectSDF(uv, shapeCornerRadii);
            if (shapeType == 2) return ellipseSDF(uv);
            if (shapeType == 3) return polygonSDF(uv, shapeSides);
            return 1.0;
        }

        void main() {
            vec2 outCoord = fs_in.texCoord;

            float shapeAlpha;
            if (shapeType == 4) {  // TRIANGLE
                float sdf = triangleSDF(outCoord,
                    outputCorners[0], outputCorners[1], outputCorners[2]);
                shapeAlpha = smoothstep(0.0, max(feather, 0.001), sdf);
            } else {
                if (!insideConvexQuad(outCoord)) discard;
                vec3 hs = shapeMaskHomography * vec3(outCoord, 1.0);
                vec2 shapeUV = hs.xy / hs.z;
                float sdf = shapeSDF(shapeUV);
                shapeAlpha = smoothstep(0.0, max(feather, 0.001), sdf);
            }
            if (shapeAlpha < 0.001) discard;

            // Source UV via homography (input region selection)
            vec3 h = homographyInverse * vec3(outCoord, 1.0);
            vec2 uv = h.xy / h.z;
            if (uv.x<0.0||uv.x>1.0||uv.y<0.0||uv.y>1.0) discard;
            uv.y = 1.0 - uv.y;   // ImGui y-down -> GL texture y-up
            vec4 c = texture(sourceTexture, uv);
            c.a *= opacity * shapeAlpha;
            FragColor = c;
        }
    )glsl";

    perspectiveShader_ = std::make_unique<gl::ShaderProgram>();
    if (!perspectiveShader_->compile(vertSrc, fragSrc)) {
        perspectiveShader_.reset();
        return false;
    }

    // Fullscreen quad in NDC. texCoord is ImGui canvas UV (0,0=TL, 1,1=BR, y-down).
    // NDC y=+1 is the top of the FBO; after V-flip display that maps to ImGui top.
    //   NDC(-1, 1) -> texCoord(0,0)  top-left     ImGui TL
    //   NDC( 1, 1) -> texCoord(1,0)  top-right    ImGui TR
    //   NDC(-1,-1) -> texCoord(0,1)  bottom-left  ImGui BL
    //   NDC( 1,-1) -> texCoord(1,1)  bottom-right ImGui BR
    float verts[] = {
        // position (NDC)    texCoord (ImGui-UV, y-down)
        -1.0f,  1.0f,        0.0f, 0.0f,  // TL
         1.0f,  1.0f,        1.0f, 0.0f,  // TR
        -1.0f, -1.0f,        0.0f, 1.0f,  // BL
         1.0f, -1.0f,        1.0f, 1.0f,  // BR
    };

    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);
    glBindVertexArray(quadVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    // location 0: position (2 floats)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float), (void*)0);
    // location 1: texCoord (2 floats, offset 2)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    shaderInitialized_ = true;
    return true;
}

} // namespace ui

