#include "sources/color_pattern_source.hpp"
#include <glad/glad.h>
#include <iostream>
#include <cstdio>

namespace sources {

// ─── GLSL fragment shaders ────────────────────────────────────────────────

static const char* kVertexSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

static const char* kSolidFrag = R"(
#version 330 core
in  vec2 vUV;
out vec4 FragColor;
uniform vec3 uColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

static const char* kCheckerFrag = R"(
#version 330 core
in  vec2 vUV;
out vec4 FragColor;
uniform vec3 uColor;
void main() {
    float scale  = 8.0;
    vec2  grid   = floor(vUV * scale);
    float check  = mod(grid.x + grid.y, 2.0);
    vec3  col    = mix(vec3(0.08), uColor, check);
    FragColor    = vec4(col, 1.0);
}
)";

static const char* kGradientFrag = R"(
#version 330 core
in  vec2 vUV;
out vec4 FragColor;
uniform vec3  uColor;
uniform float uTime;
void main() {
    float t   = mod(vUV.x + vUV.y + uTime * 0.05, 1.0);
    vec3  col = mix(uColor * 0.25, uColor, t);
    FragColor = vec4(col, 1.0);
}
)";

static const char* kCalibrationFrag = R"(
#version 330 core
in  vec2 vUV;
out vec4 FragColor;
void main() {
    float cols = 16.0;
    float rows =  9.0;
    vec2  cell = fract(vUV * vec2(cols, rows));

    float thickX = 0.06;
    float thickY = 0.035;
    float gx = 1.0 - smoothstep(0.0, thickX, min(cell.x, 1.0 - cell.x));
    float gy = 1.0 - smoothstep(0.0, thickY, min(cell.y, 1.0 - cell.y));
    float grid = max(gx, gy);

    // Centre cross-hair
    vec2 d = abs(vUV - 0.5);
    float cr = float(min(d.x, d.y) < 0.004 && max(d.x, d.y) < 0.07);

    // Quarter crosshairs (L-marks) at each canvas corner.
    // Fold UV so all four corners map to (0,0).
    float clen = 0.07;   // arm length in UV space
    float cw   = 0.004;  // arm width in UV space
    vec2  cv   = min(vUV, 1.0 - vUV);
    float h_arm = float(cv.y < cw  && cv.x < clen);
    float v_arm = float(cv.x < cw  && cv.y < clen);
    float corner = float(h_arm + v_arm > 0.0);

    float val = clamp(grid + cr + corner, 0.0, 1.0);
    FragColor = vec4(vec3(val), 1.0);
}
)";

// ─── helpers ──────────────────────────────────────────────────────────────

static unsigned int compileStage(GLenum type, const char* src) {
    unsigned int s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::cerr << "ColorPatternSource shader error: " << log << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static unsigned int linkProgram(const char* fragSrc) {
    unsigned int vert = compileStage(GL_VERTEX_SHADER,   kVertexSrc);
    unsigned int frag = compileStage(GL_FRAGMENT_SHADER, fragSrc);
    if (!vert || !frag) {
        if (vert) glDeleteShader(vert);
        if (frag) glDeleteShader(frag);
        return 0;
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
        std::cerr << "ColorPatternSource link error: " << log << "\n";
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// ─── ColorPatternSource ────────────────────────────────────────────────────

ColorPatternSource::ColorPatternSource(Pattern pattern,
                                       float r, float g, float b,
                                       int width, int height)
    : pattern_(pattern),
      r_(r), g_(g), b_(b),
      resolution_(float(width), float(height)),
      elapsedTime_(0.f),
      initialized_(false),
      shaderProgram_(0),
      framebufferObject_(0),
      textureHandle_(0),
      quadVAO_(0),
      quadVBO_(0) {}

ColorPatternSource::~ColorPatternSource() {
    shutdown();
}

bool ColorPatternSource::initialize() {
    if (initialized_) return true;
    if (!buildShader()) return false;
    createFBO();
    createQuad();
    // Force first render
    update(0.f);
    initialized_ = true;
    return true;
}

void ColorPatternSource::shutdown() {
    if (!initialized_) return;
    if (shaderProgram_)    { glDeleteProgram(shaderProgram_);           shaderProgram_    = 0; }
    if (framebufferObject_){ glDeleteFramebuffers(1, &framebufferObject_); framebufferObject_ = 0; }
    if (textureHandle_)    { glDeleteTextures(1, &textureHandle_);      textureHandle_    = 0; }
    if (quadVAO_)          { glDeleteVertexArrays(1, &quadVAO_);        quadVAO_          = 0; }
    if (quadVBO_)          { glDeleteBuffers(1, &quadVBO_);             quadVBO_          = 0; }
    initialized_ = false;
}

bool ColorPatternSource::update(float deltaTime) {
    if (!shaderProgram_ || !framebufferObject_) return false;
    elapsedTime_ += deltaTime;

    glBindFramebuffer(GL_FRAMEBUFFER, framebufferObject_);
    glViewport(0, 0, (int)resolution_.x, (int)resolution_.y);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram_);
    glUniform3f(glGetUniformLocation(shaderProgram_, "uColor"), r_, g_, b_);
    if (pattern_ == Pattern::GRADIENT)
        glUniform1f(glGetUniformLocation(shaderProgram_, "uTime"), elapsedTime_);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

std::string ColorPatternSource::getName() const {
    switch (pattern_) {
        case Pattern::SOLID_COLOR:       return "Pattern: Solid Color";
        case Pattern::CHECKERBOARD:      return "Pattern: Checkerboard";
        case Pattern::GRADIENT:          return "Pattern: Gradient";
        case Pattern::CALIBRATION_GRID:  return "Pattern: Calibration Grid";
    }
    return "Pattern";
}

void ColorPatternSource::setColor(float r, float g, float b) {
    r_ = r; g_ = g; b_ = b;
}

// ─── private ──────────────────────────────────────────────────────────────

bool ColorPatternSource::buildShader() {
    const char* fragSrc = kSolidFrag;
    if (pattern_ == Pattern::CHECKERBOARD)     fragSrc = kCheckerFrag;
    if (pattern_ == Pattern::GRADIENT)         fragSrc = kGradientFrag;
    if (pattern_ == Pattern::CALIBRATION_GRID) fragSrc = kCalibrationFrag;
    shaderProgram_ = linkProgram(fragSrc);
    return shaderProgram_ != 0;
}

void ColorPatternSource::createFBO() {
    glGenTextures(1, &textureHandle_);
    glBindTexture(GL_TEXTURE_2D, textureHandle_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 (int)resolution_.x, (int)resolution_.y,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &framebufferObject_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferObject_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, textureHandle_, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ColorPatternSource FBO incomplete\n";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ColorPatternSource::createQuad() {
    float verts[] = { -1.f, 1.f,  -1.f, -1.f,  1.f, 1.f,  1.f, -1.f };
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
