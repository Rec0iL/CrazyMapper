# GLSL Shader Files

This directory contains shader templates for CrazyMapper.

## Files

### `quad.vert`
**Vertex shader for fullscreen quad rendering**

- Simple passthrough to NDC coordinates
- Handles texture coordinate interpolation
- TODO: Update matrix uniforms if using 3D transforms later

### `perspective.frag`
**Fragment shader for perspective-corrected layer rendering**

- Applies inverse homography transform
- Performs perspective division
- Samples from source texture at transformed coordinates
- **Critical for corner-pinning functionality**

**Uniforms:**
- `sourceTexture` (sampler2D): Source media texture
- `homographyInverse` (mat3): Inverse homography matrix (output → input)
- `sourceResolution` (vec2): Resolution of source texture
- `opacity` (float): Layer opacity [0, 1]

**Key Concept:**
For each output pixel, the shader:
1. Gets the pixel position in output space
2. Transforms it via inverse homography
3. Applies perspective division
4. Samples the source texture at the transformed coordinate

### `procedural.frag`
**Example procedural shader for real-time content**

- Generates animated patterns using sin/cos
- Updates from `iTime` uniform
- Perfect for testing ShaderSource class

**Uniforms:**
- `iTime` (float): Elapsed time in seconds
- `iResolution` (vec2): Output resolution

## Integration with C++ Code

### Using Perspective Shader

```cpp
// In gl::ShaderProgram::compile()
const char* perspectiveVert = R"(
    #version 330 core
    layout(location = 0) in vec2 position;
    void main() {
        gl_Position = vec4(position, 0.0, 1.0);
    }
)";

// Read perspective.frag from file
std::string perspectiveFrag = readFile("shaders/perspective.frag");

shaderProgram.compile(perspectiveVert, perspectiveFrag);
shaderProgram.setUniformMat3("homographyInverse", H_inv);
```

### Using Procedural Shader

```cpp
// In ShaderSource::initialize()
std::string proceduralFrag = readFile("shaders/procedural.frag");
compileShader(basicVert, proceduralFrag);
```

## Shader Development Tips

1. **Test step-by-step**: 
   - Start with solid color output
   - Add texture sampling
   - Add homography multiplications
   - Test perspective division independently

2. **Debug perspective division**:
   ```glsl
   // Visualize homogeneous Z (should be near 1.0 for valid transforms)
   FragColor = vec4(vec3(homogeneousCoord.z), 1.0);
   ```

3. **Verify homography correctness**:
   - Use identity homography (should show unwarped texture)
   - Use known distortions and validate visually

4. **Performance considerations**:
   - Avoid expensive operations per-fragment
   - Pre-compute matrix inversions on CPU
   - Use lower precision if targeting mobile

## Extending Shaders

### Adding Color Correction

```glsl
vec3 applyColorGrading(vec3 color) {
    // Apply LUT or color matrix
    return color;
}

// In main():
vec4 texColor = texture(sourceTexture, inputTexCoord);
texColor.rgb = applyColorGrading(texColor.rgb);
```

### Adding Blending Modes

```glsl
uniform int blendMode;  // 0=normal, 1=add, 2=multiply, etc.

vec4 blendColors(vec4 src, vec4 dst) {
    if (blendMode == 0) return src;
    else if (blendMode == 1) return src + dst;
    else if (blendMode == 2) return src * dst;
    // ...
}
```

### Adding Distortion Effects

```glsl
// Simple barrel distortion
vec2 distort(vec2 uv) {
    vec2 center = vec2(0.5);
    vec2 delta = uv - center;
    float r = length(delta);
    float factor = 1.0 + r * r * 0.1;
    return center + delta / factor;
}

// Apply before texture sampling:
inputTexCoord = distort(inputTexCoord);
```

## Known Issues & TODOs

- [ ] **Perspective division edge cases**: Handle zero/negative Z gracefully
- [ ] **Texture wrapping**: Implement GL_CLAMP_TO_BORDER for clean edges
- [ ] **Multi-tap filtering**: Use PCF for smoother samples near edges
- [ ] **HDR support**: Consider using higher precision textures
- [ ] **Gamma correction**: Apply proper gamma space transformations

## References

- OpenGL Perspective: https://www.khronos.org/opengl/wiki/Vertex_Post_Processing
- GLSL Documentation: https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.3.30.pdf
- Homography in Graphics: https://en.wikipedia.org/wiki/Homography_(computer_vision)

