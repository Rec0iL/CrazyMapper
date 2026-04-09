#include "gl/shader_program.hpp"
#include <glad/glad.h>
#include <iostream>

namespace gl {

ShaderProgram::~ShaderProgram() {
    if (programHandle_) {
        glDeleteProgram(programHandle_);
        programHandle_ = 0;
    }
}

bool ShaderProgram::compile(const std::string& vertexSrc, 
                            const std::string& fragmentSrc) {
    unsigned int vertex = compileShader(vertexSrc, GL_VERTEX_SHADER);
    if (!vertex) return false;

    unsigned int fragment = compileShader(fragmentSrc, GL_FRAGMENT_SHADER);
    if (!fragment) {
        glDeleteShader(vertex);
        return false;
    }

    programHandle_ = glCreateProgram();
    glAttachShader(programHandle_, vertex);
    glAttachShader(programHandle_, fragment);
    glLinkProgram(programHandle_);

    // Check for linking errors
    int success;
    char infoLog[512];
    glGetProgramiv(programHandle_, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(programHandle_, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return false;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return true;
}

void ShaderProgram::use() const {
    if (programHandle_) {
        glUseProgram(programHandle_);
    }
}

void ShaderProgram::setUniform1f(const std::string& name, float value) {
    glUniform1f(getUniformLocation(name), value);
}

void ShaderProgram::setUniform2f(const std::string& name, float x, float y) {
    glUniform2f(getUniformLocation(name), x, y);
}

void ShaderProgram::setUniform3f(const std::string& name, 
                                  float x, float y, float z) {
    glUniform3f(getUniformLocation(name), x, y, z);
}

void ShaderProgram::setUniform4f(const std::string& name,
                                  float x, float y, float z, float w) {
    glUniform4f(getUniformLocation(name), x, y, z, w);
}

void ShaderProgram::setUniform1i(const std::string& name, int value) {
    glUniform1i(getUniformLocation(name), value);
}

void ShaderProgram::setUniform2i(const std::string& name, int x, int y) {
    glUniform2i(getUniformLocation(name), x, y);
}

void ShaderProgram::setUniformMat3(const std::string& name, 
                                    const glm::mat3& matrix) {
    glUniformMatrix3fv(getUniformLocation(name), 1, GL_FALSE, 
                       glm::value_ptr(const_cast<glm::mat3&>(matrix)));
}

void ShaderProgram::setUniformMat4(const std::string& name,
                                    const glm::mat4& matrix) {
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE,
                       glm::value_ptr(const_cast<glm::mat4&>(matrix)));
}

void ShaderProgram::setUniformVec2(const std::string& name, 
                                    const glm::vec2& value) {
    glUniform2fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void ShaderProgram::setUniformVec3(const std::string& name,
                                    const glm::vec3& value) {
    glUniform3fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void ShaderProgram::setUniformVec4(const std::string& name,
                                    const glm::vec4& value) {
    glUniform4fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

unsigned int ShaderProgram::compileShader(const std::string& source,
                                          unsigned int type) {
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        const char* typeStr = (type == GL_VERTEX_SHADER) ? "Vertex" : "Fragment";
        std::cerr << typeStr << " shader compilation failed: " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

int ShaderProgram::getUniformLocation(const std::string& name) const {
    if (!programHandle_) return -1;
    return glGetUniformLocation(programHandle_, name.c_str());
}

} // namespace gl

