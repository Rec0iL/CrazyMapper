#pragma once

#include "shape.hpp"

namespace layers {

/**
 * @brief N-sided polygon shape
 */
class PolygonShape : public Shape {
public:
    /**
     * @brief Create polygon from vertices
     * @param vertices Polygon vertices in local space
     */
    explicit PolygonShape(const std::vector<Vec2>& vertices);

    /**
     * @brief Create regular N-sided polygon
     * @param sides Number of sides
     * @param center Center position
     * @param radius Distance from center to vertex
     */
    PolygonShape(int sides, const Vec2& center, float radius);

    ShapeType getType() const override { return ShapeType::N_POLYGON; }
    std::array<Vec2, 4> getCorners() const override;
    void setLocalBounds(const Vec2& position, const Vec2& size) override;
    Vec2 getLocalPosition() const override;
    Vec2 getLocalSize() const override;
    bool contains(const Vec2& point) const override;
    std::vector<Vec2> getVertices() const override { return vertices_; }

    /**
     * @brief Add vertex to polygon
     */
    void addVertex(const Vec2& vertex);

    /**
     * @brief Move vertex
     */
    void setVertex(int index, const Vec2& vertex);

    /**
     * @brief Get number of vertices
     */
    int getVertexCount() const { return vertices_.size(); }

private:
    std::vector<Vec2> vertices_;

    // Helper for point-in-polygon test
    bool pointInPolygon(const Vec2& point) const;
};

} // namespace layers

