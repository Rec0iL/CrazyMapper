#include "sources/video_file_source.hpp"
#include <glad/glad.h>
#include <iostream>

namespace sources {

VideoFileSource::VideoFileSource(const std::string& filePath)
    : filePath_(filePath) {}

VideoFileSource::~VideoFileSource() {
    shutdown();
}

bool VideoFileSource::initialize() {
    if (initialized_) return true;

#ifdef HAVE_GSTREAMER
    // Build pipeline: set filesrc name so we can set location as a property
    // (avoids embedding the path in a parse-launch string).
    GError* error = nullptr;
    pipeline_ = gst_parse_launch(
        "filesrc name=src ! decodebin ! videoconvert ! "
        "videoflip method=vertical-flip ! "
        "video/x-raw,format=RGBA ! "
        "appsink name=sink sync=true max-buffers=1 drop=true",
        &error);

    if (!pipeline_ || error) {
        std::cerr << "VideoFileSource: pipeline error: "
                  << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        return false;
    }

    // Set filesrc location as a property to avoid shell-string escaping issues
    GstElement* filesrc = gst_bin_get_by_name(GST_BIN(pipeline_), "src");
    if (!filesrc) {
        std::cerr << "VideoFileSource: could not get filesrc element" << std::endl;
        gst_object_unref(pipeline_); pipeline_ = nullptr;
        return false;
    }
    g_object_set(filesrc, "location", filePath_.c_str(), nullptr);
    gst_object_unref(filesrc);

    appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
    if (!appsink_) {
        std::cerr << "VideoFileSource: could not get appsink element" << std::endl;
        gst_object_unref(pipeline_); pipeline_ = nullptr;
        return false;
    }

    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "VideoFileSource: failed to start pipeline for: "
                  << filePath_ << std::endl;
        gst_object_unref(appsink_); appsink_ = nullptr;
        gst_object_unref(pipeline_); pipeline_ = nullptr;
        return false;
    }

    // Wait up to 2 s for the pipeline to preroll (so caps are negotiated)
    gst_element_get_state(pipeline_, nullptr, nullptr, 2 * GST_SECOND);

    // Query duration
    gint64 dur = GST_CLOCK_TIME_NONE;
    if (gst_element_query_duration(pipeline_, GST_FORMAT_TIME, &dur) &&
        GST_CLOCK_TIME_IS_VALID(dur)) {
        duration_ = static_cast<float>(dur) / 1.0e9f;
    }

    // Create placeholder GL texture (size updated on first frame)
    glGenTextures(1, &textureHandle_);
    glBindTexture(GL_TEXTURE_2D, textureHandle_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Allocate 1×1 transparent so the texture is complete before the first frame
    static const unsigned char kBlack[4] = {0, 0, 0, 0};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, kBlack);
    glBindTexture(GL_TEXTURE_2D, 0);

    initialized_ = true;
    return true;
#else
    std::cerr << "VideoFileSource: built without GStreamer support." << std::endl;
    return false;
#endif
}

void VideoFileSource::shutdown() {
    if (!initialized_) return;

#ifdef HAVE_GSTREAMER
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        if (appsink_) {
            gst_object_unref(appsink_);
            appsink_ = nullptr;
        }
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
#endif

    if (textureHandle_) {
        glDeleteTextures(1, &textureHandle_);
        textureHandle_ = 0;
    }
    initialized_ = false;
}

bool VideoFileSource::update(float /* deltaTime */) {
    if (!initialized_) return false;

#ifdef HAVE_GSTREAMER
    if (!appsink_) return false;

    // Seamless loop: seek back to the beginning when the stream ends
    GstBus* bus = gst_element_get_bus(pipeline_);
    if (bus) {
        GstMessage* msg = gst_bus_pop_filtered(bus, GST_MESSAGE_EOS);
        if (msg) {
            gst_message_unref(msg);
            gst_element_seek_simple(
                pipeline_, GST_FORMAT_TIME,
                static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                0);
            currentTime_ = 0.0f;
        }
        gst_object_unref(bus);
    }

    // Non-blocking pull: returns nullptr if no new frame is ready
    GstSample* sample = gst_app_sink_try_pull_sample(
        GST_APP_SINK(appsink_), 0);
    if (!sample) return false;

    GstCaps* caps = gst_sample_get_caps(sample);
    if (caps) {
        GstStructure* s = gst_caps_get_structure(caps, 0);
        gint w = 0, h = 0;
        gst_structure_get_int(s, "width",  &w);
        gst_structure_get_int(s, "height", &h);

        if (w > 0 && h > 0) {
            resolution_ = Vec2(static_cast<float>(w), static_cast<float>(h));

            GstBuffer*  buf = gst_sample_get_buffer(sample);
            GstMapInfo  map;
            if (gst_buffer_map(buf, &map, GST_MAP_READ)) {
                // Update playback position
                gint64 pos = GST_CLOCK_TIME_NONE;
                if (gst_element_query_position(pipeline_, GST_FORMAT_TIME, &pos) &&
                    GST_CLOCK_TIME_IS_VALID(pos)) {
                    currentTime_ = static_cast<float>(pos) / 1.0e9f;
                }

                glBindTexture(GL_TEXTURE_2D, textureHandle_);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                             w, h, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, map.data);
                glBindTexture(GL_TEXTURE_2D, 0);

                gst_buffer_unmap(buf, &map);
                gst_sample_unref(sample);
                return true;
            }
        }
    }

    gst_sample_unref(sample);
#endif
    return false;
}

std::string VideoFileSource::getName() const {
    return "Video: " + filePath_;
}

void VideoFileSource::seek(float seconds) {
#ifdef HAVE_GSTREAMER
    if (pipeline_) {
        currentTime_ = seconds;
        gst_element_seek_simple(
            pipeline_, GST_FORMAT_TIME,
            static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
            static_cast<gint64>(seconds * 1.0e9));
    }
#else
    (void)seconds;
#endif
}

} // namespace sources

