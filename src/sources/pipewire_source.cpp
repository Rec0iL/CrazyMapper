#include "sources/pipewire_source.hpp"
#include <glad/glad.h>
#include <cstring>
#include <iostream>
#include <string>

namespace sources {

PipeWireSource::PipeWireSource(uint32_t targetNodeId, int portalFd,
                               std::shared_ptr<void> portalSession)
    : targetNodeId_(targetNodeId),
      portalFd_(portalFd),
      portalSession_(std::move(portalSession)) {}

PipeWireSource::~PipeWireSource() {
    shutdown();
}

bool PipeWireSource::initialize() {
    if (initialized_) return true;

#ifdef HAVE_PIPEWIRE
    // Initialise the stream events struct (must outlive the listener registration)
    std::memset(&streamEvents_, 0, sizeof(streamEvents_));
    streamEvents_.version       = PW_VERSION_STREAM_EVENTS;
    streamEvents_.process       = &PipeWireSource::onProcessStatic;
    streamEvents_.param_changed = &PipeWireSource::onParamChangedStatic;

    loop_ = pw_thread_loop_new("CrazyMapper-PW", nullptr);
    if (!loop_) {
        std::cerr << "PipeWireSource: failed to create thread loop\n";
        return false;
    }

    pw_thread_loop_lock(loop_);

    context_ = pw_context_new(pw_thread_loop_get_loop(loop_), nullptr, 0);
    if (!context_) {
        std::cerr << "PipeWireSource: failed to create context\n";
        pw_thread_loop_unlock(loop_);
        pw_thread_loop_destroy(loop_); loop_ = nullptr;
        return false;
    }

    core_ = (portalFd_ >= 0)
        ? pw_context_connect_fd(context_, portalFd_, nullptr, 0)
        : pw_context_connect(context_, nullptr, 0);
    if (!core_) {
        std::cerr << "PipeWireSource: failed to connect to PipeWire daemon\n";
        pw_context_destroy(context_); context_ = nullptr;
        pw_thread_loop_unlock(loop_);
        pw_thread_loop_destroy(loop_); loop_ = nullptr;
        return false;
    }

    stream_ = pw_stream_new(core_, "CrazyMapper Capture",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE,     "Video",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE,     "Screen",
            nullptr));
    if (!stream_) {
        std::cerr << "PipeWireSource: failed to create stream\n";
        pw_core_disconnect(core_); core_ = nullptr;
        pw_context_destroy(context_); context_ = nullptr;
        pw_thread_loop_unlock(loop_);
        pw_thread_loop_destroy(loop_); loop_ = nullptr;
        return false;
    }

    pw_stream_add_listener(stream_, &streamHook_, &streamEvents_, this);

    // Format negotiation: offer RGBA (preferred for GL) and BGRA (common in screen share)
    struct spa_rectangle defSize = { 1920, 1080 };
    struct spa_rectangle minSize = { 1, 1 };
    struct spa_rectangle maxSize = { 7680, 4320 };
    struct spa_fraction  defRate = { 30, 1 };
    struct spa_fraction  minRate = { 0, 1 };
    struct spa_fraction  maxRate = { 1000, 1 };

    uint8_t pbuf[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(pbuf, sizeof(pbuf));
    const struct spa_pod* params[1];
    params[0] = (const struct spa_pod*) spa_pod_builder_add_object(&b,
        SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
        SPA_FORMAT_mediaType,    SPA_POD_Id(SPA_MEDIA_TYPE_video),
        SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(3,
            SPA_VIDEO_FORMAT_RGBA,
            SPA_VIDEO_FORMAT_RGBA,
            SPA_VIDEO_FORMAT_BGRA),
        SPA_FORMAT_VIDEO_size,
            SPA_POD_CHOICE_RANGE_Rectangle(&defSize, &minSize, &maxSize),
        SPA_FORMAT_VIDEO_framerate,
            SPA_POD_CHOICE_RANGE_Fraction(&defRate, &minRate, &maxRate));

    pw_stream_connect(stream_,
        PW_DIRECTION_INPUT,
        targetNodeId_,
        static_cast<enum pw_stream_flags>(
            PW_STREAM_FLAG_AUTOCONNECT |
            PW_STREAM_FLAG_MAP_BUFFERS),
        params, 1);

    // Placeholder GL texture (overwritten on first frame)
    glGenTextures(1, &textureHandle_);
    glBindTexture(GL_TEXTURE_2D, textureHandle_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    static const unsigned char kBlack[4] = { 0, 0, 0, 0 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, kBlack);
    glBindTexture(GL_TEXTURE_2D, 0);

    pw_thread_loop_unlock(loop_);
    pw_thread_loop_start(loop_);

    initialized_ = true;
    return true;
#else
    std::cerr << "PipeWireSource: built without PipeWire support.\n";
    return false;
#endif
}

void PipeWireSource::shutdown() {
    if (!initialized_) return;
    initialized_ = false;

#ifdef HAVE_PIPEWIRE
    if (loop_) {
        pw_thread_loop_stop(loop_);
        if (stream_)  { pw_stream_destroy(stream_);      stream_  = nullptr; }
        if (core_)    { pw_core_disconnect(core_);       core_    = nullptr; }
        if (context_) { pw_context_destroy(context_);   context_ = nullptr; }
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;
    }
#endif

    if (textureHandle_) {
        glDeleteTextures(1, &textureHandle_);
        textureHandle_ = 0;
    }
}

bool PipeWireSource::update(float /*deltaTime*/) {
    if (!initialized_) return false;

#ifdef HAVE_PIPEWIRE
    if (!hasNewFrame_.load(std::memory_order_acquire)) return false;

    std::lock_guard<std::mutex> lock(frameMutex_);
    if (pendingFrame_.empty() || frameWidth_ <= 0 || frameHeight_ <= 0) {
        hasNewFrame_.store(false, std::memory_order_release);
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, textureHandle_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 frameWidth_, frameHeight_, 0,
                 glFormat_, GL_UNSIGNED_BYTE,
                 pendingFrame_.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    hasNewFrame_.store(false, std::memory_order_release);
    return true;
#else
    return false;
#endif
}

std::string PipeWireSource::getName() const {
    if (portalFd_ >= 0)
        return "Portal Stream: #" + std::to_string(targetNodeId_);
    if (targetNodeId_ == 0xFFFFFFFFu)
        return "PipeWire: auto";
    return "PipeWire: #" + std::to_string(targetNodeId_);
}

// ─── PipeWire callbacks ────────────────────────────────────────────────────

#ifdef HAVE_PIPEWIRE

void PipeWireSource::onProcessStatic(void* data) {
    static_cast<PipeWireSource*>(data)->onProcess();
}

void PipeWireSource::onParamChangedStatic(void* data, uint32_t id,
                                           const struct spa_pod* param) {
    static_cast<PipeWireSource*>(data)->onParamChanged(id, param);
}

void PipeWireSource::onProcess() {
    struct pw_buffer* buf = pw_stream_dequeue_buffer(stream_);
    if (!buf) return;

    struct spa_buffer* spaBuf = buf->buffer;
    if (!spaBuf || !spaBuf->datas[0].data) {
        pw_stream_queue_buffer(stream_, buf);
        return;
    }

    int w = frameWidth_;
    int h = frameHeight_;
    if (w <= 0 || h <= 0) {
        pw_stream_queue_buffer(stream_, buf);
        return;
    }

    size_t   expected = static_cast<size_t>(w * h * 4);
    uint32_t mapSize  = spaBuf->datas[0].chunk->size;
    if (mapSize < expected) {
        pw_stream_queue_buffer(stream_, buf);
        return;
    }

    const auto* src = static_cast<const uint8_t*>(spaBuf->datas[0].data);
    {
        // Flip rows vertically so the data matches the stb_image bottom-up
        // convention used by ImageFileSource; the perspective shader's
        // "1.0 - y" flip then produces the correctly oriented result.
        std::lock_guard<std::mutex> lock(frameMutex_);
        pendingFrame_.resize(expected);
        const int stride = w * 4;
        for (int row = 0; row < h; ++row) {
            std::memcpy(pendingFrame_.data() + row * stride,
                        src + (h - 1 - row) * stride,
                        stride);
        }
    }
    hasNewFrame_.store(true, std::memory_order_release);

    pw_stream_queue_buffer(stream_, buf);
}

void PipeWireSource::onParamChanged(uint32_t id, const struct spa_pod* param) {
    if (!param || id != SPA_PARAM_Format) return;
    if (spa_format_video_raw_parse(param, &videoInfo_) < 0) return;

    frameWidth_  = static_cast<int>(videoInfo_.size.width);
    frameHeight_ = static_cast<int>(videoInfo_.size.height);
    resolution_  = Vec2(frameWidth_, frameHeight_);

    // Select GL upload format matching the negotiated SPA pixel format
    glFormat_ = (videoInfo_.format == SPA_VIDEO_FORMAT_BGRA ||
                 videoInfo_.format == SPA_VIDEO_FORMAT_BGRx)
                ? 0x80E1  // GL_BGRA
                : 0x1908; // GL_RGBA

    // Tell PipeWire our buffer requirements
    uint8_t pbuf[256];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(pbuf, sizeof(pbuf));
    const struct spa_pod* params[1];
    params[0] = (const struct spa_pod*) spa_pod_builder_add_object(&b,
        SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
        SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(2, 1, 32),
        SPA_PARAM_BUFFERS_blocks,  SPA_POD_Int(1),
        SPA_PARAM_BUFFERS_size,    SPA_POD_Int(frameWidth_ * frameHeight_ * 4),
        SPA_PARAM_BUFFERS_stride,  SPA_POD_Int(frameWidth_ * 4));
    pw_stream_update_params(stream_, params, 1);
}

#endif // HAVE_PIPEWIRE

} // namespace sources


