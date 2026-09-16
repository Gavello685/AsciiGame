#pragma once

#include <string>
#include <vector>

class World;
class Player;
class TimeSystem;
class Rng;

// Result of advancing the world by one player turn.
struct TurnOutcome {
    std::vector<std::string> messages;
    bool player_died = false;
};

// Advance time and resolve every non-player actor for one turn: enemy AI and
// combat, NPC AI, loot drops and cross-chunk entity rekeying.
//
// Only chunks within the world's simulation radius are stepped, so the
// Simulation Distance setting bounds per-turn cost.
TurnOutcome resolve_turn(World& world, Player& player, TimeSystem& time, Rng& rng);
