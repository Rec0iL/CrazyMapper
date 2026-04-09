#include "gl/texture.hpp"
#include <glad/glad.h>

namespace gl {

Texture::~Texture() {
    if (textureHandle_) {
        glDeleteTextures(1, &textureHandle_);
        textureHandle_ = 0;
    }
}

bool Texture::create(int width, int height, unsigned int format) {
    width_ = width;
    height_ = height;

    if (!textureHandle_) {
        glGenTextures(1, &textureHandle_);
    }

    glBindTexture(GL_TEXTURE_2D, textureHandle_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 format, GL_UNSIGNED_BYTE, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

bool Texture::uploadData(const void* data, int width, int height,
                         unsigned int format) {
    if (!textureHandle_) {
        if (!create(width, height, format)) return false;
    }

    glBindTexture(GL_TEXTURE_2D, textureHandle_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    format, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    width_ = width;
    height_ = height;
    return true;
}

void Texture::bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, textureHandle_);
}

void Texture::setLinearFilter() {
    if (!textureHandle_) return;
    glBindTexture(GL_TEXTURE_2D, textureHandle_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::setNearestFilter() {
    if (!textureHandle_) return;
    glBindTexture(GL_TEXTURE_2D, textureHandle_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::setWrapMode(unsigned int wrapMode) {
    if (!textureHandle_) return;
    glBindTexture(GL_TEXTURE_2D, textureHandle_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace gl

