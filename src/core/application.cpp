#include "core/application.hpp"
#include "core/projection_window.hpp"
#include "layers/rectangle_shape.hpp"
#include "sources/shader_source.hpp"
#include "sources/color_pattern_source.hpp"
#include "sources/image_file_source.hpp"
#include "sources/video_file_source.hpp"
#include "sources/pipewire_source.hpp"
#include "sources/pipewire_node_enumerator.hpp"
#include "sources/portal_screencast.hpp"
#ifdef HAVE_GSTREAMER
#include <gst/gst.h>
#endif
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <iostream>

ProjectionMapper* ProjectionMapper::instancePtr_ = nullptr;

ProjectionMapper::ProjectionMapper(int windowWidth, int windowHeight)
    : windowWidth_(windowWidth),
      windowHeight_(windowHeight),
      window_(nullptr),
      uiManager_(nullptr),
      projectionWindow_(std::make_unique<ProjectionWindow>()),
      lastFrameTime_(0.0),
      shouldClose_(false),
      nextLayerId_(0) {

    instancePtr_ = this;
}

ProjectionMapper::~ProjectionMapper() {
    cleanup();
}

bool ProjectionMapper::initialize() {
    if (!initializeGLFW()) return false;
    if (!initializeOpenGL()) return false;
    if (!initializeImGui()) return false;

#ifdef HAVE_GSTREAMER
    gst_init(nullptr, nullptr);
#endif

#ifdef HAVE_PIPEWIRE
    pw_init(nullptr, nullptr);
#endif

    uiManager_ = std::make_unique<ui::UIManager>();
    if (!uiManager_->initialize(window_)) return false;

    // Create 3 built-in color pattern sources
    using CP = sources::ColorPatternSource;
    auto solid = std::make_shared<CP>(CP::Pattern::SOLID_COLOR,   0.2f, 0.5f, 1.0f);
    auto check = std::make_shared<CP>(CP::Pattern::CHECKERBOARD,  1.0f, 0.6f, 0.1f);
    auto grad  = std::make_shared<CP>(CP::Pattern::GRADIENT,      0.6f, 0.1f, 0.9f);

    solid->initialize(); sources_.push_back(solid);
    check->initialize(); sources_.push_back(check);
    grad->initialize();  sources_.push_back(grad);

    // Create a default layer linked to the solid source
    {
        auto shape = std::make_unique<layers::RectangleShape>(
            Vec2(50.0f, 50.0f), Vec2(400.0f, 300.0f));
        createLayer(solid, std::move(shape));
    }

    lastFrameTime_ = glfwGetTime();
    return true;
}

void ProjectionMapper::run() {
    while (!shouldClose_ && !glfwWindowShouldClose(window_)) {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastFrameTime_);
        lastFrameTime_ = currentTime;

        update(deltaTime);
        render();

        glfwPollEvents();
    }
}

void ProjectionMapper::addSource(Shared<sources::Source> source) {
    if (source) {
        if (!source->isValid()) source->initialize();
        sources_.push_back(source);
    }
}

Shared<layers::Layer> ProjectionMapper::createLayer(
    Shared<sources::Source> source,
    Unique<layers::Shape> shape) {

    auto layer = std::make_shared<layers::Layer>(nextLayerId_++, source,
                                                  std::move(shape));
    layers_.push_back(layer);
    return layer;
}

bool ProjectionMapper::initializeGLFW() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(windowWidth_, windowHeight_,
                               "CrazyMapper - Projection Mapping", nullptr,
                               nullptr);
    if (!window_) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // Enable vsync

    // Set callbacks
    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetCursorPosCallback(window_, cursorPosCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetKeyCallback(window_, keyCallback);

    return true;
}

bool ProjectionMapper::initializeOpenGL() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    glViewport(0, 0, windowWidth_, windowHeight_);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return true;
}

bool ProjectionMapper::initializeImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO();  // Initialize IO context
    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(window_, true)) {
        std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
        return false;
    }

    const char* glsl_version = "#version 330";
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        std::cerr << "Failed to initialize ImGui OpenGL backend" << std::endl;
        return false;
    }

    return true;
}

void ProjectionMapper::update(float deltaTime) {
    // Check for UI requests (like new layer creation)
    if (uiManager_->shouldCreateNewLayer()) {
        // Use the first available source (solid color) or create a new one
        Shared<sources::Source> src;
        if (!sources_.empty()) src = sources_[0];
        else {
            using CP = sources::ColorPatternSource;
            auto s = std::make_shared<CP>(CP::Pattern::SOLID_COLOR, 0.2f, 0.5f, 1.0f);
            s->initialize();
            sources_.push_back(s);
            src = s;
        }

        auto shape = std::make_unique<layers::RectangleShape>(
            Vec2(100.0f, 100.0f), Vec2(300.0f, 200.0f));
        createLayer(src, std::move(shape));
        uiManager_->setSelectedLayerIndex(static_cast<int>(layers_.size()) - 1);
    }

    // Handle source assignment from Sources panel
    if (uiManager_->hasPendingSourceAssignment()) {
        int srcIdx = uiManager_->getAssignSourceIndex();
        int layIdx = uiManager_->getSelectedLayerIndex();
        if (srcIdx >= 0 && srcIdx < static_cast<int>(sources_.size()) &&
            layIdx >= 0 && layIdx < static_cast<int>(layers_.size())) {
            layers_[layIdx]->setSource(sources_[srcIdx]);
        }
    }

    // Handle layer reorder
    if (uiManager_->hasPendingLayerReorder()) {
        int from = uiManager_->getReorderFrom();
        int to   = uiManager_->getReorderTo();
        if (from >= 0 && from < static_cast<int>(layers_.size()) &&
            to   >= 0 && to   < static_cast<int>(layers_.size())) {
            std::swap(layers_[from], layers_[to]);
        }
    }

    // Handle projection window toggle
    if (uiManager_->hasPendingToggleProjectionWindow()) {
        if (projectionWindow_->isOpen())
            projectionWindow_->close();
        else
            projectionWindow_->open(window_);
    }

    // Handle PipeWire portal screen capture request
    if (uiManager_->hasPendingPortalCapture()) {
        if (!portalFuture_.valid()) {
            // Launch portal dialog on a worker thread (blocks until user picks)
            uiManager_->setPortalCapturePending(true);
            portalFuture_ = std::async(std::launch::async,
                                       sources::runPortalScreenCast);
        }
    }

    // Poll portal future — non-blocking check each frame
    if (portalFuture_.valid()) {
        using namespace std::chrono_literals;
        if (portalFuture_.wait_for(0ms) == std::future_status::ready) {
            auto result = portalFuture_.get();
            uiManager_->setPortalCapturePending(false);
            if (result.success) {
                auto src = std::make_shared<sources::PipeWireSource>(
                    result.nodeId, result.pwFd, result.portalSession);
                addSource(src);
            }
        }
    }

    // Handle new source creation from Sources panel
    if (uiManager_->hasPendingAddSource()) {
        int   kind = uiManager_->getNewSourceKind();
        const float* col = uiManager_->getNewSourceColor();
        Shared<sources::Source> newSrc;

        using CP = sources::ColorPatternSource;
        switch (kind) {
        case 0:
            newSrc = std::make_shared<CP>(CP::Pattern::SOLID_COLOR,  col[0], col[1], col[2]);
            break;
        case 1:
            newSrc = std::make_shared<CP>(CP::Pattern::CHECKERBOARD, col[0], col[1], col[2]);
            break;
        case 2:
            newSrc = std::make_shared<CP>(CP::Pattern::GRADIENT,     col[0], col[1], col[2]);
            break;
        case 3:
            newSrc = std::make_shared<sources::ImageFileSource>(uiManager_->getNewImagePath());
            break;
        case 4:
            newSrc = std::make_shared<sources::VideoFileSource>(uiManager_->getNewVideoPath());
            break;
        case 5: {
            static const char* kDefaultFrag = R"glsl(
#version 330 core
out vec4 FragColor;
uniform vec2  iResolution;
uniform float iTime;
void main() {
    vec2 uv = gl_FragCoord.xy / iResolution;
    FragColor = vec4(uv, 0.5 + 0.5 * sin(iTime), 1.0);
}
)glsl";
            newSrc = std::make_shared<sources::ShaderSource>(kDefaultFrag, 1280, 720);
            break;
        }
        // case 6 (PipeWire Portal) is handled separately via portalFuture_
        default: break;
        }

        if (newSrc) addSource(newSrc);
    }

    // Update all sources
    for (auto& source : sources_) {
        source->update(deltaTime);
    }
}

void ProjectionMapper::render() {
    // ---- Projection window (rendered before ImGui frame on main context) ----
    if (projectionWindow_->isOpen()) {
        auto* outView = uiManager_->getOutputSpaceView();
        projectionWindow_->render(layers_,
                                  outView->getCanvasLocalPos(),
                                  outView->getCanvasLocalSize());
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render ImGui
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    uiManager_->render(layers_, sources_, (float)glfwGetTime(),
                       projectionWindow_->isOpen());

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window_);
}

void ProjectionMapper::cleanup() {
    if (projectionWindow_) {
        projectionWindow_->close();
        projectionWindow_.reset();
    }

    if (uiManager_) {
        uiManager_->shutdown();
        uiManager_.reset();
    }

    for (auto& layer : layers_) {
        layer.reset();
    }
    layers_.clear();

    for (auto& source : sources_) {
        source->shutdown();
        source.reset();
    }
    sources_.clear();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window_) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

void ProjectionMapper::framebufferSizeCallback(GLFWwindow* /* w */, int width,
                                               int height) {
    if (instancePtr_) {
        instancePtr_->onFramebufferSize(width, height);
    }
}

void ProjectionMapper::mouseButtonCallback(GLFWwindow* /* w */, int button,
                                           int action, int mods) {
    if (instancePtr_) {
        instancePtr_->onMouseButton(button, action, mods);
    }
}

void ProjectionMapper::cursorPosCallback(GLFWwindow* /* w */, double xpos,
                                         double ypos) {
    if (instancePtr_) {
        instancePtr_->onCursorPos(xpos, ypos);
    }
}

void ProjectionMapper::scrollCallback(GLFWwindow* /* w */, double xoff, double yoff) {
    if (instancePtr_) {
        instancePtr_->onScroll(xoff, yoff);
    }
}

void ProjectionMapper::keyCallback(GLFWwindow* /* w */, int key, int scancode,
                                   int action, int mods) {
    if (instancePtr_) {
        instancePtr_->onKey(key, scancode, action, mods);
    }
}

void ProjectionMapper::onFramebufferSize(int width, int height) {
    windowWidth_ = width;
    windowHeight_ = height;
    glViewport(0, 0, width, height);
}

void ProjectionMapper::onMouseButton(int button, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        uiManager_->onMouseButton(button, action, mods);
    }
}

void ProjectionMapper::onCursorPos(double xpos, double ypos) {
    uiManager_->onMouseMove(Vec2(xpos, ypos));
}

void ProjectionMapper::onScroll(double xoff, double yoff) {
    uiManager_->onScroll(xoff, yoff);
}

void ProjectionMapper::onKey(int key, int /* scancode */, int action, int /* mods */) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        // ESC closes the main app only if no projection window needs windowing first
        if (!projectionWindow_->isOpen())
            shouldClose_ = true;
    }
}

