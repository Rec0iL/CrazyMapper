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
     * @param projWindowStates  Open state per canvas (index matches canvas list).
     */
    void render(const std::vector<Shared<layers::Layer>>& layers,
                const std::vector<Shared<sources::Source>>& sources,
                float deltaTime,
                const std::vector<bool>& projWindowStates = {});

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
     * @brief Check/consume pending layer delete request.
     */
    bool hasPendingDeleteLayer() {
        if (pendingDeleteLayer_) { pendingDeleteLayer_ = false; return true; }
        return false;
    }
    int getDeleteLayerIndex() const { return deleteLayerIndex_; }

    /**
     * @brief Check/consume pending source delete request.
     */
    bool hasPendingDeleteSource() {
        if (pendingDeleteSource_) { pendingDeleteSource_ = false; return true; }
        return false;
    }
    int getDeleteSourceIndex() const { return deleteSourceIndex_; }

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
     * @brief Check/consume pending source-to-all-layers assignment.
     */
    bool hasPendingAssignAllLayers() {
        if (pendingAssignAll_) { pendingAssignAll_ = false; return true; }
        return false;
    }
    int  getAssignAllSourceIndex() const { return assignAllSourceIndex_; }

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
     * @param outCanvasIndex  Set to which canvas index to toggle.
     */
    bool hasPendingToggleProjectionWindow(int& outCanvasIndex) {
        if (pendingToggleProjWin_) {
            pendingToggleProjWin_ = false;
            outCanvasIndex = pendingToggleProjWinIndex_;
            return true;
        }
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
    // Returns the kind value expected by application.cpp:
    //   0-3 = pattern subtypes (solid/checker/gradient/calib),
    //   4=Image, 5=Video, 6=Shader, 7=PipeWire
    int         getNewSourceKind()  const {
        if (newSourceKind_ == 0) return newPatternKind_;  // 0..3
        return newSourceKind_ + 3;  // Image→4, Video→5, Shader→6, PipeWire→7
    }
    const float* getNewSourceColor()  const { return newSourceColor_; }
    const float* getNewSourceColor2() const { return newSourceColor2_; }
    std::string getNewImagePath()   const { return newSourceImagePath_; }
    std::string getNewVideoPath()      const { return newSourceVideoPath_; }
    std::string getNewShaderPath()     const { return newSourceShaderPath_; }

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
    void setCanvasAspect(float w, float h, int idx = 0) {
        if (idx >= 0 && idx < static_cast<int>(canvases_.size())) {
            canvases_[idx].aspectW = w;
            canvases_[idx].aspectH = h;
        }
    }
    float getCanvasAspectW(int idx = 0) const {
        return (idx >= 0 && idx < static_cast<int>(canvases_.size()))
               ? canvases_[idx].aspectW : 16.0f;
    }
    float getCanvasAspectH(int idx = 0) const {
        return (idx >= 0 && idx < static_cast<int>(canvases_.size()))
               ? canvases_[idx].aspectH : 9.0f;
    }

    /**
     * @brief Canvas aspect ratio (width / height) for canvas idx.
     */
    float getCanvasAspectRatio(int idx = 0) const {
        if (idx >= 0 && idx < static_cast<int>(canvases_.size()))
            return canvases_[idx].getAspectRatio();
        return 16.0f / 9.0f;
    }

    /**
     * @brief Access the full canvas list (for rendering, save/load).
     */
    const std::vector<CanvasConfig>& getCanvases() const { return canvases_; }
    int getCanvasCount() const { return static_cast<int>(canvases_.size()); }

    /**
     * @brief Add a canvas via Application response (keeps projWindowStates in sync).
     */
    void onCanvasAdded() {
        int n = static_cast<int>(canvases_.size()) + 1;
        CanvasConfig cfg;
        cfg.name    = "Canvas " + std::to_string(n);
        cfg.aspectW = 16.0f;
        cfg.aspectH =  9.0f;
        canvases_.push_back(cfg);
    }

    /**
     * @brief Remove a canvas (Application calls this after closing the window).
     * Idx 0 (the default canvas) cannot be deleted.
     */
    void onCanvasDeleted(int idx) {
        if (idx > 0 && idx < static_cast<int>(canvases_.size()))
            canvases_.erase(canvases_.begin() + idx);
    }

    /**
     * @brief Check/consume pending canvas-add request.
     */
    bool hasPendingAddCanvas() {
        if (pendingAddCanvas_) { pendingAddCanvas_ = false; return true; }
        return false;
    }

    /**
     * @brief Check/consume pending canvas-delete request.
     */
    bool hasPendingDeleteCanvas() {
        if (pendingDeleteCanvas_) { pendingDeleteCanvas_ = false; return true; }
        return false;
    }
    int getDeleteCanvasIndex() const { return deleteCanvasIndex_; }

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
    bool pendingAssignAll_    = false;
    int  assignAllSourceIndex_ = -1;

    // Layer reorder
    bool pendingReorder_ = false;
    int  reorderFrom_    = -1;
    int  reorderTo_      = -1;

    // Layer / source deletion
    bool pendingDeleteLayer_  = false;
    int  deleteLayerIndex_    = -1;
    bool pendingDeleteSource_ = false;
    int  deleteSourceIndex_   = -1;

    // Per-panel collapse state (updated each frame, used next frame for layout)
    bool inputCollapsed_   = false;
    bool outputCollapsed_  = false;
    bool sourcesCollapsed_   = false;
    bool addSourceCollapsed_ = false;
    bool canvasCollapsed_    = false;
    bool layersCollapsed_  = false;
    bool propsCollapsed_   = false;

    // Projection window
    bool pendingToggleProjWin_      = false;
    int  pendingToggleProjWinIndex_ = 0;    // which canvas to toggle
    std::vector<bool> projWindowStates_;    // open state per canvas (updated each frame)

    // Canvas configs
    std::vector<CanvasConfig> canvases_;    // index 0 = default (never deleted)
    bool pendingAddCanvas_    = false;
    bool pendingDeleteCanvas_ = false;
    int  deleteCanvasIndex_   = -1;

    // Shape editing — remember params across shape type switches
    float lastShapeRadius_ = 0.1f;
    int   lastShapeSides_  = 6;
    bool  lastShapePerCorner_ = false;
    std::array<float, 4> lastShapeCornerRadii_ = {0.1f, 0.1f, 0.1f, 0.1f};

    // New source creation state
    bool  pendingAddSource_   = false;
    int   newSourceKind_      = 0;    // 0=Pattern 1=Image 2=Video 3=Shader 4=PipeWire
    int   newPatternKind_     = 0;    // 0=Solid Color 1=Checkerboard 2=Gradient 3=Calibration Grid
    float newSourceColor_[3]  = {0.5f, 0.5f, 0.5f};
    float newSourceColor2_[3] = {0.08f, 0.08f, 0.08f};  ///< checkerboard background colour
    char  newSourceImagePath_[512] = {};
    char  newSourceVideoPath_[512]   = {};
    char  newSourceShaderPath_[512]  = {};
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
                            const Shared<layers::Layer>& currentLayer,
                            bool* outCollapsed = nullptr);
    void renderAddSourcePanel(bool* outCollapsed = nullptr);
    void renderLayerPanel(std::vector<Shared<layers::Layer>>& layers,
                          bool* outCollapsed = nullptr);
    void renderPropertyPanel(const Shared<layers::Layer>& layer,
                             bool* outCollapsed = nullptr);
    void renderCanvasSettings(bool* outCollapsed = nullptr);
    
    // Layer creation helper
    friend class ProjectionMapper;
};

} // namespace ui

