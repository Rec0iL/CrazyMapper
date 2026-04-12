#include "layers/layer.hpp"
#include "layers/rounded_rectangle_shape.hpp"
#include "layers/polygon_shape.hpp"
#include "layers/triangle_shape.hpp"
#include "math/homography.hpp"
#include <array>

namespace layers {

Layer::Layer(unsigned int id,
             Shared<sources::Source> source,
             Unique<Shape> shape)
    : id_(id),
      source_(source),
      shape_(std::move(shape)),
      visible_(true),
      opacity_(1.0f),
      blendMode_(0),
      featherWidth_(0.0f),
      featherStrength_(1.0f),
      perEdgeFeather_(false),
      edgeFeatherWidths_({0.0f, 0.0f, 0.0f, 0.0f}),
      canvasIndex_(0),
      inputCorners_({Vec2(0.f,0.f), Vec2(1.f,0.f), Vec2(1.f,1.f), Vec2(0.f,1.f)}),
      outputCorners_({Vec2(0.f,0.f), Vec2(1.f,0.f), Vec2(1.f,1.f), Vec2(0.f,1.f)}),
      homography_(glm::mat3(1.0f)),
      inverseHomography_(glm::mat3(1.0f)) {
}

Layer::~Layer() {
}

void Layer::setShape(Unique<Shape> shape) {
    shape_ = std::move(shape);
    updateHomography();  // shape type may change how homography is computed
}

int Layer::getShapeType() const {
    return static_cast<int>(shape_->getType());
}

std::array<float, 4> Layer::getShapeCornerRadii() const {
    if (auto* rr = dynamic_cast<RoundedRectangleShape*>(shape_.get()))
        return rr->getCornerRadii();
    return {0.f, 0.f, 0.f, 0.f};
}

bool Layer::isShapePerCorner() const {
    if (auto* rr = dynamic_cast<RoundedRectangleShape*>(shape_.get()))
        return rr->isPerCorner();
    return false;
}

float Layer::getShapeRadius() const {
    if (auto* rr = dynamic_cast<RoundedRectangleShape*>(shape_.get()))
        return rr->getCornerRadius();
    return 0.0f;
}

int Layer::getShapePolySides() const {
    if (auto* poly = dynamic_cast<PolygonShape*>(shape_.get()))
        return static_cast<int>(poly->getVertices().size());
    return 0;
}

int Layer::getActiveCornerCount() const {
    if (shape_ && shape_->getType() == ShapeType::TRIANGLE)
        return 3;
    return 4;
}

void Layer::setInputCorner(int cornerIndex, const Vec2& position) {
    if (cornerIndex >= 0 && cornerIndex < 4) {
        inputCorners_[cornerIndex] = position;
        updateHomography();
    }
}

void Layer::resetInputCorners() {
    if (shape_ && shape_->getType() == ShapeType::TRIANGLE) {
        inputCorners_[0] = Vec2(0.1f, 0.1f);
        inputCorners_[1] = Vec2(0.9f, 0.1f);
        inputCorners_[2] = Vec2(0.5f, 0.9f);
        inputCorners_[3] = Vec2(0.0f, 0.0f);
    } else {
        inputCorners_ = {Vec2(0.f,0.f), Vec2(1.f,0.f), Vec2(1.f,1.f), Vec2(0.f,1.f)};
    }
    updateHomography();
}

void Layer::setOutputCorner(int cornerIndex, const Vec2& position) {
    if (cornerIndex >= 0 && cornerIndex < 4) {
        outputCorners_[cornerIndex] = position;
        updateHomography();
    }
}

void Layer::resetOutputCorners() {
    if (shape_ && shape_->getType() == ShapeType::TRIANGLE) {
        outputCorners_[0] = Vec2(0.1f, 0.1f);
        outputCorners_[1] = Vec2(0.9f, 0.1f);
        outputCorners_[2] = Vec2(0.5f, 0.9f);
        outputCorners_[3] = Vec2(0.0f, 0.0f);
    } else {
        outputCorners_ = {Vec2(0.f,0.f), Vec2(1.f,0.f), Vec2(1.f,1.f), Vec2(0.f,1.f)};
    }
    updateHomography();
}

void Layer::updateHomography() {
    if (shape_ && shape_->getType() == ShapeType::TRIANGLE) {
        std::array<Vec2, 3> src = {inputCorners_[0], inputCorners_[1], inputCorners_[2]};
        std::array<Vec2, 3> dst = {outputCorners_[0], outputCorners_[1], outputCorners_[2]};
        homography_        = math::Homography::computeAffine(src, dst);
        inverseHomography_ = math::Homography::invert(homography_);
    } else {
        homography_        = math::Homography::compute(inputCorners_, outputCorners_);
        inverseHomography_ = math::Homography::invert(homography_);
    }
}

} // namespace layers

