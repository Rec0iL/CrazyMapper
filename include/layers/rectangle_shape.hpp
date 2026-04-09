#pragma once

#include "shape.hpp"

namespace layers {

/**
 * @brief Rectangular shape
 */
class RectangleShape : public Shape {
public:
    RectangleShape(const Vec2& position = Vec2(0.0f),
                   const Vec2& size = Vec2(100.0f));

    ShapeType getType() const override { return ShapeType::RECTANGLE; }
    std::array<Vec2, 4> getCorners() const override;
    void setLocalBounds(const Vec2& position, const Vec2& size) override;
    Vec2 getLocalPosition() const override { return position_; }
    Vec2 getLocalSize() const override { return size_; }
    bool contains(const Vec2& point) const override;
    std::vector<Vec2> getVertices() const override;

private:
    Vec2 position_;
    Vec2 size_;
};

} // namespace layers

