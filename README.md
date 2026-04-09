# CrazyMapper - 2D Projection Mapping Tool

A modular C++ application for 2D projection mapping with corner-pinning distortion. Built with GLFW, OpenGL, and Dear ImGui.

## Features

- **Multi-source support**: PipeWire streams, video files, and real-time shaders
- **Flexible layer system**: Rectangle, ellipse, rounded rectangle, and n-polygon shapes
- **Corner-pinning**: Drag the four corners of any layer to apply perspective correction
- **Homography transform**: Automatic perspective warping using 3x3 homography matrices
- **Split-pane UI**: 
  - Input Space: View and position layer shapes over source media
  - Output Space: Preview and corner-pin the distorted output
- **Clean architecture**: Modular design allows independent backend implementation

## Project Structure

```
include/
├── core/
│   ├── types.hpp           # Type aliases (Vec2, Mat3, Shared<>, etc.)
│   └── application.hpp     # Main application class
├── sources/
│   ├── source.hpp          # Abstract base class for all sources
│   ├── pipewire_source.hpp
│   ├── video_file_source.hpp
│   └── shader_source.hpp
├── layers/
│   ├── shape.hpp           # Abstract shape interface
│   ├── rectangle_shape.hpp
│   ├── ellipse_shape.hpp
│   ├── rounded_rectangle_shape.hpp
│   ├── polygon_shape.hpp
│   └── layer.hpp           # Layer combining source + shape
├── math/
│   └── homography.hpp      # Homography transform math
├── gl/
│   ├── shader_program.hpp  # OpenGL shader management
│   ├── texture.hpp         # Texture wrapper
│   ├── mesh.hpp            # VAO/VBO wrapper
│   └── framebuffer.hpp     # FBO wrapper
└── ui/
    ├── input_space_view.hpp  # Input source display
    ├── output_space_view.hpp # Corner-pinning viewport
    └── ui_manager.hpp        # Main UI controller

src/
├── core/        # Implementation files (mirroring include/ structure)
├── sources/
├── layers/
├── math/
├── gl/
├── ui/
└── main.cpp     # Entry point
```

## Core Architecture

### Source System
All input sources inherit from `Source` abstract base class:
- **PipeWireSource**: Capture from PipeWire daemon (audio/video)
- **VideoFileSource**: Play back video files (GStreamer/FFmpeg backend)
- **ShaderSource**: Generate content via GLSL fragment shaders

Each source provides:
- Initialization/shutdown
- Per-frame updates
- OpenGL texture handle access
- Resolution information

### Layer System
A `Layer` combines:
1. A linked `Source` (where media comes from)
2. A `Shape` (geometric definition in input space)
3. Homography transform state (for corner-pinning in output space)

Supported shapes:
- Rectangle
- Ellipse/Circle
- Rounded rectangle
- N-polygon (custom vertices)

### UI Layout
**Input Space View** (Top):
- Displays raw source texture
- Shows layer shape overlay
- User can drag/scale the shape to select captured region

**Output Space View** (Bottom):
- Displays the captured content
- Shows four corner handles
- User drags corners to apply perspective distortion

### Homography Transform
The corner-pinning workflow uses 3x3 homography matrices:
- **Forward**: Maps input texture coordinates → output screen coordinates
- **Inverse**: Maps output coordinates → input texture coordinates during sampling

See `include/math/homography.hpp` and [ARCHITECTURE.md](ARCHITECTURE.md) for detailed math.

## Building

### Prerequisites
- CMake 3.16+
- C++17 compatible compiler (GCC, Clang, MSVC)
- OpenGL 3.3+
- Basic build tools (make, ninja, or Visual Studio)

### Build Steps

```bash
cd CrazyMapper
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Optional Dependencies

For PipeWire and GStreamer support, uncomment in `CMakeLists.txt`:
```cmake
find_package(PipeWire)
find_package(GStreamer 1.0 REQUIRED gstreamer gstreamer-base gstreamer-app)
```

## Architecture Notes

### Modularity
- **Source backends are deferred**: PipeWire, GStreamer/FFmpeg implementations should be added independently
- **Clean separation**: Math, rendering, and UI are independent
- **Extensibility**: Add new shapes or sources by extending base classes

### Homography Implementation
The `math::Homography` namespace provides:
- `compute()`: DLT algorithm for 4-point corner homography
- `transform()`: Apply homography with perspective division
- `invert()`: Compute inverse transform
- `normalize()`: Normalize matrix by determinant

### Texture Rendering
Perspective-corrected rendering uses:
1. `gl::ShaderProgram`: Fragment shader with homography matrix uniform
2. `gl::Framebuffer`: Off-screen rendering for intermediate passes
3. `gl::Texture`: GPU texture management
4. `gl::Mesh`: Fullscreen quad VAO/VBO

### ImGui Integration
- Split-pane layout via ImGui windows
- Layer panel for source selection
- Property panel for opacity, visibility, corner-pin save/load
- Direct OpenGL rendering for preview

## API Reference

### Creating and Running a Layer

```cpp
// Create a shader source
auto shader = std::make_shared<sources::ShaderSource>(
    fragmentShaderCode, width, height
);
shader->initialize();

// Create a layer with a rectangle shape
auto layer = app.createLayer(
    shader,
    std::make_unique<layers::RectangleShape>(
        Vec2(100, 100),  // Position
        Vec2(400, 300)   // Size
    )
);

// Adjust output corners for corner-pinning
layer->setOutputCorner(0, Vec2(150, 120));  // Top-left
layer->setOutputCorner(1, Vec2(480, 110));  // Top-right
layer->setOutputCorner(2, Vec2(470, 350));  // Bottom-right
layer->setOutputCorner(3, Vec2(130, 360));  // Bottom-left
```

### Implementing a Custom Source

```cpp
class CustomSource : public sources::Source {
public:
    bool initialize() override {
        // Setup: create GL texture, connect backends
        return true;
    }
    
    bool update(float deltaTime) override {
        // Fetch new data and upload to textureHandle_
        return true;
    }
    
    unsigned int getTextureHandle() const override {
        return textureHandle_;
    }
    
    // ... other required methods
};
```

## TODO Items

- [ ] Implement full homography computation with SVD decomposition
- [ ] PipeWire backend (pw_stream integration, texture upload)
- [ ] GStreamer/FFmpeg video playback backend
- [ ] Fullscreen quad rendering for shader sources
- [ ] Perspective-corrected shader implementation
- [ ] GPU-accelerated homography transforms
- [ ] Save/load project files (layer configurations)
- [ ] Keyboard shortcuts for layer manipulation
- [ ] Multi-output rendering support
- [ ] Performance profiling and optimization

## Controls

- **Esc**: Exit application
- **Mouse drag in Input Space**: Move/scale layer shape
- **Mouse drag in Output Space**: Drag corner handles for distortion
- **Right-click**: Context menus for layers

## License

This boilerplate is provided as-is for educational and commercial use.

## Dependencies

- GLFW 3.3+ (windowing)
- GLAD (OpenGL loader)
- GLM (math library)
- Dear ImGui 1.89+ (UI)
- OpenGL 3.3+ (rendering)

Optional:
- PipeWire (audio/video capture)
- GStreamer 1.0 (video playback)

