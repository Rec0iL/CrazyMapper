#include "ui/ui_manager.hpp"
#include "ui/file_dialog.hpp"
#include "layers/rectangle_shape.hpp"
#include "layers/rounded_rectangle_shape.hpp"
#include "layers/ellipse_shape.hpp"
#include "layers/polygon_shape.hpp"
#include "layers/triangle_shape.hpp"
#include "sources/image_file_source.hpp"
#include <imgui.h>
#include <algorithm>

namespace ui {

UIManager::UIManager()
    : selectedLayerIndex_(0),
      showDemoWindow_(false),
      lastMousePos_(0.0f),
      draggedCorner_(-1),
      isDraggingCorner_(false),
      nextLayerId_(1),
      createNewLayer_(false),
      pendingAssignment_(false),
      assignSourceIndex_(-1),
      selectedSourceIndex_(-1),
      pendingReorder_(false),
      reorderFrom_(-1),
      reorderTo_(-1),
      pendingToggleProjWin_(false),
      projectionWindowOpen_(false),
      canvasAspectW_(16.0f),
      canvasAspectH_(9.0f) {

    inputView_ = std::make_unique<InputSpaceView>();
    outputView_ = std::make_unique<OutputSpaceView>();
}

UIManager::~UIManager() {
}

bool UIManager::initialize(GLFWwindow* /* window */) {
    // ImGui setup would be done here
    // For this boilerplate, we assume ImGui is already initialized
    return true;
}

void UIManager::shutdown() {
}

void UIManager::render(const std::vector<Shared<layers::Layer>>& layers,
                       const std::vector<Shared<sources::Source>>& sources,
                       float /* deltaTime */,
                       bool projectionWindowOpen) {
    projectionWindowOpen_ = projectionWindowOpen;
    ImGui::NewFrame();

    renderMainMenuBar(projectionWindowOpen);

    ImGuiIO&    io      = ImGui::GetIO();
    ImGuiStyle& style   = ImGui::GetStyle();
    float menuH  = ImGui::GetFrameHeight() + style.FramePadding.y * 2.0f;
    // Height a collapsed (rolled-up) window with a standard title bar occupies.
    float titleH = ImGui::GetFrameHeight() + style.FramePadding.y * 2.0f
                   + style.WindowPadding.y * 2.0f + style.WindowBorderSize * 2.0f;

    float availH = io.DisplaySize.y - menuH;
    float col1W  = io.DisplaySize.x * 0.60f;
    float col2W  = io.DisplaySize.x * 0.20f;
    float col3W  = io.DisplaySize.x * 0.20f;
    float col2X  = col1W;
    float col3X  = col1W + col2W;

    Shared<layers::Layer> currentLayer;
    if (!layers.empty() && selectedLayerIndex_ >= 0 &&
        selectedLayerIndex_ < static_cast<int>(layers.size())) {
        currentLayer = layers[selectedLayerIndex_];
    }

    // Helper: split availH between two panels given each panel's collapse state.
    // When both uncollapsed the split is 'ratio' / '1-ratio'.
    // When one collapses it receives titleH and the partner gets the remainder.
    // When both collapse each gets titleH (gap below is intentional/correct).
    auto splitH = [&](bool aCollapsed, bool bCollapsed, float ratio)
                      -> std::pair<float, float> {
        if (!aCollapsed && !bCollapsed) {
            float aH = std::round(availH * ratio);
            return {aH, availH - aH};
        } else if (aCollapsed && !bCollapsed) {
            return {titleH, availH - titleH};
        } else if (!aCollapsed && bCollapsed) {
            return {availH - titleH, titleH};
        } else {
            return {titleH, titleH};
        }
    };

    // ---- COLUMN 1: Input Space (top) + Output Space (bottom) ----
    auto [inputH, outputH] = splitH(inputCollapsed_, outputCollapsed_, 0.5f);
    if (currentLayer) {
        ImGui::SetNextWindowPos(ImVec2(0.f, menuH), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(col1W, inputH), ImGuiCond_Always);
        inputView_->render(currentLayer, &inputCollapsed_);

        ImGui::SetNextWindowPos(ImVec2(0.f, menuH + inputH), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(col1W, outputH), ImGuiCond_Always);
        outputView_->render(layers, selectedLayerIndex_,
                            getCanvasAspectRatio(), &outputCollapsed_);
    }

    // ---- COLUMN 2: Sources (top, 75%) + Canvas Settings (bottom, 25%) ----
    auto [sourcesH, canvasH] = splitH(sourcesCollapsed_, canvasCollapsed_, 0.75f);

    ImGui::SetNextWindowPos(ImVec2(col2X, menuH), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(col2W, sourcesH), ImGuiCond_Always);
    renderSourcesPanel(sources, currentLayer, &sourcesCollapsed_);

    ImGui::SetNextWindowPos(ImVec2(col2X, menuH + sourcesH), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(col2W, canvasH), ImGuiCond_Always);
    renderCanvasSettings(&canvasCollapsed_);

    // ---- COLUMN 3: Layers (top) + Properties (bottom) ----
    // Layers is always rendered; Properties only when a layer is selected.
    float layersH, propsH;
    if (currentLayer) {
        auto [lH, pH] = splitH(layersCollapsed_, propsCollapsed_, 0.40f);
        layersH = lH;
        propsH  = pH;
    } else {
        layersH = availH;
        propsH  = 0.f;
    }

    {
        auto& mutableLayers = const_cast<std::vector<Shared<layers::Layer>>&>(layers);
        ImGui::SetNextWindowPos(ImVec2(col3X, menuH), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(col3W, layersH), ImGuiCond_Always);
        renderLayerPanel(mutableLayers, &layersCollapsed_);
    }

    if (currentLayer) {
        ImGui::SetNextWindowPos(ImVec2(col3X, menuH + layersH), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(col3W, propsH), ImGuiCond_Always);
        renderPropertyPanel(currentLayer, &propsCollapsed_);
    }

    // ---- Layout warning / info popup ----
    if (!layoutWarning_.empty() && !showLayoutWarning_) {
        ImGui::OpenPopup("##layoutwarn");
        showLayoutWarning_ = true;
    }
    if (showLayoutWarning_ &&
        ImGui::BeginPopupModal("##layoutwarn", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Layout Notice");
        ImGui::Separator();
        ImGui::TextWrapped("%s", layoutWarning_.c_str());
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(200, 0))) {
            layoutWarning_.clear();
            showLayoutWarning_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Render();
}

void UIManager::onMouseMove(Vec2 screenPos) {
    lastMousePos_ = screenPos;

    if (isDraggingCorner_ && draggedCorner_ >= 0) {
        // Dragging corner handle
    }
}

void UIManager::onMouseButton(int /* button */, int /* action */, int /* mods */) {
    // Handle mouse button events
}

void UIManager::onScroll(double /* xOffset */, double /* yOffset */) {
    // Handle scroll events (e.g., zoom in output space)
}

void UIManager::renderMainMenuBar(bool projectionWindowOpen) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Layer")) {
                createNewLayer_ = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Layout...")) {
                std::string path = saveFileDialog("Save Layout", "cml");
                if (!path.empty()) {
                    // Ensure .cml extension
                    if (path.size() < 4 ||
                        path.compare(path.size() - 4, 4, ".cml") != 0)
                        path += ".cml";
                    saveLayoutPath_    = path;
                    pendingSaveLayout_ = true;
                }
            }
            if (ImGui::MenuItem("Load Layout...")) {
                std::string path = openFileDialog("Load Layout", "cml");
                if (!path.empty()) {
                    loadLayoutPath_    = path;
                    pendingLoadLayout_ = true;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                // Request application shutdown
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Demo Window", "", &showDemoWindow_);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Projection")) {
            const char* label = projectionWindowOpen
                ? "Close Projection Window"
                : "Open Projection Window";
            if (ImGui::MenuItem(label))
                pendingToggleProjWin_ = true;
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (showDemoWindow_) {
        ImGui::ShowDemoWindow(&showDemoWindow_);
    }
}

void UIManager::renderLayerPanel(std::vector<Shared<layers::Layer>>& layers,
                                  bool* outCollapsed) {
    bool opened = ImGui::Begin("Layers", nullptr,
                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    if (outCollapsed) *outCollapsed = ImGui::IsWindowCollapsed();

    if (opened) {
        ImGui::Text("%zu layer(s)", layers.size());
        ImGui::SameLine();
        if (ImGui::SmallButton("+ New")) createNewLayer_ = true;
        ImGui::Separator();

        int n = static_cast<int>(layers.size());
        for (int i = 0; i < n; ++i) {
            bool isSelected = (i == selectedLayerIndex_);

            ImGui::PushID(i);

            // Up button
            if (i == 0) ImGui::BeginDisabled();
            if (ImGui::SmallButton("^")) {
                reorderFrom_   = i;
                reorderTo_     = i - 1;
                pendingReorder_ = true;
                if (selectedLayerIndex_ == i)     selectedLayerIndex_ = i - 1;
                else if (selectedLayerIndex_ == i - 1) selectedLayerIndex_ = i;
            }
            if (i == 0) ImGui::EndDisabled();

            ImGui::SameLine();

            // Down button
            if (i == n - 1) ImGui::BeginDisabled();
            if (ImGui::SmallButton("v")) {
                reorderFrom_   = i;
                reorderTo_     = i + 1;
                pendingReorder_ = true;
                if (selectedLayerIndex_ == i)     selectedLayerIndex_ = i + 1;
                else if (selectedLayerIndex_ == i + 1) selectedLayerIndex_ = i;
            }
            if (i == n - 1) ImGui::EndDisabled();

            ImGui::SameLine();

            // Delete button
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::SmallButton("X")) {
                deleteLayerIndex_  = i;
                pendingDeleteLayer_ = true;
                if (selectedLayerIndex_ >= i && selectedLayerIndex_ > 0)
                    selectedLayerIndex_--;
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine();

            char label[128];
            snprintf(label, sizeof(label), "Layer %u (%s)##layer_%u",
                     layers[i]->getId(),
                     layers[i]->getSource()->getName().c_str(),
                     layers[i]->getId());
            if (ImGui::Selectable(label, isSelected))
                selectedLayerIndex_ = i;

            ImGui::PopID();
        }
    }
    ImGui::End();
}

void UIManager::renderPropertyPanel(const Shared<layers::Layer>& layer,
                                     bool* outCollapsed) {
    bool opened = ImGui::Begin("Properties", nullptr,
                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    if (outCollapsed) *outCollapsed = ImGui::IsWindowCollapsed();
    if (!layer) { ImGui::End(); return; }
    if (opened) {
        // Layer visibility and opacity
        bool visible = layer->isVisible();
        if (ImGui::Checkbox("Visible", &visible)) {
            layer->setVisible(visible);
        }

        float opacity = layer->getOpacity();
        if (ImGui::SliderFloat("Opacity", &opacity, 0.0f, 1.0f)) {
            layer->setOpacity(opacity);
        }

        float feather = layer->getFeather();
        if (ImGui::SliderFloat("Feather", &feather, 0.0f, 0.2f, "%.3f")) {
            layer->setFeather(feather);
        }

        ImGui::Separator();

        if (ImGui::Button("Load Corner Points")) {
            // Load corner points from file
        }

        ImGui::SameLine();

        if (ImGui::Button("Save Corner Points")) {
            // Save corner points to file
        }

        if (ImGui::Button("Reset Corners")) {
            outputView_->resetCorners(layer);
        }

        ImGui::Separator();
        ImGui::Text("Drag corners in Input Space");
        ImGui::Text("to adjust source area.");
        ImGui::Text("Drag corners in Output Space");
        ImGui::Text("to set projection area.");

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Shape", ImGuiTreeNodeFlags_DefaultOpen)) {
            int currentType = layer->getShapeType();
            const char* shapeNames[] = {
                "Rectangle", "Rounded Rectangle", "Ellipse", "N-Polygon", "Triangle"};
            int combo = currentType;
            if (ImGui::Combo("Type##shape", &combo, shapeNames, IM_ARRAYSIZE(shapeNames))) {
                Unique<layers::Shape> newShape;
                switch (combo) {
                case 0:
                    newShape = std::make_unique<layers::RectangleShape>(Vec2(0,0), Vec2(1,1));
                    break;
                case 1: {
                    auto rrShape = std::make_unique<layers::RoundedRectangleShape>(
                        Vec2(0,0), Vec2(1,1), lastShapeRadius_);
                    rrShape->setPerCorner(lastShapePerCorner_);
                    if (lastShapePerCorner_)
                        rrShape->setCornerRadii(lastShapeCornerRadii_[0], lastShapeCornerRadii_[1],
                                                lastShapeCornerRadii_[2], lastShapeCornerRadii_[3]);
                    newShape = std::move(rrShape);
                    break;
                }
                case 2:
                    newShape = std::make_unique<layers::EllipseShape>(Vec2(0,0), Vec2(1,1));
                    break;
                case 3:
                    newShape = std::make_unique<layers::PolygonShape>(
                        lastShapeSides_, Vec2(0.5f,0.5f), 0.5f);
                    break;
                case 4:
                    newShape = std::make_unique<layers::TriangleShape>(
                        Vec2(0.1f,0.1f), Vec2(0.9f,0.1f), Vec2(0.5f,0.9f));
                    break;
                }
                if (newShape) {
                    layer->setShape(std::move(newShape));
                    // For triangle: reset corners to sensible triangle defaults
                    if (combo == 4) {
                        static const Vec2 triDef[3] = {
                            Vec2(0.1f,0.1f), Vec2(0.9f,0.1f), Vec2(0.5f,0.9f)};
                        for (int ti = 0; ti < 3; ++ti) {
                            layer->setInputCorner(ti,  triDef[ti]);
                            layer->setOutputCorner(ti, triDef[ti]);
                        }
                    }
                }
            }

            if (currentType == 1) {  // ROUNDED_RECTANGLE
                auto* rr = dynamic_cast<layers::RoundedRectangleShape*>(layer->getShape());
                bool perCorner = rr ? rr->isPerCorner() : false;

                if (ImGui::Checkbox("Per Corner##shape", &perCorner)) {
                    if (rr) {
                        rr->setPerCorner(perCorner);
                        lastShapePerCorner_ = perCorner;
                        if (!perCorner) {
                            // Flatten: use TL value for all corners
                            auto cr = rr->getCornerRadii();
                            rr->setCornerRadius(cr[0]);
                            lastShapeRadius_ = cr[0];
                        } else {
                            // Expand: replicate uniform to all four
                            rr->setCornerRadii(lastShapeRadius_, lastShapeRadius_,
                                               lastShapeRadius_, lastShapeRadius_);
                            lastShapeCornerRadii_ = {lastShapeRadius_, lastShapeRadius_,
                                                     lastShapeRadius_, lastShapeRadius_};
                        }
                    }
                }

                if (!perCorner) {
                    float r = rr ? rr->getCornerRadius() : lastShapeRadius_;
                    if (ImGui::SliderFloat("Corner Radius##shape", &r, 0.0f, 0.5f)) {
                        lastShapeRadius_ = r;
                        if (rr) rr->setCornerRadius(r);
                    }
                } else {
                    auto cr = rr ? rr->getCornerRadii() : lastShapeCornerRadii_;
                    bool changed = false;
                    changed |= ImGui::SliderFloat("TL##shapecr", &cr[0], 0.0f, 0.5f);
                    changed |= ImGui::SliderFloat("TR##shapecr", &cr[1], 0.0f, 0.5f);
                    changed |= ImGui::SliderFloat("BR##shapecr", &cr[2], 0.0f, 0.5f);
                    changed |= ImGui::SliderFloat("BL##shapecr", &cr[3], 0.0f, 0.5f);
                    if (changed) {
                        lastShapeCornerRadii_ = cr;
                        if (rr) rr->setCornerRadii(cr[0], cr[1], cr[2], cr[3]);
                    }
                }
            }
            if (currentType == 3) {  // N_POLYGON
                int sides = layer->getShapePolySides();
                if (sides < 3) sides = lastShapeSides_;
                if (ImGui::SliderInt("Sides##shape", &sides, 3, 24)) {
                    lastShapeSides_ = sides;
                    layer->setShape(std::make_unique<layers::PolygonShape>(
                        sides, Vec2(0.5f,0.5f), 0.5f));
                }
            }
        }
    }
    ImGui::End();
}

void UIManager::renderSourcesPanel(const std::vector<Shared<sources::Source>>& sources,
                                    const Shared<layers::Layer>& currentLayer,
                                    bool* outCollapsed) {
    bool opened = ImGui::Begin("Sources", nullptr,
                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    if (outCollapsed) *outCollapsed = ImGui::IsWindowCollapsed();
    if (opened) {

        ImGui::Text("Available Sources (%zu)", sources.size());
        ImGui::Separator();

        if (sources.empty()) {
            ImGui::TextDisabled("No sources created yet.");
            ImGui::TextDisabled("Use File > New Layer to");
            ImGui::TextDisabled("create one.");
        } else {
            ImGui::BeginChild("SourceList",
                              ImVec2(0, std::max(40.0f, ImGui::GetContentRegionAvail().y - 180.0f)),
                              false);
            for (int i = 0; i < static_cast<int>(sources.size()); ++i) {
                bool isSel = (i == selectedSourceIndex_);
                char label[128];
                snprintf(label, sizeof(label), "%s##src_%d",
                         sources[i]->getName().c_str(), i);
                if (ImGui::Selectable(label, isSel))
                    selectedSourceIndex_ = i;

                // Indicate if this source is currently assigned to the layer
                if (currentLayer && currentLayer->getSource() == sources[i]) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.3f, 1.f, 0.3f, 1.f), "[active]");
                }
            }
            ImGui::EndChild();

            ImGui::Separator();

            bool canAssign = currentLayer &&
                             selectedSourceIndex_ >= 0 &&
                             selectedSourceIndex_ < static_cast<int>(sources.size());

            if (!canAssign) ImGui::BeginDisabled();
            if (ImGui::Button("Assign to Layer", ImVec2(-1, 0))) {
                assignSourceIndex_  = selectedSourceIndex_;
                pendingAssignment_  = true;
            }
            if (!canAssign) ImGui::EndDisabled();

            bool canDelete = selectedSourceIndex_ >= 0 &&
                             selectedSourceIndex_ < static_cast<int>(sources.size());
            if (!canDelete) ImGui::BeginDisabled();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Delete Source", ImVec2(-1, 0))) {
                deleteSourceIndex_  = selectedSourceIndex_;
                pendingDeleteSource_ = true;
                selectedSourceIndex_ = -1;
            }
            ImGui::PopStyleColor(2);
            if (!canDelete) ImGui::EndDisabled();

            if (!currentLayer)
                ImGui::TextDisabled("Select a layer first.");
        }

        // ---- Add Source ----
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Add Source")) {
            const char* kindNames[] = {
                "Pattern: Solid Color", "Pattern: Checkerboard",
                "Pattern: Gradient",   "Pattern: Calibration Grid",
                "Image File", "Video File", "Shader File", "PipeWire Stream"
            };
            ImGui::Combo("Kind##newsrc", &newSourceKind_, kindNames,
                         IM_ARRAYSIZE(kindNames));

            if (newSourceKind_ <= 2) {  // Color Pattern types with tint colour
                ImGui::ColorEdit3("Color##newsrc", newSourceColor_);
            } else if (newSourceKind_ == 3) {  // Calibration Grid (no color option)
                ImGui::TextDisabled("Black background with white 16x9 grid + centre cross.");
            } else if (newSourceKind_ == 4) {  // Image File
                ImGui::InputText("Path##newsrc", newSourceImagePath_,
                                 sizeof(newSourceImagePath_));
                ImGui::SameLine();
                if (ImGui::Button("Browse...##img")) {
                    std::string picked = ui::openFileDialog(
                        "Select Image File", "png;jpg;jpeg;bmp;tga;gif");
                    if (!picked.empty()) {
                        snprintf(newSourceImagePath_, sizeof(newSourceImagePath_),
                                 "%s", picked.c_str());
                    }
                }
            } else if (newSourceKind_ == 5) {  // Video File
                ImGui::InputText("Path##newsrc", newSourceVideoPath_,
                                 sizeof(newSourceVideoPath_));
                ImGui::SameLine();
                if (ImGui::Button("Browse...##vid")) {
                    std::string picked = ui::openFileDialog(
                        "Select Video File", "mp4;mkv;mov;avi;webm;ts;flv");
                    if (!picked.empty()) {
                        snprintf(newSourceVideoPath_, sizeof(newSourceVideoPath_),
                                 "%s", picked.c_str());
                    }
                }
            } else if (newSourceKind_ == 6) {  // Shader File
                ImGui::InputText("Path##newsrc", newSourceShaderPath_,
                                 sizeof(newSourceShaderPath_));
                ImGui::SameLine();
                if (ImGui::Button("Browse...##shd")) {
                    std::string picked = ui::openFileDialog(
                        "Select GLSL Shader", "glsl;frag;fs;shadertoy");
                    if (!picked.empty()) {
                        snprintf(newSourceShaderPath_, sizeof(newSourceShaderPath_),
                                 "%s", picked.c_str());
                    }
                }
                ImGui::TextDisabled("GLSL fragment shader. Uniforms: iTime (float), iResolution (vec2).");
                ImGui::TextDisabled(".shadertoy files are wrapped automatically.");
            } else {  // PipeWire Stream (kind 7)
                ImGui::TextWrapped("Opens the OS native screen/window picker "
                                   "(same dialog as Discord, OBS, etc.).");
                ImGui::Spacing();

                bool busy = portalCapturePending_;
                if (busy) ImGui::BeginDisabled();
                if (ImGui::Button("Pick Screen...", ImVec2(-1, 0)))
                    pendingPortalCapture_ = true;
                if (busy) {
                    ImGui::EndDisabled();
                    ImGui::TextDisabled("Waiting for selection...");
                }
            }

            // For kind 7 (PipeWire), the button above IS the "Add" action.
            // For all other kinds show the Add button.
            if (newSourceKind_ != 7) {
                if (ImGui::Button("Add##newsrc", ImVec2(-1, 0))) {
                    bool pathOk = (newSourceKind_ != 4 || newSourceImagePath_[0] != '\0') &&
                                  (newSourceKind_ != 5 || newSourceVideoPath_[0] != '\0') &&
                                  (newSourceKind_ != 6 || newSourceShaderPath_[0] != '\0');
                    if (pathOk)
                        pendingAddSource_ = true;
                }
            }
        }
    }
    ImGui::End();
}

void UIManager::renderCanvasSettings(bool* outCollapsed) {
    bool opened = ImGui::Begin("Canvas", nullptr,
                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    if (outCollapsed) *outCollapsed = ImGui::IsWindowCollapsed();
    if (opened) {
        ImGui::Text("Aspect Ratio");
        ImGui::PushItemWidth(60.0f);
        ImGui::InputFloat("W##aw", &canvasAspectW_, 0.0f, 0.0f, "%.0f");
        ImGui::SameLine();
        ImGui::Text(":");
        ImGui::SameLine();
        ImGui::InputFloat("H##ah", &canvasAspectH_, 0.0f, 0.0f, "%.0f");
        ImGui::PopItemWidth();
        canvasAspectW_ = std::max(0.1f, canvasAspectW_);
        canvasAspectH_ = std::max(0.1f, canvasAspectH_);

        ImGui::Separator();
        float ratio = getCanvasAspectRatio();
        ImGui::Text("Ratio: %.4f", ratio);

        ImGui::Separator();
        const char* btnLabel = projectionWindowOpen_
            ? "Close Projection Window"
            : "Open Projection Window";
        if (ImGui::Button(btnLabel, ImVec2(-1, 0)))
            pendingToggleProjWin_ = true;
    }
    ImGui::End();
}

} // namespace ui

