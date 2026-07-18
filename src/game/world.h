#pragma once

#include "game/chunk.h"
#include "game/entity.h"
#include "game/npc.h"
#include "game/enemy.h"
#include "game/biome.h"
#include "game/structure.h"
#include <unordered_map>
#include <vector>
#include <functional>

// Hash for chunk coordinate pairs
struct ChunkCoord {
    int x, y;
    bool operator==(const ChunkCoord& o) const { return x == o.x && y == o.y; }
    bool operator!=(const ChunkCoord& o) const { return !(*this == o); }
};

struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& c) const {
        return std::hash<int>()(c.x) ^ (std::hash<int>()(c.y) << 16);
    }
};

// Per-chunk entity storage
struct ChunkEntities {
    std::vector<Entity> items;
    std::vector<Npc> npcs;
    std::vector<Enemy> enemies;
};

class World {
public:
    World() = default;

    // Initialize with a seed
    void init(uint32_t seed);

    uint32_t seed() const { return seed_; }

    // Chunk management
    void update_loaded_chunks(int player_world_x, int player_world_y);
    Chunk* get_chunk(int cx, int cy);
    const Chunk* get_chunk(int cx, int cy) const;

    // World-space tile queries
    Tile get_tile(int world_x, int world_y) const;
    void set_tile(int world_x, int world_y, TileType type);
    bool in_bounds(int world_x, int world_y) const;
    bool is_passable(int world_x, int world_y) const;
    bool is_opaque(int world_x, int world_y) const;

    // Visibility/explored (world-space)
    void set_visible(int world_x, int world_y, bool v);
    void set_explored(int world_x, int world_y, bool v);
    void set_light_level(int world_x, int world_y, uint8_t level);

    // Coordinate conversion
    static void world_to_chunk(int world_x, int world_y, int& cx, int& cy, int& lx, int& ly);
    static void chunk_to_world(int cx, int cy, int lx, int ly, int& world_x, int& world_y);
    static int world_to_chunk_x(int world_x);
    static int world_to_chunk_y(int world_y);

    // Chunk loading settings
    int load_radius() const { return load_radius_; }
    int render_radius() const { return render_radius_; }
    int simulation_radius() const { return sim_radius_; }
    void set_load_radius(int r);
    void set_render_radius(int r);
    void set_simulation_radius(int r);

    // Check if a world position is within render/simulation distance of player
    bool is_in_render_range(int world_x, int world_y, int player_wx, int player_wy) const;
    bool is_in_simulation_range(int world_x, int world_y, int player_wx, int player_wy) const;

    // Get all loaded chunks (for iteration)
    const std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash>& loaded_chunks() const { return chunks_; }

    // Get list of chunk keys near player (for save/load)
    std::vector<ChunkCoord> nearby_chunk_keys(int player_wx, int player_wy) const;

    // Place a tile and mark chunk dirty
    void place_tile(int world_x, int world_y, TileType type);

    // Placed objects
    void add_placed_object(int world_x, int world_y, const PlacedObject& obj);

    // Biome/structure queries (world-space)
    BiomeType get_biome_at(int world_x, int world_y) const;
    StructureType get_structure_at(int world_x, int world_y) const;

    // ── Entity management (per-chunk storage) ────────────────────────

    // Spawn entities
    void spawn_item(Entity&& e);
    void spawn_npc(Npc&& n);
    void spawn_enemy(Enemy&& e);

    // Remove entities (returns true if found and removed)
    Entity remove_item_at(int wx, int wy);
    bool remove_enemy_at(int wx, int wy);

    // Spatial queries
    Entity* item_at(int wx, int wy);
    Npc* npc_at(int wx, int wy);
    Enemy* enemy_at(int wx, int wy);

    // Collision queries (for AI)
    bool has_enemy_at(int wx, int wy) const;
    bool has_npc_at(int wx, int wy) const;

    // Bulk queries for rendering/game loop
    struct NearbyEntities {
        std::vector<Entity*> items;
        std::vector<Npc*> npcs;
        std::vector<Enemy*> enemies;
    };
    NearbyEntities get_nearby_entities(int center_wx, int center_wy, int radius_chunks) const;

    // All enemies/NPCs (for iteration like AI update)
    std::vector<Enemy*> get_all_enemies();
    std::vector<Npc*> get_all_npcs();

    // Get mutable NPC by position (for trade, dialogue)
    Npc* get_npc_mut(int wx, int wy);

    // Clear all entities (used on death/restart)
    void clear_all_entities();

    // Clear entities for a specific chunk (used on chunk unload)
    void clear_entities_in_chunk(int cx, int cy);

    // Fix entities that moved across chunk boundaries during AI update
    void rekey_entities();

    // Per-chunk save/load support
    struct ChunkSaveData {
        int cx, cy;
        std::vector<bool> explored;
        std::vector<TileMod> modifications;
        std::vector<PlacedObject> placed_objects;
    };
    std::vector<ChunkSaveData> gather_save_data() const;
    void apply_save_data(const ChunkSaveData& data);

    // Entity save/load
    struct ChunkEntitySaveData {
        int cx, cy;
        std::vector<Entity> items;
        std::vector<Npc> npcs;
        std::vector<Enemy> enemies;
    };
    std::vector<ChunkEntitySaveData> gather_entity_save_data() const;
    void apply_entity_save_data(const std::vector<ChunkEntitySaveData>& data);

private:
    uint32_t seed_ = 0;
    int load_radius_ = 2;
    int render_radius_ = 2;
    int sim_radius_ = 1;

    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> chunks_;
    std::unordered_map<ChunkCoord, ChunkEntities, ChunkCoordHash> entities_;

    // Helpers for first-visit chunk setup
    void spawn_biome_entities(Chunk& chunk, int cx, int cy);
    void spawn_structure_entities(Chunk& chunk, int cx, int cy, StructureType stype);
};
