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

/**
 * @brief Open a native save-file dialog (blocking).
 *
 * @param title       Dialog window title.
 * @param defaultExt  Default file extension (without dot), e.g. "cml".
 * @return            Absolute path chosen by the user, or empty if cancelled.
 */
std::string saveFileDialog(const char* title, const char* defaultExt = "cml");

} // namespace ui
