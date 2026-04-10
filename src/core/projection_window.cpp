#include "core/projection_window.hpp"
#include "math/homography.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>



// ---------------------------------------------------------------------------
// Embedded shader sources (same logic as output_space_view.cpp per-pixel pass)
// ---------------------------------------------------------------------------

static const char* kProjVertSrc = R"glsl(
#version 330 core
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;
out VS_OUT { vec2 texCoord; } vs_out;
void main() {
    gl_Position    = vec4(position, 0.0, 1.0);
    vs_out.texCoord = texCoord;
}
)glsl";

static const char* kProjFragSrc = R"glsl(
#version 330 core
in VS_OUT { vec2 texCoord; } fs_in;
out vec4 FragColor;

uniform sampler2D sourceTexture;
uniform mat3      homographyInverse;
uniform mat3      shapeMaskHomography;
uniform float     opacity;
uniform float     feather;              // edge feather width in shape-local UV space
uniform vec2      outputCorners[4];
uniform int       shapeType;         // 0=rect 1=rounded_rect 2=ellipse 3=n_polygon 4=triangle
uniform vec4      shapeCornerRadii;  // TL TR BR BL corner radii [0..0.5]
uniform int       shapeSides;        // number of sides for n_polygon

float cross2d(vec2 a, vec2 b) { return a.x*b.y - a.y*b.x; }

bool insideConvexQuad(vec2 p) {
    float d0 = cross2d(outputCorners[1]-outputCorners[0], p-outputCorners[0]);
    float d1 = cross2d(outputCorners[2]-outputCorners[1], p-outputCorners[1]);
    float d2 = cross2d(outputCorners[3]-outputCorners[2], p-outputCorners[2]);
    float d3 = cross2d(outputCorners[0]-outputCorners[3], p-outputCorners[3]);
    bool allPos = (d0>=0.0)&&(d1>=0.0)&&(d2>=0.0)&&(d3>=0.0);
    bool allNeg = (d0<=0.0)&&(d1<=0.0)&&(d2<=0.0)&&(d3<=0.0);
    return allPos || allNeg;
}

// --- SDF functions (positive = inside, 0 = on edge, negative = outside) ---

float rectSDF(vec2 uv) {
    return min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
}

float roundedRectSDF(vec2 p, vec4 r) {
    if (p.x < r.x && p.y < r.x)
        return r.x - length(p - vec2(r.x, r.x));
    if (p.x > 1.0-r.y && p.y < r.y)
        return r.y - length(p - vec2(1.0-r.y, r.y));
    if (p.x > 1.0-r.z && p.y > 1.0-r.z)
        return r.z - length(p - vec2(1.0-r.z, 1.0-r.z));
    if (p.x < r.w && p.y > 1.0-r.w)
        return r.w - length(p - vec2(r.w, 1.0-r.w));
    return min(min(p.x, 1.0-p.x), min(p.y, 1.0-p.y));
}

float ellipseSDF(vec2 uv) {
    return 0.5 - length(uv - vec2(0.5));
}

float polygonSDF(vec2 uv, int sides) {
    float n = float(sides);
    vec2 p = uv - vec2(0.5);
    float plen = length(p);
    if (plen < 0.001) return 0.5;
    float sector = 6.28318530 / n;
    float a = mod(atan(p.y, p.x), sector);
    if (a < 0.0) a += sector;
    float theta = abs(a - sector * 0.5);
    float apothem = 0.5 * cos(3.14159265 / n);
    return apothem - plen * cos(theta);
}

// Triangle SDF in canvas UV space. Positive = inside.
float triangleSDF(vec2 p, vec2 a, vec2 b, vec2 c) {
    vec2 e0 = b - a, e1 = c - b, e2 = a - c;
    vec2 v0 = p - a, v1 = p - b, v2 = p - c;
    vec2 pq0 = v0 - e0 * clamp(dot(v0,e0)/dot(e0,e0), 0.0, 1.0);
    vec2 pq1 = v1 - e1 * clamp(dot(v1,e1)/dot(e1,e1), 0.0, 1.0);
    vec2 pq2 = v2 - e2 * clamp(dot(v2,e2)/dot(e2,e2), 0.0, 1.0);
    float s = sign(cross2d(e0, e2));
    vec2 d = min(min(vec2(dot(pq0,pq0), s*cross2d(v0,e0)),
                     vec2(dot(pq1,pq1), s*cross2d(v1,e1))),
                     vec2(dot(pq2,pq2), s*cross2d(v2,e2)));
    return sqrt(d.x) * sign(d.y);
}

float shapeSDF(vec2 uv) {
    if (shapeType == 0) return rectSDF(uv);
    if (shapeType == 1) return roundedRectSDF(uv, shapeCornerRadii);
    if (shapeType == 2) return ellipseSDF(uv);
    if (shapeType == 3) return polygonSDF(uv, shapeSides);
    return 1.0;
}

void main() {
    vec2 outCoord = fs_in.texCoord;

    float shapeAlpha;
    if (shapeType == 4) {  // TRIANGLE
        float sdf = triangleSDF(outCoord,
            outputCorners[0], outputCorners[1], outputCorners[2]);
        shapeAlpha = smoothstep(0.0, max(feather, 0.001), sdf);
    } else {
        if (!insideConvexQuad(outCoord)) discard;
        vec3 hs = shapeMaskHomography * vec3(outCoord, 1.0);
        vec2 shapeUV = hs.xy / hs.z;
        float sdf = shapeSDF(shapeUV);
        shapeAlpha = smoothstep(0.0, max(feather, 0.001), sdf);
    }
    if (shapeAlpha < 0.001) discard;

    // Source UV via input-region homography/affine
    vec3 h  = homographyInverse * vec3(outCoord, 1.0);
    vec2 uv = h.xy / h.z;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) discard;

    uv.y = 1.0 - uv.y;   // ImGui y-down -> GL texture y-up
    vec4 c = texture(sourceTexture, uv);
    c.a *= opacity * shapeAlpha;
    FragColor = c;
}
)glsl";

// ---------------------------------------------------------------------------

ProjectionWindow::ProjectionWindow() {
}

ProjectionWindow::~ProjectionWindow() {
    close();
}

bool ProjectionWindow::open(GLFWwindow* shareContext) {
    if (window_) return true;  // already open

    // Use the same GL version as the main window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    // Do NOT auto-iconify when the window loses focus — essential when the
    // projection window is fullscreen on a second monitor and the user is
    // working in the editor on the primary monitor.
    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);

    // Initial windowed size matches saved state
    window_ = glfwCreateWindow(savedW_, savedH_,
                               "CrazyMapper — Projection Output",
                               nullptr, shareContext);
    if (!window_) {
        std::cerr << "[ProjectionWindow] Failed to create GLFW window\n";
        return false;
    }

    glfwSetWindowPos(window_, savedX_, savedY_);
    glfwSetWindowUserPointer(window_, this);
    glfwSetKeyCallback(window_, keyCallback);
    glfwSetWindowCloseCallback(window_, closeCallback);

    return true;
}

void ProjectionWindow::close() {
    if (!window_) return;

    // Shader / VAO live in the projection window's context
    GLFWwindow* prevCtx = glfwGetCurrentContext();
    glfwMakeContextCurrent(window_);

    if (quadVAO_) { glDeleteVertexArrays(1, &quadVAO_); quadVAO_ = 0; }
    if (quadVBO_) { glDeleteBuffers(1, &quadVBO_);      quadVBO_ = 0; }
    shader_.reset();
    shaderInit_ = false;

    glfwMakeContextCurrent(prevCtx);

    glfwDestroyWindow(window_);
    window_      = nullptr;
    isFullscreen_ = false;
}

void ProjectionWindow::toggleFullscreen() {
    if (!window_) return;

    if (!isFullscreen_) {
        // Save current windowed position/size
        glfwGetWindowPos(window_,  &savedX_, &savedY_);
        glfwGetWindowSize(window_, &savedW_, &savedH_);

        // Find the monitor whose work area contains the window centre
        int cx = savedX_ + savedW_ / 2;
        int cy = savedY_ + savedH_ / 2;

        int monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
        GLFWmonitor* target = glfwGetPrimaryMonitor();

        for (int i = 0; i < monitorCount; ++i) {
            int mx, my, mw, mh;
            glfwGetMonitorWorkarea(monitors[i], &mx, &my, &mw, &mh);
            if (cx >= mx && cx < mx + mw && cy >= my && cy < my + mh) {
                target = monitors[i];
                break;
            }
        }

        const GLFWvidmode* mode = glfwGetVideoMode(target);
        glfwSetWindowMonitor(window_, target, 0, 0,
                             mode->width, mode->height, mode->refreshRate);
        // Re-apply after setWindowMonitor — some drivers reset window attributes
        glfwSetWindowAttrib(window_, GLFW_AUTO_ICONIFY, GLFW_FALSE);
        isFullscreen_ = true;
    } else {
        // Return to saved windowed state
        glfwSetWindowMonitor(window_, nullptr,
                             savedX_, savedY_, savedW_, savedH_, 0);
        isFullscreen_ = false;
    }
}

bool ProjectionWindow::initShader() {
    // Must be called while the projection window's context is current
    shader_ = std::make_unique<gl::ShaderProgram>();
    if (!shader_->compile(kProjVertSrc, kProjFragSrc)) {
        shader_.reset();
        return false;
    }

    // Fullscreen NDC quad, texCoord: TL=(0,0) to BR=(1,1) — ImGui y-down convention
    float verts[] = {
        -1.0f,  1.0f,   0.0f, 0.0f,   // TL
         1.0f,  1.0f,   1.0f, 0.0f,   // TR
        -1.0f, -1.0f,   0.0f, 1.0f,   // BL
         1.0f, -1.0f,   1.0f, 1.0f,   // BR
    };

    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);
    glBindVertexArray(quadVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    shaderInit_ = true;
    return true;
}

void ProjectionWindow::render(const std::vector<Shared<layers::Layer>>& layers,
                              Vec2 /*canvasLocalPos*/,
                              Vec2 canvasLocalSize) {
    if (!window_) return;

    // Window may have been closed by the user (clicking X)
    if (glfwWindowShouldClose(window_)) {
        close();
        return;
    }

    if (canvasLocalSize.x < 1.0f || canvasLocalSize.y < 1.0f) return;

    // Switch to the projection window's GL context
    GLFWwindow* prevCtx = glfwGetCurrentContext();
    glfwMakeContextCurrent(window_);

    // Lazy shader/VAO init (must happen in this context)
    if (!shaderInit_) {
        if (!initShader()) {
            glfwMakeContextCurrent(prevCtx);
            return;
        }
    }

    int fbW, fbH;
    glfwGetFramebufferSize(window_, &fbW, &fbH);

    glViewport(0, 0, fbW, fbH);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_->use();
    shader_->setUniform1i("sourceTexture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(quadVAO_);

    GLint cornersLoc = glGetUniformLocation(shader_->getHandle(), "outputCorners");

    for (auto& layer : layers) {
        if (!layer->isVisible()) continue;
        auto source = layer->getSource();
        if (!source || !source->isValid() || source->getTextureHandle() == 0) continue;

        auto outCorners = layer->getOutputCorners();
        auto inCorners  = layer->getInputCorners();

        // Output corners are already stored as canvas UV [0..1]
        std::array<Vec2, 4> normOut;
        for (int i = 0; i < 4; ++i)
            normOut[i] = outCorners[i];

        bool isTriangle = (layer->getShapeType() == 4);

        glm::mat3 H;
        if (isTriangle) {
            std::array<Vec2, 3> triOut = {normOut[0], normOut[1], normOut[2]};
            std::array<Vec2, 3> triIn  = {inCorners[0], inCorners[1], inCorners[2]};
            H = math::Homography::computeAffine(triOut, triIn);
        } else {
            H = math::Homography::compute(normOut, inCorners);
        }

        glm::mat3 H_shape = glm::mat3(1.0f);
        if (!isTriangle) {
            std::array<Vec2, 4> unit = {Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1)};
            H_shape = math::Homography::compute(normOut, unit);
        }

        shader_->setUniformMat3("homographyInverse", H);
        shader_->setUniformMat3("shapeMaskHomography", H_shape);
        shader_->setUniform1f("opacity", layer->getOpacity());
        shader_->setUniform1f("feather", layer->getFeather());
        shader_->setUniform1i("shapeType",   layer->getShapeType());
        auto cr = layer->getShapeCornerRadii();
        shader_->setUniform4f("shapeCornerRadii", cr[0], cr[1], cr[2], cr[3]);
        shader_->setUniform1i("shapeSides",  std::max(3, layer->getShapePolySides()));

        if (cornersLoc >= 0) {
            float cd[8] = {
                normOut[0].x, normOut[0].y,
                normOut[1].x, normOut[1].y,
                normOut[2].x, normOut[2].y,
                normOut[3].x, normOut[3].y,
            };
            glUniform2fv(cornersLoc, 4, cd);
        }

        glBindTexture(GL_TEXTURE_2D, source->getTextureHandle());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glfwSwapBuffers(window_);

    // Restore main window context
    glfwMakeContextCurrent(prevCtx);
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

void ProjectionWindow::keyCallback(GLFWwindow* w, int key, int /*sc*/,
                                   int action, int /*mods*/) {
    auto* self = static_cast<ProjectionWindow*>(glfwGetWindowUserPointer(w));
    if (!self || action != GLFW_PRESS) return;

    if (key == GLFW_KEY_F) {
        self->toggleFullscreen();
    } else if (key == GLFW_KEY_ESCAPE) {
        // ESC always goes back to windowed (not close)
        if (self->isFullscreen_)
            self->toggleFullscreen();
    }
}

void ProjectionWindow::closeCallback(GLFWwindow* w) {
    // Prevent GLFW from closing; the application will handle it
    glfwSetWindowShouldClose(w, GLFW_FALSE);
    auto* self = static_cast<ProjectionWindow*>(glfwGetWindowUserPointer(w));
    if (self) self->close();
}
