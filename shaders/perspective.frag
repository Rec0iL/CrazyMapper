#version 330 core

// Fragment Shader: Per-pixel perspective-correct texture mapping via homography.
//
// Coordinate convention (all in [0..1], y-down like ImGui canvas):
//   outputCoord (0,0) = ImGui canvas top-left
//   outputCoord (1,1) = ImGui canvas bottom-right
//
// homographyInverse maps: outputCoord -> source ImGui-UV [0..1 y-down]
// A V-flip is applied before sampling because GL textures use y-up storage.

in VS_OUT {
    vec2 texCoord;        // ImGui canvas UV for this fragment [0..1, y-down]
} fs_in;

out vec4 FragColor;

uniform sampler2D sourceTexture;

// Maps output canvas UV -> source ImGui-UV (y-down)
uniform mat3 homographyInverse;

// Layer opacity
uniform float opacity;

// Normalized output quad corners [0..1, y-down] — TL, TR, BR, BL
// Used for per-pixel polygon masking so only pixels inside the quad get rendered.
uniform vec2 outputCorners[4];

// 2D cross product (signed area of parallelogram)
float cross2d(vec2 a, vec2 b) { return a.x * b.y - a.y * b.x; }

// Returns true when p is inside the (possibly non-axis-aligned) convex quad.
// Works for both CW and CCW winding.
bool insideConvexQuad(vec2 p) {
    float d0 = cross2d(outputCorners[1] - outputCorners[0], p - outputCorners[0]);
    float d1 = cross2d(outputCorners[2] - outputCorners[1], p - outputCorners[1]);
    float d2 = cross2d(outputCorners[3] - outputCorners[2], p - outputCorners[2]);
    float d3 = cross2d(outputCorners[0] - outputCorners[3], p - outputCorners[3]);
    bool allPos = (d0 >= 0.0) && (d1 >= 0.0) && (d2 >= 0.0) && (d3 >= 0.0);
    bool allNeg = (d0 <= 0.0) && (d1 <= 0.0) && (d2 <= 0.0) && (d3 <= 0.0);
    return allPos || allNeg;
}

void main() {
    vec2 outputCoord = fs_in.texCoord;

    // Discard fragments outside the output quad boundary
    if (!insideConvexQuad(outputCoord)) discard;

    // Per-pixel perspective-correct UV lookup via homography
    vec3 h = homographyInverse * vec3(outputCoord, 1.0);
    vec2 sourceUV = h.xy / h.z;   // still in ImGui y-down [0..1]

    // Discard if the homography maps outside the source region
    if (sourceUV.x < 0.0 || sourceUV.x > 1.0 ||
        sourceUV.y < 0.0 || sourceUV.y > 1.0)
        discard;

    // Convert ImGui y-down UV to OpenGL y-up for texture sampling
    sourceUV.y = 1.0 - sourceUV.y;

    vec4 color = texture(sourceTexture, sourceUV);
    color.a *= opacity;
    FragColor = color;
}

