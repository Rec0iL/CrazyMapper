#pragma once

#include "core/types.hpp"
#include "layers/layer.hpp"
#include "gl/texture.hpp"
#include <memory>

namespace ui {

/**
 * @brief Manages input space view (raw source media display)
 * 
 * Shows the source texture with an overlay of the layer shape.
 * User can drag/scale the shape to select which part of input to capture.
 */
class InputSpaceView {
public:
    InputSpaceView();
    ~InputSpaceView();

    /**
     * @brief Render input space (ImGui window).
     * @param layer        Current layer to display.
     * @param outCollapsed If non-null, written with true when the window is collapsed.
     */
    void render(const Shared<layers::Layer>& layer, bool* outCollapsed = nullptr);

    /**
     * @brief Get view region dimensions
     */
    Vec2 getViewSize() const { return viewSize_; }

    /**
     * @brief Get view region bottom-left corner (ImGui coordinates)
     */
    Vec2 getViewPosition() const { return viewPosition_; }

    /**
     * @brief Get mouse position in view coords (if over view)
     */
    Vec2 getMouseInViewSpace(Vec2 screenMouse) const;

    /**
     * @brief Check if mouse is inside view region
     */
    bool isMouseInView(Vec2 screenMouse) const;

private:
    Vec2 viewSize_;
    Vec2 viewPosition_;
    bool showGrid_;
    int  gridDivisionsX_;
    int  gridDivisionsY_;
    int  draggedCorner_ = -1;  // index of corner being dragged, -1 = none
    int  hoveredCorner_ = -1;   // index of corner hovered for arrow-key fine control

    void renderShapeOverlay(const Shared<layers::Layer>& layer);
    void renderGrid();
};

} // namespace ui

