#pragma once

#include "source.hpp"
#include <string>

namespace sources {

/**
 * @brief Built-in color pattern source for testing and default layers.
 *
 * Renders one of several GPU-generated patterns into a texture
 * (solid color, checkerboard, or gradient). Requires an OpenGL context.
 */
class ColorPatternSource : public Source {
public:
    enum class Pattern {
        SOLID_COLOR,        ///< Uniform solid color
        CHECKERBOARD,       ///< Black and colored checkerboard
        GRADIENT,           ///< Diagonal linear gradient
        CALIBRATION_GRID,   ///< Black with white grid lines for alignment
    };

    /**
     * @param pattern   Which pattern to generate
     * @param r/g/b     Primary color components [0-1]
     * @param width     Texture width in pixels
     * @param height    Texture height in pixels
     */
    ColorPatternSource(Pattern pattern,
                       float r, float g, float b,
                       int width = 512, int height = 512);
    ~ColorPatternSource() override;

    bool initialize() override;
    void shutdown()   override;
    bool update(float deltaTime) override;

    unsigned int getTextureHandle() const override { return textureHandle_; }
    Vec2         getResolution()    const override { return resolution_; }
    bool         isValid()          const override { return initialized_; }
    SourceType   getType()          const override { return SourceType::SHADER_REALTIME; }
    std::string  getName()          const override;

    void setColor(float r, float g, float b);
    Pattern getPattern() const { return pattern_; }
    float getR() const { return r_; }
    float getG() const { return g_; }
    float getB() const { return b_; }

private:
    Pattern      pattern_;
    float        r_, g_, b_;
    Vec2         resolution_;
    float        elapsedTime_;
    bool         initialized_;

    unsigned int shaderProgram_;
    unsigned int framebufferObject_;
    unsigned int textureHandle_;
    unsigned int quadVAO_;
    unsigned int quadVBO_;

    bool buildShader();
    void createFBO();
    void createQuad();
};

} // namespace sources
