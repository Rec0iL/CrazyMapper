#pragma once

#include "core/types.hpp"
#include <array>

namespace math {

/**
 * @brief Homography transform for perspective correction
 * 
 * Computes a 3x3 homography matrix from four source points (corners of input)
 * to four destination points (dragged corner-pinning points).
 * This matrix corrects perspective distortion in OpenGL texture mapping.
 */
class Homography {
public:
    /**
     * @brief Compute homography matrix from 4 corner points
     * @param srcCorners Four corners of source texture [TL, TR, BR, BL]
     * @param dstCorners Four corners after perspective distortion [TL, TR, BR, BL]
     * @return 3x3 homography matrix
     */
    static Mat3 compute(const std::array<Vec2, 4>& srcCorners,
                        const std::array<Vec2, 4>& dstCorners);

    /**
     * @brief Transform a 2D point using homography matrix
     * @param H Homography matrix
     * @param point Point to transform
     * @return Transformed point (with perspective division applied)
     */
    static Vec2 transform(const Mat3& H, const Vec2& point);

    /**
     * @brief Invert a homography matrix
     * @param H Input homography matrix
     * @return Inverse homography matrix
     */
    static Mat3 invert(const Mat3& H);

    /**
     * @brief Create normalized homography (if determinant is non-zero)
     * @param H Input homography matrix
     * @return Normalized homography
     */
    static Mat3 normalize(const Mat3& H);
};

} // namespace math

