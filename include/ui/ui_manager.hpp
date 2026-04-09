#pragma once

#include "core/types.hpp"
#include "input_space_view.hpp"
#include "output_space_view.hpp"
#include "layers/layer.hpp"
#include "sources/source.hpp"
#include <imgui.h>
#include <memory>
#include <vector>
#include <functional>
#include <array>

struct GLFWwindow;

namespace ui {

/**
 * @brief Main UI manager for the projection mapping application
 * 
 * Handles the split-pane layout with source/destination views, layer
 * management, and interaction state.
 */
class UIManager {
public:
    UIManager();
    ~UIManager();

    /**
     * @brief Initialize UI (ImGui context setup, etc.)
     */
    bool initialize(GLFWwindow* window);

    /**
     * @brief Shutdown UI
     */
    void shutdown();

    /**
     * @brief Render full UI frame
     * @param layers  All layers
     * @param sources All available sources
     * @param deltaTime Frame time
     */
    void render(const std::vector<Shared<layers::Layer>>& layers,
                const std::vector<Shared<sources::Source>>& sources,
                float deltaTime,
                bool projectionWindowOpen = false);

    /**
     * @brief Handle mouse input
     */
    void onMouseMove(Vec2 screenPos);
    void onMouseButton(int button, int action, int mods);
    void onScroll(double xOffset, double yOffset);

    /**
     * @brief Get currently selected layer
     */
    int getSelectedLayerIndex() const { return selectedLayerIndex_; }
    void setSelectedLayerIndex(int index) { selectedLayerIndex_ = index; }

    /**
     * @brief ImGui demo window toggle
     */
    void setShowDemoWindow(bool show) { showDemoWindow_ = show; }

    /**
     * @brief Check/consume pending source-to-layer assignment.
     * Returns true once when the user clicked "Assign"; caller should
     * call getAssignSourceIndex() to read the chosen source index.
     */
    bool hasPendingSourceAssignment() {
        if (pendingAssignment_) { pendingAssignment_ = false; return true; }
        return false;
    }
    int  getAssignSourceIndex() const  { return assignSourceIndex_; }

    /**
     * @brief Check/consume pending layer reorder request.
     * Application should swap layers[getReorderFrom()] and layers[getReorderTo()].
     */
    bool hasPendingLayerReorder() {
        if (pendingReorder_) { pendingReorder_ = false; return true; }
        return false;
    }
    int getReorderFrom() const { return reorderFrom_; }
    int getReorderTo()   const { return reorderTo_; }

    /**
     * @brief Check/consume pending toggle of the projection window.
     */
    bool hasPendingToggleProjectionWindow() {
        if (pendingToggleProjWin_) { pendingToggleProjWin_ = false; return true; }
        return false;
    }

    /**
     * @brief Check/consume pending source creation request.
     * Call getNewSourceKind(), getNewSourceColor(), getNewImagePath() after this.
     */
    bool hasPendingAddSource() {
        if (pendingAddSource_) { pendingAddSource_ = false; return true; }
        return false;
    }
    int         getNewSourceKind()  const { return newSourceKind_; }
    const float* getNewSourceColor() const { return newSourceColor_; }
    std::string getNewImagePath()   const { return newSourceImagePath_; }
    std::string getNewVideoPath()      const { return newSourceVideoPath_; }

    /**
     * @brief Check/consume pending portal screen capture request.
     * Application should launch the portal dialog on a worker thread.
     */
    bool hasPendingPortalCapture() {
        if (pendingPortalCapture_) { pendingPortalCapture_ = false; return true; }
        return false;
    }
    void setPortalCapturePending(bool v) { portalCapturePending_ = v; }

    /**
     * @brief Check/consume pending layout save request.
     */
    bool hasPendingSaveLayout() {
        if (pendingSaveLayout_) { pendingSaveLayout_ = false; return true; }
        return false;
    }
    const std::string& getSaveLayoutPath() const { return saveLayoutPath_; }

    /**
     * @brief Check/consume pending layout load request.
     */
    bool hasPendingLoadLayout() {
        if (pendingLoadLayout_) { pendingLoadLayout_ = false; return true; }
        return false;
    }
    const std::string& getLoadLayoutPath() const { return loadLayoutPath_; }

    /**
     * @brief Show a warning/info popup on the next rendered frame.
     */
    void setLayoutWarning(const std::string& msg) { layoutWarning_ = msg; }

    /**
     * @brief Set canvas aspect ratio (used when loading a layout).
     */
    void setCanvasAspect(float w, float h) { canvasAspectW_ = w; canvasAspectH_ = h; }
    float getCanvasAspectW() const { return canvasAspectW_; }
    float getCanvasAspectH() const { return canvasAspectH_; }

    /**
     * @brief Canvas aspect ratio (width / height) chosen by the user.
     */
    float getCanvasAspectRatio() const {
        return canvasAspectH_ > 0.0f ? canvasAspectW_ / canvasAspectH_ : 16.0f / 9.0f;
    }

    /**
     * @brief Check if new layer should be created
     * @return True if "New Layer" was clicked, resets after reading
     */
    bool shouldCreateNewLayer() {
        if (createNewLayer_) {
            createNewLayer_ = false;
            return true;
        }
        return false;
    }

    /**
     * @brief Get next layer ID to use
     */
    unsigned int getNextLayerId() { return nextLayerId_++; }

    /**
     * @brief Get input/output views for advanced access
     */
    InputSpaceView* getInputSpaceView() { return inputView_.get(); }
    OutputSpaceView* getOutputSpaceView() { return outputView_.get(); }

private:
    Unique<InputSpaceView> inputView_;
    Unique<OutputSpaceView> outputView_;

    int selectedLayerIndex_;
    bool showDemoWindow_;
    
    // Input state tracking
    Vec2 lastMousePos_;
    int draggedCorner_;
    bool isDraggingCorner_;
    
    // Layer management
    unsigned int nextLayerId_;
    bool createNewLayer_;

    // Source assignment
    bool pendingAssignment_;
    int  assignSourceIndex_;
    int  selectedSourceIndex_;

    // Layer reorder
    bool pendingReorder_ = false;
    int  reorderFrom_    = -1;
    int  reorderTo_      = -1;

    // Projection window
    bool pendingToggleProjWin_ = false;
    bool projectionWindowOpen_ = false;

    // Canvas aspect ratio
    float canvasAspectW_ = 16.0f;
    float canvasAspectH_ =  9.0f;

    // Shape editing — remember params across shape type switches
    float lastShapeRadius_ = 0.1f;
    int   lastShapeSides_  = 6;
    bool  lastShapePerCorner_ = false;
    std::array<float, 4> lastShapeCornerRadii_ = {0.1f, 0.1f, 0.1f, 0.1f};

    // New source creation state
    bool  pendingAddSource_   = false;
    int   newSourceKind_      = 0;    // 0=Solid 1=Checker 2=Gradient 3=Image 4=Video 5=Shader
    float newSourceColor_[3]  = {0.5f, 0.5f, 0.5f};
    char  newSourceImagePath_[512] = {};
    char  newSourceVideoPath_[512] = {};
    // PipeWire portal capture
    bool  pendingPortalCapture_  = false;
    bool  portalCapturePending_  = false;  // true while worker thread is running

    // Layout save / load
    bool        pendingSaveLayout_ = false;
    bool        pendingLoadLayout_ = false;
    std::string saveLayoutPath_;
    std::string loadLayoutPath_;
    std::string layoutWarning_;
    bool        showLayoutWarning_ = false;

    void renderMainMenuBar(bool projectionWindowOpen);
    void renderSourcesPanel(const std::vector<Shared<sources::Source>>& sources,
                            const Shared<layers::Layer>& currentLayer);
    void renderLayerPanel(std::vector<Shared<layers::Layer>>& layers);
    void renderPropertyPanel(const Shared<layers::Layer>& layer);
    void renderCanvasSettings();
    
    // Layer creation helper
    friend class ProjectionMapper;
};

} // namespace ui

