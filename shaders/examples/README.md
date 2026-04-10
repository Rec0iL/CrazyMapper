# Example Shaders

Drop any `.glsl` or `.frag` file here and load it via **Sources → Add Source → Shader File**.

## Required uniforms

| Uniform | Type | Description |
|---------|------|-------------|
| `iResolution` | `vec2` | Output texture size in pixels |
| `iTime` | `float` | Elapsed seconds since source creation |

## Minimal template

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

## Shadertoy compatibility

Shadertoy shaders use `mainImage(out vec4 fragColor, in vec2 fragCoord)`.
To adapt one, wrap it:

```glsl
#version 330 core
out vec4 FragColor;
uniform vec2  iResolution;
uniform float iTime;

// -- paste the Shadertoy shader body here --

void main() {
    mainImage(FragColor, gl_FragCoord.xy);
}
```

Most single-pass Shadertoy shaders work with no other changes.
Multi-pass shaders (Buffer A/B/C/D) are not supported.
