#pragma once

#include "game/tile.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

static const int CHUNK_SIZE = 64;

// Sparse tile modification for persistence
struct TileMod {
    int local_x, local_y;
    TileType type;
};

// Placed object (torch, campfire, etc.) — stored per chunk
struct PlacedObject {
    int local_x, local_y;
    uint32_t glyph;
    std::string name;
    uint8_t fg_r, fg_g, fg_b;
    bool is_light = false;
    int light_radius = 0;
};

class Chunk {
public:
    Chunk() = default;
    Chunk(int cx, int cy, uint32_t world_seed);

    int cx() const { return cx_; }
    int cy() const { return cy_; }

    Tile get(int local_x, int local_y) const;
    void set(int local_x, int local_y, TileType type);

    void set_visible(int local_x, int local_y, bool v);
    void set_explored(int local_x, int local_y, bool v);
    void set_light(int local_x, int local_y, uint8_t level);

    bool in_bounds(int local_x, int local_y) const;
    bool is_passable(int local_x, int local_y) const;
    bool is_opaque(int local_x, int local_y) const;

    // Dirty tracking
    bool is_dirty() const { return dirty_; }
    void mark_dirty() { dirty_ = true; }
    bool is_visited() const { return visited_; }
    void mark_visited() { visited_ = true; }

    // Modification tracking (for save/load)
    const std::vector<TileMod>& modifications() const { return mods_; }
    void clear_modifications() { mods_.clear(); dirty_ = false; }

    // Placed objects
    const std::vector<PlacedObject>& placed_objects() const { return placed_; }
    void add_placed_object(const PlacedObject& obj);
    void remove_placed_object(int local_x, int local_y);

    // Apply saved modifications (used on load)
    void apply_modifications(const std::vector<TileMod>& mods);
    void apply_placed_objects(const std::vector<PlacedObject>& objs);
    void apply_explored(const std::vector<bool>& bits);

    // Get/set explored as compact bitset for serialization
    std::vector<bool> explored_bits() const;

    // Get biome ID
    int biome_id() const { return biome_id_; }
    void set_biome_id(int id) { biome_id_ = id; }

    // Get structure ID (0 = none)
    int structure_id() const { return structure_id_; }
    void set_structure_id(int id) { structure_id_ = id; }

    // Raw tile access for iteration (used by FOV, rendering)
    const std::vector<Tile>& tiles() const { return tiles_; }

private:
    int cx_ = 0;
    int cy_ = 0;
    int biome_id_ = 0;
    int structure_id_ = 0;
    bool dirty_ = false;
    bool visited_ = false;

    std::vector<Tile> tiles_;
    std::vector<TileMod> mods_;
    std::vector<PlacedObject> placed_;
};
