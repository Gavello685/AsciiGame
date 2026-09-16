#pragma once

#include "game/player.h"
#include "game/npc.h"
#include "game/enemy.h"
#include "game/entity.h"
#include "game/chunk.h"
#include "game/time_system.h"
#include "game/building.h"
#include <string>
#include <vector>

class World;

// v7: enemies/NPCs are rebuilt from their archetype name on load rather than
// from serialised stat fields, and the RNG state round-trips so a reloaded
// game continues the same random sequence.
static const int SAVE_VERSION = 7;
static const int MAX_SAVE_SLOTS = 8;

struct SaveMeta {
    int slot = -1;
    std::string character_name;
    int play_time_seconds = 0;
    std::string timestamp;
    int level = 1;
    int day = 1;
};

struct GameState {
    // Player
    std::string player_name;
    int player_x = 0, player_y = 0;
    int gold = 0;
    int hp = 0;
    PlayerStats stats;
    std::vector<std::pair<Item, int>> inventory;
    std::unordered_map<EquipSlot, Item> equipment;
    int xp = 0;
    int level = 1;

    // World
    uint32_t map_seed = 0;

    // Deterministic RNG state, so a reloaded game continues the same sequence.
    uint64_t rng_state = 0;

    // NPCs. Glyph, colours, merchant flag, dialogue and base shop stock come
    // from make_npc(name); only mutable state is stored.
    struct NpcSave {
        int x = 0, y = 0;
        std::string name;
        int affinity = 30;
        std::vector<std::pair<Item, int>> shop_inventory;
    };

    // Enemies. Stats, colours, glyph and drops come from make_enemy(name);
    // only position and current HP are stored.
    struct EnemySave {
        int x = 0, y = 0;
        std::string name;
        int hp = 0;
    };

    // World items. Glyph and colours come from the item database.
    struct WorldItemSave {
        int x = 0, y = 0;
        std::string item_name;
    };

    // Meta
    SaveMeta meta;

    // Time
    int turn_of_day = 150;
    int day = 1;

    // Zones (v6+)
    std::vector<Zone> zones;

    // Per-chunk data (explored tiles, modifications, placed objects, entities)
    struct ChunkSave {
        int cx, cy;
        std::string explored_hex;
        std::vector<TileMod> modifications;
        std::vector<PlacedObject> placed_objects;
        std::vector<NpcSave> npcs;
        std::vector<EnemySave> enemies;
        std::vector<WorldItemSave> world_items;
    };
    std::vector<ChunkSave> chunks;
};

// Save/load operations
bool save_game(int slot, const GameState& state);
bool load_game(int slot, GameState& state);
bool delete_save(int slot);
std::vector<SaveMeta> list_saves();

// Serialization split out from file I/O so it can be round-tripped in tests.
std::string serialize_state(const GameState& state);
bool deserialize_state(const std::string& json, GameState& state);

// Helpers to convert between game objects and save state
GameState capture_state(const Player& player, const World& world,
                        uint32_t map_seed,
                        int play_time_seconds,
                        const TimeSystem& time_system,
                        uint64_t rng_state);

// Apply chunk save data to a world
void apply_chunk_data(World& world, const std::vector<GameState::ChunkSave>& chunks);
