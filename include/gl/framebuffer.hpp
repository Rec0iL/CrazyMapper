#pragma once

#include "core/types.hpp"
#include <vector>

namespace gl {

/**
 * @brief Framebuffer object for off-screen rendering
 */
class Framebuffer {
public:
    Framebuffer(int width, int height);
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    /**
     * @brief Bind framebuffer for rendering into
     */
    void bind() const;

    /**
     * @brief Unbind framebuffer (render to screen)
     */
    static void unbind();

    /**
     * @brief Get color attachment texture
     */
    unsigned int getColorTexture() const { return colorTexture_; }

    /**
     * @brief Get framebuffer dimensions
     */
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    /**
     * @brief Resize framebuffer
     */
    void resize(int width, int height);

private:
    unsigned int fbo_ = 0;
    unsigned int colorTexture_ = 0;
    unsigned int rbo_ = 0; // renderbuffer for depth/stencil
    int width_ = 0;
    int height_ = 0;

    void createFramebuffer();
};

} // namespace gl

