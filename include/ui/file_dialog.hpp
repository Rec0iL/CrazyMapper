#pragma once

#include <string>

namespace ui {

/**
 * @brief Open a native file selection dialog (blocking).
 *
 * Uses zenity on GNOME or kdialog on KDE.  Blocks until the user
 * confirms or cancels.
 *
 * @param title   Dialog window title.
 * @param filter  Semicolon-separated extensions without dots, e.g. "png;jpg;jpeg".
 *                Ignored by kdialog fall-back (all files shown).
 * @return        Absolute path selected by the user, or empty string if cancelled.
 */
std::string openFileDialog(const char* title, const char* filter = "");

} // namespace ui
