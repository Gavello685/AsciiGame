#include "game/chunk.h"
#include "game/noise.h"
#include "game/biome.h"

Chunk::Chunk(int cx, int cy, uint32_t world_seed)
    : cx_(cx), cy_(cy) {
    tiles_.resize(CHUNK_SIZE * CHUNK_SIZE, {TileType::Grass, false, false, 15});

    // Determine dominant biome for this chunk (sample at center)
    int center_wx = cx * CHUNK_SIZE + CHUNK_SIZE / 2;
    int center_wy = cy * CHUNK_SIZE + CHUNK_SIZE / 2;
    BiomeType dominant = determine_biome(center_wx, center_wy, world_seed);
    biome_id_ = static_cast<int>(dominant);

    // Generate terrain tile-by-tile using biome system
    for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            int wx = cx * CHUNK_SIZE + lx;
            int wy = cy * CHUNK_SIZE + ly;

            // Each tile gets its own biome (allows transitions)
            BiomeType tile_biome = determine_biome(wx, wy, world_seed);
            TileType type = generate_tile(wx, wy, tile_biome, world_seed);

            set(lx, ly, type);
        }
    }
}

Tile Chunk::get(int local_x, int local_y) const {
    if (!in_bounds(local_x, local_y))
        return {TileType::None, false, false, 0};
    return tiles_[local_y * CHUNK_SIZE + local_x];
}

void Chunk::set(int local_x, int local_y, TileType type) {
    if (!in_bounds(local_x, local_y)) return;
    tiles_[local_y * CHUNK_SIZE + local_x].type = type;
}

void Chunk::set_visible(int local_x, int local_y, bool v) {
    if (!in_bounds(local_x, local_y)) return;
    tiles_[local_y * CHUNK_SIZE + local_x].visible = v;
}

void Chunk::set_explored(int local_x, int local_y, bool v) {
    if (!in_bounds(local_x, local_y)) return;
    tiles_[local_y * CHUNK_SIZE + local_x].explored = v;
}

void Chunk::set_light(int local_x, int local_y, uint8_t level) {
    if (!in_bounds(local_x, local_y)) return;
    tiles_[local_y * CHUNK_SIZE + local_x].light_level = level;
}

bool Chunk::in_bounds(int local_x, int local_y) const {
    return local_x >= 0 && local_x < CHUNK_SIZE &&
           local_y >= 0 && local_y < CHUNK_SIZE;
}

bool Chunk::is_passable(int local_x, int local_y) const {
    if (!in_bounds(local_x, local_y)) return false;
    return !tile_blocks_movement(tiles_[local_y * CHUNK_SIZE + local_x].type);
}

bool Chunk::is_opaque(int local_x, int local_y) const {
    if (!in_bounds(local_x, local_y)) return true;
    return tile_blocks_light(tiles_[local_y * CHUNK_SIZE + local_x].type);
}

void Chunk::add_placed_object(const PlacedObject& obj) {
    placed_.push_back(obj);
    mark_dirty();
}

void Chunk::remove_placed_object(int local_x, int local_y) {
    for (auto it = placed_.begin(); it != placed_.end(); ++it) {
        if (it->local_x == local_x && it->local_y == local_y) {
            placed_.erase(it);
            mark_dirty();
            return;
        }
    }
}

void Chunk::apply_modifications(const std::vector<TileMod>& mods) {
    for (const auto& mod : mods) {
        set(mod.local_x, mod.local_y, mod.type);
    }
    mods_ = mods;
}

void Chunk::apply_placed_objects(const std::vector<PlacedObject>& objs) {
    placed_ = objs;
}

void Chunk::apply_explored(const std::vector<bool>& bits) {
    for (int i = 0; i < CHUNK_SIZE * CHUNK_SIZE && i < static_cast<int>(bits.size()); ++i) {
        tiles_[i].explored = bits[i];
    }
}

std::vector<bool> Chunk::explored_bits() const {
    std::vector<bool> bits(CHUNK_SIZE * CHUNK_SIZE);
    for (int i = 0; i < CHUNK_SIZE * CHUNK_SIZE; ++i) {
        bits[i] = tiles_[i].explored;
    }
    return bits;
}
