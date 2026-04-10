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
                       float r2 = 0.08f, float g2 = 0.08f, float b2 = 0.08f,
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
    void setColor2(float r, float g, float b) { r2_ = r; g2_ = g; b2_ = b; }
    Pattern getPattern() const { return pattern_; }
    float getR() const { return r_; }
    float getG() const { return g_; }
    float getB() const { return b_; }
    float getR2() const { return r2_; }
    float getG2() const { return g2_; }
    float getB2() const { return b2_; }

private:
    Pattern      pattern_;
    float        r_, g_, b_;
    float        r2_, g2_, b2_;  ///< second colour (checkerboard background)
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
