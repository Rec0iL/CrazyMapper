# CrazyMapper Architecture & Homography Math

## Overview

This document details the mathematical foundation and architecture of the CrazyMapper projection mapping system.

## Homography Transform

### Mathematical Basis

A **homography** is a projective transformation represented by a 3×3 matrix that maps points from one plane to another:

$$p' = H \cdot p$$

where $p$ and $p'$ are 2D homogeneous coordinates (augmented with 1 for the z-component), and $H$ is the homography matrix.

### Homogeneous Coordinates

A 2D point $(x, y)$ in Cartesian coordinates is represented as $(x, y, 1)$ in homogeneous coordinates:

$$\begin{pmatrix} x' \\ y' \\ w' \end{pmatrix} = \begin{pmatrix} h_{11} & h_{12} & h_{13} \\ h_{21} & h_{22} & h_{23} \\ h_{31} & h_{32} & h_{33} \end{pmatrix} \begin{pmatrix} x \\ y \\ 1 \end{pmatrix}$$

After transformation, we recover Cartesian coordinates via **perspective division**:

$$x_{final} = \frac{x'}{w'}, \quad y_{final} = \frac{y'}{w'}$$

### Four-Point Homography (DLT Algorithm)

Given four source corners $\{s_0, s_1, s_2, s_3\}$ and four destination corners $\{d_0, d_1, d_2, d_3\}$, we solve for the 8 unknowns in $H$ (the bottom-right element is normalized to 1).

The Direct Linear Transform (DLT) formulation creates an 8×9 system $A \mathbf{h} = 0$, where $\mathbf{h}$ is the vectorized homography matrix.

For each correspondence pair $(s_i, d_i)$, we have two equations:

$$-s_i \cdot h_1 + d_i.x \cdot s_i \cdot h_3 = 0$$
$$-s_i \cdot h_2 + d_i.y \cdot s_i \cdot h_3 = 0$$

where $h_1, h_2, h_3$ are the rows of $H$.

The solution is found via **SVD decomposition**—the right singular vector corresponding to the smallest singular value gives the homography matrix.

### Implementation Notes

The `math::Homography::compute()` function:

1. Constructs the $A$ matrix from the four point correspondences
2. Solves via SVD: $A = U \Sigma V^T$
3. Takes the null vector (last column of $V$) as the solution
4. Normalizes so that $\det(H) \approx 1$

**Important**: For production use, implement SVD via Eigen or LAPACK. The current boilerplate provides placeholder structure.

## Corner-Pinning Workflow

### Input Space
- User has a **source texture** (from PipeWire, video file, or shader)
- User defines a **shape** (rectangle, ellipse, polygon) within the texture
- The shape specifies *which part of the source is captured*
- Position and size are adjusted interactively

### Output Space
- The captured region is displayed in an **output viewport**
- Four **corner handles** represent the corners of the shape
- User drags these corners to apply **perspective distortion**
- The homography matrix is recomputed when corners move

### Homography Computation
```
Input corners:  [TL, TR, BR, BL]  (from shape in input space)
Output corners: [TL', TR', BR', BL']  (dragged positions in output space)

H = Homography::compute(inputCorners, outputCorners)
H_inv = inverse(H)

For each output pixel:
  - Apply H_inv to map back to input texture coordinates
  - Sample texture at computed coordinates
  - Blend/render the result
```

## 3D vs 2D Considerations

### Why No 3D Mesh?
The corner-pinning approach works entirely in 2D:

- **2D Homography**: Applies a planar projective transform
- **No Z-axis scaling**: Assumes the projection plane is perpendicular to view direction
- **GPU-efficient**: Single 3×3 matrix multiply + perspective division in fragment shader

### When Would You Need 3D?
If you require:
- Tilt/rotation along Z-axis at different depths
- 3D extrusion of the layer
- Mesh warping (vertex shader deformation)

Then use 3D transforms with mesh-based rendering instead.

## Perspective Correction Formula

In the fragment shader, for each output pixel at $(x_{out}, y_{out})$:

```glsl
vec3 p = H_inv * vec3(x_out, y_out, 1.0);
vec2 texCoord = p.xy / p.z;  // Perspective division
vec4 color = texture(sourceTexture, texCoord);
```

The key insight is that **perspective division** automatically corrects for foreshortening—pixels farther away appear smaller, creating the illusion of spatial depth from a planar transformation.

## Numerical Stability

### Considerations
- **Singular matrices**: Avoid degenerate corner configurations (e.g., three corners collinear)
- **Determinant**: Keep $|\det(H)| \approx 1$ for numerical stability
- **Condition number**: Poorly scaled point correspondences lead to ill-conditioned $A$ matrices

### Mitigation Strategies
- Normalize point coordinates to $[-1, 1]$ range before solving
- Check for near-singular configurations before computing
- Use double precision for intermediate calculations
- Validate $\det(H)$ and reject extreme values

## Implementation Strategy

### Phase 1: Core Boilerplate (Current)
- ✅ Architecture and headers
- ✅ Basic shape classes
- ✅ Layer linking
- ✅ UI framework

### Phase 2: Math Implementation
1. Integrate Eigen or Armadillo for SVD
2. Implement `Homography::compute()` with DLT + SVD
3. Test with known point correspondences
4. Validate perspective division

### Phase 3: Rendering Pipeline
1. Write fragment shader with homography uniform
2. Implement fullscreen quad rendering
3. Set up offscreen framebuffer for per-layer rendering
4. Composite layers with opacity/blending

### Phase 4: Backend Integration
1. PipeWire stream capture (pw_stream callbacks, buffer upload)
2. GStreamer video decoding (playback loop, frame extraction)
3. Shader source rendering (compute shader or fragment pass)

### Phase 5: Polish
1. Performance optimization (GPU uploads, caching)
2. Save/load corner configurations
3. Multi-layer compositing
4. Full-screen output rendering

## Code Integration Points

### Math Layer
```cpp
// src/math/homography.cpp
// - Implement DLT with SVD
// - Handle edge cases (singular matrices, numerical precision)
```

### Rendering Layer
```cpp
// Vertex shader: src/shaders/perspective.vert
// Fragment shader: src/shaders/perspective.frag
// - Set H_inv uniform
// - Apply perspective division
// - Sample and composite
```

### Layer Update
```cpp
// src/layers/layer.cpp::updateHomography()
void Layer::updateHomography() {
    homography_ = math::Homography::compute(
        shape_->getCorners(),
        outputCorners_
    );
    inverseHomography_ = math::Homography::invert(homography_);
}
```

## References

- Hartley, R., & Zisserman, A. (2003). *Multiple View Geometry in Computer Vision*. Cambridge University Press.
- Zhang, Z. (1998). "Flexible camera calibration by viewing a plane from unknown orientations." ICCV.
- OpenGL Perspective Divide documentation: https://www.khronos.org/opengl/wiki/Vertex_Post_Processing

## Testing Recommendations

1. **Unit tests** for homography computation with known correspondences
2. **Integration tests** for layer corner-pinning workflow
3. **Visual regression** tests comparing rendered output
4. **Performance benchmarks** for different layer counts and resolutions

