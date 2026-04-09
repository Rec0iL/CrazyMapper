#pragma once

#include <cstdint>
#include <memory>

namespace sources {

/**
 * @brief Result of an XDG ScreenCast portal session.
 *
 * On success, nodeId is the PipeWire node for the selected screen/window, and
 * pwFd is an open file descriptor for the portal's PipeWire remote.  Pass both
 * to PipeWireSource(nodeId, pwFd).  The fd is owned by the PipeWire context
 * created inside PipeWireSource — do NOT close it manually.
 *
 * portalSession holds the D-Bus connection alive.  The portal daemon closes
 * the screencast session when the connection drops, so this must stay alive
 * for the entire lifetime of the resulting PipeWireSource.
 */
struct PortalScreenCastResult {
    bool                  success       = false;
    uint32_t              nodeId        = 0;
    int                   pwFd          = -1;
    std::shared_ptr<void> portalSession;  // GDBusConnection* — keeps session alive
};

/**
 * @brief Opens the OS native screen/window picker dialog via the XDG Desktop
 *        Portal and returns the selected stream as a PipeWire node.
 *
 * Blocking.  Designed to be called on a dedicated worker thread.
 * The native dialog (supplied by the compositor — GNOME, KDE, wlroots …) will
 * appear on top while the user makes their selection.
 *
 * Returns an empty result if:
 *  - built without HAVE_PORTAL
 *  - the portal daemon is not reachable (no xdg-desktop-portal process)
 *  - the user cancels the dialog
 */
PortalScreenCastResult runPortalScreenCast();

} // namespace sources
