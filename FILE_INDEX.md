# CrazyMapper - File Index & Status

Generated: April 9, 2026

---

## Documentation Files

### Getting Started
- **`QUICKSTART.md`** - 5-minute overview, build instructions, priority tasks
  - *Start here if you just want to build and run*
- **`README.md`** - Full feature overview, architecture summary, user guide
  - *Read after QUICKSTART for comprehensive understanding*

### Technical Details
- **`ARCHITECTURE.md`** - Homography mathematics, 2D vs 3D reasoning, implementation strategy
  - *Read before implementing the homography math*
- **`BUILD.md`** - Platform-specific build instructions, CMake details, troubleshooting
  - *Reference when building on different systems*
- **`IMPLEMENTATION_CHECKLIST.md`** - Component status, priority roadmap, testing strategy
  - *Track progress and identify what needs implementation*
- **`DELIVERY_SUMMARY.md`** - What was delivered, design decisions, performance characteristics
  - *Executive summary of the boilerplate*
- **`shaders/README.md`** - Shader documentation, integration guide, extension tips
  - *Reference when working with GLSL code*

---

## Core Project Files

### Configuration
- **`CMakeLists.txt`** ✅
  - GLFW, GLAD, GLM, ImGui dependency management
  - Compiler flags and platform detection
  - Optional PipeWire/GStreamer support (commented)

### Entry Point
- **`src/main.cpp`** ✅
  - Creates and runs ProjectionMapper application

---

## Application Core

### Headers
- **`include/core/types.hpp`** ✅
  - GLM type aliases (Vec2, Mat3, Mat4, etc.)
  - Smart pointer helpers (Unique, Shared, Weak)

- **`include/core/application.hpp`** ✅
  - Main ProjectionMapper class
  - GLFW callback declarations
  - Source and layer management

### Implementation
- **`src/core/application.cpp`** ✅
  - GLFW window creation
  - OpenGL context initialization
  - ImGui setup
  - Main render loop
  - Input callback implementation
  - ~390 lines

---

## Source System

### Headers & Implementations
- **`include/sources/source.hpp`** ✅ abstract interface
- **`include/sources/pipewire_source.hpp`** 🔨 placeholder
- **`include/sources/video_file_source.hpp`** 🔨 placeholder
- **`include/sources/shader_source.hpp`** 🔨 partial

### Implementations
- **`src/sources/pipewire_source.cpp`** 🔨
  - Structure ready, backend deferred
  - TODO: PipeWire connection setup
  
- **`src/sources/video_file_source.cpp`** 🔨
  - Structure ready, backend deferred
  - TODO: GStreamer/FFmpeg decoding
  
- **`src/sources/shader_source.cpp`** 🔨
  - Shader compilation stub
  - Framebuffer creation placeholder
  - TODO: Actual compilation logic

---

## Layer System

### Shape Classes

#### Headers & Implementations
- **`include/layers/shape.hpp`** ✅ abstract interface
- **`include/layers/rectangle_shape.hpp`** ✅
  - `src/layers/rectangle_shape.cpp` ✅
  - Complete: constructor, corners, bounds, hit-testing, vertices
  
- **`include/layers/ellipse_shape.hpp`** ✅
  - `src/layers/ellipse_shape.cpp` ✅
  - Complete: circle/ellipse approximation with configurable segments
  
- **`include/layers/rounded_rectangle_shape.hpp`** ✅
  - `src/layers/rounded_rectangle_shape.cpp` ✅
  - Complete: rounded corners with radius and segment control
  
- **`include/layers/polygon_shape.hpp`** ✅
  - `src/layers/polygon_shape.cpp` ✅
  - Complete: n-sided polygons, ray-casting point-in-polygon test

### Layer Management
- **`include/layers/layer.hpp`** ✅
  - Combines source + shape + homography state
  - Corner-pinning interface

- **`src/layers/layer.cpp`** ✅
  - Layer linking and state management
  - TODO: Homography computation calls (deferred to math implementation)

---

## Math & Transforms

### Homography
- **`include/math/homography.hpp`** ✅ interface
  - `src/math/homography.cpp` 🔨 framework
  - Declared functions: compute(), transform(), invert(), normalize()
  - TODO: SVD-based DLT implementation
  - ~80 lines (framework), needs ~300 lines (complete)

---

## OpenGL Utilities

### Shader Program
- **`include/gl/shader_program.hpp`** ✅
  - `src/gl/shader_program.cpp` ✅
  - Compilation, linking, uniform helpers
  - Complete with error checking
  - ~150 lines

### Texture
- **`include/gl/texture.hpp`** ✅
  - `src/gl/texture.cpp` ✅
  - Creation, upload, filtering, wrapping
  - Complete with lifecycle management
  - ~100 lines

### Mesh
- **`include/gl/mesh.hpp`** ✅
  - `src/gl/mesh.cpp` ✅
  - VAO/VBO management
  - Dynamic vertex updates
  - ~60 lines

### Framebuffer
- **`include/gl/framebuffer.hpp`** ✅
  - `src/gl/framebuffer.cpp` ✅
  - FBO with color attachment and renderbuffer
  - Resizing support
  - ~110 lines

---

## UI System

### Input Space View
- **`include/ui/input_space_view.hpp`** ✅
  - `src/ui/input_space_view.cpp` ✅
  - ImGui window rendering
  - Grid overlay, shape visualization
  - TODO: Interactive shape dragging

### Output Space View
- **`include/ui/output_space_view.hpp`** ✅
  - `src/ui/output_space_view.cpp` ✅
  - ImGui window for corner-pinning
  - Corner handle rendering
  - Save/load corner configuration
  - TODO: Perspective rendering and corner drag interaction

### UI Manager
- **`include/ui/ui_manager.hpp`** ✅
  - `src/ui/ui_manager.cpp` ✅
  - Main UI coordinator
  - Menu bar, layer panel, property panel
  - Event routing

---

## Shader Files

### Templates
- **`shaders/quad.vert`** ✅
  - Fullscreen quad vertex shader
  - ~15 lines (template)

- **`shaders/perspective.frag`** ✅
  - **CRITICAL**: Perspective-corrected texture sampling
  - Homography transform + perspective division
  - ~35 lines

- **`shaders/procedural.frag`** ✅
  - Example animated procedural shader
  - Good for testing ShaderSource
  - ~20 lines

- **`shaders/README.md`** ✅
  - Shader documentation and usage

---

## Code Statistics

### By Component
| Component | Headers | Impls | Lines | Status |
|-----------|---------|-------|-------|--------|
| Core | 2 | 1 | ~450 | ✅ |
| Sources | 4 | 3 | ~200 | 🔨 |
| Shapes | 5 | 5 | ~450 | ✅ |
| Layers | 2 | 1 | ~80 | ✅ |
| Math | 1 | 1 | ~80 | 🔨 |
| GL Utils | 4 | 4 | ~420 | ✅ |
| UI | 3 | 3 | ~300 | ✅ |
| Shaders | - | 3 | ~70 | ✅ |
| Tests | 0 | 0 | - | 📋 |

### Totals
- **Headers**: 21 files
- **Implementations**: 18 files
- **Shaders**: 3 files
- **Documentation**: 7 files
- **Config**: 1 file (CMakeLists.txt)
- **Total LOC**: ~2,500 (code) + ~1,500 (documentation)

---

## Implementation Status Summary

### ✅ Complete (Ready to use)
- Type system and utilities
- GLFW/OpenGL initialization
- ImGui integration
- All shape implementations
- Layer management
- Shader program management
- Texture management
- Mesh management
- Framebuffer management
- UI structure and layout

### 🔨 Needs Implementation (Placeholders)
- Homography computation (priority: HIGH)
- Shader compilation (priority: MEDIUM)
- Shader source rendering (priority: MEDIUM)
- PipeWire backend (priority: MEDIUM, optional)
- Video file backend (priority: MEDIUM, optional)
- Corner dragging interaction (priority: MEDIUM)
- Perspective shader binding (priority: MEDIUM)

### 📋 Deferred (Design only)
- Multi-output support
- Undo/redo system
- Project file I/O
- GPU batch processing
- Network streaming

---

## Directory Structure

```
CrazyMapper/
├── include/
│   ├── core/
│   │   ├── types.hpp
│   │   └── application.hpp
│   ├── sources/
│   │   ├── source.hpp
│   │   ├── pipewire_source.hpp
│   │   ├── video_file_source.hpp
│   │   └── shader_source.hpp
│   ├── layers/
│   │   ├── shape.hpp
│   │   ├── rectangle_shape.hpp
│   │   ├── ellipse_shape.hpp
│   │   ├── rounded_rectangle_shape.hpp
│   │   ├── polygon_shape.hpp
│   │   └── layer.hpp
│   ├── math/
│   │   └── homography.hpp
│   ├── gl/
│   │   ├── shader_program.hpp
│   │   ├── texture.hpp
│   │   ├── mesh.hpp
│   │   └── framebuffer.hpp
│   └── ui/
│       ├── input_space_view.hpp
│       ├── output_space_view.hpp
│       └── ui_manager.hpp
├── src/
│   ├── core/
│   │   └── application.cpp
│   ├── sources/
│   │   ├── pipewire_source.cpp
│   │   ├── video_file_source.cpp
│   │   └── shader_source.cpp
│   ├── layers/
│   │   ├── rectangle_shape.cpp
│   │   ├── ellipse_shape.cpp
│   │   ├── rounded_rectangle_shape.cpp
│   │   ├── polygon_shape.cpp
│   │   └── layer.cpp
│   ├── math/
│   │   └── homography.cpp
│   ├── gl/
│   │   ├── shader_program.cpp
│   │   ├── texture.cpp
│   │   ├── mesh.cpp
│   │   └── framebuffer.cpp
│   ├── ui/
│   │   ├── input_space_view.cpp
│   │   ├── output_space_view.cpp
│   │   └── ui_manager.cpp
│   └── main.cpp
├── shaders/
│   ├── quad.vert
│   ├── perspective.frag
│   ├── procedural.frag
│   └── README.md
├── CMakeLists.txt
├── QUICKSTART.md
├── README.md
├── ARCHITECTURE.md
├── BUILD.md
├── IMPLEMENTATION_CHECKLIST.md
├── DELIVERY_SUMMARY.md
├── FILE_INDEX.md (this file)
└── .gitignore (recommended)
```

---

## How to Navigate This Project

### If You Want to...

**...understand what this project does**
→ Start with `QUICKSTART.md` (5 min)

**...build and run it**
→ Follow `BUILD.md` for your OS (5-10 min)

**...understand the architecture**
→ Read `README.md` then `ARCHITECTURE.md` (30 min)

**...implement the next feature**
→ Check `IMPLEMENTATION_CHECKLIST.md` (5 min)

**...understand the homography math**
→ Read `ARCHITECTURE.md` section "Four-Point Homography" (20 min)

**...add a new source type**
→ See `README.md` section "API Reference" (15 min)

**...add a new shape**
→ Look at `include/layers/rectangle_shape.hpp` and copy the pattern (30 min)

**...modify UI**
→ Edit `src/ui/` files using ImGui documentation (varies)

**...write shaders**
→ Read `shaders/README.md` and use `shaders/perspective.frag` as template (varies)

---

## Key Files to Understand First

### Essential (Read in This Order)

1. **`include/core/types.hpp`** - Understand the type system
2. **`include/layers/layer.hpp`** - See how source + shape interact
3. **`include/math/homography.hpp`** - Understand the interface (math in ARCHITECTURE.md)
4. **`include/gl/shader_program.hpp`** - How shaders are managed
5. **`src/core/application.cpp`** - Main application flow
6. **`include/ui/ui_manager.hpp`** - UI structure

### Next Important

7. **`include/sources/source.hpp`** - Source interface
8. **`include/layers/shape.hpp`** - Shape interface
9. **`shaders/perspective.frag`** - The key shader

---

## Estimated Implementation Time

| Task | Time | Difficulty | Blocker |
|------|------|-----------|---------|
| Build project | 10 min | Easy | No |
| Understand architecture | 30 min | Medium | No |
| Homography math | 2-3 hrs | Hard | YES |
| Shader compilation | 1-2 hrs | Medium | For ShaderSource |
| Corner dragging | 1-2 hrs | Medium | For usability |
| Video playback | 3-4 hrs | Medium | Optional |
| PipeWire capture | 3-4 hrs | Medium | Optional |
| Full integration | 1-2 hrs | Medium | After above |

---

## Quality Assurance Checklist

- [x] Code compiles without errors
- [x] No unresolved includes
- [x] Consistent naming conventions
- [x] Proper resource cleanup (destructors)
- [x] Error checking in critical functions
- [x] Clear separation of concerns
- [x] Documentation for all major classes
- [x] TODO comments for deferred work
- [ ] Unit tests (deferred)
- [ ] Integration tests (deferred)

---

## Next Steps

1. **Read** `QUICKSTART.md` (5 minutes)
2. **Build** following `BUILD.md` (10 minutes)
3. **Explore** the code structure (15 minutes)
4. **Identify** what you want to implement first
5. **Review** `IMPLEMENTATION_CHECKLIST.md` for guidance
6. **Start coding!** 🚀

---

**Status**: ✅ Complete architecture, 🔨 Placeholder implementations ready for development

**Generated**: April 9, 2026  
**For**: 2D Projection Mapping Tool with Corner-Pinning  
**Technology**: C++17, GLFW, OpenGL 3.3+, Dear ImGui  

