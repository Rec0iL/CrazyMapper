#include "layers/triangle_shape.hpp"
#include <algorithm>

namespace layers {

TriangleShape::TriangleShape(const Vec2& v0, const Vec2& v1, const Vec2& v2)
    : vertices_({v0, v1, v2}) {}

std::array<Vec2, 4> TriangleShape::getCorners() const {
    // 4th element repeats v0 for API compatibility
    return {vertices_[0], vertices_[1], vertices_[2], vertices_[0]};
}

void TriangleShape::setLocalBounds(const Vec2& position, const Vec2& size) {
    Vec2 curPos  = getLocalPosition();
    Vec2 curSize = getLocalSize();
    if (curSize.x < 1e-6f || curSize.y < 1e-6f) return;
    for (auto& v : vertices_) {
        v.x = position.x + (v.x - curPos.x) * size.x / curSize.x;
        v.y = position.y + (v.y - curPos.y) * size.y / curSize.y;
    }
}

Vec2 TriangleShape::getLocalPosition() const {
    return Vec2(
        std::min({vertices_[0].x, vertices_[1].x, vertices_[2].x}),
        std::min({vertices_[0].y, vertices_[1].y, vertices_[2].y}));
}

Vec2 TriangleShape::getLocalSize() const {
    float minX = std::min({vertices_[0].x, vertices_[1].x, vertices_[2].x});
    float maxX = std::max({vertices_[0].x, vertices_[1].x, vertices_[2].x});
    float minY = std::min({vertices_[0].y, vertices_[1].y, vertices_[2].y});
    float maxY = std::max({vertices_[0].y, vertices_[1].y, vertices_[2].y});
    return Vec2(maxX - minX, maxY - minY);
}

float TriangleShape::cross2d(Vec2 a, Vec2 b) const {
    return a.x * b.y - a.y * b.x;
}

bool TriangleShape::contains(const Vec2& point) const {
    float d0 = cross2d(vertices_[1] - vertices_[0], point - vertices_[0]);
    float d1 = cross2d(vertices_[2] - vertices_[1], point - vertices_[1]);
    float d2 = cross2d(vertices_[0] - vertices_[2], point - vertices_[2]);
    bool hasNeg = (d0 < 0.0f) || (d1 < 0.0f) || (d2 < 0.0f);
    bool hasPos = (d0 > 0.0f) || (d1 > 0.0f) || (d2 > 0.0f);
    return !(hasNeg && hasPos);
}

std::vector<Vec2> TriangleShape::getVertices() const {
    return {vertices_[0], vertices_[1], vertices_[2]};
}

void TriangleShape::setVertex(int i, const Vec2& v) {
    if (i >= 0 && i < 3) vertices_[i] = v;
}

Vec2 TriangleShape::getVertex(int i) const {
    if (i >= 0 && i < 3) return vertices_[i];
    return Vec2(0.0f, 0.0f);
}

} // namespace layers
