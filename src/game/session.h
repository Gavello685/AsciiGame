#pragma once

#include "game/player.h"
#include "game/rng.h"
#include "game/save.h"
#include "game/settings.h"
#include "game/time_system.h"
#include "game/world.h"

#include <cstdint>
#include <string>

// One playthrough: the world, the character, the clock, and the RNG that
// drives both. Deliberately holds no interface state, so a run can be
// created, saved, reloaded and stepped in tests with no window present.
struct Session {
    World world;
    Player player;
    TimeSystem time;

    // Seeded from the map seed and round-tripped through the save file, so a
    // reloaded game continues the same random sequence.
    Rng rng;

    uint32_t map_seed = 0;
    int turn_counter = 0;
    int play_time_seconds = 0;

    // False on the title screen, true once a run is in progress.
    bool started = false;
};

// Generate a world from the seed, place the player in the origin village,
// hand out starting gear and populate the chunks around them. Returns the
// arrival message for the log.
std::string session_start(Session& s, const Settings& settings, uint32_t seed);

// Rebuild a run from a loaded save. Terrain comes back from the seed; the
// save supplies only the parts generation cannot reproduce.
void session_restore(Session& s, const GameState& state, const Settings& settings);

// Snapshot the run for writing to a save slot.
GameState session_capture(const Session& s);

// Push the settings radii onto the world, then read back what the world's
// invariants actually allowed. Settings is updated in place because the
// world silently corrects values that would break the nesting rules.
void session_apply_settings(Session& s, Settings& settings);
