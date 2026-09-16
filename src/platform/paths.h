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

// Path to a monospace TrueType font the renderer can load.
//
// Resolution order:
//   1. ASCII_GAME_FONT environment variable
//   2. any .ttf under assets/fonts, searched from the working directory and
//      one and two levels up so the game runs from a build subdirectory
//   3. the first readable entry in a per-platform list of system fonts
//
// Returns an empty path when nothing is found; the caller reports that rather
// than handing an invalid path to SDL_ttf.
std::filesystem::path find_monospace_font();

}
