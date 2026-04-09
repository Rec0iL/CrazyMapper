#include "layers/rounded_rectangle_shape.hpp"
#include <glm/gtc/constants.hpp>
#include <cmath>

namespace layers {

RoundedRectangleShape::RoundedRectangleShape(const Vec2& position,
                                             const Vec2& size,
                                             float cornerRadius)
    : position_(position),
      size_(size),
      radii_({cornerRadius, cornerRadius, cornerRadius, cornerRadius}),
      perCorner_(false),
      segments_(8) {
}

RoundedRectangleShape::RoundedRectangleShape(const Vec2& position,
                                             const Vec2& size,
                                             float tlRadius, float trRadius,
                                             float brRadius, float blRadius)
    : position_(position),
      size_(size),
      radii_({tlRadius, trRadius, brRadius, blRadius}),
      perCorner_(true),
      segments_(8) {
}

std::array<Vec2, 4> RoundedRectangleShape::getCorners() const {
    return {
        position_ + Vec2(radii_[0], radii_[0]),
        position_ + Vec2(size_.x - radii_[1], radii_[1]),
        position_ + size_ - Vec2(radii_[2], radii_[2]),
        position_ + Vec2(radii_[3], size_.y - radii_[3])
    };
}

void RoundedRectangleShape::setLocalBounds(const Vec2& position, const Vec2& size) {
    position_ = position;
    size_ = size;
}

bool RoundedRectangleShape::contains(const Vec2& point) const {
    return point.x >= position_.x &&
           point.x <= position_.x + size_.x &&
           point.y >= position_.y &&
           point.y <= position_.y + size_.y;
}

std::vector<Vec2> RoundedRectangleShape::getVertices() const {
    std::vector<Vec2> vertices;
    // Helper to append an arc for one corner
    // cx,cy = arc center, startAngle = start, sweepDir = +1 or -1 (quarter turn)
    auto appendArc = [&](float cx, float cy, float r, float startAngle) {
        int steps = std::max(2, segments_);
        for (int i = 0; i <= steps; ++i) {
            float angle = startAngle + glm::half_pi<float>() * (float)i / (float)steps;
            vertices.emplace_back(cx + r * std::cos(angle),
                                  cy + r * std::sin(angle));
        }
    };

    // TL corner (radii_[0])
    appendArc(position_.x + radii_[0], position_.y + radii_[0], radii_[0],
              glm::pi<float>());
    // TR corner (radii_[1]) — arc from 270° to 360°(=0°)
    appendArc(position_.x + size_.x - radii_[1], position_.y + radii_[1], radii_[1],
              -glm::half_pi<float>());
    // BR corner (radii_[2])
    appendArc(position_.x + size_.x - radii_[2], position_.y + size_.y - radii_[2], radii_[2],
              0.0f);
    // BL corner (radii_[3])
    appendArc(position_.x + radii_[3], position_.y + size_.y - radii_[3], radii_[3],
              glm::half_pi<float>());

    return vertices;
}

void RoundedRectangleShape::setCornerRadius(float radius) {
    radii_ = {radius, radius, radius, radius};
}

void RoundedRectangleShape::setCornerRadii(float tl, float tr, float br, float bl) {
    radii_ = {tl, tr, br, bl};
}

void RoundedRectangleShape::setSegments(int segments) {
    segments_ = glm::max(segments, 2);
}

} // namespace layers

