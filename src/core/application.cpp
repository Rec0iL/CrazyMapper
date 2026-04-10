#include "core/application.hpp"
#include "core/projection_window.hpp"
#include "layers/rectangle_shape.hpp"
#include "layers/rounded_rectangle_shape.hpp"
#include "layers/ellipse_shape.hpp"
#include "layers/polygon_shape.hpp"
#include "layers/triangle_shape.hpp"
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
#include <fstream>
#include <sstream>
#include <regex>

ProjectionMapper* ProjectionMapper::instancePtr_ = nullptr;

// Rename GLSL reserved words that Shadertoy shaders sometimes use as identifiers.
// WebGL GLSL compilers are lenient; desktop GL 3.30 is not.
static std::string sanitizeShadertoyGLSL(std::string src) {
    // Reserved words commonly seen as identifiers in Shadertoy shaders
    static const char* reserved[] = { "char", "short", "long", "double", "fixed",
                                      "unsigned", "half", "input", "output" };
    for (const char* word : reserved) {
        // Replace word-boundary occurrences with a safe prefixed name
        std::string safe = std::string("_st_") + word;
        src = std::regex_replace(src,
            std::regex(std::string("\\b") + word + "\\b"), safe);
    }
    return src;
}

ProjectionMapper::ProjectionMapper(int windowWidth, int windowHeight)
    : windowWidth_(windowWidth),
      windowHeight_(windowHeight),
      window_(nullptr),
      uiManager_(nullptr),
      lastFrameTime_(0.0),
      shouldClose_(false),
      nextLayerId_(0) {

    instancePtr_ = this;
    // Create the one default projection window slot
    projectionWindows_.push_back(std::make_unique<ProjectionWindow>());
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

    // Handle layout save / load
    if (uiManager_->hasPendingSaveLayout())
        saveLayout(uiManager_->getSaveLayoutPath());
    if (uiManager_->hasPendingLoadLayout())
        loadLayout(uiManager_->getLoadLayoutPath());

    // Handle layer deletion
    if (uiManager_->hasPendingDeleteLayer()) {
        int idx = uiManager_->getDeleteLayerIndex();
        if (idx >= 0 && idx < static_cast<int>(layers_.size())) {
            layers_.erase(layers_.begin() + idx);
            int sel = uiManager_->getSelectedLayerIndex();
            if (layers_.empty())
                uiManager_->setSelectedLayerIndex(-1);
            else if (sel >= static_cast<int>(layers_.size()))
                uiManager_->setSelectedLayerIndex(static_cast<int>(layers_.size()) - 1);
        }
    }

    // Handle source deletion
    if (uiManager_->hasPendingDeleteSource()) {
        int idx = uiManager_->getDeleteSourceIndex();
        if (idx >= 0 && idx < static_cast<int>(sources_.size())) {
            auto src = sources_[idx];
            // Reassign any layers using this source to the first remaining source
            sources_.erase(sources_.begin() + idx);
            src->shutdown();
            for (auto& layer : layers_) {
                if (layer->getSource() == src) {
                    if (!sources_.empty())
                        layer->setSource(sources_[0]);
                }
            }
        }
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

    // Handle projection window toggle (per canvas)
    {
        int toggleIdx = 0;
        if (uiManager_->hasPendingToggleProjectionWindow(toggleIdx)) {
            if (toggleIdx >= 0 && toggleIdx < static_cast<int>(projectionWindows_.size())) {
                if (projectionWindows_[toggleIdx]->isOpen())
                    projectionWindows_[toggleIdx]->close();
                else
                    projectionWindows_[toggleIdx]->open(window_);
            }
        }
    }

    // Handle canvas addition
    if (uiManager_->hasPendingAddCanvas()) {
        uiManager_->onCanvasAdded();
        projectionWindows_.push_back(std::make_unique<ProjectionWindow>());
    }

    // Handle canvas deletion (idx 0 is protected)
    if (uiManager_->hasPendingDeleteCanvas()) {
        int idx = uiManager_->getDeleteCanvasIndex();
        if (idx > 0 && idx < static_cast<int>(projectionWindows_.size())) {
            projectionWindows_[idx]->close();
            projectionWindows_.erase(projectionWindows_.begin() + idx);
            uiManager_->onCanvasDeleted(idx);
            // Reassign layers that were on the deleted canvas to canvas 0;
            // shift indices for canvases above the deleted one
            for (auto& layer : layers_) {
                int ci = layer->getCanvasIndex();
                if (ci == idx)
                    layer->setCanvasIndex(0);
                else if (ci > idx)
                    layer->setCanvasIndex(ci - 1);
            }
        }
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
        const float* col  = uiManager_->getNewSourceColor();
        const float* col2 = uiManager_->getNewSourceColor2();
        Shared<sources::Source> newSrc;

        using CP = sources::ColorPatternSource;
        switch (kind) {
        case 0:
            newSrc = std::make_shared<CP>(CP::Pattern::SOLID_COLOR,  col[0], col[1], col[2]);
            break;
        case 1:
            newSrc = std::make_shared<CP>(CP::Pattern::CHECKERBOARD,
                                          col[0], col[1], col[2],
                                          col2[0], col2[1], col2[2]);
            break;
        case 2:
            newSrc = std::make_shared<CP>(CP::Pattern::GRADIENT,        col[0], col[1], col[2]);
            break;
        case 3:
            newSrc = std::make_shared<CP>(CP::Pattern::CALIBRATION_GRID, 1.f, 1.f, 1.f);
            break;
        case 4:
            newSrc = std::make_shared<sources::ImageFileSource>(uiManager_->getNewImagePath());
            break;
        case 5:
            newSrc = std::make_shared<sources::VideoFileSource>(uiManager_->getNewVideoPath());
            break;
        case 6: {
            std::string shaderPath = uiManager_->getNewShaderPath();
            std::ifstream shaderFile(shaderPath);
            if (!shaderFile.is_open()) {
                std::cerr << "ShaderFile: could not open " << shaderPath << "\n";
                break;
            }
            std::string fragCode((std::istreambuf_iterator<char>(shaderFile)),
                                  std::istreambuf_iterator<char>());
            fragCode = sanitizeShadertoyGLSL(fragCode);

            // Auto-wrap .shadertoy files (raw Shadertoy editor content)
            auto ext = shaderPath.substr(shaderPath.find_last_of('.') + 1);
            if (ext == "shadertoy") {
                fragCode =
                    "#version 330 core\n"
                    "out vec4 FragColor;\n"
                    "uniform vec2  iResolution;\n"
                    "uniform float iTime;\n"
                    "uniform vec4  iMouse;\n"      // xy=pos, z=click, w=click toggle
                    "uniform int   iFrame;\n"
                    "uniform float iTimeDelta;\n"
                    "\n"
                    + fragCode +
                    "\nvoid main() { mainImage(FragColor, gl_FragCoord.xy); }\n";
            }

            newSrc = std::make_shared<sources::ShaderSource>(fragCode, 1280, 720, shaderPath);
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
    // ---- Projection windows (rendered before ImGui frame on main context) ----
    auto* outView = uiManager_->getOutputSpaceView();
    for (int ci = 0; ci < static_cast<int>(projectionWindows_.size()); ++ci) {
        auto& pw = projectionWindows_[ci];
        if (!pw || !pw->isOpen()) continue;

        // Collect layers belonging to this canvas
        std::vector<Shared<layers::Layer>> canvasLayers;
        canvasLayers.reserve(layers_.size());
        for (auto& layer : layers_)
            if (layer->getCanvasIndex() == ci)
                canvasLayers.push_back(layer);

        pw->render(canvasLayers,
                   outView->getCanvasLocalPos(ci),
                   outView->getCanvasLocalSize(ci));
    }

    // Build per-canvas open-state vector to pass to UIManager
    std::vector<bool> pwStates;
    pwStates.reserve(projectionWindows_.size());
    for (auto& pw : projectionWindows_)
        pwStates.push_back(pw && pw->isOpen());

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render ImGui
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    uiManager_->render(layers_, sources_, (float)glfwGetTime(), pwStates);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window_);
}

void ProjectionMapper::saveLayout(const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) {
        uiManager_->setLayoutWarning("Failed to save layout:\n" + path);
        return;
    }

    const auto& canvases = uiManager_->getCanvases();

    f << "CrazyMapper_Layout_v2\n";
    f << "canvas_count " << canvases.size() << "\n";
    for (int ci = 0; ci < static_cast<int>(canvases.size()); ++ci) {
        f << "canvas " << ci
          << " w " << canvases[ci].aspectW
          << " h " << canvases[ci].aspectH
          << " name " << canvases[ci].name << "\n";
    }
    f << "layers "   << layers_.size() << "\n";

    for (auto& layer : layers_) {
        f << "layer_start\n";
        f << "canvas_index " << layer->getCanvasIndex() << "\n";
        f << "visible "    << (layer->isVisible() ? 1 : 0) << "\n";
        f << "opacity "    << layer->getOpacity()           << "\n";
        f << "blend_mode " << layer->getBlendMode()         << "\n";
        f << "feather "    << layer->getFeather()           << "\n";

        auto ic = layer->getInputCorners();
        f << "input_corners";
        for (auto& c : ic) f << " " << c.x << " " << c.y;
        f << "\n";

        auto oc = layer->getOutputCorners();
        f << "output_corners";
        for (auto& c : oc) f << " " << c.x << " " << c.y;
        f << "\n";

        int st = layer->getShapeType();
        f << "shape_type " << st << "\n";
        if (st == 1) {  // ROUNDED_RECTANGLE
            auto cr = layer->getShapeCornerRadii();
            f << "shape_rr_per_corner " << (layer->isShapePerCorner() ? 1 : 0) << "\n";
            f << "shape_rr_radii "
              << cr[0] << " " << cr[1] << " " << cr[2] << " " << cr[3] << "\n";
        } else if (st == 3) {  // N_POLYGON
            f << "shape_poly_sides " << layer->getShapePolySides() << "\n";
        }
        // TRIANGLE (st == 4): corners already saved in input_corners / output_corners

        auto src = layer->getSource();
        using CP  = sources::ColorPatternSource;
        using IMG = sources::ImageFileSource;
        using VID = sources::VideoFileSource;
        if (auto* cp = dynamic_cast<CP*>(src.get())) {
            int pt = static_cast<int>(cp->getPattern());
            const char* ptnames[] = { "solid_color", "checkerboard", "gradient" };
            f << "source_type "  << ptnames[pt]  << "\n";
            f << "source_color " << cp->getR() << " " << cp->getG() << " " << cp->getB() << "\n";
        } else if (auto* img = dynamic_cast<IMG*>(src.get())) {
            f << "source_type image\n";
            f << "source_path " << img->getFilePath() << "\n";
        } else if (auto* vid = dynamic_cast<VID*>(src.get())) {
            f << "source_type video\n";
            f << "source_path " << vid->getFilePath() << "\n";
        } else if (src->getType() == sources::SourceType::PIPEWIRE_STREAM) {
            f << "source_type pipewire\n";
        } else {
            // ShaderSource or anything else → recreate as default shader
            f << "source_type shader\n";
        }

        f << "layer_end\n";
    }
}

void ProjectionMapper::loadLayout(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        uiManager_->setLayoutWarning("Failed to open layout:\n" + path);
        return;
    }

    std::string line, token;

    auto nextLine = [&]() -> bool {
        while (std::getline(f, line)) {
            if (!line.empty() && line[0] != '#') return true;
        }
        return false;
    };

    if (!nextLine()) {
        uiManager_->setLayoutWarning("Not a valid CrazyMapper layout file.");
        return;
    }

    bool isV2 = (line == "CrazyMapper_Layout_v2");
    bool isV1 = (line == "CrazyMapper_Layout_v1");
    if (!isV1 && !isV2) {
        uiManager_->setLayoutWarning("Not a valid CrazyMapper layout file.");
        return;
    }

    // v1 headers: canvas_w / canvas_h
    float canvasW = 16.0f, canvasH = 9.0f;
    // v2 headers: canvas_count N  then canvas 0 w W h H name NAME  ...
    int canvasCount = 1;
    std::vector<CanvasConfig> loadedCanvases;
    int layerCount = 0;

    while (nextLine()) {
        std::istringstream ss(line);
        ss >> token;
        if      (token == "canvas_w")    ss >> canvasW;
        else if (token == "canvas_h")    ss >> canvasH;
        else if (token == "canvas_count") { ss >> canvasCount; loadedCanvases.resize(canvasCount); }
        else if (token == "canvas") {
            int idx; ss >> idx;
            if (idx >= 0 && idx < static_cast<int>(loadedCanvases.size())) {
                std::string key;
                while (ss >> key) {
                    if (key == "w")    ss >> loadedCanvases[idx].aspectW;
                    else if (key == "h") ss >> loadedCanvases[idx].aspectH;
                    else if (key == "name") std::getline(ss >> std::ws, loadedCanvases[idx].name);
                }
            }
        }
        else if (token == "layers") { ss >> layerCount; break; }
    }

    // Apply canvas configurations
    // Close extra existing projection windows first
    for (int ci = static_cast<int>(projectionWindows_.size()) - 1; ci >= 0; --ci)
        projectionWindows_[ci]->close();
    projectionWindows_.clear();

    if (isV1) {
        // Single canvas from v1 data
        CanvasConfig cfg; cfg.name = "Canvas 1"; cfg.aspectW = canvasW; cfg.aspectH = canvasH;
        uiManager_->onCanvasAdded();  // replaces default
        // Rebuild: reset UIManager canvases by calling onCanvasAdded doesn't reset existing...
        // Actually just set it directly via setCanvasAspect after the loop below.
        loadedCanvases = { cfg };
    }

    // Rebuild UIManager canvas list and projection window slots
    // (UIManager already has one default canvas from construction)
    // Reset to match loaded canvases:
    {
        // Remove extra canvases from UIManager (delete from end, protecting idx 0)
        while (uiManager_->getCanvasCount() > 1)
            uiManager_->onCanvasDeleted(uiManager_->getCanvasCount() - 1);
        // Now UIManager has exactly 1 canvas. Set its config.
        if (!loadedCanvases.empty())
            uiManager_->setCanvasAspect(loadedCanvases[0].aspectW, loadedCanvases[0].aspectH, 0);
        // Add additional canvases
        for (int ci = 1; ci < static_cast<int>(loadedCanvases.size()); ++ci) {
            uiManager_->onCanvasAdded();
            uiManager_->setCanvasAspect(loadedCanvases[ci].aspectW, loadedCanvases[ci].aspectH, ci);
        }
    }

    // Recreate projection window slots to match
    for (int ci = 0; ci < uiManager_->getCanvasCount(); ++ci)
        projectionWindows_.push_back(std::make_unique<ProjectionWindow>());

    // Clear existing layer/source state
    for (auto& src : sources_) src->shutdown();
    sources_.clear();
    layers_.clear();
    uiManager_->setSelectedLayerIndex(-1);

    int pipewireCount = 0;

    static const char* kDefaultFragSrc = R"glsl(
#version 330 core
out vec4 FragColor;
uniform vec2  iResolution;
uniform float iTime;
void main() {
    vec2 uv = gl_FragCoord.xy / iResolution;
    FragColor = vec4(uv, 0.5 + 0.5 * sin(iTime), 1.0);
}
)glsl";

    for (int li = 0; li < layerCount; ++li) {
        // Advance to "layer_start"
        while (nextLine() && line.find("layer_start") == std::string::npos) {}

        bool visible = true;
        float opacity = 1.0f;
        int blendMode = 0;
        float feather = 0.0f;
        int canvasIdx = 0;
        std::array<Vec2, 4> inC  = { Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) };
        std::array<Vec2, 4> outC = { Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) };
        int shapeType = 0;
        int rrPerCorner = 0;
        std::array<float, 4> rrRadii = { 0.1f, 0.1f, 0.1f, 0.1f };
        int polySides = 6;
        std::string srcType = "solid_color";
        float srcR = 0.5f, srcG = 0.5f, srcB = 0.5f;
        std::string srcPath;

        while (nextLine() && line.find("layer_end") == std::string::npos) {
            std::istringstream ss(line);
            ss >> token;
            if      (token == "canvas_index")    ss >> canvasIdx;
            else if (token == "visible")         { int v; ss >> v; visible = (v != 0); }
            else if (token == "opacity")         ss >> opacity;
            else if (token == "blend_mode")      ss >> blendMode;
            else if (token == "feather")         ss >> feather;
            else if (token == "input_corners")   { for (auto& c : inC)  ss >> c.x >> c.y; }
            else if (token == "output_corners")  { for (auto& c : outC) ss >> c.x >> c.y; }
            else if (token == "shape_type")      ss >> shapeType;
            else if (token == "shape_rr_per_corner") ss >> rrPerCorner;
            else if (token == "shape_rr_radii")  { for (auto& r : rrRadii) ss >> r; }
            else if (token == "shape_poly_sides") ss >> polySides;
            else if (token == "source_type")     ss >> srcType;
            else if (token == "source_color")    ss >> srcR >> srcG >> srcB;
            else if (token == "source_path")     std::getline(ss >> std::ws, srcPath);
        }

        // Create source
        using CP  = sources::ColorPatternSource;
        Shared<sources::Source> src;
        if (srcType == "solid_color") {
            auto s = std::make_shared<CP>(CP::Pattern::SOLID_COLOR,   srcR, srcG, srcB);
            s->initialize(); src = s;
        } else if (srcType == "checkerboard") {
            auto s = std::make_shared<CP>(CP::Pattern::CHECKERBOARD,  srcR, srcG, srcB);
            s->initialize(); src = s;
        } else if (srcType == "gradient") {
            auto s = std::make_shared<CP>(CP::Pattern::GRADIENT,      srcR, srcG, srcB);
            s->initialize(); src = s;
        } else if (srcType == "image") {
            auto s = std::make_shared<sources::ImageFileSource>(srcPath);
            s->initialize(); src = s;
        } else if (srcType == "video") {
            auto s = std::make_shared<sources::VideoFileSource>(srcPath);
            s->initialize(); src = s;
        } else if (srcType == "shader") {
            auto s = std::make_shared<sources::ShaderSource>(kDefaultFragSrc, 1280, 720);
            s->initialize(); src = s;
        } else if (srcType == "pipewire") {
            ++pipewireCount;
            // Placeholder — dark solid color so the layer is visible but obviously empty
            auto s = std::make_shared<CP>(CP::Pattern::SOLID_COLOR, 0.15f, 0.15f, 0.15f);
            s->initialize(); src = s;
        } else {
            auto s = std::make_shared<CP>(CP::Pattern::SOLID_COLOR, 0.5f, 0.5f, 0.5f);
            s->initialize(); src = s;
        }
        sources_.push_back(src);

        // Create shape
        Unique<layers::Shape> shape;
        switch (shapeType) {
        case 1: {
            auto rr = std::make_unique<layers::RoundedRectangleShape>(
                Vec2(0, 0), Vec2(1, 1), rrRadii[0]);
            rr->setPerCorner(rrPerCorner != 0);
            rr->setCornerRadii(rrRadii[0], rrRadii[1], rrRadii[2], rrRadii[3]);
            shape = std::move(rr);
            break;
        }
        case 2:
            shape = std::make_unique<layers::EllipseShape>(Vec2(0, 0), Vec2(1, 1));
            break;
        case 3:
            shape = std::make_unique<layers::PolygonShape>(
                polySides, Vec2(0.5f, 0.5f), 0.5f);
            break;
        case 4:
            shape = std::make_unique<layers::TriangleShape>(
                inC[0], inC[1], inC[2]);
            break;
        default:
            shape = std::make_unique<layers::RectangleShape>(Vec2(0, 0), Vec2(1, 1));
            break;
        }

        auto layer = createLayer(src, std::move(shape));
        layer->setVisible(visible);
        layer->setOpacity(opacity);
        layer->setBlendMode(blendMode);
        layer->setFeather(feather);
        layer->setCanvasIndex(
            std::min(canvasIdx, uiManager_->getCanvasCount() - 1));
        for (int i = 0; i < 4; ++i) {
            layer->setInputCorner(i, inC[i]);
            layer->setOutputCorner(i, outC[i]);
        }
    }

    if (!layers_.empty())
        uiManager_->setSelectedLayerIndex(0);

    if (pipewireCount > 0) {
        std::string msg = std::to_string(pipewireCount) +
                          (pipewireCount == 1
                               ? " PipeWire source"
                               : " PipeWire sources");
        msg += " could not be restored automatically and "
               "has been replaced with a dark placeholder.\n\n"
               "Please re-add the stream(s) via:\n"
               "  Sources > Add Source > PipeWire Stream\n"
               "and re-assign to the affected layer(s).";
        uiManager_->setLayoutWarning(msg);
    }
}

void ProjectionMapper::cleanup() {
    for (auto& pw : projectionWindows_)
        if (pw) pw->close();
    projectionWindows_.clear();

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
        bool anyOpen = std::any_of(projectionWindows_.begin(), projectionWindows_.end(),
                                   [](const auto& pw) { return pw->isOpen(); });
        if (!anyOpen)
            shouldClose_ = true;
    }
}

