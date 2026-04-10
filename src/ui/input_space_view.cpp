#include "ui/input_space_view.hpp"
#include <imgui.h>
#include <glm/glm.hpp>
#include <algorithm>

namespace ui {

InputSpaceView::InputSpaceView()
    : viewSize_(800.0f, 600.0f),
      viewPosition_(0.0f),
      showGrid_(true),
      gridDivisionsX_(8),
      gridDivisionsY_(8) {
}

InputSpaceView::~InputSpaceView() {
}

void InputSpaceView::render(const Shared<layers::Layer>& layer, bool* outCollapsed) {
    if (!layer) return;

    bool opened = ImGui::Begin("Input Space", nullptr,
                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    if (outCollapsed) *outCollapsed = ImGui::IsWindowCollapsed();

    if (opened) {
        // ---- Toolbar ----
        ImGui::Checkbox("Grid##inputgrid", &showGrid_);
        if (showGrid_) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            ImGui::SliderInt("X##inputgridx", &gridDivisionsX_, 2, 32);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            ImGui::SliderInt("Y##inputgridy", &gridDivisionsY_, 2, 32);
        }
        ImGui::Separator();

        // ---- Canvas area ----
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        viewPosition_ = Vec2(canvasPos.x, canvasPos.y);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();

        viewSize_ = Vec2(canvasSize.x, canvasSize.y);

        // Draw background
        drawList->AddRectFilled(
            canvasPos,
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
            IM_COL32(30, 30, 30, 255)
        );

        // Draw source texture filling the whole canvas
        auto source = layer->getSource();
        if (source && source->isValid() && source->getTextureHandle() != 0) {
            ImTextureID texId = (ImTextureID)(intptr_t)source->getTextureHandle();
            drawList->AddImage(
                texId,
                canvasPos,
                ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                ImVec2(0.f, 1.f),
                ImVec2(1.f, 0.f)
            );
        }

        if (showGrid_) renderGrid();

        renderShapeOverlay(layer);

        ImGui::InvisibleButton("InputSpaceCanvas", canvasSize,
                               ImGuiButtonFlags_MouseButtonLeft |
                               ImGuiButtonFlags_MouseButtonRight);
    }
    ImGui::End();
}

Vec2 InputSpaceView::getMouseInViewSpace(Vec2 screenMouse) const {
    return screenMouse - viewPosition_;
}

bool InputSpaceView::isMouseInView(Vec2 screenMouse) const {
    return screenMouse.x >= viewPosition_.x &&
           screenMouse.x <= viewPosition_.x + viewSize_.x &&
           screenMouse.y >= viewPosition_.y &&
           screenMouse.y <= viewPosition_.y + viewSize_.y;
}

void InputSpaceView::renderShapeOverlay(const Shared<layers::Layer>& layer) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    // Input corners are stored as UV [0..1] (ImGui y-down).
    // Scale to canvas pixels for display.
    auto corners = layer->getInputCorners();
    int numCorners = layer->getActiveCornerCount();  // 3 for triangle, 4 for others

    // Draw shape outline
    ImU32 color = IM_COL32(0, 255, 100, 255);
    for (int i = 0; i < numCorners; ++i) {
        ImVec2 p1(canvasPos.x + corners[i].x * viewSize_.x,
                  canvasPos.y + corners[i].y * viewSize_.y);
        ImVec2 p2(canvasPos.x + corners[(i + 1) % numCorners].x * viewSize_.x,
                  canvasPos.y + corners[(i + 1) % numCorners].y * viewSize_.y);
        drawList->AddLine(p1, p2, color, 2.0f);
    }

    // Draw corner handles with interaction
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;
    float cornerSize = 6.0f;

    // Release drag when mouse button is lifted
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        draggedCorner_ = -1;

    hoveredCorner_ = -1;
    for (int i = 0; i < numCorners; ++i) {
        ImVec2 cornerPos(canvasPos.x + corners[i].x * viewSize_.x,
                         canvasPos.y + corners[i].y * viewSize_.y);

        ImVec2 delta = ImVec2(mousePos.x - cornerPos.x, mousePos.y - cornerPos.y);
        float distSq = delta.x * delta.x + delta.y * delta.y;
        float hoverRadiusSq = (cornerSize * 3.0f) * (cornerSize * 3.0f);

        bool isHovered = distSq < hoverRadiusSq;

        // Begin drag on click
        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            draggedCorner_ = i;

        bool isDragging = (draggedCorner_ == i);
        if (isHovered && !isDragging)
            hoveredCorner_ = i;
        ImU32 cornerColor = (isHovered || isDragging)
            ? IM_COL32(255, 255, 100, 255)
            : IM_COL32(255, 100, 0, 255);

        drawList->AddCircleFilled(cornerPos, cornerSize, cornerColor);
        drawList->AddCircle(cornerPos, cornerSize,
                           IM_COL32(255, 255, 255, 255), 0, 2.0f);

        if (isHovered || isDragging)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        // Move the corner while this index is being dragged
        if (isDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            float u = glm::clamp((mousePos.x - canvasPos.x) / viewSize_.x, 0.0f, 1.0f);
            float v = glm::clamp((mousePos.y - canvasPos.y) / viewSize_.y, 0.0f, 1.0f);
            if (showGrid_ && gridDivisionsX_ > 1 && gridDivisionsY_ > 1) {
                u = std::round(u * gridDivisionsX_) / gridDivisionsX_;
                v = std::round(v * gridDivisionsY_) / gridDivisionsY_;
            }
            layer->setInputCorner(i, Vec2(u, v));
        }
    }

    // Arrow-key fine control for hovered (non-dragged) corner
    if (hoveredCorner_ >= 0 && !ImGui::GetIO().WantTextInput) {
        const float stepU = 1.0f / std::max(viewSize_.x, 1.0f);
        const float stepV = 1.0f / std::max(viewSize_.y, 1.0f);
        Vec2 pos = layer->getInputCorners()[hoveredCorner_];
        bool moved = false;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow,  true)) { pos.x -= stepU; moved = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) { pos.x += stepU; moved = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow,    true)) { pos.y -= stepV; moved = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow,  true)) { pos.y += stepV; moved = true; }
        if (moved) {
            layer->setInputCorner(hoveredCorner_, pos);
        }
        ImGui::BeginTooltip();
        Vec2 c = layer->getInputCorners()[hoveredCorner_];
        ImGui::Text("Corner %d: (%.4f, %.4f)", hoveredCorner_, c.x, c.y);
        ImGui::TextDisabled("Arrow keys: fine-tune position");
        ImGui::EndTooltip();
    }
}

void InputSpaceView::renderGrid() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImU32 gridColor = IM_COL32(200, 200, 200, 80);

    // Vertical lines (X divisions)
    for (int i = 1; i < gridDivisionsX_; ++i) {
        float x = canvasPos.x + (i / (float)gridDivisionsX_) * viewSize_.x;
        drawList->AddLine(
            ImVec2(x, canvasPos.y),
            ImVec2(x, canvasPos.y + viewSize_.y),
            gridColor, 1.0f);
    }

    // Horizontal lines (Y divisions)
    for (int i = 1; i < gridDivisionsY_; ++i) {
        float y = canvasPos.y + (i / (float)gridDivisionsY_) * viewSize_.y;
        drawList->AddLine(
            ImVec2(canvasPos.x, y),
            ImVec2(canvasPos.x + viewSize_.x, y),
            gridColor, 1.0f);
    }
}

} // namespace ui

