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
                             float canvasAspect) {
    if (ImGui::Begin("Output Space (Corner-Pin)", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
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
        // un-dim the canvas area by drawing a transparent rectangle on top
        drawList->AddRectFilled(canvTL, canvBR, IM_COL32(0, 0, 0, 0));

        // Lazy shader/VAO init
        if (!shaderInitialized_) initPerspectiveShader();

        // Render all visible layers into a shared composite FBO
        if (shaderInitialized_ && !layers.empty())
            renderQADWithPerspective(layers, canvPos, canvSize,
                                     Vec2(screenTL.x, screenTL.y));

        // Canvas outline (bright, always on top)
        drawList->AddRect(canvTL, canvBR, IM_COL32(220, 220, 220, 200), 0.0f, 0, 2.0f);

        // Corner handles for the selected layer only
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(layers.size()))
            renderCornerHandles(layers[selectedIndex]);

        ImGui::InvisibleButton("OutputSpaceCanvas", panelSize,
                               ImGuiButtonFlags_MouseButtonLeft);

        ImGui::End();
    }
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
        glm::mat3 H = math::Homography::compute(normOut, inputCorners);

        perspectiveShader_->setUniformMat3("homographyInverse", H);
        perspectiveShader_->setUniform1f("opacity", layer->getOpacity());
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

    // Draw the selected layer's quad outline in cyan
    {
        std::array<ImVec2, 4> pts;
        for (int i = 0; i < 4; ++i)
            pts[i] = uvToScreen(corners[i]);
        drawList->AddQuad(pts[0], pts[1], pts[2], pts[3],
                          IM_COL32(0, 220, 255, 200), 1.5f);
    }

    for (int i = 0; i < 4; ++i) {
        ImVec2 cornerPos = uvToScreen(corners[i]);

        // Calculate distance to corner
        ImVec2 delta = ImVec2(mousePos.x - cornerPos.x, mousePos.y - cornerPos.y);
        float distSq = delta.x * delta.x + delta.y * delta.y;

        // Check if mouse is over this corner
        bool isHovered = distSq < (cornerHandleRadius_ * 3.0f) * (cornerHandleRadius_ * 3.0f);

        ImU32 color = isHovered ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 150, 0, 255);
        drawList->AddCircleFilled(cornerPos, cornerHandleRadius_, color);
        drawList->AddCircle(cornerPos, cornerHandleRadius_,
                           IM_COL32(255, 255, 255, 255), 0, 2.0f);

        if (isHovered) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        // Handle dragging — convert mouse panel-local pixel to canvas UV
        if (isHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)
            && canvasLocalSize_.x > 0.0f && canvasLocalSize_.y > 0.0f) {
            float u = (mousePos.x - canvasPos.x - canvasLocalPos_.x) / canvasLocalSize_.x;
            float v = (mousePos.y - canvasPos.y - canvasLocalPos_.y) / canvasLocalSize_.y;
            layer->setOutputCorner(i, Vec2(u, v));
        }
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
        uniform float opacity;
        uniform vec2 outputCorners[4];
        uniform int   shapeType;         // 0=rect 1=rounded_rect 2=ellipse 3=n_polygon
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

        // Per-corner rounded rectangle test in UV space [0,1]x[0,1] y-down.
        // radii: x=TL, y=TR, z=BR, w=BL
        bool insideRoundedRect(vec2 p, vec4 r) {
            // TL zone
            if (p.x < r.x && p.y < r.x) {
                vec2 d = p - vec2(r.x, r.x);
                return dot(d,d) <= r.x*r.x;
            }
            // TR zone
            if (p.x > 1.0-r.y && p.y < r.y) {
                vec2 d = p - vec2(1.0-r.y, r.y);
                return dot(d,d) <= r.y*r.y;
            }
            // BR zone
            if (p.x > 1.0-r.z && p.y > 1.0-r.z) {
                vec2 d = p - vec2(1.0-r.z, 1.0-r.z);
                return dot(d,d) <= r.z*r.z;
            }
            // BL zone
            if (p.x < r.w && p.y > 1.0-r.w) {
                vec2 d = p - vec2(r.w, 1.0-r.w);
                return dot(d,d) <= r.w*r.w;
            }
            return true;
        }

        bool insideShapeMask(vec2 uv) {
            if (shapeType == 2) {  // ELLIPSE
                vec2 d = uv - vec2(0.5);
                return dot(d, d) <= 0.25;
            }
            if (shapeType == 1) {  // ROUNDED_RECTANGLE
                return insideRoundedRect(uv, shapeCornerRadii);
            }
            if (shapeType == 3) {  // N_POLYGON
                float n = float(shapeSides);
                vec2 p = uv - vec2(0.5);
                if (length(p) < 0.001) return true;
                float sector = 6.28318530 / n;
                float a = mod(atan(p.y, p.x), sector);
                if (a < 0.0) a += sector;
                float theta = abs(a - sector * 0.5);
                float apothem = 0.5 * cos(3.14159265 / n);
                return length(p) * cos(theta) <= apothem;
            }
            return true;  // RECTANGLE: full quad, no extra mask
        }

        void main() {
            vec2 outCoord = fs_in.texCoord;
            if (!insideConvexQuad(outCoord)) discard;
            vec3 h = homographyInverse * vec3(outCoord, 1.0);
            vec2 uv = h.xy / h.z;
            if (uv.x<0.0||uv.x>1.0||uv.y<0.0||uv.y>1.0) discard;
            if (!insideShapeMask(uv)) discard;
            uv.y = 1.0 - uv.y;   // ImGui y-down -> GL texture y-up
            vec4 c = texture(sourceTexture, uv);
            c.a *= opacity;
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

