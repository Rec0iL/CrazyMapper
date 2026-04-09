#pragma once

#include "core/types.hpp"
#include <array>

namespace layers {

enum class ShapeType {
    RECTANGLE,
    ROUNDED_RECTANGLE,
    ELLIPSE,
    N_POLYGON
};

/**
 * @brief 2D shape definition with optional corner-pinning points
 * 
 * Represents the layer geometry in input space. Can be distorted
 * in output space via corner-pinning (homography transform).
 */
class Shape {
public:
    virtual ~Shape() = default;

    /**
     * @brief Get type of this shape
     */
    virtual ShapeType getType() const = 0;

    /**
     * @brief Get corners in local space (before distortion)
     * @return Four corners [TL, TR, BR, BL] for rendering/dragging
     */
    virtual std::array<Vec2, 4> getCorners() const = 0;

    /**
     * @brief Set shape bounds in input space
     * @param position Top-left corner
     * @param size Width and height
     */
    virtual void setLocalBounds(const Vec2& position, const Vec2& size) = 0;

    /**
     * @brief Get current local bounds
     */
    virtual Vec2 getLocalPosition() const = 0;
    virtual Vec2 getLocalSize() const = 0;

    /**
     * @brief Check if point is inside shape (for hit testing)
     */
    virtual bool contains(const Vec2& point) const = 0;

    /**
     * @brief Get shape as vertices for rendering
     * @return Vector of vertices (should form a closed loop)
     */
    virtual std::vector<Vec2> getVertices() const = 0;
};

} // namespace layers

