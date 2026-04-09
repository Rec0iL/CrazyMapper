#include "sources/shader_source.hpp"
#include <glad/glad.h>
#include <iostream>

namespace sources {

ShaderSource::ShaderSource(const std::string& fragmentShaderCode, 
                           int width, int height)
    : fragmentShaderCode_(fragmentShaderCode),
      shaderProgram_(0),
      framebufferObject_(0),
      textureHandle_(0),
      quadVAO_(0),
      quadVBO_(0),
      resolution_(float(width), float(height)),
      initialized_(false),
      elapsedTime_(0.0f) {
}

ShaderSource::~ShaderSource() {
    shutdown();
}

bool ShaderSource::initialize() {
    if (initialized_) return true;

    // Basic vertex shader for fullscreen quad
    const char* vertexSrc = R"(
        #version 330 core
        layout(location = 0) in vec2 position;
        void main() {
            gl_Position = vec4(position, 0.0, 1.0);
        }
    )";

    if (!compileShader(vertexSrc, fragmentShaderCode_)) {
        return false;
    }

    createFramebuffer();
    createQuad();
    initialized_ = true;
    return true;
}

void ShaderSource::shutdown() {
    if (!initialized_) return;

    if (shaderProgram_) {
        glDeleteProgram(shaderProgram_);
        shaderProgram_ = 0;
    }
    if (framebufferObject_) {
        glDeleteFramebuffers(1, &framebufferObject_);
        framebufferObject_ = 0;
    }
    if (textureHandle_) {
        glDeleteTextures(1, &textureHandle_);
        textureHandle_ = 0;
    }
    if (quadVAO_) {
        glDeleteVertexArrays(1, &quadVAO_);
        quadVAO_ = 0;
    }
    if (quadVBO_) {
        glDeleteBuffers(1, &quadVBO_);
        quadVBO_ = 0;
    }

    initialized_ = false;
}

bool ShaderSource::update(float deltaTime) {
    if (!initialized_ || !shaderProgram_) return false;

    elapsedTime_ += deltaTime;

    // Render to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferObject_);
    glViewport(0, 0, (int)resolution_.x, (int)resolution_.y);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram_);
    
    // Set shader uniforms
    int timeLoc = glGetUniformLocation(shaderProgram_, "iTime");
    if (timeLoc >= 0) glUniform1f(timeLoc, elapsedTime_);

    int resLoc = glGetUniformLocation(shaderProgram_, "iResolution");
    if (resLoc >= 0) glUniform2f(resLoc, resolution_.x, resolution_.y);

    // Draw fullscreen quad
    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

unsigned int ShaderSource::getTextureHandle() const {
    return textureHandle_;
}

Vec2 ShaderSource::getResolution() const {
    return resolution_;
}

bool ShaderSource::isValid() const {
    return initialized_ && shaderProgram_ != 0;
}

std::string ShaderSource::getName() const {
    return "Shader: Procedural";
}

void ShaderSource::setUniform(const std::string& uniformName, float value) {
    if (!shaderProgram_) return;
    glUseProgram(shaderProgram_);
    int loc = glGetUniformLocation(shaderProgram_, uniformName.c_str());
    if (loc >= 0) glUniform1f(loc, value);
}

bool ShaderSource::recompileShader(const std::string& newFragmentCode) {
    fragmentShaderCode_ = newFragmentCode;
    
    const char* vertexSrc = R"(
        #version 330 core
        layout(location = 0) in vec2 position;
        void main() {
            gl_Position = vec4(position, 0.0, 1.0);
        }
    )";

    return compileShader(vertexSrc, fragmentShaderCode_);
}

bool ShaderSource::compileShader(const std::string& vertexSrc,
                                 const std::string& fragmentSrc) {
    auto compile = [](GLenum type, const std::string& src) -> unsigned int {
        unsigned int shader = glCreateShader(type);
        const char* ptr = src.c_str();
        glShaderSource(shader, 1, &ptr, nullptr);
        glCompileShader(shader);
        int ok;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::cerr << "Shader compile error: " << log << "\n";
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    };

    unsigned int vert = compile(GL_VERTEX_SHADER,   vertexSrc);
    unsigned int frag = compile(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!vert || !frag) {
        if (vert) glDeleteShader(vert);
        if (frag) glDeleteShader(frag);
        return false;
    }

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    glDeleteShader(vert);
    glDeleteShader(frag);

    int ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::cerr << "Shader link error: " << log << "\n";
        glDeleteProgram(prog);
        return false;
    }

    if (shaderProgram_) glDeleteProgram(shaderProgram_);
    shaderProgram_ = prog;
    return true;
}

void ShaderSource::createFramebuffer() {
    // Color texture
    glGenTextures(1, &textureHandle_);
    glBindTexture(GL_TEXTURE_2D, textureHandle_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 (int)resolution_.x, (int)resolution_.y,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Framebuffer
    glGenFramebuffers(1, &framebufferObject_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferObject_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, textureHandle_, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ShaderSource: framebuffer incomplete\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShaderSource::createQuad() {
    // Fullscreen triangle strip: TL, BL, TR, BR (NDC)
    float verts[] = {
        -1.f,  1.f,
        -1.f, -1.f,
         1.f,  1.f,
         1.f, -1.f,
    };
    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);
    glBindVertexArray(quadVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindVertexArray(0);
}

} // namespace sources

