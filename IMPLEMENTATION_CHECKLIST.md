# Implementation Checklist

Use this guide to track which components have been implemented and which are placeholders.

## Core System

- [x] **Type System** (`include/core/types.hpp`)
  - GLM type aliases, smart pointers, standard types

- [x] **Application Framework** (`src/core/application.cpp`)
  - GLFW window creation, OpenGL 3.3 initialization
  - ImGui setup and main loop
  - Three built-in color pattern sources created at startup
  - Source-to-layer assignment, layer reorder, new source creation handled in update loop

- [x] **Layer System** (`src/layers/layer.cpp`)
  - Source linking with live `setSource()` swapping
  - Input corners (source UV) and output corners (canvas UV), both freely draggable
  - Shape replacement at runtime via `setShape()`
  - Homography recomputed on every corner change

## Math & Transforms

- [x] **Homography Computation** (`src/math/homography.cpp`)
  - DLT algorithm, pure C++/GLM, no external SVD
  - Recomputed live on every corner change

## Source Backends

### Color Pattern Source ✅
- [x] Solid Color, Checkerboard, Gradient — GPU shader + FBO each frame
- [x] Three default instances created on startup

### Image File Source ✅ NEW
- [x] Loads PNG/JPG/BMP/etc. via stb_image
- [x] Uploads to GL texture with mip-maps
- [x] Static (no per-frame update needed)

### Shader Source ✅
- [x] GLSL compile + link with error reporting
- [x] FBO + fullscreen quad, `iTime` / `iResolution` uniforms

### PipeWire Source (`src/sources/pipewire_source.cpp`)
- [ ] **Backend** (❌ PLACEHOLDER — PipeWire deps not linked)

### Video File Source (`src/sources/video_file_source.cpp`)
- [ ] **Backend** (❌ PLACEHOLDER — FFmpeg/GStreamer not linked)

## Shapes

- [x] **Rectangle**
- [x] **Ellipse**
- [x] **Rounded Rectangle** — uniform or per-corner radii (TL/TR/BR/BL)
- [x] **N-Polygon** — configurable N (3..24)
- [x] **Runtime shape change** — combo in Properties panel, GPU mask updates instantly

## OpenGL Utilities

- [x] Shader Program, Texture, Mesh, Framebuffer

## UI System

- [x] **Input Space View** — grid, free-form 4-corner dragging per layer
- [x] **Output Space View**
  - Grid, canvas with fixed aspect ratio (user-configurable W:H)
  - All layers composited via per-pixel homography FBO shader
  - Corner handles only on selected layer; other layers visible
  - Drag corner handles beyond canvas border supported
- [x] **UI Manager**
  - 3-column layout (locked)
  - Layer panel: select, reorder (^/v), `+ New`
  - Sources panel: list, `[active]` indicator, Assign to Layer, **Add Source** (Solid/Checker/Gradient/Image/Shader)
  - Properties panel: visibility, opacity, reset corners, Shape section (type combo + per-type params)
  - Canvas panel: W:H aspect ratio, projection window open/close
  - Menu: File → New Layer, Projection → Open/Close Projection Window

## Projection Window ✅

- [x] Second GLFW window with shared GL context
- [x] Same per-pixel homography composite as Output Space View
- [x] F key → fullscreen on correct monitor, ESC → exit fullscreen
- [x] `GLFW_AUTO_ICONIFY=FALSE` — no minimize when focus moves to editor

## Rendering Pipeline ✅

- [x] Per-pixel perspective-correct homography (not bilinear two-triangle)
- [x] Convex-quad mask via cross products in fragment shader
- [x] Shape mask in source UV space (ellipse, rounded-rect SDF, N-polygon apothem)
- [x] All layers composited in order with alpha blending

## Build

```bash
cd build && cmake --build . --target CrazyMapper
```

## Pending / Optional

| Task | Priority |
|---|---|
| Video file source (FFmpeg/GStreamer) | Low |
| PipeWire screen capture | Low |
| Save/load project state (JSON) | Medium |
| Edge blending for multi-projector | Low |

---
**Last Updated**: April 9, 2026

