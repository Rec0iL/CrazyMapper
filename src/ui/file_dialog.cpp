#include "ui/file_dialog.hpp"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

namespace ui {

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

    char cmd[1024];
    char path[1024] = {};

    // --- Try zenity (GNOME / most major desktops) ---
    if (!patterns.empty())
        snprintf(cmd, sizeof(cmd),
                 "zenity --file-selection --title='%s'"
                 " --file-filter='%s' 2>/dev/null",
                 title, patterns.c_str());
    else
        snprintf(cmd, sizeof(cmd),
                 "zenity --file-selection --title='%s' 2>/dev/null",
                 title);

    if (FILE* fp = popen(cmd, "r")) {
        fgets(path, static_cast<int>(sizeof(path)) - 1, fp);
        int rc = pclose(fp);
        if (rc == 0 && path[0] != '\0') {
            size_t n = strlen(path);
            if (n && path[n - 1] == '\n') path[n - 1] = '\0';
            return path;
        }
        memset(path, 0, sizeof(path));
    }

    // --- Fall back to kdialog (KDE) ---
    if (!patterns.empty())
        snprintf(cmd, sizeof(cmd),
                 "kdialog --getopenfilename . '%s' 2>/dev/null",
                 patterns.c_str());
    else
        snprintf(cmd, sizeof(cmd),
                 "kdialog --getopenfilename . 2>/dev/null");

    if (FILE* fp = popen(cmd, "r")) {
        fgets(path, static_cast<int>(sizeof(path)) - 1, fp);
        int rc = pclose(fp);
        if (rc == 0 && path[0] != '\0') {
            size_t n = strlen(path);
            if (n && path[n - 1] == '\n') path[n - 1] = '\0';
            return path;
        }
    }

    return {};
}

} // namespace ui
