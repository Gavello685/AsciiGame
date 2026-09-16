#include "platform/paths.h"

#include <cstdlib>
#include <ctime>

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

}
