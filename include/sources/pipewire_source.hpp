#pragma once

#include "source.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifdef HAVE_PIPEWIRE
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>
#endif

namespace sources {

/**
 * @brief PipeWire video stream source.
 *
 * Connects to a PipeWire node (screen share, camera, portal stream, etc.)
 * and uploads each decoded RGBA/BGRA frame to an OpenGL texture.
 *
 * @param targetNodeId  PipeWire node ID to connect to.
 *                      0xFFFFFFFF (UINT32_MAX / PW_ID_ANY) = auto-connect
 *                      to the first available compatible video source.
 */
class PipeWireSource : public Source {
public:
    /**
     * @param targetNodeId  PipeWire node ID.  0xFFFFFFFF = PW_ID_ANY (auto).
     * @param portalFd      FD returned by the XDG ScreenCast portal's
     *                      OpenPipeWireRemote.  -1 = connect normally.
     *                      The FD is owned by this object; PipeWire takes
     *                      it when pw_context_connect_fd is called.
     */
    explicit PipeWireSource(uint32_t              targetNodeId   = 0xFFFFFFFFu,
                            int                   portalFd       = -1,
                            std::shared_ptr<void> portalSession  = nullptr);
    ~PipeWireSource() override;

    bool         initialize()                  override;
    void         shutdown()                    override;
    bool         update(float deltaTime)       override;

    unsigned int getTextureHandle() const override { return textureHandle_; }
    Vec2         getResolution()    const override {
        return Vec2(static_cast<float>(resWidth_.load(std::memory_order_relaxed)),
                    static_cast<float>(resHeight_.load(std::memory_order_relaxed)));
    }
    bool         isValid()          const override { return initialized_ && textureHandle_ != 0; }
    SourceType   getType()          const override { return SourceType::PIPEWIRE_STREAM; }
    std::string  getName()          const override;

private:
    uint32_t              targetNodeId_;
    int                   portalFd_      = -1;  // -1 = normal connect, >=0 = portal FD
    bool                  portalFdOwned_ = false;  // false once PipeWire takes it
    std::shared_ptr<void> portalSession_;        // keeps D-Bus conn alive (portal streams)

    /// Closes portalFd_ if we still own it. Safe to call more than once.
    void closePortalFd();
    unsigned int textureHandle_ = 0;
    bool         initialized_   = false;

    // Written on the PipeWire thread during format negotiation, read on the
    // render thread via getResolution(). Atomic rather than a plain Vec2 so
    // the read is not a data race.
    std::atomic<int> resWidth_{0};
    std::atomic<int> resHeight_{0};

#ifdef HAVE_PIPEWIRE
    pw_thread_loop*    loop_     = nullptr;
    pw_context*        context_  = nullptr;
    pw_core*           core_     = nullptr;
    pw_stream*         stream_   = nullptr;
    pw_stream_events   streamEvents_ = {};
    spa_hook           streamHook_   = {};

    // Negotiated format. Touched only on the PipeWire thread
    // (onParamChanged / onProcess), never by the render thread.
    spa_video_info_raw videoInfo_ = {};
    int                frameWidth_  = 0;
    int                frameHeight_ = 0;
    unsigned int       glFormat_    = 0x1908; // GL_RGBA

    // The staged frame and the geometry it was captured with. These travel
    // together under frameMutex_: a format renegotiation between capture and
    // upload would otherwise have update() read pendingFrame_ with mismatched
    // dimensions and run off the end of the buffer.
    std::mutex           frameMutex_;
    std::vector<uint8_t> pendingFrame_;
    int                  pendingWidth_  = 0;
    int                  pendingHeight_ = 0;
    unsigned int         pendingFormat_ = 0x1908; // GL_RGBA
    std::atomic<bool>    hasNewFrame_{false};

    static void onProcessStatic(void* data);
    static void onParamChangedStatic(void* data, uint32_t id, const struct spa_pod* param);
    void onProcess();
    void onParamChanged(uint32_t id, const struct spa_pod* param);
#endif
};

} // namespace sources


