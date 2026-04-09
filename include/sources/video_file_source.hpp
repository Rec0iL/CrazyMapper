#pragma once

#include "source.hpp"

#ifdef HAVE_GSTREAMER
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#endif

namespace sources {

/**
 * @brief Video file source backed by GStreamer.
 *
 * Decodes any GStreamer-supported video format and uploads each frame
 * to an OpenGL RGBA texture via appsink.
 */
class VideoFileSource : public Source {
public:
    explicit VideoFileSource(const std::string& filePath);
    ~VideoFileSource() override;

    bool initialize() override;
    void shutdown() override;
    bool update(float deltaTime) override;

    unsigned int getTextureHandle() const override { return textureHandle_; }
    Vec2         getResolution()    const override { return resolution_; }
    bool         isValid()          const override { return initialized_ && textureHandle_ != 0; }
    SourceType   getType()          const override { return SourceType::VIDEO_FILE; }
    std::string  getName()          const override;

    void  seek(float seconds);
    float getDuration()    const { return duration_; }
    float getCurrentTime() const { return currentTime_; }
    const std::string& getFilePath() const { return filePath_; }

private:
    std::string  filePath_;
    unsigned int textureHandle_ = 0;
    Vec2         resolution_    = {};
    float        duration_      = 0.0f;
    float        currentTime_   = 0.0f;
    bool         initialized_   = false;

#ifdef HAVE_GSTREAMER
    GstElement* pipeline_ = nullptr;
    GstElement* appsink_  = nullptr;
#endif
};

} // namespace sources


