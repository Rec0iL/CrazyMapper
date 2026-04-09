# Quick Start Guide

**Time to first build**: ~5 minutes  
**Time to understand architecture**: ~30 minutes  
**Time to first projection**: ~1-2 hours (after implementing homography)

---

## The 60-Second Overview

CrazyMapper is a 2D projection mapping tool with:
- **Sources**: Where media comes from (PipeWire, video, shader)
- **Layers**: Combine source + shape + corner-pinning
- **UI**: Split pane showing input (source) and output (warped) spaces

The magic happens via **homography transforms**: When you drag a corner, the math warps the texture perspective.

---

## Build & Run (Linux)

```bash
sudo apt-get install -y build-essential cmake libgl1-mesa-dev \
  libxrandr-dev libxinerama-dev libxi-dev libxcursor-dev

cd CrazyMapper
mkdir build && cd build
cmake ..
make -j$(nproc)
./CrazyMapper
```

On **macOS**: Same but `brew install cmake` instead of apt-get.

On **Windows**: Use Visual Studio 2019+ with CMake Tools extension.

---

## File Map: What Does What?

### Architecture Files (Read First)
```
README.md                       ← Start here for overview
ARCHITECTURE.md                 ← Homography math explained  
IMPLEMENTATION_CHECKLIST.md     ← What's done, what's not
```

### Core Components
```
include/core/application.hpp            Main app class (GLFW, OpenGL, ImGui)
include/sources/source.hpp              Abstract source interface
include/layers/layer.hpp                Combines source + shape + homography
include/math/homography.hpp             THE CRITICAL MATH (needs implementation)
include/gl/shader_program.hpp           GPU shader management
include/ui/ui_manager.hpp               UI coordinator
```

### What You Need to Add
```
src/math/homography.cpp                 ← PRIORITY: Implement SVD-based DLT
src/sources/shader_source.cpp           ← Shader compilation + FBO rendering
src/ui/output_space_view.cpp            ← Wire up corner dragging + rendering
```

---

## Key Concepts

### 1. Sources (Where data comes from)
```cpp
auto source = std::make_shared<sources::ShaderSource>(fragmentCode, 1920, 1080);
source->initialize();
unsigned int textureHandle = source->getTextureHandle();  // GPU texture ID
```

Available sources:
- `ShaderSource`: Procedural GLSL (easiest to implement first)
- `VideoFileSource`: Play back video files (GStreamer/FFmpeg backend)
- `PipeWireSource`: Capture streams (PipeWire backend)

### 2. Shapes (What geometry in input space?)
```cpp
auto shape = std::make_unique<layers::RectangleShape>(
    Vec2(100, 100),   // position
    Vec2(400, 300)    // size
);
```

Available shapes (already implemented):
- Rectangle
- Ellipse (circular + ovals)
- RoundedRectangle
- Polygon (n-sided with custom vertices)

### 3. Layers (Combining source + shape)
```cpp
auto layer = app.createLayer(source, std::move(shape));

// In output space, drag corners to distort:
layer->setOutputCorner(0, Vec2(150, 120));  // Top-left
layer->setOutputCorner(1, Vec2(480, 110));  // Top-right
layer->setOutputCorner(2, Vec2(470, 350));  // Bottom-right
layer->setOutputCorner(3, Vec2(130, 360));  // Bottom-left

// Automatic: layer->getHomography() computes the transform
```

### 4. Homography (The Math)
```
Input corners:  (0,0), (500,0), (500,500), (0,500)
Output corners: (150,120), (480,110), (470,350), (130,360)
                           ↓ (dragged by user)
Homography H = compute(input, output)

For each output pixel:
  input_texCoord = H_inv * output_pixel
  color = texture_sample(input_texCoord)
```

This is **all perspective math is**.

---

## The 5-Minute Implementation Plan

### Goal: Get perspective warping working

**Step 1: Implement Homography Math** (60 min)
```cpp
// src/math/homography.cpp
Mat3 Homography::compute(const std::array<Vec2, 4>& src,
                         const std::array<Vec2, 4>& dst) {
    // Use SVD to solve: A * h = 0
    // Return 3x3 homography matrix
    // See ARCHITECTURE.md for detailed math
}
```

**Step 2: Wire Shader Uniform** (30 min)
```cpp
// In output_space_view.cpp render function:
gl::ShaderProgram perspShader;
perspShader.compile(vertexCode, fragmentCode);
perspShader.setUniformMat3("homographyInverse", 
                          layer->getInverseHomography());
```

**Step 3: Render Distorted Quad** (30 min)
```cpp
// Render fullscreen quad with perspective-corrected sampling
// The shader does: texCoord = (H_inv * fragCoord).xy / .z
```

**Result**: Dragging corners warps the layer content.

---

## Source Files to Modify (Priority Order)

### CRITICAL
```
1. src/math/homography.cpp          Implement SVD-based 4-point homography
2. src/sources/shader_source.cpp    Shader compilation + framebuffer
3. src/ui/output_space_view.cpp     Bind homography uniform + render
```

### IMPORTANT
```
4. src/ui/ui_manager.cpp            Wire corner drag events to layer updates
5. src/sources/video_file_source.cpp  GStreamer/FFmpeg backend (optional)
6. src/sources/pipewire_source.cpp    PipeWire backend (optional)
```

---

## Testing Your Implementation

### Minimal Test Case
```cpp
// Test homography with identity transform
std::array<Vec2, 4> corners;
corners[0] = Vec2(0, 0);
corners[1] = Vec2(100, 0);
corners[2] = Vec2(100, 100);
corners[3] = Vec2(0, 100);

Mat3 H = math::Homography::compute(corners, corners);
// Should be approximately identity matrix
```

### Full Integration Test
```cpp
// Create layer and drag corners
auto layer = app.createLayer(source, shape);
layer->setOutputCorner(0, Vec2(10, 10));  // Drag corner
layer->setOutputCorner(1, Vec2(90, -5));

// Homography should update automatically
Mat3 H = layer->getHomography();
// Should transform texture correctly to distorted quad
```

---

## Common Mistakes

❌ **Wrong**: Trying to use homography before it's computed
✅ **Right**: Call `layer->setOutputCorner()` which triggers computation

❌ **Wrong**: Not applying perspective division in shader
✅ **Right**: `texCoord = (H * coord).xy / .z`

❌ **Wrong**: Using forward homography in shader (should use inverse)
✅ **Right**: Map output → input for texture sampling

❌ **Wrong**: Singular matrix from degenerate corners
✅ **Right**: Validate corner configuration before computing

---

## Quick Reference: The Three Core Files

### 1. Application Entry Point
**File**: `src/core/application.cpp`
- **Handles**: GLFW window, ImGui setup, main loop, input callbacks
- **What works**: ✅ Everything
- **What's missing**: Just need to implement render()`s perspective rendering

### 2. Layer Management  
**File**: `include/layers/layer.hpp` & `src/layers/layer.cpp`
- **Handles**: Linking source + shape, homography state
- **What works**: ✅ Data structures
- **What's missing**: Homography computation (line 114 in layer.cpp)

### 3. The Math (Most Important!)
**File**: `include/math/homography.hpp` & `src/math/homography.cpp`
- **Handles**: Computing perspective transform from 4 corners
- **What works**: ❌ PLACEHOLDER
- **What's missing**: This is your first task!

---

## Shader Integration

The shader files are in `shaders/`:
- `perspective.frag`: **THE KEY FILE** - Does perspective-corrected sampling
- `quad.vert`: Simple fullscreen quad vertex shader
- `procedural.frag`: Example animated content

The critical shader line:
```glsl
vec3 homogeneous = homographyInverse * vec3(outputCoord, 1.0);
vec2 inputTexCoord = homogeneous.xy / homogeneous.z;  // Perspective division
```

---

## I'm Stuck, Where Do I Look?

| Problem | File |
|---------|------|
| "How do I build?" | `BUILD.md` |
| "What's the homography math?" | `ARCHITECTURE.md` |
| "What still needs implementing?" | `IMPLEMENTATION_CHECKLIST.md` |
| "How do I add a custom source?" | `README.md` → API Reference |
| "Where's the shader perspective code?" | `shaders/perspective.frag` + `shaders/README.md` |
| "What are the control inputs?" | `README.md` → Controls section |
| "How do I save/load corners?" | `include/ui/output_space_view.hpp` → saveCornerPoints() |

---

## Next Steps

1. ✅ **Read this file** (you're done!)
2. 📖 **Skim `ARCHITECTURE.md`** for homography understanding (15 min)
3. 🏗️  **Build the project** (`BUILD.md` - 5 min)
4. 💻 **Implement homography computation** (2-3 hours)
5. 🎨 **Wire shader rendering** (1-2 hours)
6. 🖱️  **Add corner dragging** (1-2 hours)

---

## Success Checklist

- [ ] Project builds without errors
- [ ] Running `CrazyMapper` opens a window
- [ ] ImGui windows are visible
- [ ] Can see input/output space panes
- [ ] Homography math implemented and tested
- [ ] Dragging corners warps the layer
- [ ] Perspective looks correct (farther = smaller)

---

**You've got this!** 💪

The architecture is solid, implementations are straightforward, and the hardest part (math) is well-documented.

Start with reading `ARCHITECTURE.md` for 15 minutes, then jump into implementing the homography DLT algorithm.

---

*Generated: April 9, 2026*  
*Project: CrazyMapper - 2D Projection Mapping Tool*

