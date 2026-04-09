#pragma once

#include "shape.hpp"
#include <array>

namespace layers {

/**
 * @brief Round-cornered rectangle
 *
 * Radii array order: [0]=TL, [1]=TR, [2]=BR, [3]=BL (y-down)
 */
class RoundedRectangleShape : public Shape {
public:
    /** All four corners use the same radius. */
    RoundedRectangleShape(const Vec2& position = Vec2(0.0f),
                          const Vec2& size = Vec2(100.0f),
                          float cornerRadius = 10.0f);

    /** Individual radius per corner: TL, TR, BR, BL. */
    RoundedRectangleShape(const Vec2& position,
                          const Vec2& size,
                          float tlRadius, float trRadius,
                          float brRadius, float blRadius);

    ShapeType getType() const override { return ShapeType::ROUNDED_RECTANGLE; }
    std::array<Vec2, 4> getCorners() const override;
    void setLocalBounds(const Vec2& position, const Vec2& size) override;
    Vec2 getLocalPosition() const override { return position_; }
    Vec2 getLocalSize() const override { return size_; }
    bool contains(const Vec2& point) const override;
    std::vector<Vec2> getVertices() const override;

    /** Set all four corners to the same radius. */
    void setCornerRadius(float radius);
    float getCornerRadius() const { return radii_[0]; }

    /** Set individual corner radii: TL, TR, BR, BL. */
    void setCornerRadii(float tl, float tr, float br, float bl);
    std::array<float, 4> getCornerRadii() const { return radii_; }

    /** Whether per-corner mode is active (drives the UI). */
    bool isPerCorner() const { return perCorner_; }
    void setPerCorner(bool v) { perCorner_ = v; }

    void setSegments(int segments);

private:
    Vec2  position_;
    Vec2  size_;
    std::array<float, 4> radii_ = {};  // TL TR BR BL
    bool  perCorner_ = false;
    int   segments_  = 8;
};

} // namespace layers

