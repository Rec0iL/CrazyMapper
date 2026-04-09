cmake_minimum_required(VERSION 3.16)
project(CrazyMapper)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# Dependencies
# ============================================================================

# Find packages
find_package(OpenGL REQUIRED)

# Fetch external dependencies using CMake's FetchContent
include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
FetchContent_MakeAvailable(glfw)

# GLAD
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v0.1.36
)
FetchContent_MakeAvailable(glad)

# GLM (header-only)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.89.9
)
FetchContent_MakeAvailable(imgui)

# stb (single-file image loading library, header-only, no CMake build step)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# ============================================================================
# Source Files
# ============================================================================

set(SOURCES
    # Math
    src/math/homography.cpp

    # Sources
    src/sources/pipewire_source.cpp
    src/sources/video_file_source.cpp
    src/sources/shader_source.cpp
    src/sources/color_pattern_source.cpp
    src/sources/image_file_source.cpp
    src/sources/pipewire_node_enumerator.cpp
    src/sources/portal_screencast.cpp

    # Vendor (single-file library implementations)
    src/vendor/stb_image_impl.cpp

    # Layers
    src/layers/rectangle_shape.cpp
    src/layers/ellipse_shape.cpp
    src/layers/rounded_rectangle_shape.cpp
    src/layers/polygon_shape.cpp
    src/layers/layer.cpp

    # OpenGL utilities
    src/gl/shader_program.cpp
    src/gl/texture.cpp
    src/gl/mesh.cpp
    src/gl/framebuffer.cpp

    # UI
    src/ui/input_space_view.cpp
    src/ui/output_space_view.cpp
    src/ui/ui_manager.cpp
    src/ui/file_dialog.cpp

    # Core
    src/core/projection_window.cpp
    src/core/application.cpp

    # Main
    src/main.cpp

    # ImGui implementations
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# ============================================================================
# Executable
# ============================================================================

add_executable(CrazyMapper ${SOURCES})

# ============================================================================
# Include Directories
# ============================================================================

target_include_directories(CrazyMapper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glad_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${stb_SOURCE_DIR}
)

# ============================================================================
# Link Libraries
# ============================================================================

target_link_libraries(CrazyMapper PRIVATE
    glfw
    OpenGL::GL
    glad
    glm
)

# ============================================================================
# Compiler Flags
# ============================================================================

if (MSVC)
    target_compile_options(CrazyMapper PRIVATE /W4)
else()
    target_compile_options(CrazyMapper PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ============================================================================
# Optional: PipeWire and GStreamer support
# ============================================================================

# PipeWire support (screen/camera capture)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE IMPORTED_TARGET libpipewire-0.3)
if(PIPEWIRE_FOUND)
    message(STATUS "PipeWire ${PIPEWIRE_VERSION} found — enabling PipeWire source")
    target_link_libraries(CrazyMapper PRIVATE PkgConfig::PIPEWIRE)
    target_compile_definitions(CrazyMapper PRIVATE HAVE_PIPEWIRE)
else()
    message(STATUS "PipeWire not found — PipeWire source disabled")
endif()

# XDG Desktop Portal ScreenCast (shows native OS screen picker)
pkg_check_modules(GIO_UNIX IMPORTED_TARGET gio-unix-2.0)
if(GIO_UNIX_FOUND)
    message(STATUS "gio-unix ${GIO_UNIX_VERSION} found — enabling portal screen capture")
    target_link_libraries(CrazyMapper PRIVATE PkgConfig::GIO_UNIX)
    target_compile_definitions(CrazyMapper PRIVATE HAVE_PORTAL)
else()
    message(STATUS "gio-unix not found — portal screen capture disabled")
endif()

# GStreamer support (video file playback)
pkg_check_modules(GSTREAMER IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0)
if(GSTREAMER_FOUND)
    message(STATUS "GStreamer ${GSTREAMER_VERSION} found — enabling video file source")
    target_link_libraries(CrazyMapper PRIVATE PkgConfig::GSTREAMER)
    target_compile_definitions(CrazyMapper PRIVATE HAVE_GSTREAMER)
else()
    message(STATUS "GStreamer not found — video file source disabled")
endif()


