#pragma once

#include "game/player.h"
#include "game/npc.h"
#include "game/enemy.h"
#include "game/entity.h"
#include "game/chunk.h"
#include "game/time_system.h"
#include <string>
#include <vector>

class World;

static const int SAVE_VERSION = 5;
static const int MAX_SAVE_SLOTS = 8;

struct SaveMeta {
    int slot = -1;
    std::string character_name;
    int play_time_seconds = 0;
    std::string timestamp;
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

    // NPCs
    struct NpcSave {
        int x, y;
        uint32_t glyph;
        uint8_t fg_r, fg_g, fg_b;
        std::string name;
        bool is_merchant;
        int affinity;
        std::vector<std::pair<Item, int>> shop_inventory;
    };
    std::vector<NpcSave> npcs;

    // Enemies
    struct EnemySave {
        int x, y;
        uint32_t glyph;
        uint8_t fg_r, fg_g, fg_b;
        std::string name;
        int hp, max_hp, attack, defense, damage_variance, xp_value;
    };
    std::vector<EnemySave> enemies;

    // World items
    struct WorldItemSave {
        int x, y;
        uint32_t glyph;
        uint8_t fg_r, fg_g, fg_b;
        std::string item_name;
    };
    std::vector<WorldItemSave> world_items;

    // Meta
    SaveMeta meta;

    // Time
    int turn_of_day = 150;

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

// Helpers to convert between game objects and save state
GameState capture_state(const Player& player, const World& world,
                        uint32_t map_seed,
                        int play_time_seconds,
                        const TimeSystem& time_system);

// Apply chunk save data to a world
void apply_chunk_data(World& world, const std::vector<GameState::ChunkSave>& chunks);
