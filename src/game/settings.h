#pragma once

// Persistent game settings, stored alongside saves in the user data directory.
//
// The three radii are nested rings around the player and must satisfy the
// invariants World enforces: 2 <= load <= 4, 1 <= render <= load - 1 and
// 1 <= sim <= render. The defaults below are a valid configuration; the
// previous default of load = render = 2 was not, so applying it at startup
// silently shrank the render distance.
struct Settings {
    int load_radius = 3;    // chunks kept in memory around player (2-4)
    int render_radius = 2;  // chunks rendered around player (1 .. load - 1)
    int sim_radius = 1;     // chunks where entities simulate (1 .. render)
};

// Load settings from disk. Returns false if no file exists (defaults kept).
bool load_settings(Settings& s);

// Save settings to disk.
bool save_settings(const Settings& s);
