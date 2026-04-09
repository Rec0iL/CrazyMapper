#include "sources/pipewire_node_enumerator.hpp"

#ifdef HAVE_PIPEWIRE
#include <pipewire/pipewire.h>
#include <spa/utils/string.h>
#include <cstring>
#include <iostream>
#endif

namespace sources {

#ifdef HAVE_PIPEWIRE

// ─── Internal state for the one-shot enumeration ──────────────────────────

struct EnumData {
    pw_main_loop*              loop     = nullptr;
    std::vector<PipeWireNodeInfo> nodes;
    spa_hook                   regHook  = {};
    spa_hook                   coreHook = {};
};

static void onRegistryGlobal(void* data,
                              uint32_t id,
                              uint32_t /*permissions*/,
                              const char* type,
                              uint32_t /*version*/,
                              const struct spa_dict* props) {
    if (!spa_streq(type, PW_TYPE_INTERFACE_Node) || !props) return;

    const char* mediaClass = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if (!mediaClass) return;

    // Accept any node whose media.class contains "Video" (case-sensitive as
    // PipeWire itself uses "Video", "video" only for legacy nodes)
    if (!strstr(mediaClass, "Video") && !strstr(mediaClass, "video")) return;

    auto* ed = static_cast<EnumData*>(data);

    PipeWireNodeInfo info;
    info.id         = id;
    info.mediaClass = mediaClass;

    // Prefer human-readable description, fall back to node name
    const char* desc = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
    const char* name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    info.name = desc ? desc : name ? name : ("Node #" + std::to_string(id));

    ed->nodes.push_back(std::move(info));
}

static void onCoreDone(void* data, uint32_t id, int /*seq*/) {
    // Fires after all current registry objects have been dispatched
    if (id == PW_ID_CORE)
        pw_main_loop_quit(static_cast<EnumData*>(data)->loop);
}

#endif // HAVE_PIPEWIRE

// ─── Public API ───────────────────────────────────────────────────────────

std::vector<PipeWireNodeInfo> enumeratePipeWireVideoNodes() {
#ifdef HAVE_PIPEWIRE
    EnumData ed;

    ed.loop = pw_main_loop_new(nullptr);
    if (!ed.loop) {
        std::cerr << "PW enumerator: failed to create main loop\n";
        return {};
    }

    pw_context* ctx = pw_context_new(pw_main_loop_get_loop(ed.loop), nullptr, 0);
    if (!ctx) {
        pw_main_loop_destroy(ed.loop);
        return {};
    }

    pw_core* core = pw_context_connect(ctx, nullptr, 0);
    if (!core) {
        pw_context_destroy(ctx);
        pw_main_loop_destroy(ed.loop);
        return {};
    }

    // Register core listener to detect roundtrip completion
    pw_core_events coreEvents = {};
    coreEvents.version = PW_VERSION_CORE_EVENTS;
    coreEvents.done    = onCoreDone;
    pw_core_add_listener(core, &ed.coreHook, &coreEvents, &ed);

    // Register registry listener to collect nodes
    pw_registry* registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
    pw_registry_events regEvents = {};
    regEvents.version = PW_VERSION_REGISTRY_EVENTS;
    regEvents.global  = onRegistryGlobal;
    pw_registry_add_listener(registry, &ed.regHook, &regEvents, &ed);

    // Send a roundtrip marker: the "done" callback fires after all existing
    // registry objects have been dispatched to our listener above.
    pw_core_sync(core, PW_ID_CORE, 0);

    // Block until the roundtrip completes (usually < 10 ms)
    pw_main_loop_run(ed.loop);

    // ── Cleanup ──
    spa_hook_remove(&ed.regHook);
    spa_hook_remove(&ed.coreHook);
    pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry));
    pw_core_disconnect(core);
    pw_context_destroy(ctx);
    pw_main_loop_destroy(ed.loop);

    return ed.nodes;
#else
    return {};
#endif
}

} // namespace sources
