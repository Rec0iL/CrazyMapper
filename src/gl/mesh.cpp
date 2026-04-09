#include "gl/mesh.hpp"
#include <glad/glad.h>

namespace gl {

Mesh::Mesh(const std::vector<Vec2>& vertices) : vertexCount_(vertices.size()) {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vec2),
                 vertices.data(), GL_DYNAMIC_DRAW);

    // Vertex attributes
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vec2), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

Mesh::~Mesh() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
}

void Mesh::bind() const {
    glBindVertexArray(vao_);
}

void Mesh::draw() const {
    bind();
    glDrawArrays(GL_LINE_LOOP, 0, vertexCount_);
}

void Mesh::updateVertices(const std::vector<Vec2>& vertices) {
    vertexCount_ = vertices.size();
    glBindBuffer(GL_COPY_WRITE_BUFFER, vbo_);
    glBufferSubData(GL_COPY_WRITE_BUFFER, 0, 
                    vertices.size() * sizeof(Vec2), vertices.data());
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

} // namespace gl

