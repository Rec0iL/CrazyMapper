# CrazyMapper — Current Status

**Updated**: April 9, 2026
**Build**: `cmake --build build --target CrazyMapper` → clean, no warnings from project code

---

## What's Working Now

### Core Pipeline
| Component | Status |
|-----------|--------|
| GLFW window + OpenGL 3.3 context | ✅ |
| ImGui rendering | ✅ |
| Homography DLT (Gaussian elim.) | ✅ |
| ShaderSource compile + FBO + quad | ✅ |
| ColorPatternSource (3 patterns) | ✅ NEW |
| Source ↔ Layer assignment (live swap) | ✅ NEW |
| Input Space free-form corner dragging | ✅ |
| Output Space free-form corner dragging | ✅ |
| Layer selection (all layers, fixed IDs) | ✅ |
| Sources panel with Assign button | ✅ NEW |
| Homography recomputed on corner drag | ✅ |

### Built-in Color Pattern Sources
Three instances created at startup, always available in the Sources panel:

| Name | Pattern | Default Color |
|------|---------|---------------|
| Pattern: Solid Color | Uniform fill | Blue (0.2, 0.5, 1.0) |
| Pattern: Checkerboard | 8×8 grid | Orange (1.0, 0.6, 0.1) |
| Pattern: Gradient | Animated diagonal | Purple (0.6, 0.1, 0.9) |

All three re-render to GPU texture every frame via FBO.

---

## What's Still Needed

### Immediate Next Step: Perspective Rendering
The homography matrix is computed but not yet used to warp textures visually.

**File**: `src/ui/output_space_view.cpp`
**Task**:
1. Bind `layer->getSource()->getTextureHandle()` as a GL texture
2. Set `homographyInverse` uniform on `shaders/perspective.frag`
3. Draw warped quad in OpenGL (not just ImGui lines)

All the infrastructure (shader template, FBO textures, homography matrix) is ready — this is wiring.

### Optional Backends
- GStreamer/FFmpeg video file decoding → `src/sources/video_file_source.cpp`
- PipeWire stream capture → `src/sources/pipewire_source.cpp`

---

## Architecture Overview

```
ProjectionMapper (application.cpp)
  ├── sources_: Source[]          ← Solid, Checker, Gradient + future media
  ├── layers_: Layer[]            ← Each wraps a source + input/output corners
  └── UIManager
        ├── InputSpaceView        ← Drag input corners → updates layer.inputCorners_
        ├── OutputSpaceView       ← Drag output corners → updates layer.outputCorners_
        ├── SourcesPanel          ← List + Assign to Layer
        ├── LayersPanel           ← Select active layer
        └── PropertiesPanel       ← Visibility, opacity, reset
```

### Source ↔ Layer Assignment Flow
```
User clicks source in Sources panel
→ selectedSourceIndex_ set in UIManager
User clicks "Assign to Layer"
→ pendingAssignment_ = true, assignSourceIndex_ = i
ProjectionMapper::update():
→ reads hasPendingSourceAssignment()
→ calls layers_[layIdx]->setSource(sources_[srcIdx])
Next frame: layer renders with new source
```

---

## File Structure (Key Files)

```
include/sources/
  color_pattern_source.hpp   ✅ NEW — 3 GPU-rendered patterns
  shader_source.hpp          ✅ full compile + FBO
  source.hpp                 ✅ abstract interface

src/sources/
  color_pattern_source.cpp   ✅ NEW — solid / checker / gradient shaders
  shader_source.cpp          ✅ compile, FBO, quad render

include/layers/
  layer.hpp                  ✅ inputCorners_ + outputCorners_ + setSource()

src/layers/
  layer.cpp                  ✅ homography wired to Homography::compute()

src/math/
  homography.cpp             ✅ real DLT — no external libs

src/ui/
  ui_manager.cpp             ✅ sources passed in, assignment handled
  input_space_view.cpp       ✅ free-form corner drag via setInputCorner()
  output_space_view.cpp      ✅ free-form corner drag via setOutputCorner()
```

---

**Priority Action**: Implement perspective-corrected texture rendering in Output Space.

## What's Included

### ✅ Complete Infrastructure

1. **Project Structure**
   - Organized include/src directory layout
   - CMakeLists.txt with automatic dependency management (GLFW, GLAD, GLM, ImGui)
   - Platform-independent build system

2. **Core Application**
   - GLFW window management with OpenGL 3.3+ context
   - ImGui UI framework initialization and rendering loop
   - Main application class with full lifecycle management
   - GLFW input callbacks for mouse/keyboard/scroll

3. **Type System & Utilities**
   - GLM type aliases (Vec2, Mat3, Mat4, etc.)
   - Smart pointer helpers (Unique, Shared, Weak)
   - Organized namespace structure for maintainability

### ✅ Source System (Architecture)

**Abstract Base**: `sources::Source`
- Pure virtual interface for all input types
- Methods: initialize, shutdown, update, getTextureHandle, getResolution, isValid

**Three Concrete Implementations** (placeholders):
- `PipeWireSource`: Capture from PipeWire daemon
- `VideoFileSource`: Play back video files
- `ShaderSource`: Generate procedural content via GLSL
- Each with stub methods and TODOs for backend integration

### ✅ Layer System (Complete Implementation)

**Shape Types**:
- `RectangleShape`: Axis-aligned rectangles (complete)
- `EllipseShape`: Circles/ellipses with customizable segments (complete)
- `RoundedRectangleShape`: Rounded corners with configurable radius (complete)
- `PolygonShape`: N-sided polygons with point-in-polygon testing (complete)

**Layer Class**:
- Combines source + shape + homography state
- Corner-pinning state management (4 output corners)
- Opacity, visibility, blend mode properties
- Homography matrix computation callbacks (structure ready)

### ✅ Math Framework

**Homography Utilities** (`math::Homography`):
- `compute()`: 4-point homography calculation (DLT framework, SVD deferred)
- `transform()`: Point transformation with perspective division
- `invert()`: Matrix inversion
- `normalize()`: Determinant normalization
- **Status**: Structure complete, SVD implementation required

### ✅ OpenGL Utilities (Complete)

- `ShaderProgram`: Compilation, linking, uniform setting helpers
- `Texture`: Creation, upload, filtering, wrapping
- `Mesh`: VAO/VBO management
- `Framebuffer`: Color attachment, depth/stencil, resizing

All with proper error handling and resource cleanup.

### ✅ UI System

**Input Space View**:
- ImGui window rendering
- Grid overlay (configurable)
- Shape overlay rendering with corner handles
- Mouse coordinate conversion
- Structure for shape drag interactions

**Output Space View**:
- ImGui window rendering
- Quadrilateral visualization
- Corner handle rendering
- Save/load corner point files (JSON-style)
- Mouse hit-detection for corner handles
- Structure for perspective rendering

**UI Manager**:
- Main menu bar
- Layer selection panel
- Property panel (visibility, opacity, corner management)
- Event routing and state management

### ✅ Shaders (Templates)

- `quad.vert`: Fullscreen quad vertex shader
- `perspective.frag`: **KEY SHADER** - Perspective-corrected texture sampling with homography
- `procedural.frag`: Example animated procedural content

All with detailed comments explaining the mathematics.

### ✅ Documentation

1. **README.md**: User guide, features, controls, building instructions
2. **ARCHITECTURE.md**: Detailed homography mathematics, 2D vs 3D reasoning, numerical stability
3. **BUILD.md**: Comprehensive compilation guide for all platforms (Linux, macOS, Windows)
4. **IMPLEMENTATION_CHECKLIST.md**: Status of all components with phase roadmap
5. **shaders/README.md**: Shader documentation and extension guidelines
6. **This file**: Delivery summary

---

## Component Checklist

### Fully Implemented ✅
- [x] Type system and core utilities
- [x] GLFW window and OpenGL context
- [x] ImGui integration and UI rendering
- [x] All four shape types
- [x] Layer management system
- [x] OpenGL shader/texture/mesh/framebuffer utilities
- [x] Input space view (rendering structure)
- [x] Output space view (rendering structure)
- [x] File I/O for corner configurations
- [x] Shader templates with perspective correction logic

### Architecture Ready (Placeholders) 🔨
- [ ] Homography DLT + SVD implementation
- [ ] Source initialization backends (PipeWire, GStreamer, shader compilation)
- [ ] Perspective shader uniform binding in output view
- [ ] Full interactive corner dragging
- [ ] Multi-layer rendering pipeline

### Deferred to Implementation Phase 📋
- [ ] PipeWire stream capture
- [ ] GStreamer/FFmpeg video decoding
- [ ] Fullscreen quad rendering
- [ ] GPU-accelerated texture warping
- [ ] Performance optimization
- [ ] Multi-output support

---

## How to Get Started

### 1. Build the Project
```bash
cd CrazyMapper
mkdir build && cd build
cmake ..
make -j$(nproc)
```
**Result**: Executable that runs successfully with test layer (no actual projection yet)

### 2. Implement Homography Math (CRITICAL PATH)
- **File**: `src/math/homography.cpp`
- **Dependency**: Eigen, Armadillo, or LAPACK for SVD
- **Docs**: See ARCHITECTURE.md section "Four-Point Homography (DLT Algorithm)"
- **Est. Time**: 2-3 hours

### 3. Implement Shader Rendering
- **File**: `src/sources/shader_source.cpp`
- **Tasks**: Shader compilation, framebuffer creation, fullscreen quad rendering
- **Docs**: See shaders/README.md
- **Est. Time**: 2-3 hours

### 4. Wire Up UI Interaction
- **Files**: `src/ui/output_space_view.cpp`, `src/ui/ui_manager.cpp`
- **Tasks**: Corner drag detection, homography updates, visual feedback
- **Est. Time**: 3-4 hours

### 5. Implement Source Backend
- **Choose One**: GStreamer/FFmpeg (video) or PipeWire (stream capture)
- **Files**: `src/sources/video_file_source.cpp` or `src/sources/pipewire_source.cpp`
- **Est. Time**: 4-6 hours per backend

**Total time to full functionality**: ~2 weeks for one person

---

## Architecture Highlights

### Clean Separation of Concerns

```
UI Layer (ImGui)
    ↓
Application (GLFW window, event loop)
    ↓
Layer Management (source + shape + transform)
    ↓
Source System (PipeWire, Video, Shader)
    ↓
Math (Homography transforms)
    ↓
OpenGL (Shaders, textures, rendering)
```

### Modular Source System

New source types can be added without affecting existing code:
```cpp
class MyCustomSource : public sources::Source {
    // Implement abstract methods
};

app.addSource(std::make_shared<MyCustomSource>());
```

### Shape Extensibility

New shape types follow the same pattern:
```cpp
class CustomShape : public layers::Shape {
    // Implement getCorners(), getVertices(), contains(), etc.
};
```

### Rendering Pipeline Ready

Once homography is implemented, perspective rendering is automatic:
```cpp
// Homography matrix computed from shape corners + output corners
Mat3 H_inv = layer->getInverseHomography();

// Passed to shader:
shader.setUniformMat3("homographyInverse", H_inv);

// Shader does: texCoord = (H_inv * vec3(fragCoord, 1.0)).xy / .z
```

---

## Key Design Decisions

### 1. 2D Homography (Not 3D Mesh)
- ✅ Simpler math and implementation
- ✅ Better performance (no mesh deformation needed)
- ✅ Sufficient for projection mapping use case
- ❌ Cannot do Z-axis rotations or extrusions

### 2. CPU-Side Matrix Computation
- ✅ Small matrices (3×3) computed infrequently
- ✅ No GPU overhead for corner changes
- ✅ Easier to debug in CPU space
- Passed to GPU via shader uniform

### 3. Modular Source System
- ✅ Backends can be implemented independently
- ✅ Can stub/mock sources for testing
- ✅ No coupling between source types
- Allows parallel development

### 4. ImGui for UI
- ✅ Immediate mode simplicity
- ✅ No XML/layout file complexity
- ✅ Good for rapid iteration
- ❌ Not optimized for heavily-populated UIs

### 5. CMake with FetchContent
- ✅ No manual dependency installation (mostly)
- ✅ Reproducible builds
- ✅ Cross-platform
- Optional: Can switch to system packages

---

## Customization Points

### Visual Appearance
- Edit shader files (`shaders/`)
- Adjust ImGui colors/style in `src/ui/ui_manager.cpp`
- Modify grid display parameters

### Input Handling
- Expand GLFW callbacks in `src/core/application.cpp`
- Add keyboard shortcuts
- Support touch input (if targeting tablets)

### Rendering
- Add post-processing effects
- Implement multi-layer blending modes
- Add shadow/glow effects to corners

### Data Persistence
- Extend save/load to project files
- Add undo/redo system
- Support configuration files

---

## Testing Recommendations

### Unit Tests
```cpp
// Test homography computation
TEST(HomographyTest, IdentityTransform) { ... }
TEST(HomographyTest, 90DegreePerspective) { ... }
TEST(HomographyTest, InverseRoundTrip) { ... }
```

### Integration Tests
```cpp
// Test full layer workflow
TEST(LayerTest, CornerPinningUpdatesHomography) { ... }
TEST(SourceTest, ShaderRendersToTexture) { ... }
```

### Visual Regression Tests
```cpp
// Render reference images and compare
VISUAL_TEST(CornerPinning, CompareAgainstReference);
```

---

## Performance Characteristics (Estimated)

- **Empty application**: ~100 MB memory, 60 FPS
- **Per layer**: +5-10 MB (depends on source resolution)
- **Per source update**: <1 ms CPU, GPU depends on resolution
- **Homography computation**: <0.1 ms (3×3 matrix)
- **ImGui render**: ~2-5 ms per frame

---

## Known Limitations & Future Work

### Current Limitations
- No multi-output rendering
- Single-threaded (sources should use async internally)
- Planar 2D projections only
- No GPU texture array support for efficiency

### Future Enhancements
- GPU-parallel source decoding
- Docking UI layout
- Timeline scrubbing for videos
- Preset saving/loading
- OSC remote control
- HDMI/DisplayPort output management

---

## Support & Next Steps

1. **Start Here**: Read `BUILD.md` to compile the project
2. **Understand Architecture**: Review `ARCHITECTURE.md` for math details
3. **Check Status**: See `IMPLEMENTATION_CHECKLIST.md` for next tasks
4. **Implement Core**: Focus on homography computation first
5. **Test Iteratively**: Build each component incrementally

---

## Files Delivered

```
CrazyMapper/
├── include/
│   ├── core/
│   │   ├── types.hpp                    (✅ complete)
│   │   └── application.hpp              (✅ complete)
│   ├── sources/
│   │   ├── source.hpp                   (✅ complete)
│   │   ├── pipewire_source.hpp          (✅ placeholder)
│   │   ├── video_file_source.hpp        (✅ placeholder)
│   │   └── shader_source.hpp            (✅ complete structure)
│   ├── layers/
│   │   ├── shape.hpp                    (✅ complete)
│   │   ├── rectangle_shape.hpp          (✅ complete)
│   │   ├── ellipse_shape.hpp            (✅ complete)
│   │   ├── rounded_rectangle_shape.hpp  (✅ complete)
│   │   ├── polygon_shape.hpp            (✅ complete)
│   │   └── layer.hpp                    (✅ complete)
│   ├── math/
│   │   └── homography.hpp               (✅ interface, 🔨 SVD deferred)
│   ├── gl/
│   │   ├── shader_program.hpp           (✅ complete)
│   │   ├── texture.hpp                  (✅ complete)
│   │   ├── mesh.hpp                     (✅ complete)
│   │   └── framebuffer.hpp              (✅ complete)
│   └── ui/
│       ├── input_space_view.hpp         (✅ complete)
│       ├── output_space_view.hpp        (✅ complete)
│       └── ui_manager.hpp               (✅ complete)
├── src/
│   ├── core/application.cpp             (✅ complete)
│   ├── main.cpp                         (✅ complete)
│   ├── sources/
│   │   ├── pipewire_source.cpp          (✅ stub)
│   │   ├── video_file_source.cpp        (✅ stub)
│   │   └── shader_source.cpp            (✅ structure)
│   ├── layers/
│   │   ├── rectangle_shape.cpp          (✅ complete)
│   │   ├── ellipse_shape.cpp            (✅ complete)
│   │   ├── rounded_rectangle_shape.cpp  (✅ complete)
│   │   ├── polygon_shape.cpp            (✅ complete)
│   │   └── layer.cpp                    (✅ complete)
│   ├── math/
│   │   └── homography.cpp               (🔨 structure ready, SVD deferred)
│   ├── gl/
│   │   ├── shader_program.cpp           (✅ complete)
│   │   ├── texture.cpp                  (✅ complete)
│   │   ├── mesh.cpp                     (✅ complete)
│   │   └── framebuffer.cpp              (✅ complete)
│   └── ui/
│       ├── input_space_view.cpp         (✅ complete)
│       ├── output_space_view.cpp        (✅ complete)
│       └── ui_manager.cpp               (✅ complete)
├── shaders/
│   ├── quad.vert                        (✅ template)
│   ├── perspective.frag                 (✅ template)
│   ├── procedural.frag                  (✅ example)
│   └── README.md                        (✅ documentation)
├── CMakeLists.txt                       (✅ complete)
├── README.md                            (✅ user guide)
├── ARCHITECTURE.md                      (✅ detailed design)
├── BUILD.md                             (✅ build instructions)
├── IMPLEMENTATION_CHECKLIST.md          (✅ status tracking)
└── This file

Total: 20+ headers, 15+ implementations, 6 documentation files
```

---

## Contact & Questions

For questions about the architecture or implementation approach, refer to the appropriate documentation file:

- **How do I build it?** → `BUILD.md`
- **How does the math work?** → `ARCHITECTURE.md`
- **What still needs to be done?** → `IMPLEMENTATION_CHECKLIST.md`
- **How do I use the UI?** → `README.md`
- **What are the shader templates?** → `shaders/README.md`

---

**Delivery Date**: April 9, 2026  
**Status**: Ready for implementation  
**Quality Level**: Production-ready architecture, placeholder implementations

Enjoy building! 🎉

