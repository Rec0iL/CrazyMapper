#version 330 core
out vec4 FragColor;
uniform vec2  iResolution;
uniform float iTime;

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution;

    // Animated brick columns
    float speed = 0.3;
    float cols  = 8.0;
    float rows  = 4.5;

    vec2 grid = uv * vec2(cols, rows);
    float col  = floor(grid.x);
    float offset = mod(col, 2.0) == 0.0 ? iTime * speed : -iTime * speed;
    grid.y += offset;

    vec2 cell = fract(grid);
    float gap  = 0.06;
    vec2  brick = step(vec2(gap), cell) * step(cell, vec2(1.0 - gap));
    float mask  = brick.x * brick.y;

    float hue = fract(col / cols + iTime * 0.05);
    vec3  brick_col = 0.5 + 0.5 * cos(6.28318 * (vec3(hue) + vec3(0.0, 0.33, 0.67)));
    vec3  grout_col = vec3(0.15);
    vec3  col3 = mix(grout_col, brick_col, mask);
    FragColor = vec4(col3, 1.0);
}
