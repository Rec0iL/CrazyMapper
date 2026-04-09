#pragma once

#include "core/types.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <string>

namespace gl {

/**
 * @brief OpenGL shader compilation and program management
 */
class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    /**
     * @brief Compile and link shader program
     * @param vertexSrc Vertex shader source
     * @param fragmentSrc Fragment shader source
     * @return true if compilation successful
     */
    bool compile(const std::string& vertexSrc, const std::string& fragmentSrc);

    /**
     * @brief Use this shader program
     */
    void use() const;

    /**
     * @brief Get program handle
     */
    unsigned int getHandle() const { return programHandle_; }

    // ===== Uniform Setting Helpers =====

    void setUniform1f(const std::string& name, float value);
    void setUniform2f(const std::string& name, float x, float y);
    void setUniform3f(const std::string& name, float x, float y, float z);
    void setUniform4f(const std::string& name, float x, float y, float z, float w);
    
    void setUniform1i(const std::string& name, int value);
    void setUniform2i(const std::string& name, int x, int y);
    
    void setUniformMat3(const std::string& name, const glm::mat3& matrix);
    void setUniformMat4(const std::string& name, const glm::mat4& matrix);
    
    void setUniformVec2(const std::string& name, const glm::vec2& value);
    void setUniformVec3(const std::string& name, const glm::vec3& value);
    void setUniformVec4(const std::string& name, const glm::vec4& value);

private:
    unsigned int programHandle_ = 0;

    unsigned int compileShader(const std::string& source, unsigned int type);
    int getUniformLocation(const std::string& name) const;
};

} // namespace gl

