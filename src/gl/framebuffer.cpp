#include "gl/framebuffer.hpp"
#include <glad/glad.h>
#include <iostream>

namespace gl {

Framebuffer::Framebuffer(int width, int height)
    : width_(width), height_(height) {
    createFramebuffer();
}

Framebuffer::~Framebuffer() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (colorTexture_) glDeleteTextures(1, &colorTexture_);
    if (rbo_) glDeleteRenderbuffers(1, &rbo_);
}

void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
}

void Framebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::resize(int width, int height) {
    width_ = width;
    height_ = height;

    if (fbo_) { glDeleteFramebuffers(1, &fbo_);          fbo_          = 0; }
    if (colorTexture_) { glDeleteTextures(1, &colorTexture_); colorTexture_ = 0; }
    if (rbo_) { glDeleteRenderbuffers(1, &rbo_);         rbo_          = 0; }

    createFramebuffer();
}

void Framebuffer::createFramebuffer() {
    // A zero or negative dimension yields an incomplete FBO; clamp so callers
    // that briefly report an empty panel size still get a usable target.
    if (width_  < 1) width_  = 1;
    if (height_ < 1) height_ = 1;

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    // Create color texture
    glGenTextures(1, &colorTexture_);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, colorTexture_, 0);

    // Create renderbuffer for depth/stencil
    glGenRenderbuffers(1, &rbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width_, height_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, rbo_);

    // Check completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    complete_ = (status == GL_FRAMEBUFFER_COMPLETE);
    if (!complete_) {
        std::cerr << "Framebuffer: incomplete (status 0x" << std::hex << status
                  << std::dec << ") at " << width_ << "x" << height_ << "\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace gl

