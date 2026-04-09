#pragma once

#include "core/types.hpp"

namespace gl {

/**
 * @brief Manages OpenGL texture resources
 */
class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    /**
     * @brief Create empty texture
     * @param width Texture width
     * @param height Texture height
     * @param format OpenGL texture format (GL_RGB, GL_RGBA, etc.)
     */
    bool create(int width, int height, unsigned int format = 0x1908); // GL_RGBA

    /**
     * @brief Upload image data to GPU
     */
    bool uploadData(const void* data, int width, int height, 
                   unsigned int format = 0x1908); // GL_RGBA

    /**
     * @brief Bind texture to texture unit
     */
    void bind(int unit = 0) const;

    /**
     * @brief Get texture handle
     */
    unsigned int getHandle() const { return textureHandle_; }

    /**
     * @brief Get dimensions
     */
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    /**
     * @brief Enable linear filtering
     */
    void setLinearFilter();

    /**
     * @brief Enable nearest neighbor filtering
     */
    void setNearestFilter();

    /**
     * @brief Set texture wrapping mode
     */
    void setWrapMode(unsigned int wrapMode); // GL_REPEAT, GL_CLAMP_TO_EDGE, etc.

private:
    unsigned int textureHandle_ = 0;
    int width_ = 0;
    int height_ = 0;
};

} // namespace gl

