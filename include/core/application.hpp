#pragma once

#include "core/types.hpp"
#include "core/projection_window.hpp"
#include "ui/ui_manager.hpp"
#include "layers/layer.hpp"
#include "sources/source.hpp"
#include "sources/portal_screencast.hpp"
#include <memory>
#include <vector>
#include <future>

struct GLFWwindow;

/**
 * @brief Main application class
 * 
 * Orchestrates the projection mapping workflow:
 * - Manages layers and sources
 * - Updates rendering
 * - Handles input/UI interaction
 * - Runs main loop
 */
class ProjectionMapper {
public:
    ProjectionMapper(int windowWidth = 1600, int windowHeight = 900);
    ~ProjectionMapper();

    /**
     * @brief Initialize the application
     */
    bool initialize();

    /**
     * @brief Run main event loop
     */
    void run();

    /**
     * @brief Request application shutdown
     */
    void requestShutdown() { shouldClose_ = true; }

    /**
     * @brief Add a source to the application
     */
    void addSource(Shared<sources::Source> source);

    /**
     * @brief Create a new layer with given source
     */
    Shared<layers::Layer> createLayer(Shared<sources::Source> source,
                                      Unique<layers::Shape> shape);

    /**
     * @brief Get all layers
     */
    const std::vector<Shared<layers::Layer>>& getLayers() const { return layers_; }

    /**
     * @brief Get all sources
     */
    const std::vector<Shared<sources::Source>>& getSources() const { return sources_; }

private:
    int windowWidth_;
    int windowHeight_;
    GLFWwindow* window_;
    Unique<ui::UIManager> uiManager_;
    Unique<ProjectionWindow> projectionWindow_;

    std::vector<Shared<sources::Source>> sources_;
    std::vector<Shared<layers::Layer>> layers_;

    double lastFrameTime_;
    bool shouldClose_;
    unsigned int nextLayerId_;

    // XDG portal ScreenCast — runs on a worker thread
    std::future<sources::PortalScreenCastResult> portalFuture_;

    // GLFW callbacks (static)
    static ProjectionMapper* instancePtr_;
    static void framebufferSizeCallback(GLFWwindow* w, int width, int height);
    static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* w, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* w, double xoff, double yoff);
    static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods);

    // Internal lifecycle
    bool initializeGLFW();
    bool initializeOpenGL();
    bool initializeImGui();
    
    void update(float deltaTime);
    void render();
    void cleanup();

    // Layout serialization
    void saveLayout(const std::string& path);
    void loadLayout(const std::string& path);

    // Callback bridge methods
    void onFramebufferSize(int width, int height);
    void onMouseButton(int button, int action, int mods);
    void onCursorPos(double xpos, double ypos);
    void onScroll(double xoff, double yoff);
    void onKey(int key, int scancode, int action, int mods);
};

