#version 330 core
out vec4 FragColor;
uniform vec2  iResolution;
uniform float iTime;

void main() {
    vec2 uv = (gl_FragCoord.xy / iResolution) * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;

    float v = 0.0;
    v += sin(uv.x * 5.0 + iTime);
    v += sin(uv.y * 4.0 + iTime * 0.7);
    v += sin((uv.x + uv.y) * 3.0 + iTime * 1.3);
    float d = length(uv);
    v += sin(d * 6.0 - iTime * 1.5);
    v = v * 0.25 + 0.5;

    float r = sin(v * 3.14159);
    float g = sin(v * 3.14159 + 2.094);
    float b = sin(v * 3.14159 + 4.189);
    FragColor = vec4(r, g, b, 1.0);
}
