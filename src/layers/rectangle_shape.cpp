#include "layers/rectangle_shape.hpp"
#include <glm/gtc/constants.hpp>

namespace layers {

RectangleShape::RectangleShape(const Vec2& position, const Vec2& size)
    : position_(position), size_(size) {
}

std::array<Vec2, 4> RectangleShape::getCorners() const {
    return {
        position_,                          // Top-left
        position_ + Vec2(size_.x, 0.0f),   // Top-right
        position_ + size_,                  // Bottom-right
        position_ + Vec2(0.0f, size_.y)    // Bottom-left
    };
}

void RectangleShape::setLocalBounds(const Vec2& position, const Vec2& size) {
    position_ = position;
    size_ = size;
}

bool RectangleShape::contains(const Vec2& point) const {
    return point.x >= position_.x &&
           point.x <= position_.x + size_.x &&
           point.y >= position_.y &&
           point.y <= position_.y + size_.y;
}

std::vector<Vec2> RectangleShape::getVertices() const {
    return {
        position_,                          // Top-left
        position_ + Vec2(size_.x, 0.0f),   // Top-right
        position_ + size_,                  // Bottom-right
        position_ + Vec2(0.0f, size_.y)    // Bottom-left
    };
}

} // namespace layers

