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
      quadVAO_(0),
      quadVBO_(0),
      shaderInitialized_(false) {
}

OutputSpaceView::~OutputSpaceView() {
    if (quadVAO_) { glDeleteVertexArrays(1, &quadVAO_); quadVAO_ = 0; }
    if (quadVBO_) { glDeleteBuffers(1, &quadVBO_); quadVBO_ = 0; }
}

// ---------------------------------------------------------------------------
// Public: render
// ---------------------------------------------------------------------------

void OutputSpaceView::render(const std::vector<Shared<layers::Layer>>& layers,
                             int selectedIndex,
                             const std::vector<CanvasConfig>& canvases,
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

        // Compute and persist per-canvas rects
        static const std::vector<CanvasConfig> defaultCanvas{CanvasConfig{}};
        const std::vector<CanvasConfig>& cvs = canvases.empty()
            ? defaultCanvas  // fallback: one default canvas
            : canvases;

        computeCanvasRects(cvs, canvasLocalPos_, canvasLocalSize_);

        if (!shaderInitialized_) initPerspectiveShader();

        // Determine which canvas the selected layer belongs to
        int selectedCanvas = -1;
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(layers.size()))
            selectedCanvas = layers[selectedIndex]->getCanvasIndex();

        // Draw each canvas
        for (int ci = 0; ci < static_cast<int>(cvs.size()); ++ci) {
            Vec2 canvPos  = canvasLocalPos_[ci];
            Vec2 canvSize = canvasLocalSize_[ci];

            ImVec2 canvTL(screenTL.x + canvPos.x, screenTL.y + canvPos.y);
            ImVec2 canvBR(canvTL.x + canvSize.x,  canvTL.y + canvSize.y);

            // Canvas background (slightly lighter than view bg)
            drawList->AddRectFilled(canvTL, canvBR, IM_COL32(20, 20, 30, 255));

            // Render layers belonging to this canvas
            if (shaderInitialized_ && !layers.empty())
                renderCanvasContent(layers, ci, canvPos, canvSize,
                                    Vec2(screenTL.x, screenTL.y));

            // Canvas border — highlighted if it contains the selected layer
            bool isActiveCanvas = (ci == selectedCanvas);
            ImU32 borderColor = isActiveCanvas
                ? IM_COL32(80, 160, 255, 230)   // blue for active
                : IM_COL32(180, 180, 180, 160);  // grey for others
            float borderThick = isActiveCanvas ? 2.5f : 1.5f;
            drawList->AddRect(canvTL, canvBR, borderColor, 0.0f, 0, borderThick);

            // Canvas label (top-left of canvas rect)
            char label[64];
            snprintf(label, sizeof(label), "%s", cvs[ci].name.c_str());
            drawList->AddText(ImVec2(canvTL.x + 4.f, canvTL.y + 4.f),
                              IM_COL32(200, 200, 200, 200), label);
        }

        // Corner handles for the selected layer
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(layers.size())
            && selectedCanvas >= 0 && selectedCanvas < static_cast<int>(cvs.size())) {
            renderCornerHandles(layers[selectedIndex], selectedCanvas);
        }

        ImGui::InvisibleButton("OutputSpaceCanvas", panelSize,
                               ImGuiButtonFlags_MouseButtonLeft);
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Public: canvas rect accessors
// ---------------------------------------------------------------------------

Vec2 OutputSpaceView::getCanvasLocalPos(int i) const {
    if (i >= 0 && i < static_cast<int>(canvasLocalPos_.size()))
        return canvasLocalPos_[i];
    return Vec2(0.0f);
}

Vec2 OutputSpaceView::getCanvasLocalSize(int i) const {
    if (i >= 0 && i < static_cast<int>(canvasLocalSize_.size()))
        return canvasLocalSize_[i];
    return Vec2(1.0f);
}

// ---------------------------------------------------------------------------
// Public: interaction helpers
// ---------------------------------------------------------------------------

void OutputSpaceView::onCornerDragged(int /* cornerIndex */, Vec2 /* newPosition */) {}

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
    if (layer) layer->resetOutputCorners();
}

bool OutputSpaceView::saveCornerPoints(const std::string& filepath,
                                       const Shared<layers::Layer>& layer) {
    if (!layer) return false;
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    auto corners = layer->getOutputCorners();
    for (int i = 0; i < 4; ++i)
        file << corners[i].x << " " << corners[i].y << "\n";
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

// ---------------------------------------------------------------------------
// Private: layout
// ---------------------------------------------------------------------------

void OutputSpaceView::computeCanvasRects(const std::vector<CanvasConfig>& canvases,
                                         std::vector<Vec2>& outPos,
                                         std::vector<Vec2>& outSize) const {
    const int   N      = static_cast<int>(canvases.size());
    const float margin = 0.05f;
    const float gap    = 24.0f;  // pixels between canvases

    float avW = viewSize_.x * (1.0f - 2.0f * margin);
    float avH = viewSize_.y * (1.0f - 2.0f * margin);

    // Compute widths at a normalized height of 1.0, then scale
    std::vector<float> widths(N);
    float totalNormW = 0.0f;
    for (int i = 0; i < N; ++i) {
        widths[i]   = canvases[i].getAspectRatio();
        totalNormW += widths[i];
    }

    // Height where all canvases fit side-by-side (accounting for gaps)
    float gapTotal = gap * std::max(N - 1, 0);
    float hByW     = (avW - gapTotal) / std::max(totalNormW, 0.001f);
    float hByH     = avH;
    float h        = std::min(hByW, hByH);

    // Total actual width
    float totalW = 0.0f;
    for (int i = 0; i < N; ++i) totalW += widths[i] * h;
    totalW += gapTotal;

    // Center the whole block
    float startX = (viewSize_.x - totalW) * 0.5f;
    float startY = (viewSize_.y - h)      * 0.5f;

    outPos .resize(N);
    outSize.resize(N);

    float curX = startX;
    for (int i = 0; i < N; ++i) {
        float w = widths[i] * h;
        outPos [i] = Vec2(curX, startY);
        outSize[i] = Vec2(w, h);
        curX += w + gap;
    }
}

// ---------------------------------------------------------------------------
// Private: rendering helpers
// ---------------------------------------------------------------------------

void OutputSpaceView::renderGrid() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImU32 gridColor = IM_COL32(60, 60, 80, 100);

    for (float x = 0; x < viewSize_.x; x += gridSize_) {
        drawList->AddLine(
            ImVec2(canvasPos.x + x, canvasPos.y),
            ImVec2(canvasPos.x + x, canvasPos.y + viewSize_.y),
            gridColor, 1.0f);
    }
    for (float y = 0; y < viewSize_.y; y += gridSize_) {
        drawList->AddLine(
            ImVec2(canvasPos.x, canvasPos.y + y),
            ImVec2(canvasPos.x + viewSize_.x, canvasPos.y + y),
            gridColor, 1.0f);
    }
}

void OutputSpaceView::renderCanvasContent(
        const std::vector<Shared<layers::Layer>>& layers,
        int canvasIndex,
        Vec2 canvasPos, Vec2 canvasSize,
        Vec2 screenTL) {
    renderQADWithPerspective(layers, canvasIndex, canvasPos, canvasSize, screenTL);
}

void OutputSpaceView::renderQADWithPerspective(
        const std::vector<Shared<layers::Layer>>& layers,
        int canvasIndex,
        Vec2 canvasPos, Vec2 canvasSize,
        Vec2 screenTL) {
    int fboW = std::max(1, (int)canvasSize.x);
    int fboH = std::max(1, (int)canvasSize.y);

    // Ensure per-canvas FBO vector is large enough
    if (canvasIndex >= static_cast<int>(canvasFbos_.size())) {
        canvasFbos_    .resize(canvasIndex + 1);
        canvasFboSizes_.resize(canvasIndex + 1, Vec2(0.0f));
    }

    Vec2 needed{float(fboW), float(fboH)};
    if (!canvasFbos_[canvasIndex] || canvasFboSizes_[canvasIndex] != needed) {
        canvasFbos_    [canvasIndex] = std::make_unique<gl::Framebuffer>(fboW, fboH);
        canvasFboSizes_[canvasIndex] = needed;
    }
    auto& fbo = *canvasFbos_[canvasIndex];

    // ---- Save GL state ----
    GLint prevFBO = 0;   glGetIntegerv(GL_FRAMEBUFFER_BINDING,  &prevFBO);
    GLint prevVP[4];     glGetIntegerv(GL_VIEWPORT,             prevVP);
    GLint prevProg = 0;  glGetIntegerv(GL_CURRENT_PROGRAM,      &prevProg);
    GLint prevVAO = 0;   glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
    GLint prevTex = 0;   glGetIntegerv(GL_TEXTURE_BINDING_2D,   &prevTex);
    GLboolean prevBlend; glGetBooleanv(GL_BLEND,                &prevBlend);

    // ---- Composite layers for this canvas into its own FBO ----
    fbo.bind();
    glViewport(0, 0, fboW, fboH);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
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
        if (layer->getCanvasIndex() != canvasIndex) continue;
        if (!layer->isVisible()) continue;
        auto source = layer->getSource();
        if (!source || !source->isValid() || source->getTextureHandle() == 0) continue;

        auto outCorners   = layer->getOutputCorners();
        auto inputCorners = layer->getInputCorners();

        std::array<Vec2, 4> normOut;
        for (int i = 0; i < 4; ++i) normOut[i] = outCorners[i];

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

        perspectiveShader_->setUniformMat3("homographyInverse",   H);
        perspectiveShader_->setUniformMat3("shapeMaskHomography", H_shape);
        perspectiveShader_->setUniform1f("opacity",          layer->getOpacity());
        perspectiveShader_->setUniform1f("featherWidth",     layer->getFeatherWidth());
        perspectiveShader_->setUniform1f("featherStrength",  layer->getFeatherStrength());
        perspectiveShader_->setUniform1i("perEdgeFeather",   layer->isPerEdgeFeather() ? 1 : 0);
        auto ew = layer->getEdgeFeatherWidths();
        perspectiveShader_->setUniform4f("edgeFeatherWidths", ew[0], ew[1], ew[2], ew[3]);
        perspectiveShader_->setUniform1i("shapeType",        layer->getShapeType());
        auto cr = layer->getShapeCornerRadii();
        perspectiveShader_->setUniform4f("shapeCornerRadii", cr[0], cr[1], cr[2], cr[3]);
        perspectiveShader_->setUniform1i("shapeSides",
                                         std::max(3, layer->getShapePolySides()));

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

    // ---- Blit composite FBO to ImGui draw list ----
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImTextureID fboTex   = (ImTextureID)(intptr_t)fbo.getColorTexture();
    ImVec2 canvScreen(screenTL.x + canvasPos.x, screenTL.y + canvasPos.y);
    drawList->AddImage(
        fboTex,
        canvScreen,
        ImVec2(canvScreen.x + canvasSize.x, canvScreen.y + canvasSize.y),
        ImVec2(0.f, 1.f),  // V-flip: GL y=0 is bottom → ImGui top
        ImVec2(1.f, 0.f));
}

void OutputSpaceView::renderCornerHandles(const Shared<layers::Layer>& layer,
                                          int canvasIndex) {
    if (!layer) return;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    // Canvas UV -> screen: screenTL + canvasLocalPos_[ci] + uv * canvasLocalSize_[ci]
    Vec2 cPos  = (canvasIndex < static_cast<int>(canvasLocalPos_.size()))
                   ? canvasLocalPos_[canvasIndex]
                   : Vec2(0.0f);
    Vec2 cSize = (canvasIndex < static_cast<int>(canvasLocalSize_.size()))
                   ? canvasLocalSize_[canvasIndex]
                   : Vec2(1.0f);

    int numCorners = layer->getActiveCornerCount();
    auto corners   = layer->getOutputCorners();

    ImGuiIO& io = ImGui::GetIO();
    ImVec2   mousePos = io.MousePos;

    auto uvToScreen = [&](Vec2 uv) -> ImVec2 {
        return ImVec2(canvasPos.x + cPos.x + uv.x * cSize.x,
                      canvasPos.y + cPos.y + uv.y * cSize.y);
    };

    // Draw outline for the selected layer
    {
        std::array<ImVec2, 4> pts;
        for (int i = 0; i < numCorners; ++i) pts[i] = uvToScreen(corners[i]);
        if (numCorners == 3) {
            drawList->AddTriangle(pts[0], pts[1], pts[2],
                                  IM_COL32(0, 220, 255, 200), 1.5f);
        } else {
            drawList->AddQuad(pts[0], pts[1], pts[2], pts[3],
                              IM_COL32(0, 220, 255, 200), 1.5f);
        }
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        draggedCorner_ = -1;

    hoveredCorner_ = -1;
    for (int i = 0; i < numCorners; ++i) {
        ImVec2 cornerPos  = uvToScreen(corners[i]);
        ImVec2 delta      = ImVec2(mousePos.x - cornerPos.x, mousePos.y - cornerPos.y);
        float  distSq     = delta.x * delta.x + delta.y * delta.y;
        bool   isHovered  = distSq < (cornerHandleRadius_ * 3.0f) * (cornerHandleRadius_ * 3.0f);

        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            draggedCorner_ = i;

        bool isDragging = (draggedCorner_ == i);
        if (isHovered && !isDragging) hoveredCorner_ = i;

        ImU32 color = (isHovered || isDragging)
            ? IM_COL32(255, 255,   0, 255)
            : IM_COL32(255, 150,   0, 255);
        drawList->AddCircleFilled(cornerPos, cornerHandleRadius_, color);
        drawList->AddCircle(cornerPos, cornerHandleRadius_,
                            IM_COL32(255, 255, 255, 255), 0, 2.0f);

        if (isHovered || isDragging)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        if (isDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)
            && cSize.x > 0.0f && cSize.y > 0.0f) {
            float u = (mousePos.x - canvasPos.x - cPos.x) / cSize.x;
            float v = (mousePos.y - canvasPos.y - cPos.y) / cSize.y;
            layer->setOutputCorner(i, Vec2(u, v));
        }
    }

    // Arrow-key fine control for hovered corner
    if (hoveredCorner_ >= 0 && !ImGui::GetIO().WantTextInput
        && cSize.x > 0.0f && cSize.y > 0.0f) {
        const float stepU = 1.0f / std::max(cSize.x, 1.0f);
        const float stepV = 1.0f / std::max(cSize.y, 1.0f);
        Vec2 pos = layer->getOutputCorners()[hoveredCorner_];
        bool moved = false;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow,  true)) { pos.x -= stepU; moved = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) { pos.x += stepU; moved = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow,    true)) { pos.y -= stepV; moved = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow,  true)) { pos.y += stepV; moved = true; }
        if (moved) layer->setOutputCorner(hoveredCorner_, pos);

        ImGui::BeginTooltip();
        Vec2 c = layer->getOutputCorners()[hoveredCorner_];
        ImGui::Text("Corner %d: (%.4f, %.4f)", hoveredCorner_, c.x, c.y);
        ImGui::TextDisabled("Arrow keys: fine-tune position");
        ImGui::EndTooltip();
    }
}

// ---------------------------------------------------------------------------
// Private: shader init
// ---------------------------------------------------------------------------

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
        uniform float featherWidth;          // edge feather width in shape-local UV space
        uniform float featherStrength;      // 0 = hard edge, 1 = full feather
        uniform int   perEdgeFeather;       // 0 = uniform feather, 1 = per-edge
        uniform vec4  edgeFeatherWidths;    // top/right/bottom/left (or edge0/edge1/edge2/unused for triangle)
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

        // Per-edge feather helpers
        float edgeAlpha(float dist, float width) {
            return smoothstep(0.0, max(width, 0.001), dist);
        }

        float rectPerEdgeAlpha(vec2 uv, vec4 ew) {
            return edgeAlpha(uv.y, ew.x)           // top
                 * edgeAlpha(1.0 - uv.x, ew.y)     // right
                 * edgeAlpha(1.0 - uv.y, ew.z)     // bottom
                 * edgeAlpha(uv.x, ew.w);           // left
        }

        float roundedRectPerEdgeAlpha(vec2 p, vec4 r, vec4 ew) {
            // Outer shape: original rounded rect.
            float outer = roundedRectSDF(p, r);
            if (outer <= 0.0) return 0.0;

            // Inner contour: each edge moved inward by its feather width,
            // reconnected with tangential circular arcs at the corners.
            float L = ew.w;           // left   edge inset
            float T = ew.x;           // top    edge inset
            float R = 1.0 - ew.y;    // right  edge (from right)
            float B = 1.0 - ew.z;    // bottom edge (from bottom)

            // Inner corner radii: shrink original radius by the average
            // of the two adjacent feather widths so the arc stays
            // tangent to both inset edges.
            float rTL = max(0.0, r.x - (ew.x + ew.w) * 0.5);
            float rTR = max(0.0, r.y - (ew.x + ew.y) * 0.5);
            float rBR = max(0.0, r.z - (ew.z + ew.y) * 0.5);
            float rBL = max(0.0, r.w - (ew.z + ew.w) * 0.5);

            // SDF of the inner rounded rect (positive=inside)
            float inner;
            if (p.x < L + rTL && p.y < T + rTL)
                inner = rTL - length(p - vec2(L + rTL, T + rTL));
            else if (p.x > R - rTR && p.y < T + rTR)
                inner = rTR - length(p - vec2(R - rTR, T + rTR));
            else if (p.x > R - rBR && p.y > B - rBR)
                inner = rBR - length(p - vec2(R - rBR, B - rBR));
            else if (p.x < L + rBL && p.y > B - rBL)
                inner = rBL - length(p - vec2(L + rBL, B - rBL));
            else
                inner = min(min(p.x - L, R - p.x), min(p.y - T, B - p.y));

            // Inside inner contour: fully opaque
            if (inner >= 0.0) return 1.0;

            // Feather zone: smooth transition between outer and inner
            float t = outer / (outer - inner);
            return smoothstep(0.0, 1.0, t);
        }

        float trianglePerEdgeAlpha(vec2 p, vec2 a, vec2 b, vec2 c, vec3 ew) {
            vec2 e0 = b - a, e1 = c - b, e2 = a - c;
            float s = sign(cross2d(e0, c - a));
            float d0 = s * cross2d(e0, p - a) / max(length(e0), 0.001);
            float d1 = s * cross2d(e1, p - b) / max(length(e1), 0.001);
            float d2 = s * cross2d(e2, p - c) / max(length(e2), 0.001);
            return edgeAlpha(d0, ew.x) * edgeAlpha(d1, ew.y) * edgeAlpha(d2, ew.z);
        }

        void main() {
            vec2 outCoord = fs_in.texCoord;

            float shapeAlpha;
            if (shapeType == 4) {  // TRIANGLE
                float sdf = triangleSDF(outCoord,
                    outputCorners[0], outputCorners[1], outputCorners[2]);
                float hardAlpha = step(0.0, sdf);
                float featheredAlpha;
                if (perEdgeFeather != 0) {
                    featheredAlpha = trianglePerEdgeAlpha(outCoord,
                        outputCorners[0], outputCorners[1], outputCorners[2],
                        edgeFeatherWidths.xyz);
                } else {
                    featheredAlpha = smoothstep(0.0, max(featherWidth, 0.001), sdf);
                }
                shapeAlpha = mix(hardAlpha, featheredAlpha, featherStrength);
            } else {
                if (!insideConvexQuad(outCoord)) discard;
                vec3 hs = shapeMaskHomography * vec3(outCoord, 1.0);
                vec2 shapeUV = hs.xy / hs.z;
                float sdf = shapeSDF(shapeUV);
                float hardAlpha = step(0.0, sdf);
                float featheredAlpha;
                if (perEdgeFeather != 0 && (shapeType == 0 || shapeType == 1)) {
                    if (shapeType == 0)
                        featheredAlpha = rectPerEdgeAlpha(shapeUV, edgeFeatherWidths);
                    else
                        featheredAlpha = roundedRectPerEdgeAlpha(shapeUV, shapeCornerRadii, edgeFeatherWidths);
                } else {
                    featheredAlpha = smoothstep(0.0, max(featherWidth, 0.001), sdf);
                }
                shapeAlpha = mix(hardAlpha, featheredAlpha, featherStrength);
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

