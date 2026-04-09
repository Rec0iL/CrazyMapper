#include "layers/layer.hpp"
#include "layers/rounded_rectangle_shape.hpp"
#include "layers/polygon_shape.hpp"
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
      inputCorners_({Vec2(0.f,0.f), Vec2(1.f,0.f), Vec2(1.f,1.f), Vec2(0.f,1.f)}),
      outputCorners_({Vec2(0.f,0.f), Vec2(1.f,0.f), Vec2(1.f,1.f), Vec2(0.f,1.f)}),
      homography_(glm::mat3(1.0f)),
      inverseHomography_(glm::mat3(1.0f)) {
}

Layer::~Layer() {
}

void Layer::setShape(Unique<Shape> shape) {
    shape_ = std::move(shape);
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

void Layer::setInputCorner(int cornerIndex, const Vec2& position) {
    if (cornerIndex >= 0 && cornerIndex < 4) {
        inputCorners_[cornerIndex] = position;
        updateHomography();
    }
}

void Layer::resetInputCorners() {
    inputCorners_ = {Vec2(0.f,0.f), Vec2(1.f,0.f), Vec2(1.f,1.f), Vec2(0.f,1.f)};
    updateHomography();
}

void Layer::setOutputCorner(int cornerIndex, const Vec2& position) {
    if (cornerIndex >= 0 && cornerIndex < 4) {
        outputCorners_[cornerIndex] = position;
        updateHomography();
    }
}

void Layer::resetOutputCorners() {
    // Default: full canvas span [0..1]
    outputCorners_ = {Vec2(0.f,0.f), Vec2(1.f,0.f), Vec2(1.f,1.f), Vec2(0.f,1.f)};
    updateHomography();
}

void Layer::updateHomography() {
    homography_        = math::Homography::compute(inputCorners_, outputCorners_);
    inverseHomography_ = math::Homography::invert(homography_);
}

} // namespace layers

