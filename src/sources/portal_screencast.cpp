#include "sources/portal_screencast.hpp"
#include <iostream>

#ifdef HAVE_PORTAL
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#endif

namespace sources {

#ifdef HAVE_PORTAL

// ─── Internal helpers ─────────────────────────────────────────────────────

/**
 * Convert the D-Bus unique bus name (e.g. ":1.234") to a form safe for use
 * in object paths: strip leading ':', replace '.' with '_'.
 * Result: "1_234"
 */
static std::string senderComponent(GDBusConnection* conn) {
    const char* name = g_dbus_connection_get_unique_name(conn);
    std::string s(name ? name : "");
    if (!s.empty() && s[0] == ':') s = s.substr(1);
    for (char& c : s) if (c == '.') c = '_';
    return s;
}

static std::string makeToken() {
    static uint32_t n = 0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "cm%04u%08x",
                  ++n, static_cast<uint32_t>(g_random_int()));
    return buf;
}

// ─── Signal wait infrastructure ───────────────────────────────────────────

struct ResponseState {
    GMainLoop* loop    = nullptr;
    uint32_t   code    = 1;       // 1 = cancelled by default
    GVariant*  results = nullptr;
};

static void onPortalResponse(GDBusConnection*, const gchar*, const gchar*,
                               const gchar*, const gchar*,
                               GVariant* params, gpointer data) {
    auto* rs = static_cast<ResponseState*>(data);
    g_variant_get(params, "(u@a{sv})", &rs->code, &rs->results);
    g_main_loop_quit(rs->loop);
}

/**
 * Calls an XDG ScreenCast portal method and waits for the Response signal.
 *
 * @param conn         Dedicated D-Bus connection (must use the caller's ctx).
 * @param loop         GMainLoop bound to the same context.
 * @param sender       Our bus-name path component (see senderComponent()).
 * @param method       Portal method name (e.g. "CreateSession").
 * @param prefixParams GVariant tuple with args that come BEFORE the options
 *                     dict.  Pass nullptr when there are no prefix args.
 *                     The function takes ownership (unref's it).
 * @param opts         GVariantBuilder for the options a{sv}.
 *                     handle_token is injected before calling g_variant_builder_end.
 * @return             a{sv} results dict on success (code == 0).
 *                     Caller must g_variant_unref.  Returns nullptr on failure.
 */
static GVariant* portalCall(
    GDBusConnection*   conn,
    GMainLoop*         loop,
    const std::string& sender,
    const char*        method,
    GVariant*          prefixParams,   // consumed
    GVariantBuilder&   opts)
{
    // Inject handle_token — the request object path is deterministic.
    std::string token   = makeToken();
    std::string reqPath = "/org/freedesktop/portal/desktop/request/"
                          + sender + "/" + token;
    g_variant_builder_add(&opts, "{sv}", "handle_token",
                          g_variant_new_string(token.c_str()));

    // Subscribe to Response BEFORE calling the method to avoid any race.
    ResponseState rs;
    rs.loop = loop;
    guint sub = g_dbus_connection_signal_subscribe(
        conn,
        "org.freedesktop.portal.Desktop",
        "org.freedesktop.portal.Request",
        "Response",
        reqPath.c_str(),
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        onPortalResponse, &rs, nullptr);

    // Build full call parameters: (prefix_args..., options_a{sv})
    GVariantBuilder tup;
    g_variant_builder_init(&tup, G_VARIANT_TYPE_TUPLE);
    if (prefixParams) {
        GVariantIter iter;
        g_variant_iter_init(&iter, prefixParams);
        GVariant* child;
        while ((child = g_variant_iter_next_value(&iter)) != nullptr) {
            g_variant_builder_add_value(&tup, child);
            g_variant_unref(child);
        }
        g_variant_unref(prefixParams);
    }
    g_variant_builder_add_value(&tup, g_variant_builder_end(&opts));
    GVariant* callParams = g_variant_builder_end(&tup);

    GError*  err = nullptr;
    GVariant* ret = g_dbus_connection_call_sync(
        conn,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.ScreenCast",
        method,
        callParams,
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        30000,   // 30 s — user might take time in the picker
        nullptr,
        &err);

    if (err) {
        std::cerr << "Portal::" << method << " error: " << err->message << "\n";
        g_error_free(err);
        g_dbus_connection_signal_unsubscribe(conn, sub);
        return nullptr;
    }
    if (ret) g_variant_unref(ret);

    // Block until the Response signal fires
    g_main_loop_run(loop);
    g_dbus_connection_signal_unsubscribe(conn, sub);

    if (rs.code != 0) {
        std::cerr << "Portal::" << method << " cancelled (code=" << rs.code << ")\n";
        if (rs.results) g_variant_unref(rs.results);
        return nullptr;
    }
    return rs.results;  // caller must g_variant_unref
}

#endif // HAVE_PORTAL

// ─── Public API ───────────────────────────────────────────────────────────

PortalScreenCastResult runPortalScreenCast() {
#ifdef HAVE_PORTAL
    // Create a dedicated GMainContext so our GMainLoop runs on this thread
    // and signal subscriptions are dispatched here (not on the main thread).
    GMainContext* ctx = g_main_context_new();
    g_main_context_push_thread_default(ctx);
    GMainLoop* loop = g_main_loop_new(ctx, FALSE);

    // Create a *fresh* D-Bus connection that uses our ctx.
    // Using the shared g_bus_get_sync() connection would dispatch signals
    // on the main thread's context, breaking our GMainLoop wait.
    GError*  err     = nullptr;
    gchar*   busAddr = g_dbus_address_get_for_bus_sync(
                           G_BUS_TYPE_SESSION, nullptr, &err);
    if (!busAddr) {
        std::cerr << "Portal: cannot get session bus address: "
                  << (err ? err->message : "?") << "\n";
        if (err) g_error_free(err);
        g_main_loop_unref(loop);
        g_main_context_pop_thread_default(ctx);
        g_main_context_unref(ctx);
        return {};
    }

    GDBusConnection* conn = g_dbus_connection_new_for_address_sync(
        busAddr,
        static_cast<GDBusConnectionFlags>(
            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
            G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
        nullptr, nullptr, &err);
    g_free(busAddr);

    if (!conn) {
        std::cerr << "Portal: D-Bus connect failed: "
                  << (err ? err->message : "?") << "\n";
        if (err) g_error_free(err);
        g_main_loop_unref(loop);
        g_main_context_pop_thread_default(ctx);
        g_main_context_unref(ctx);
        return {};
    }

    std::string sender = senderComponent(conn);

    std::string sessionHandle;
    uint32_t    nodeId = 0;
    int         pwFd   = -1;
    bool        ok     = false;

    // ── Step 1: CreateSession ─────────────────────────────────────────────
    {
        std::string sessToken = makeToken();
        GVariantBuilder opts;
        g_variant_builder_init(&opts, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&opts, "{sv}", "session_handle_token",
                              g_variant_new_string(sessToken.c_str()));

        GVariant* res = portalCall(conn, loop, sender,
                                   "CreateSession", nullptr, opts);
        if (!res) goto done;

        // KDE portal returns session_handle as type 's'; GNOME returns 'o'.
        // Use nullptr type to accept either.
        GVariant* sessVar = g_variant_lookup_value(
            res, "session_handle", nullptr);
        if (!sessVar) { g_variant_unref(res); goto done; }
        sessionHandle = g_variant_get_string(sessVar, nullptr);
        g_variant_unref(sessVar);
        g_variant_unref(res);
    }

    // ── Step 2: SelectSources — shows the native OS screen/window picker ──
    {
        GVariantBuilder opts;
        g_variant_builder_init(&opts, G_VARIANT_TYPE_VARDICT);
        // types: bitmask — 1=monitor, 2=window,  3=both
        g_variant_builder_add(&opts, "{sv}", "types",
                              g_variant_new_uint32(3));
        // multiple: FALSE — one stream per source add
        g_variant_builder_add(&opts, "{sv}", "multiple",
                              g_variant_new_boolean(FALSE));
        // cursor_mode: 2=embedded in stream
        g_variant_builder_add(&opts, "{sv}", "cursor_mode",
                              g_variant_new_uint32(2));

        GVariant* res = portalCall(conn, loop, sender,
                                   "SelectSources",
                                   g_variant_new("(o)", sessionHandle.c_str()),
                                   opts);
        if (!res) goto done;
        g_variant_unref(res);
    }

    // ── Step 3: Start ─────────────────────────────────────────────────────
    {
        GVariantBuilder opts;
        g_variant_builder_init(&opts, G_VARIANT_TYPE_VARDICT);

        GVariant* res = portalCall(conn, loop, sender,
                                   "Start",
                                   g_variant_new("(os)", sessionHandle.c_str(), ""),
                                   opts);
        if (!res) goto done;

        // Parse streams: a(ua{sv}) — each entry is (node_id, properties)
        GVariant* streams = g_variant_lookup_value(
            res, "streams", G_VARIANT_TYPE("a(ua{sv})"));
        if (streams && g_variant_n_children(streams) > 0) {
            GVariant* first = g_variant_get_child_value(streams, 0);
            g_variant_get_child(first, 0, "u", &nodeId);
            g_variant_unref(first);
        }
        if (streams) g_variant_unref(streams);
        g_variant_unref(res);

        if (nodeId == 0) {
            std::cerr << "Portal Start: no streams in response\n";
            goto done;
        }
    }

    // ── Step 4: OpenPipeWireRemote — get the PipeWire socket FD ──────────
    {
        GVariantBuilder opts;
        g_variant_builder_init(&opts, G_VARIANT_TYPE_VARDICT);

        // This method returns (h) — a unix fd index — NOT via the request/response pattern.
        GVariant* params = g_variant_new("(o@a{sv})",
                                          sessionHandle.c_str(),
                                          g_variant_builder_end(&opts));
        err = nullptr;
        GUnixFDList* fdList = nullptr;
        GVariant* ret = g_dbus_connection_call_with_unix_fd_list_sync(
            conn,
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.ScreenCast",
            "OpenPipeWireRemote",
            params,
            G_VARIANT_TYPE("(h)"),
            G_DBUS_CALL_FLAGS_NONE,
            5000,
            nullptr,    // in_fd_list
            &fdList,
            nullptr,
            &err);

        if (!ret || err) {
            std::cerr << "Portal OpenPipeWireRemote: "
                      << (err ? err->message : "unknown") << "\n";
            if (err)    g_error_free(err);
            if (ret)    g_variant_unref(ret);
            if (fdList) g_object_unref(fdList);
            goto done;
        }

        gint idx = -1;
        g_variant_get(ret, "(h)", &idx);
        g_variant_unref(ret);

        if (fdList && idx >= 0) {
            err = nullptr;
            pwFd = g_unix_fd_list_get(fdList, idx, &err);
            if (err) { g_error_free(err); pwFd = -1; }
        }
        if (fdList) g_object_unref(fdList);

        if (pwFd < 0) {
            std::cerr << "Portal OpenPipeWireRemote: invalid fd\n";
            goto done;
        }

        ok = true;
    }

done:
    g_main_loop_unref(loop);
    g_main_context_pop_thread_default(ctx);
    g_main_context_unref(ctx);

    if (ok) {
        std::cerr << "Portal: got node=" << nodeId << " fd=" << pwFd << "\n";
        // Transfer D-Bus connection ownership to the result via shared_ptr.
        // The portal daemon will close the screencast session as soon as this
        // connection is released (g_object_unref'd), so the connection MUST
        // stay alive for the entire lifetime of the resulting PipeWireSource.
        PortalScreenCastResult r;
        r.success = true;
        r.nodeId  = nodeId;
        r.pwFd    = pwFd;
        r.portalSession = std::shared_ptr<void>(
            conn, [](void* p) { g_object_unref(static_cast<GDBusConnection*>(p)); });
        return r;
    }
    g_object_unref(conn);
    return {};
#else
    std::cerr << "runPortalScreenCast: built without HAVE_PORTAL\n";
    return {};
#endif
}

} // namespace sources
