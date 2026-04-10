#version 330 core
out vec4 FragColor;
uniform vec2  iResolution;
uniform float iTime;

float hash(float n) { return fract(sin(n) * 43758.5453); }

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i.x + i.y * 57.0);
    float b = hash(i.x + 1.0 + i.y * 57.0);
    float c = hash(i.x + (i.y + 1.0) * 57.0);
    float d = hash(i.x + 1.0 + (i.y + 1.0) * 57.0);
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 6; i++) {
        v += a * noise(p);
        p *= 2.1;
        a *= 0.5;
    }
    return v;
}

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution;
    vec2 p  = uv * 3.0 + vec2(iTime * 0.1);

    float n = fbm(p + fbm(p + fbm(p)));
    vec3 col = mix(vec3(0.05, 0.05, 0.2), vec3(0.8, 0.5, 0.1), n);
    col = mix(col, vec3(0.9, 0.9, 1.0), n * n * n);
    FragColor = vec4(col, 1.0);
}
