#pragma once

#include "core/types.hpp"
#include "layers/layer.hpp"
#include "gl/mesh.hpp"
#include "gl/shader_program.hpp"
#include "gl/framebuffer.hpp"
#include <memory>

namespace ui {

enum class CornerHandle {
    TOP_LEFT = 0,
    TOP_RIGHT = 1,
    BOTTOM_RIGHT = 2,
    BOTTOM_LEFT = 3,
    NONE = 4
};

/**
 * @brief Manages output space view (corner-pinning/projection display)
 * 
 * Shows the captured layer content with perspective distortion applied.
 * User can drag the corner points to perform corner-pinning.
 */
class OutputSpaceView {
public:
    OutputSpaceView();
    ~OutputSpaceView();

    /**
     * @brief Render output space.
     * @param layers      All layers, drawn bottom-to-top.
     * @param selectedIndex Index of the layer being edited (shows corner handles).
     * @param canvasAspect Canvas width/height ratio for the visible canvas rect.
     */
    void render(const std::vector<Shared<layers::Layer>>& layers,
                int selectedIndex,
                float canvasAspect);

    /**
     * @brief Handle mouse drag for corner-pinning
     * @param cornerIndex Which corner (0-3: TL, TR, BR, BL)
     * @param newPosition New position in screen space
     */
    void onCornerDragged(int cornerIndex, Vec2 newPosition);

    /**
     * @brief Get which corner handle is at screen position (if any)
     */
    CornerHandle getCornerAtPosition(Vec2 screenPos, 
                                     const Shared<layers::Layer>& layer);

    /**
     * @brief Get the canvas rectangle in panel-local pixel coordinates.
     * Valid after the first render() call.
     */
    Vec2 getCanvasLocalPos()  const { return canvasLocalPos_; }
    Vec2 getCanvasLocalSize() const { return canvasLocalSize_; }

    /**
     * @brief Get view region dimensions
     */
    Vec2 getViewSize() const { return viewSize_; }

    /**
     * @brief Get view position (top-left in screen coords)
     */
    Vec2 getViewPosition() const { return viewPosition_; }

    /**
     * @brief Convert screen coordinates to view coordinates
     */
    Vec2 screenToViewCoords(Vec2 screenPos) const;

    /**
     * @brief Check if mouse is inside view
     */
    bool isMouseInView(Vec2 screenMouse) const;

    /**
     * @brief Asset size of corner handles (for UI hit-testing)
     */
    float getCornerHandleRadius() const { return cornerHandleRadius_; }

    /**
     * @brief Reset output corners to match input
     */
    void resetCorners(const Shared<layers::Layer>& layer);

    /**
     * @brief Save/load corner positions from file
     */
    bool saveCornerPoints(const std::string& filepath, 
                         const Shared<layers::Layer>& layer);
    bool loadCornerPoints(const std::string& filepath,
                         Shared<layers::Layer>& layer);

private:
    Vec2 viewSize_;
    Vec2 viewPosition_;
    float cornerHandleRadius_;
    bool showGrid_;
    float gridSize_;

    Unique<gl::ShaderProgram> perspectiveShader_;
    Unique<gl::Mesh> layerMesh_;

    // Perspective-correct preview rendering resources
    Unique<gl::Framebuffer> previewFbo_;
    unsigned int quadVAO_ = 0;
    unsigned int quadVBO_ = 0;
    bool shaderInitialized_ = false;
    Vec2 lastFboSize_ = Vec2(0.0f);

    // Canvas rect (updated every render call, panel-local pixels)
    Vec2 canvasLocalPos_  = Vec2(0.0f);
    Vec2 canvasLocalSize_ = Vec2(1.0f);
    int  draggedCorner_   = -1;  // index of output corner being dragged, -1 = none

    void computeCanvasRect(float aspect, Vec2& outPos, Vec2& outSize) const;

    void renderQADWithPerspective(const std::vector<Shared<layers::Layer>>& layers,
                                  Vec2 canvasPos, Vec2 canvasSize,
                                  Vec2 screenTL);
    void renderCornerHandles(const Shared<layers::Layer>& layer);
    void renderGrid();

    bool initPerspectiveShader();
};

} // namespace ui

