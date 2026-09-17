#pragma once

#include "game/chunk.h"
#include "game/entity.h"
#include "game/npc.h"
#include "game/enemy.h"
#include "game/biome.h"
#include "game/structure.h"
#include "game/building.h"
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

    // Chunk loading settings.
    // Invariants: 2 <= load <= 4, 1 <= render <= load - 1, 1 <= sim <= render.
    static constexpr int MIN_LOAD_RADIUS = 2;
    static constexpr int MAX_LOAD_RADIUS = 4;

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

    // Player-caused terrain change. Recorded as a chunk delta so it persists.
    void place_tile(int world_x, int world_y, TileType type);

    // Placed objects
    void add_placed_object(int world_x, int world_y, const PlacedObject& obj);
    const PlacedObject* placed_object_at(int world_x, int world_y) const;

    // Biome/structure queries (world-space)
    BiomeType get_biome_at(int world_x, int world_y) const;
    StructureType get_structure_at(int world_x, int world_y) const;

    // Per-chunk summary for the world map view
    struct ChunkMapInfo {
        BiomeType biome = BiomeType::Grassland;
        StructureType structure = StructureType::None;
    };

    // Biome + structure for a chunk. Uses the live chunk when loaded;
    // otherwise predicts deterministically (matches what generation will produce).
    ChunkMapInfo chunk_map_info(int cx, int cy) const;

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

    // ── Indexed entity access ───────────────────────────────────────
    //
    // Turn resolution mutates entity vectors while walking them (enemies die,
    // loot spawns). Handing out bulk pointer lists made that unsafe, so
    // callers iterate chunk-by-chunk and re-fetch the pointer each step.

    std::vector<ChunkCoord> entity_chunk_keys() const;

    size_t enemy_count_in_chunk(ChunkCoord key) const;
    Enemy* enemy_in_chunk(ChunkCoord key, size_t index);
    void erase_enemy_in_chunk(ChunkCoord key, size_t index);

    size_t npc_count_in_chunk(ChunkCoord key) const;
    Npc* npc_in_chunk(ChunkCoord key, size_t index);

    // Clear all entities (used on death/restart)
    void clear_all_entities();

    // Fix entities that moved across chunk boundaries during AI update
    void rekey_entities();

    // Spawn a structure's NPCs/enemies. Called once, on a chunk's first visit.
    void spawn_structure_entities(int cx, int cy, StructureType stype);

    // ── Zones ─────────────────────────────────────────────────────
    void add_zone(const Zone& z);
    bool remove_zone_at(int wx, int wy);
    const Zone* zone_at(int wx, int wy) const;
    const std::vector<Zone>& zones() const { return zones_; }
    void clear_zones() { zones_.clear(); }

    // ── Per-chunk persistence ───────────────────────────────────────
    //
    // Terrain is reproducible from the seed, so only the parts that aren't
    // are retained: explored bits, player tile deltas, player-placed objects
    // and live entity state. These are archived when a chunk unloads and
    // replayed when it comes back, which is what makes a wall you built five
    // chunks ago still there when you walk home.

    struct ChunkSaveData {
        int cx = 0, cy = 0;
        std::vector<bool> explored;
        std::vector<TileMod> modifications;
        std::vector<PlacedObject> placed_objects;
    };

    // Terrain/exploration state for every chunk that has been visited,
    // whether it is currently loaded or archived.
    std::vector<ChunkSaveData> gather_save_data() const;

    // Stage terrain/exploration state for a chunk. Applied immediately if the
    // chunk is loaded, otherwise archived until it is next generated.
    void apply_save_data(const ChunkSaveData& data);

    struct ChunkEntitySaveData {
        int cx = 0, cy = 0;
        std::vector<Entity> items;
        std::vector<Npc> npcs;
        std::vector<Enemy> enemies;
    };

    std::vector<ChunkEntitySaveData> gather_entity_save_data() const;

    // Stage entity state for a chunk, replacing whatever is there.
    void apply_entity_save_data(const ChunkEntitySaveData& data);

    // True once a chunk has been generated or restored at least once. Used to
    // decide whether to spawn fresh biome/structure entities.
    bool is_chunk_known(int cx, int cy) const;

private:
    // Everything about a chunk that generation cannot reproduce.
    struct ChunkArchive {
        std::vector<bool> explored;
        std::vector<TileMod> modifications;
        std::vector<PlacedObject> placed_objects;
        ChunkEntities entities;
    };

    void archive_chunk(ChunkCoord key);
    void restore_chunk_terrain(ChunkCoord key, Chunk& chunk);
    void restore_chunk(ChunkCoord key, Chunk& chunk);
    void generate_chunk_contents(ChunkCoord key, Chunk& chunk);
    void reconcile_radii();

    // Helpers for first-visit chunk setup
    void spawn_biome_entities(Chunk& chunk, int cx, int cy);

    uint32_t seed_ = 0;
    int load_radius_ = 3;
    int render_radius_ = 2;
    int sim_radius_ = 1;

    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> chunks_;
    std::unordered_map<ChunkCoord, ChunkEntities, ChunkCoordHash> entities_;
    std::unordered_map<ChunkCoord, ChunkArchive, ChunkCoordHash> archive_;
    std::vector<Zone> zones_;
};
