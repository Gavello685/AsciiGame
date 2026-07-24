#pragma once

// Persistent game settings, stored in %APPDATA%/AsciiGame/settings.json
struct Settings {
    int load_radius = 2;    // chunks kept in memory around player (2-4)
    int render_radius = 2;  // chunks rendered around player (1-4)
    int sim_radius = 1;     // chunks where entities simulate (1-3)
};

// Load settings from disk. Returns false if no file exists (defaults kept).
bool load_settings(Settings& s);

// Save settings to disk.
bool save_settings(const Settings& s);
