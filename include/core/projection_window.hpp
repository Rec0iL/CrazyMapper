#pragma once

#include "core/types.hpp"
#include "layers/layer.hpp"
#include "gl/shader_program.hpp"
#include <vector>
#include <memory>

struct GLFWwindow;

/**
 * @brief Separate output window for final projection display.
 *
 * Renders all layers composited in correct order directly via OpenGL.
 * Uses a GL context shared with the main window so source textures created
 * on the main context are directly accessible here.
 *
 * Key bindings (active on this window):
 *   F   — toggle fullscreen / windowed
 *   ESC — exit fullscreen (return to windowed)
 */
class ProjectionWindow {
public:
    ProjectionWindow();
    ~ProjectionWindow();

    ProjectionWindow(const ProjectionWindow&) = delete;
    ProjectionWindow& operator=(const ProjectionWindow&) = delete;

    /**
     * @brief Open the projection window.
     * @param shareContext The main GLFW window used for GL context sharing.
     * @return true on success.
     */
    bool open(GLFWwindow* shareContext);

    /**
     * @brief Close and destroy the projection window.
     */
    void close();

    bool isOpen() const { return window_ != nullptr; }

    /**
     * @brief Toggle between fullscreen and windowed mode.
     * Goes fullscreen on the monitor containing the window center.
     */
    void toggleFullscreen();
    bool isFullscreen() const { return isFullscreen_; }

    /**
     * @brief Render all (visible) layers onto the projection output.
     *
     * @param layers        All layers in bottom-to-top draw order.
     * @param canvasLocalPos  Canvas rectangle TL in output-panel local pixels.
     * @param canvasLocalSize Canvas rectangle size in output-panel local pixels.
     *
     * Output corners stored in each layer are in output-panel pixel coords.
     * They are normalised by the canvas rect before the homography is computed,
     * so the projection window fills its entire framebuffer with the canvas.
     */
    void render(const std::vector<Shared<layers::Layer>>& layers,
                Vec2 canvasLocalPos,
                Vec2 canvasLocalSize);

private:
    GLFWwindow* window_    = nullptr;
    bool isFullscreen_     = false;
    float aspectRatio_     = 16.0f / 9.0f;

    // Saved windowed position/size (for restoring from fullscreen)
    int savedX_ = 100, savedY_ = 100;
    int savedW_ = 1280, savedH_ = 720;

    // GL resources — created lazily in the projection window's context
    Unique<gl::ShaderProgram> shader_;
    unsigned int quadVAO_ = 0;
    unsigned int quadVBO_ = 0;
    bool shaderInit_      = false;

    bool initShader();

    // Per-window callbacks (dispatched via glfwGetWindowUserPointer)
    static void keyCallback(GLFWwindow* w, int key, int sc, int action, int mods);
    static void closeCallback(GLFWwindow* w);
};
