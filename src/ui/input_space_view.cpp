#include "ui/input_space_view.hpp"
#include <imgui.h>
#include <glm/glm.hpp>
#include <algorithm>

namespace ui {

InputSpaceView::InputSpaceView()
    : viewSize_(800.0f, 600.0f),
      viewPosition_(0.0f),
      showGrid_(true),
      gridSize_(50.0f) {
}

InputSpaceView::~InputSpaceView() {
}

void InputSpaceView::render(const Shared<layers::Layer>& layer) {
    if (!layer) return;

    if (ImGui::Begin("Input Space", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        viewPosition_ = Vec2(pos.x, pos.y);
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();

        viewSize_ = Vec2(canvasSize.x, canvasSize.y);

        // Draw background
        drawList->AddRectFilled(
            canvasPos,
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
            IM_COL32(30, 30, 30, 255)
        );

        if (showGrid_) {
            renderGrid();
        }

        // Draw source texture filling the whole canvas
        auto source = layer->getSource();
        if (source && source->isValid() && source->getTextureHandle() != 0) {
            ImTextureID texId = (ImTextureID)(intptr_t)source->getTextureHandle();
            // ImGui expects UVs: (0,0)=top-left but OpenGL FBOs have (0,0)=bottom-left
            // Flip V to correct orientation
            drawList->AddImage(
                texId,
                canvasPos,
                ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                ImVec2(0.f, 1.f),   // UV top-left  (flipped)
                ImVec2(1.f, 0.f)    // UV bot-right (flipped)
            );
        }

        // Draw layer shape overlay
        renderShapeOverlay(layer);

        // Detect mouse interaction
        ImGui::InvisibleButton("InputSpaceCanvas", canvasSize,
                               ImGuiButtonFlags_MouseButtonLeft |
                               ImGuiButtonFlags_MouseButtonRight);

        ImGui::End();
    }
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

    // Draw quadrilateral outline
    ImU32 color = IM_COL32(0, 255, 100, 255);
    for (int i = 0; i < 4; ++i) {
        ImVec2 p1(canvasPos.x + corners[i].x * viewSize_.x,
                  canvasPos.y + corners[i].y * viewSize_.y);
        ImVec2 p2(canvasPos.x + corners[(i + 1) % 4].x * viewSize_.x,
                  canvasPos.y + corners[(i + 1) % 4].y * viewSize_.y);
        drawList->AddLine(p1, p2, color, 2.0f);
    }

    // Draw corner handles with interaction
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;
    float cornerSize = 6.0f;

    for (int i = 0; i < 4; ++i) {
        ImVec2 cornerPos(canvasPos.x + corners[i].x * viewSize_.x,
                         canvasPos.y + corners[i].y * viewSize_.y);
        
        // Calculate distance to corner
        ImVec2 delta = ImVec2(mousePos.x - cornerPos.x, mousePos.y - cornerPos.y);
        float distSq = delta.x * delta.x + delta.y * delta.y;
        float hoverRadiusSq = (cornerSize * 3.0f) * (cornerSize * 3.0f);
        
        bool isHovered = distSq < hoverRadiusSq;
        ImU32 cornerColor = isHovered ? IM_COL32(255, 255, 100, 255) : IM_COL32(255, 100, 0, 255);
        
        drawList->AddCircleFilled(cornerPos, cornerSize, cornerColor);
        drawList->AddCircle(cornerPos, cornerSize,
                           IM_COL32(255, 255, 255, 255), 0, 2.0f);
        
        if (isHovered) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        // Handle dragging: store as UV [0..1]
        if (isHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            float u = glm::clamp((mousePos.x - canvasPos.x) / viewSize_.x, 0.0f, 1.0f);
            float v = glm::clamp((mousePos.y - canvasPos.y) / viewSize_.y, 0.0f, 1.0f);
            layer->setInputCorner(i, Vec2(u, v));
        }
    }
}

void InputSpaceView::renderGrid() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImU32 gridColor = IM_COL32(100, 100, 100, 100);

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

} // namespace ui

