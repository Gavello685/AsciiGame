#pragma once

#include "game/tile.h"
#include <cstdint>
#include <string>
#include <vector>

enum class BiomeType : uint8_t {
    Grassland,
    Forest,
    Desert,
    Swamp,
    Mountain,
    Tundra,
    BiomeCount
};

struct BiomeConfig {
    const char* name;
    uint8_t hud_r, hud_g, hud_b;
    bool has_structures;
    float structure_chance;
};

const BiomeConfig& biome_config(BiomeType biome);
const char* biome_name(BiomeType biome);

// Determine biome from world coordinates using noise
BiomeType determine_biome(int world_x, int world_y, uint32_t seed);

// Generate terrain tile for a position within a biome
TileType generate_tile(int world_x, int world_y, BiomeType biome, uint32_t seed);
