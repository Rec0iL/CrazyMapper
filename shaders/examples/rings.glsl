#version 330 core
out vec4 FragColor;
uniform vec2  iResolution;
uniform float iTime;

// Radially pulsing rings — good for projection center-pieces
void main() {
    vec2 uv = gl_FragCoord.xy / iResolution - 0.5;
    uv.x *= iResolution.x / iResolution.y;

    float radius = length(uv);
    float angle  = atan(uv.y, uv.x);

    // Rings that pulse outward
    float rings = sin(radius * 20.0 - iTime * 3.0);
    rings = rings * 0.5 + 0.5;
    rings = pow(rings, 3.0);

    // Angular color sweep
    float hue = fract(angle / 6.28318 + iTime * 0.1);
    vec3 col = 0.5 + 0.5 * cos(6.28318 * (hue + vec3(0.0, 0.33, 0.67)));

    // Fade toward edges
    float vignette = 1.0 - smoothstep(0.35, 0.5, radius);
    FragColor = vec4(col * rings * vignette, 1.0);
}
