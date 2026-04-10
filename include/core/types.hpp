#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <string>
#include <vector>

// Basic type aliases for clarity
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;

// Smart pointers
template <typename T>
using Unique = std::unique_ptr<T>;

template <typename T>
using Shared = std::shared_ptr<T>;

template <typename T>
using Weak = std::weak_ptr<T>;

/**
 * @brief Configuration for a single projector canvas.
 */
struct CanvasConfig {
    std::string name     = "Canvas 1";
    float       aspectW  = 16.0f;
    float       aspectH  =  9.0f;

    float getAspectRatio() const {
        return aspectH > 0.0f ? aspectW / aspectH : 16.0f / 9.0f;
    }
};

