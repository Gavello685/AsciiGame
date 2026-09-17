#include "platform/paths.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <vector>

namespace fs = std::filesystem;

namespace {

// getenv wrapper that treats an empty value the same as unset.
const char* env_or_null(const char* name) {
    const char* v = std::getenv(name);
    if (!v || v[0] == '\0') return nullptr;
    return v;
}

fs::path resolve_data_dir() {
    if (const char* override_dir = env_or_null("ASCII_GAME_DATA_DIR")) {
        return fs::path(override_dir);
    }

#ifdef _WIN32
    if (const char* appdata = env_or_null("APPDATA")) {
        return fs::path(appdata) / "AsciiGame";
    }
#else
    if (const char* xdg = env_or_null("XDG_DATA_HOME")) {
        return fs::path(xdg) / "AsciiGame";
    }
    if (const char* home = env_or_null("HOME")) {
        return fs::path(home) / ".local" / "share" / "AsciiGame";
    }
#endif

    return fs::path("AsciiGame");
}

// The bundled font directory, relative to wherever the game was started
// from. Running out of build/ is common enough to be worth walking up.
const char* const FONT_SEARCH_ROOTS[] = {
    "assets/fonts", "../assets/fonts", "../../assets/fonts",
};

const char* const SYSTEM_FONTS[] = {
#ifdef _WIN32
    "C:/Windows/Fonts/consola.ttf",
    "C:/Windows/Fonts/cour.ttf",
    "C:/Windows/Fonts/lucon.ttf",
#elif defined(__APPLE__)
    "/System/Library/Fonts/Menlo.ttc",
    "/System/Library/Fonts/Monaco.ttf",
    "/Library/Fonts/Courier New.ttf",
#else
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
    "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
    "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
#endif
};

// First .ttf in a directory, in sorted order so the choice is stable across
// runs rather than dependent on directory iteration order.
fs::path first_font_in(const fs::path& dir) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return {};

    std::vector<fs::path> fonts;
    for (const fs::directory_entry& entry : fs::directory_iterator(dir, ec)) {
        if (ec) return {};
        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext == ".ttf" || ext == ".otf" || ext == ".ttc") fonts.push_back(entry.path());
    }
    if (fonts.empty()) return {};

    std::sort(fonts.begin(), fonts.end());
    return fonts.front();
}

}

namespace platform {

fs::path user_data_dir() {
    fs::path dir = resolve_data_dir();
    std::error_code ec;
    fs::create_directories(dir, ec); // ignored: callers handle open failures
    return dir;
}

std::string local_timestamp() {
    std::time_t now = std::time(nullptr);
    std::tm parts{};
#ifdef _WIN32
    localtime_s(&parts, &now);
#else
    localtime_r(&now, &parts);
#endif
    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &parts) == 0) {
        return "";
    }
    return buf;
}

fs::path find_monospace_font() {
    std::error_code ec;

    if (const char* override_path = env_or_null("ASCII_GAME_FONT")) {
        fs::path path(override_path);
        if (fs::is_regular_file(path, ec)) return path;
    }

    for (const char* root : FONT_SEARCH_ROOTS) {
        fs::path found = first_font_in(root);
        if (!found.empty()) return found;
    }

    for (const char* candidate : SYSTEM_FONTS) {
        if (fs::is_regular_file(candidate, ec)) return fs::path(candidate);
    }

    return {};
}

}
