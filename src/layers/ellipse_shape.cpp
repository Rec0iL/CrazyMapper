#include "layers/ellipse_shape.hpp"
#include <glm/gtc/constants.hpp>
#include <cmath>

namespace layers {

EllipseShape::EllipseShape(const Vec2& position, const Vec2& size)
    : position_(position), size_(size), segments_(32) {
}

std::array<Vec2, 4> EllipseShape::getCorners() const {
    // Return bounding box corners
    return {
        position_,                          // Top-left
        position_ + Vec2(size_.x, 0.0f),   // Top-right
        position_ + size_,                  // Bottom-right
        position_ + Vec2(0.0f, size_.y)    // Bottom-left
    };
}

void EllipseShape::setLocalBounds(const Vec2& position, const Vec2& size) {
    position_ = position;
    size_ = size;
}

bool EllipseShape::contains(const Vec2& point) const {
    Vec2 center = position_ + size_ * 0.5f;
    Vec2 radii = size_ * 0.5f;
    Vec2 delta = point - center;
    
    if (radii.x > 0 && radii.y > 0) {
        float dist = (delta.x / radii.x) * (delta.x / radii.x) +
                     (delta.y / radii.y) * (delta.y / radii.y);
        return dist <= 1.0f;
    }
    return false;
}

std::vector<Vec2> EllipseShape::getVertices() const {
    Vec2 center = position_ + size_ * 0.5f;
    Vec2 radii = size_ * 0.5f;
    
    std::vector<Vec2> vertices;
    for (int i = 0; i < segments_; ++i) {
        float angle = 2.0f * glm::pi<float>() * i / segments_;
        float x = center.x + radii.x * std::cos(angle);
        float y = center.y + radii.y * std::sin(angle);
        vertices.emplace_back(x, y);
    }
    return vertices;
}

void EllipseShape::setSegments(int segments) {
    segments_ = glm::max(segments, 4);
}

} // namespace layers

