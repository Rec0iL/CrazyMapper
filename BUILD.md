# Build & Compilation Guide

## Quick Start

### Linux (Ubuntu/Debian)

```bash
# Install system dependencies
sudo apt-get install -y \
    build-essential \
    cmake \
    libgl1-mesa-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxi-dev \
    libxcursor-dev

# Build the project
cd CrazyMapper
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run
./CrazyMapper
```

### macOS

```bash
# Install via Homebrew
brew install cmake

# Build
cd CrazyMapper
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)

./CrazyMapper
```

### Windows (Visual Studio)

```bash
# Ensure you have Visual Studio 2019 or later installed

cd CrazyMapper
mkdir build
cd build
cmake -G "Visual Studio 16 2019" ..
cmake --build . --config Release
Release\CrazyMapper.exe
```

## Dependency Management

### Automatic (FetchContent)
The `CMakeLists.txt` automatically downloads:
- **GLFW**: Window management
- **GLAD**: OpenGL loader
- **GLM**: Math library
- **Dear ImGui**: UI framework

### Manual Installation (Alternative)

If FetchContent fails:

```bash
# Ubuntu
sudo apt-get install libglfw3-dev libglm-dev

# macOS
brew install glfw glm

# Then update CMakeLists.txt:
# find_package(glfw3 REQUIRED)
# find_package(glm REQUIRED)
# find_package(Glad REQUIRED)
```

## Optional: PipeWire & GStreamer

### Enable in CMakeLists.txt

Uncomment the optional dependency sections:

```cmake
# Find PipeWire
find_package(PipeWire QUIET)
if(PipeWire_FOUND)
    target_link_libraries(CrazyMapper PRIVATE PipeWire::PipeWire)
    target_compile_definitions(CrazyMapper PRIVATE HAVE_PIPEWIRE)
endif()

# Find GStreamer
find_package(GStreamer 1.0 REQUIRED gstreamer gstreamer-base gstreamer-app)
target_link_libraries(CrazyMapper PRIVATE ${GSTREAMER_LIBRARIES})
target_include_directories(CrazyMapper PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_compile_definitions(CrazyMapper PRIVATE HAVE_GSTREAMER)
```

### Install System Libraries

**Ubuntu/Debian:**
```bash
sudo apt-get install -y \
    libpipewire-0.3-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev
```

**macOS:**
```bash
brew install pipewire gstreamer gst-plugins-base
```

## Troubleshooting

### CMake not found
```bash
# Ubuntu
sudo apt-get install cmake

# macOS
brew install cmake

# Download from https://cmake.org/download/
```

### GLAD compilation errors
Ensure OpenGL headers are installed:
```bash
# Ubuntu
sudo apt-get install libgl1-mesa-dev

# macOS typically includes these
```

### ImGui linking errors
Verify all ImGui source files are in the build. Check that `${imgui_SOURCE_DIR}` paths are correct in `CMakeLists.txt`.

### GLFW window creation fails
- Ensure X11/Wayland libraries are installed (Linux)
- Check graphics drivers are up-to-date
- Verify OpenGL 3.3+ support: `glxinfo | grep "OpenGL version"`

## Source Code Organization

```
CrazyMapper/
├── include/              # Header files (public API)
│   ├── core/
│   ├── sources/
│   ├── layers/
│   ├── math/
│   ├── gl/
│   └── ui/
├── src/                  # Implementation files
│   ├── core/
│   ├── sources/
│   ├── layers/
│   ├── math/
│   ├── gl/
│   ├── ui/
│   └── main.cpp
├── CMakeLists.txt        # Build configuration
├── README.md             # This file
└── ARCHITECTURE.md       # Detailed design documentation
```

## Development Workflow

### Adding New Source Type

1. Create header: `include/sources/my_source.hpp`
2. Create implementation: `src/sources/my_source.cpp`
3. Inherit from `sources::Source`
4. Implement all pure virtual methods
5. Add to `CMakeLists.txt` source list

### Adding New Shape Type

1. Create header: `include/layers/my_shape.hpp`
2. Create implementation: `src/layers/my_shape.cpp`
3. Inherit from `layers::Shape`
4. Implement `getVertices()`, `contains()`, `getCorners()`
5. Add to `CMakeLists.txt` source list

### Building in Debug Mode

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Using AddressSanitizer

```bash
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address" ..
make
./CrazyMapper
```

## Integration with IDEs

### Visual Studio Code

Create `.vscode/settings.json`:
```json
{
    "cmake.sourceDirectory": "${workspaceFolder}",
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "C_Cpp.default.configurationName": "Release"
}
```

Install extensions:
- C/C++ (Microsoft)
- CMake (twxs)
- CMake Tools (Microsoft)

### Visual Studio

```bash
cmake -G "Visual Studio 16 2019" -S . -B build
start build/CrazyMapper.sln
```

### CLion

- Open `CMakeLists.txt` as project
- CLion auto-detects CMake configuration
- Build via Ctrl+F9

## Performance Profiling

### With `perf` (Linux)

```bash
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -g" ..
make

perf record ./CrazyMapper
perf report
```

### With Valgrind

```bash
valgrind --tool=callgrind ./CrazyMapper
kcachegrind callgrind.out.*
```

## Next Steps

1. Complete the homography math implementation (Phase 2)
2. Integrate shader rendering for shader sources
3. Add PipeWire backend support
4. Implement GStreamer video playback
5. Optimize GPU rendering pipeline

See [ARCHITECTURE.md](ARCHITECTURE.md) for technical details.

