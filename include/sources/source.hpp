#pragma once

#include "core/types.hpp"
#include <string>

namespace sources {

enum class SourceType {
    PIPEWIRE_STREAM,
    VIDEO_FILE,
    SHADER_REALTIME,
    IMAGE_FILE
};

/**
 * @brief Abstract base class for all media sources
 * 
 * Defines the interface for different input types (PipeWire, video, shader).
 * Subclasses implement specific backend logic.
 */
class Source {
public:
    virtual ~Source() = default;

    /**
     * @brief Initialize the source
     * @return true if initialization successful
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shutdown the source
     */
    virtual void shutdown() = 0;

    /**
     * @brief Update source (e.g., decode next frame, generate shader)
     * @return true if new data available
     */
    virtual bool update(float deltaTime) = 0;

    /**
     * @brief Get the OpenGL texture handle for current frame
     * @return OpenGL texture ID (0 if not available)
     */
    virtual unsigned int getTextureHandle() const = 0;

    /**
     * @brief Get source resolution
     */
    virtual Vec2 getResolution() const = 0;

    /**
     * @brief Check if source is valid/ready
     */
    virtual bool isValid() const = 0;

    /**
     * @brief Get source type
     */
    virtual SourceType getType() const = 0;

    /**
     * @brief Get descriptive name of this source
     */
    virtual std::string getName() const = 0;
};

} // namespace sources

