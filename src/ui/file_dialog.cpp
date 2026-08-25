#include "ui/file_dialog.hpp"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

namespace ui {

namespace {

/**
 * Wraps a string in single quotes for safe use in a /bin/sh command line.
 * Embedded single quotes are escaped by closing the quote, emitting an
 * escaped quote, and reopening: don't -> 'don'\''t'.
 *
 * Every caller currently passes a literal, but these strings feed popen() —
 * one future caller passing a filename or a translated title would otherwise
 * turn a dialog label into arbitrary shell execution.
 */
std::string shellQuote(const char* s) {
    std::string out = "'";
    for (const char* p = s ? s : ""; *p; ++p) {
        if (*p == '\'') out += "'\\''";
        else            out += *p;
    }
    out += '\'';
    return out;
}

/// Reads the first line of a dialog's stdout, trimming the trailing newline.
bool readDialogLine(FILE* fp, char* out, size_t outSize) {
    if (!std::fgets(out, static_cast<int>(outSize), fp)) {
        out[0] = '\0';
        return false;
    }
    size_t n = strlen(out);
    if (n && out[n - 1] == '\n') out[n - 1] = '\0';
    return out[0] != '\0';
}

} // namespace

std::string openFileDialog(const char* title, const char* filter) {
    // Build space-separated glob patterns from semicolon list, e.g. "*.png *.jpg"
    std::string patterns;
    if (filter && filter[0] != '\0') {
        std::istringstream iss(filter);
        std::string ext;
        while (std::getline(iss, ext, ';')) {
            if (ext.empty()) continue;
            if (!patterns.empty()) patterns += ' ';
            patterns += "*.";
            patterns += ext;
        }
    }

    char path[1024] = {};
    const std::string qTitle    = shellQuote(title);
    const std::string qPatterns = shellQuote(patterns.c_str());

    // --- Try zenity (GNOME / most major desktops) ---
    std::string cmd = "zenity --file-selection --title=" + qTitle;
    if (!patterns.empty()) cmd += " --file-filter=" + qPatterns;
    cmd += " 2>/dev/null";

    if (FILE* fp = popen(cmd.c_str(), "r")) {
        bool got = readDialogLine(fp, path, sizeof(path));
        int rc = pclose(fp);
        if (rc == 0 && got) return path;
        path[0] = '\0';
    }

    // --- Fall back to kdialog (KDE) ---
    cmd = "kdialog --getopenfilename .";
    if (!patterns.empty()) cmd += " " + qPatterns;
    cmd += " 2>/dev/null";

    if (FILE* fp = popen(cmd.c_str(), "r")) {
        bool got = readDialogLine(fp, path, sizeof(path));
        int rc = pclose(fp);
        if (rc == 0 && got) return path;
    }

    return {};
}

std::string saveFileDialog(const char* title, const char* defaultExt) {
    char path[1024] = {};
    const std::string ext = defaultExt ? defaultExt : "";

    // --- zenity ---
    std::string cmd = "zenity --file-selection --save --confirm-overwrite"
                      " --title=" + shellQuote(title) +
                      " --filename=" + shellQuote(("layout." + ext).c_str()) +
                      " 2>/dev/null";
    if (FILE* fp = popen(cmd.c_str(), "r")) {
        bool got = readDialogLine(fp, path, sizeof(path));
        int rc = pclose(fp);
        if (rc == 0 && got) return path;
        path[0] = '\0';
    }

    // --- kdialog ---
    cmd = "kdialog --getsavefilename . " + shellQuote(("*." + ext).c_str()) +
          " 2>/dev/null";
    if (FILE* fp = popen(cmd.c_str(), "r")) {
        bool got = readDialogLine(fp, path, sizeof(path));
        int rc = pclose(fp);
        if (rc == 0 && got) return path;
    }

    return {};
}

} // namespace ui
