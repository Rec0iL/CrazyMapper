# CrazyMapper

A projection mapping tool for Linux. Point a projector at any surface, load your content, and warp it to fit using corner pins, shapes, and real-time shaders.

---

## What it does

- **Sources** — load images, videos, live screen captures (PipeWire), or real-time GLSL shaders
- **Layers** — each layer shows one source through a shape (rectangle, ellipse, triangle, rounded rectangle, n-polygon)
- **Corner pinning** — drag the corner handles in the Output Space to warp the content to fit your surface
- **Edge feathering** — blend layer edges softly into the background instead of hard cuts
- **Multiple layers** — stack as many layers as you need, reorder and toggle visibility independently
- **Projection window** — opens a second window you send to your projector, always showing the final mapped output
- **Save / Load** — the full layout (all layers, corners, sources) can be saved and reloaded

---

## Installation

### Dependencies

GLFW, GLAD, GLM, and Dear ImGui are downloaded automatically by CMake. You only need to install the system libraries below.

**Ubuntu / Debian**
```bash
sudo apt install \
  cmake build-essential \
  libgl1-mesa-dev libglu1-mesa-dev \
  libpipewire-0.3-dev \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
```

**Fedora**
```bash
sudo dnf install \
  cmake gcc-c++ \
  mesa-libGL-devel mesa-libGLU-devel \
  pipewire-devel \
  gstreamer1-devel gstreamer1-plugins-base-devel
```

**Arch Linux**
```bash
sudo pacman -S \
  cmake base-devel \
  mesa \
  pipewire \
  gstreamer gst-plugins-base
```

PipeWire and GStreamer are optional — the app builds without them, but screen capture and video playback won't be available.

### Build

```bash
git clone https://github.com/Rec0iL/CrazyMapper
cd CrazyMapper
mkdir build && cd build
cmake ..
make -j$(nproc)
./CrazyMapper
```

---

## Usage

### 1. Open the projection window

Click **Open Projection Window** in the Canvas panel. Move that window to your projector display and press **F** to go fullscreen. Keep it there — you'll be mapping to the surface you see while you work in the main window.

### 3. Add a source

Open **Sources → Add Source**, pick a type, and click **Add**:

| Type | What it is |
|------|-----------|
| Pattern: Calibration Grid | Black + white grid with corner marks — start here to align your projector |
| Pattern: Solid Color | Flat color fill |
| Pattern: Checkerboard | Classic checker, useful for testing |
| Pattern: Gradient | Animated diagonal gradient |
| Image File | PNG, JPG, BMP, and more |
| Video File | MP4, MKV, MOV, WebM, and more |
| Shader File | A GLSL fragment shader (`.glsl`, `.frag`, `.shadertoy`) |
| PipeWire Stream | Live screen/window capture via the system picker |

### 4. Create a layer

Click **+ New** in the Layers panel. A new layer appears using the first available source.

### 5. Assign a source to the layer

Select the source in the Sources list, select the layer, then click **Assign to Layer**.

### 6. Adjust the input region

In the **Input Space** panel, drag the corner handles to choose which part of the source is captured.

### 7. Pin the output

In the **Output Space** panel, drag the corner handles to warp the layer onto your surface. The projection window updates live as you drag.

---

## Controls

### Mouse

| Action | What it does |
|--------|-------------|
| Drag a corner handle | Move that corner — works in both Input and Output Space |
| Hover over a corner (don't click) | Activates fine control with arrow keys |

### Keyboard

| Key | What it does |
|-----|-------------|
| **Arrow keys** (while hovering a corner) | Nudge the corner one pixel at a time |
| **F** | Toggle fullscreen on the projection window |
| **Esc** | Exit |

### Fine control tip

Hover your mouse over a corner handle without clicking. The handle highlights and a tooltip shows its exact position. Now use the arrow keys to nudge it pixel by pixel — useful for precise alignment or when corners are outside the visible canvas area.

---

## Shapes

Each layer has a shape that controls which area is projected:

| Shape | Description |
|-------|-------------|
| Rectangle | Simple quad |
| Rounded Rectangle | Quad with adjustable corner radii |
| Ellipse | Oval or circle |
| N-Polygon | Any regular polygon (triangle → 24-sided) |
| Triangle | Exactly three corner points in both input and output |

Change a layer's shape in the **Properties → Shape** section.

---

## Edge feathering

In **Properties**, the **Feather** slider blends the layer edges softly into the background. Useful for:
- Hiding hard edges on curved surfaces
- Making overlapping layers blend together
- Softening rounded shape edges

---

## Shader sources

CrazyMapper can run GLSL fragment shaders as live animated sources.

### Your own shaders (`.glsl` / `.frag`)

Must have this structure:

```glsl
#version 330 core
out vec4 FragColor;
uniform vec2  iResolution;
uniform float iTime;

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution;
    FragColor = vec4(uv, 0.5 + 0.5 * sin(iTime), 1.0);
}
```

### Shadertoy shaders (`.shadertoy`)

Save the shader code from the **Image** tab into a `.shadertoy` file — no wrapper needed, CrazyMapper adds it automatically.

**What works:**
- Single-pass shaders (Image tab only)
- `iTime`, `iResolution`, `iMouse`, `iFrame`, `iTimeDelta`

**What doesn't work:**
- Multi-pass shaders (Buffer A/B/C/D)
- `iChannel` textures (audio, video, cubemaps) — if the channel is just a noise texture you can replace the `texture(iChannel0, ...)` calls with procedural noise

Example shaders are in `shaders/examples/`.

---

## Layout

```
┌─────────────────────┬──────────────┬──────────────┐
│   Input Space       │   Sources    │   Layers     │
│                     │              │              │
│  (source texture    ├──────────────│──────────────│
│   with shape        │   Canvas     │  Properties  │
│   overlay)          │              │              │
├─────────────────────│              │              │
│   Output Space      │              │              │
│                     │              │              │
│  (warped preview    │              │              │
│   + corner pins)    │              │              │
└─────────────────────┴──────────────┴──────────────┘
```

Panels can be collapsed — the adjacent panel expands to fill the space.

---

## Save and load

Use **File → Save Layout** / **File → Load Layout** to save and restore the entire scene (layers, sources, corner positions, shapes, feather values, canvas aspect ratio).


