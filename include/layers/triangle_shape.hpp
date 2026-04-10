#pragma once

#include "shape.hpp"
#include <array>

namespace layers {

/**
 * @brief Triangle shape with 3 explicit vertices.
 *
 * Both input and output spaces use exactly 3 corner points (not 4 like quads).
 * Uses an affine transform (3-point mapping) for source UV lookup.
 */
class TriangleShape : public Shape {
public:
    /**
     * @brief Create triangle from 3 vertices in UV space [0..1].
     */
    TriangleShape(const Vec2& v0, const Vec2& v1, const Vec2& v2);

    ShapeType getType() const override { return ShapeType::TRIANGLE; }
    std::array<Vec2, 4> getCorners() const override;
    void setLocalBounds(const Vec2& position, const Vec2& size) override;
    Vec2 getLocalPosition() const override;
    Vec2 getLocalSize() const override;
    bool contains(const Vec2& point) const override;
    std::vector<Vec2> getVertices() const override;

    void setVertex(int i, const Vec2& v);
    Vec2 getVertex(int i) const;

private:
    std::array<Vec2, 3> vertices_;

    float cross2d(Vec2 a, Vec2 b) const;
};

} // namespace layers
