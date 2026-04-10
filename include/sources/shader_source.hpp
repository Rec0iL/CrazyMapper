#pragma once

#include "source.hpp"

namespace sources {

/**
 * @brief Real-time shader source
 * 
 * Generates procedural content via GLSL fragment shader.
 * Perfect for test patterns, dynamic content, etc.
 */
class ShaderSource : public Source {
public:
    /**
     * @brief Create shader source with fragment shader code
     * @param fragmentShaderCode GLSL fragment shader source
     * @param width Output texture width
     * @param height Output texture height
     */
    ShaderSource(const std::string& fragmentShaderCode, 
                 int width, int height,
                 const std::string& filePath = "");
    ~ShaderSource() override;

    bool initialize() override;
    void shutdown() override;
    bool update(float deltaTime) override;
    unsigned int getTextureHandle() const override;
    Vec2 getResolution() const override;
    bool isValid() const override;
    SourceType getType() const override { return SourceType::SHADER_REALTIME; }
    std::string getName() const override;

    /**
     * @brief Update shader uniform values
     * @param uniformName Name of the uniform
     * @param value Float value
     */
    void setUniform(const std::string& uniformName, float value);

    /**
     * @brief Recompile shader with new code
     */
    bool recompileShader(const std::string& newFragmentCode);

    const std::string& getFilePath() const { return filePath_; }

private:
    std::string fragmentShaderCode_;
    std::string filePath_;            // empty if built-in
    unsigned int shaderProgram_;
    unsigned int framebufferObject_;
    unsigned int textureHandle_;
    unsigned int quadVAO_;
    unsigned int quadVBO_;
    Vec2 resolution_;
    bool initialized_;
    float elapsedTime_;

    bool compileShader(const std::string& vertexSrc, 
                       const std::string& fragmentSrc);
    void createFramebuffer();
    void createQuad();
};

} // namespace sources

