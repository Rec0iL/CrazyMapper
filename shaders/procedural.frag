// TODO: GLSL version must match OpenGL 3.3+ (minimum)
#version 330 core

// Fragment Shader: Simple procedural shader for testing ShaderSource

out vec4 FragColor;

// Uniforms set from ShaderSource class
uniform float iTime;           // Elapsed time in seconds
uniform vec2 iResolution;      // Canvas resolution

void main() {
    // Normalize coordinates to [0, 1]
    vec2 uv = gl_FragCoord.xy / iResolution;
    
    // Simple animated pattern using sin/cos
    float r = sin(uv.x * 3.0 + iTime) * 0.5 + 0.5;
    float g = sin(uv.y * 3.0 + iTime + 2.0) * 0.5 + 0.5;
    float b = sin((uv.x + uv.y) * 3.0 + iTime + 4.0) * 0.5 + 0.5;
    
    FragColor = vec4(r, g, b, 1.0);
}

