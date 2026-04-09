#include "math/homography.hpp"
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace math {

// Gaussian elimination with partial pivoting on an 8x9 augmented matrix [A|b]
// Solves Ax = b in-place, returns false if system is singular
static bool gaussianElimination(float M[8][9]) {
    for (int col = 0; col < 8; ++col) {
        // Find pivot row
        int pivot = col;
        for (int row = col + 1; row < 8; ++row) {
            if (std::abs(M[row][col]) > std::abs(M[pivot][col]))
                pivot = row;
        }
        if (std::abs(M[pivot][col]) < 1e-8f) return false;

        // Swap pivot row into position
        if (pivot != col)
            for (int j = 0; j <= 8; ++j) std::swap(M[col][j], M[pivot][j]);

        // Eliminate below
        for (int row = col + 1; row < 8; ++row) {
            float factor = M[row][col] / M[col][col];
            for (int j = col; j <= 8; ++j)
                M[row][j] -= factor * M[col][j];
        }
    }

    // Back substitution
    for (int col = 7; col >= 0; --col) {
        M[col][8] /= M[col][col];
        M[col][col] = 1.0f;
        for (int row = col - 1; row >= 0; --row) {
            M[row][8] -= M[row][col] * M[col][8];
            M[row][col] = 0.0f;
        }
    }
    return true;
}

Mat3 Homography::compute(const std::array<Vec2, 4>& srcCorners,
                         const std::array<Vec2, 4>& dstCorners) {
    // DLT: for each correspondence (x,y)->(x',y') with h33=1, rearranged:
    //   h1*x + h2*y + h3             - h7*x*x' - h8*y*x' = x'
    //             h4*x + h5*y + h6   - h7*x*y' - h8*y*y' = y'
    // Build 8x9 augmented matrix [A | b]
    float M[8][9] = {};

    for (int i = 0; i < 4; ++i) {
        float x  = srcCorners[i].x, y  = srcCorners[i].y;
        float xp = dstCorners[i].x, yp = dstCorners[i].y;

        // Even row
        M[2*i][0] = x;   M[2*i][1] = y;   M[2*i][2] = 1.f;
        M[2*i][3] = 0.f; M[2*i][4] = 0.f; M[2*i][5] = 0.f;
        M[2*i][6] = -x * xp; M[2*i][7] = -y * xp;
        M[2*i][8] = xp;

        // Odd row
        M[2*i+1][0] = 0.f; M[2*i+1][1] = 0.f; M[2*i+1][2] = 0.f;
        M[2*i+1][3] = x;   M[2*i+1][4] = y;   M[2*i+1][5] = 1.f;
        M[2*i+1][6] = -x * yp; M[2*i+1][7] = -y * yp;
        M[2*i+1][8] = yp;
    }

    if (!gaussianElimination(M)) return glm::mat3(1.0f);

    // h[0..7] are back in M[0..7][8], h[8]=1
    // Matrix layout (column-major for GLM):
    //  | h0 h1 h2 |   | col0 |
    //  | h3 h4 h5 | = | col1 |
    //  | h6 h7  1 |   | col2 |
    glm::mat3 H;
    H[0][0] = M[0][8]; H[0][1] = M[3][8]; H[0][2] = M[6][8];
    H[1][0] = M[1][8]; H[1][1] = M[4][8]; H[1][2] = M[7][8];
    H[2][0] = M[2][8]; H[2][1] = M[5][8]; H[2][2] = 1.0f;

    return H;
}

Vec2 Homography::transform(const Mat3& H, const Vec2& point) {
    // Transform point using homography matrix with perspective division
    glm::vec3 p(point.x, point.y, 1.0f);
    glm::vec3 p_transformed = H * p;
    
    // Perspective division
    if (std::abs(p_transformed.z) > 1e-6f) {
        return Vec2(p_transformed.x / p_transformed.z, 
                    p_transformed.y / p_transformed.z);
    }
    return point; // Fallback if degenerate
}

Mat3 Homography::invert(const Mat3& H) {
    return glm::inverse(H);
}

Mat3 Homography::normalize(const Mat3& H) {
    float det = glm::determinant(H);
    if (std::abs(det) > 1e-6f) {
        return H / det;
    }
    return H;
}

} // namespace math

