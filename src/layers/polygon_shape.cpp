#include "layers/polygon_shape.hpp"
#include <algorithm>
#include <cmath>

namespace layers {

PolygonShape::PolygonShape(const std::vector<Vec2>& vertices)
    : vertices_(vertices) {
}

PolygonShape::PolygonShape(int sides, const Vec2& center, float radius) {
    vertices_.clear();
    for (int i = 0; i < sides; ++i) {
        float angle = 2.0f * 3.14159f * i / sides;
        float x = center.x + radius * std::cos(angle);
        float y = center.y + radius * std::sin(angle);
        vertices_.emplace_back(x, y);
    }
}

std::array<Vec2, 4> PolygonShape::getCorners() const {
    // Return bounding box corners
    Vec2 minP = vertices_.empty() ? Vec2(0.0f) : vertices_[0];
    Vec2 maxP = minP;
    
    for (const auto& v : vertices_) {
        minP = glm::min(minP, v);
        maxP = glm::max(maxP, v);
    }
    
    return {
        minP,                           // Top-left
        Vec2(maxP.x, minP.y),           // Top-right
        maxP,                           // Bottom-right
        Vec2(minP.x, maxP.y)            // Bottom-left
    };
}

void PolygonShape::setLocalBounds(const Vec2& position, const Vec2& size) {
    // Normalize to bounds and reposition
    if (vertices_.empty()) return;
    
    Vec2 minP = vertices_[0];
    Vec2 maxP = minP;
    
    for (const auto& v : vertices_) {
        minP = glm::min(minP, v);
        maxP = glm::max(maxP, v);
    }
    
    Vec2 oldSize = maxP - minP;
    if (oldSize.x > 0 && oldSize.y > 0) {
        for (auto& v : vertices_) {
            v = (v - minP);
            v.x = (v.x / oldSize.x) * size.x;
            v.y = (v.y / oldSize.y) * size.y;
            v += position;
        }
    }
}

Vec2 PolygonShape::getLocalPosition() const {
    if (vertices_.empty()) return Vec2(0.0f);
    return *std::min_element(vertices_.begin(), vertices_.end(),
                             [](const Vec2& a, const Vec2& b) {
                                 return a.x + a.y < b.x + b.y;
                             });
}

Vec2 PolygonShape::getLocalSize() const {
    if (vertices_.empty()) return Vec2(0.0f);
    Vec2 minP = vertices_[0];
    Vec2 maxP = minP;
    
    for (const auto& v : vertices_) {
        minP = glm::min(minP, v);
        maxP = glm::max(maxP, v);
    }
    
    return maxP - minP;
}

bool PolygonShape::contains(const Vec2& point) const {
    return pointInPolygon(point);
}

void PolygonShape::addVertex(const Vec2& vertex) {
    vertices_.push_back(vertex);
}

void PolygonShape::setVertex(int index, const Vec2& vertex) {
    if (index >= 0 && index < static_cast<int>(vertices_.size())) {
        vertices_[index] = vertex;
    }
}

bool PolygonShape::pointInPolygon(const Vec2& point) const {
    if (vertices_.size() < 3) return false;
    
    // Ray casting algorithm
    int count = 0;
    for (size_t i = 0; i < vertices_.size(); ++i) {
        const Vec2& v1 = vertices_[i];
        const Vec2& v2 = vertices_[(i + 1) % vertices_.size()];
        
        if ((v1.y <= point.y && point.y < v2.y) ||
            (v2.y <= point.y && point.y < v1.y)) {
            float x_intersect = v1.x + (point.y - v1.y) * (v2.x - v1.x) / (v2.y - v1.y);
            if (point.x < x_intersect) {
                count++;
            }
        }
    }
    
    return (count % 2) == 1;
}

} // namespace layers

