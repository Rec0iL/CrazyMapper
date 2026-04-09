#pragma once

#include "shape.hpp"

namespace layers {

/**
 * @brief Ellipse/circle shape with optional corner pinning
 */
class EllipseShape : public Shape {
public:
    EllipseShape(const Vec2& position = Vec2(0.0f),
                 const Vec2& size = Vec2(100.0f));

    ShapeType getType() const override { return ShapeType::ELLIPSE; }
    std::array<Vec2, 4> getCorners() const override;
    void setLocalBounds(const Vec2& position, const Vec2& size) override;
    Vec2 getLocalPosition() const override { return position_; }
    Vec2 getLocalSize() const override { return size_; }
    bool contains(const Vec2& point) const override;
    std::vector<Vec2> getVertices() const override;

    /**
     * @brief Set number of segments for ellipse approximation
     */
    void setSegments(int segments);

private:
    Vec2 position_;
    Vec2 size_;
    int segments_;
};

} // namespace layers

