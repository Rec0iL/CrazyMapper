#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sources {

/**
 * @brief Metadata for one discovered PipeWire video node.
 */
struct PipeWireNodeInfo {
    uint32_t    id;          ///< PipeWire node ID
    std::string name;        ///< Human-readable name (description or node name)
    std::string mediaClass;  ///< e.g. "Video/Source", "Stream/Output/Video"
};

/**
 * @brief Enumerate all currently available PipeWire video nodes.
 *
 * Opens a short-lived PipeWire connection, performs a registry roundtrip
 * to collect all nodes whose media.class contains "Video" or "video",
 * then disconnects.  Blocks for at most a few hundred milliseconds.
 *
 * Returns an empty vector when built without HAVE_PIPEWIRE or when the
 * PipeWire daemon is not reachable.
 */
std::vector<PipeWireNodeInfo> enumeratePipeWireVideoNodes();

} // namespace sources
