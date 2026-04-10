#pragma once

#include "core/types.hpp"
#include "layers/layer.hpp"
#include "gl/mesh.hpp"
#include "gl/shader_program.hpp"
#include "gl/framebuffer.hpp"
#include <memory>
#include <vector>

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
 * Shows all canvases side-by-side with per-canvas borders.
 * User can drag corner handles to perform corner-pinning.
 */
class OutputSpaceView {
public:
    OutputSpaceView();
    ~OutputSpaceView();

    /**
     * @brief Render output space with one or more canvases laid out left-to-right.
     * @param layers       All layers, drawn bottom-to-top within their respective canvas.
     * @param selectedIndex Index of the layer being edited (shows corner handles).
     * @param canvases     List of canvas configurations (aspect ratio, name, …).
     */
    void render(const std::vector<Shared<layers::Layer>>& layers,
                int selectedIndex,
                const std::vector<CanvasConfig>& canvases,
                bool* outCollapsed = nullptr);

    /**
     * @brief Handle mouse drag for corner-pinning
     */
    void onCornerDragged(int cornerIndex, Vec2 newPosition);

    /**
     * @brief Get which corner handle is at screen position (if any)
     */
    CornerHandle getCornerAtPosition(Vec2 screenPos,
                                     const Shared<layers::Layer>& layer);

    /**
     * @brief Get the canvas rectangle (panel-local pixel coords) for canvas i.
     * Valid after the first render() call.
     */
    Vec2 getCanvasLocalPos(int i = 0)  const;
    Vec2 getCanvasLocalSize(int i = 0) const;

    /**
     * @brief Get view region dimensions / position
     */
    Vec2 getViewSize()     const { return viewSize_; }
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
     * @brief Size of corner handles (for UI hit-testing)
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
    Vec2  viewSize_;
    Vec2  viewPosition_;
    float cornerHandleRadius_;
    bool  showGrid_;
    float gridSize_;

    Unique<gl::ShaderProgram> perspectiveShader_;
    Unique<gl::Mesh>          layerMesh_;

    // Perspective-correct preview rendering resources
    // One FBO per canvas so simultaneous AddImage calls reference distinct textures.
    std::vector<Unique<gl::Framebuffer>> canvasFbos_;
    std::vector<Vec2>                   canvasFboSizes_;
    unsigned int quadVAO_           = 0;
    unsigned int quadVBO_           = 0;
    bool         shaderInitialized_ = false;

    // Per-canvas rects (panel-local pixels), updated every render call
    std::vector<Vec2> canvasLocalPos_;
    std::vector<Vec2> canvasLocalSize_;

    int draggedCorner_ = -1;
    int hoveredCorner_ = -1;

    // ---- helpers ----
    void computeCanvasRects(const std::vector<CanvasConfig>& canvases,
                            std::vector<Vec2>& outPos,
                            std::vector<Vec2>& outSize) const;

    void renderGrid();
    bool initPerspectiveShader();

    /** Render all layers that belong to canvas ci into a temporary FBO,
     *  then blit that FBO to the ImGui draw list at the canvas screen position. */
    void renderCanvasContent(
            const std::vector<Shared<layers::Layer>>& layers,
            int canvasIndex,
            Vec2 canvasPos, Vec2 canvasSize,
            Vec2 screenTL);

    void renderCornerHandles(const Shared<layers::Layer>& layer,
                             int canvasIndex);

    void renderQADWithPerspective(
            const std::vector<Shared<layers::Layer>>& layers,
            int canvasIndex,
            Vec2 canvasPos, Vec2 canvasSize,
            Vec2 screenTL);
};

} // namespace ui

