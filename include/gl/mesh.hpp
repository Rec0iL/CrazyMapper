#pragma once

#include "core/types.hpp"
#include <vector>

namespace gl {

/**
 * @brief Vertex array & buffer management for rendering
 */
class Mesh {
public:
    Mesh(const std::vector<Vec2>& vertices);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    /**
     * @brief Bind VAO for rendering
     */
    void bind() const;

    /**
     * @brief Draw mesh
     */
    void draw() const;

    /**
     * @brief Get vertex count
     */
    int getVertexCount() const { return vertexCount_; }

    /**
     * @brief Update vertices
     */
    void updateVertices(const std::vector<Vec2>& vertices);

private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    int vertexCount_ = 0;
};

} // namespace gl

