#pragma once

#include <filesystem>
#include <string>

namespace platform {

// Root directory for saves and settings.
//
// Resolution order:
//   1. ASCII_GAME_DATA_DIR environment variable (used by tests)
//   2. %APPDATA%/AsciiGame on Windows, $XDG_DATA_HOME or ~/.local/share on POSIX
//   3. ./AsciiGame relative to the working directory
//
// The directory is created if it does not exist. On failure the returned path
// still names a location; callers must handle stream-open failures.
std::filesystem::path user_data_dir();

// Local wall-clock time as "YYYY-MM-DDTHH:MM:SS".
std::string local_timestamp();

}
