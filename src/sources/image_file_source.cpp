#include "sources/image_file_source.hpp"
#include <glad/glad.h>
#include <stb_image.h>
#include <iostream>

namespace sources {

ImageFileSource::ImageFileSource(const std::string& filePath)
    : filePath_(filePath) {}

bool ImageFileSource::initialize() {
    if (textureHandle_) return true;   // already loaded
    if (filePath_.empty()) return false;

    // GL convention: y=0 at bottom; stb loads y=0 at top so we flip.
    stbi_set_flip_vertically_on_load(true);

    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load(filePath_.c_str(), &w, &h, &channels, 4);
    if (!data) {
        std::cerr << "[ImageFileSource] Failed to load '" << filePath_
                  << "': " << stbi_failure_reason() << "\n";
        return false;
    }

    glGenTextures(1, &textureHandle_);
    glBindTexture(GL_TEXTURE_2D, textureHandle_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);

    resolution_ = Vec2(static_cast<float>(w), static_cast<float>(h));
    return true;
}

void ImageFileSource::shutdown() {
    if (textureHandle_) {
        glDeleteTextures(1, &textureHandle_);
        textureHandle_ = 0;
    }
    resolution_ = {};
}

std::string ImageFileSource::getName() const {
    if (filePath_.empty()) return "Image (no file)";
    auto pos = filePath_.find_last_of("/\\");
    return (pos != std::string::npos) ? filePath_.substr(pos + 1) : filePath_;
}

} // namespace sources
