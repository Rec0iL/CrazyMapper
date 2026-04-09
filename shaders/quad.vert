// TODO: GLSL version must match OpenGL 3.3+ (minimum)
#version 330 core

// Vertex Shader: Simple fullscreen quad

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;

out VS_OUT {
    vec2 texCoord;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main() {
    // For fullscreen rendering, position should be in NDC [-1, 1]
    gl_Position = vec4(position, 0.0, 1.0);
    vs_out.texCoord = texCoord;
}

