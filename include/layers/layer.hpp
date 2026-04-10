#pragma once

#include "shape.hpp"
#include "sources/source.hpp"
#include "math/homography.hpp"
#include <memory>
#include <array>

namespace layers {

/**
 * @brief Projection layer combining source input with geometric shape
 * 
 * Each layer:
 * - Links to a media source
 * - Defines a shape in input space (which part of source to display)
 * - Can be corner-pinned in output space (perspective distortion)
 */
class Layer {
public:
    /**
     * @brief Create layer with source and shape
     */
    Layer(unsigned int id, 
          Shared<sources::Source> source,
          Unique<Shape> shape);

    ~Layer();

    /**
     * @brief Get layer ID
     */
    unsigned int getId() const { return id_; }

    /**
     * @brief Get linked source
     */
    Shared<sources::Source> getSource() const { return source_; }

    /**
     * @brief Swap the source linked to this layer
     */
    void setSource(Shared<sources::Source> source) { source_ = std::move(source); }

    /**
     * @brief Set layer visibility
     */
    void setVisible(bool visible) { visible_ = visible; }
    bool isVisible() const { return visible_; }

    /**
     * @brief Get shape in input space
     */
    Shape* getShape() const { return shape_.get(); }

    /**
     * @brief Replace the layer's shape.
     */
    void setShape(Unique<Shape> shape);

    /** Shape type as int (matches ShapeType enum: 0=Rect,1=RoundedRect,2=Ellipse,3=Polygon). */
    int   getShapeType()     const;
    /** All four corner radii for ROUNDED_RECTANGLE shapes [TL,TR,BR,BL] (UV units), else {0,0,0,0}. */
    std::array<float, 4> getShapeCornerRadii() const;
    /** True when RR shape is in per-corner mode. */
    bool  isShapePerCorner() const;
    /** Corner radius for ROUNDED_RECTANGLE shapes (UV units, 0..0.5), else 0. */
    float getShapeRadius()   const;
    /** Number of sides for N_POLYGON shapes, else 0. */
    int   getShapePolySides() const;

    /**
     * @brief Get/set opacity
     */
    void setOpacity(float opacity) { opacity_ = glm::clamp(opacity, 0.0f, 1.0f); }
    float getOpacity() const { return opacity_; }

    /**
     * @brief Get/set blend mode
     */
    void setBlendMode(int mode) { blendMode_ = mode; }
    int getBlendMode() const { return blendMode_; }

    // ===== INPUT CORNERS (Source Region) =====

    /**
     * @brief Get input corners (source region in input space)
     * @return Four corners [TL, TR, BR, BL]
     */
    const std::array<Vec2, 4>& getInputCorners() const { return inputCorners_; }

    /**
     * @brief Set input corner (for corner dragging in input space)
     * @param cornerIndex 0=TL, 1=TR, 2=BR, 3=BL
     * @param position New position in input coordinates
     */
    void setInputCorner(int cornerIndex, const Vec2& position);

    /**
     * @brief Reset input corners to match shape defaults
     */
    void resetInputCorners();

    // ===== CORNER-PINNING (Output Space Distortion) =====

    /**
     * @brief Get destination corners (after perspective distortion in output space)
     * @return Four corners [TL, TR, BR, BL]
     */
    const std::array<Vec2, 4>& getOutputCorners() const { return outputCorners_; }

    /**
     * @brief Set destination corner (for corner-pinning UI)
     * @param cornerIndex 0=TL, 1=TR, 2=BR, 3=BL
     * @param position New position in output coordinates
     */
    void setOutputCorner(int cornerIndex, const Vec2& position);

    /**
     * @brief Reset output corners to match input shape
     */
    void resetOutputCorners();

    /**
     * @brief Get/set edge feather width (0 = hard edge, 0.2 = wide soft edge).
     * Units are in shape-local UV space for non-triangle shapes, canvas UV for triangles.
     */
    void setFeather(float f) { feather_ = glm::clamp(f, 0.0f, 0.5f); }
    float getFeather() const { return feather_; }

    /**
     * @brief Index of the canvas this layer is rendered on.
     * 0 = default canvas, 1+ = additional canvases.
     */
    void setCanvasIndex(int idx) { canvasIndex_ = (idx >= 0) ? idx : 0; }
    int  getCanvasIndex() const  { return canvasIndex_; }

    /**
     * @brief Number of active corners: 3 for TRIANGLE, 4 for all other shapes.
     */
    int getActiveCornerCount() const;

    /**
     * @brief Get homography transform (input space -> output space)
     */
    const glm::mat3& getHomography() const { return homography_; }

    /**
     * @brief Get inverse homography (output space -> input space)
     */
    const glm::mat3& getInverseHomography() const { return inverseHomography_; }

private:
    unsigned int id_;
    Shared<sources::Source> source_;
    Unique<Shape> shape_;
    bool visible_;
    float opacity_;
    int blendMode_;
    float feather_;
    int canvasIndex_ = 0;  ///< Which canvas this layer belongs to

    // Corner state
    std::array<Vec2, 4> inputCorners_;
    std::array<Vec2, 4> outputCorners_;
    glm::mat3 homography_;
    glm::mat3 inverseHomography_;

    /**
     * @brief Recompute homography matrices when corners change
     */
    void updateHomography();
};

} // namespace layers

