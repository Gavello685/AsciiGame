#include "game/biome.h"
#include "game/noise.h"
#include <cmath>

static const BiomeConfig biome_configs[] = {
    // name,          r,   g,   b,  has_structures, structure_chance
    {"Grassland",    100, 180,  80, true,  0.02f},
    {"Forest",        40, 120,  40, true,  0.015f},
    {"Desert",       220, 180, 100, true,  0.01f},
    {"Swamp",         80, 100,  60, true,  0.008f},
    {"Mountain",     140, 130, 120, true,  0.012f},
    {"Tundra",       180, 200, 220, true,  0.008f},
};

const BiomeConfig& biome_config(BiomeType biome) {
    int idx = static_cast<int>(biome);
    if (idx < 0 || idx >= static_cast<int>(BiomeType::BiomeCount))
        return biome_configs[0];
    return biome_configs[idx];
}

const char* biome_name(BiomeType biome) {
    return biome_config(biome).name;
}

BiomeType determine_biome(int world_x, int world_y, uint32_t seed) {
    float seed_x = static_cast<float>((seed >> 16) & 0xFFFF) * 0.1f;
    float seed_y = static_cast<float>(seed & 0xFFFF) * 0.1f;

    float wx = static_cast<float>(world_x);
    float wy = static_cast<float>(world_y);

    // Temperature: hot near y=0, cold far from y=0 (equator effect)
    float base_temp = noise::fractal2d(
        (wx + seed_x) * 0.005f,
        (wy + seed_y) * 0.005f,
        4, 0.5f);
    // Add latitude influence
    float latitude = std::abs(wy) * 0.002f;
    float temp = noise::normalize(base_temp) - latitude;
    if (temp < 0.0f) temp = 0.0f;
    if (temp > 1.0f) temp = 1.0f;

    // Moisture
    float moisture_raw = noise::fractal2d(
        (wx + seed_x + 1000) * 0.005f,
        (wy + seed_y + 1000) * 0.005f,
        4, 0.5f);
    float moisture = noise::normalize(moisture_raw);

    // Height (for mountain override)
    float height = noise::fractal2d(
        (wx + seed_x + 2000) * 0.008f,
        (wy + seed_y + 2000) * 0.008f,
        4, 0.5f);

    // High elevation overrides to Mountain
    if (height > 0.55f) return BiomeType::Mountain;

    // Biome selection from temperature/moisture grid
    if (temp < 0.3f) {
        if (moisture > 0.6f) return BiomeType::Swamp;
        return BiomeType::Tundra;
    } else if (temp < 0.6f) {
        if (moisture < 0.3f) return BiomeType::Grassland;
        if (moisture > 0.65f) return BiomeType::Swamp;
        return BiomeType::Forest;
    } else {
        if (moisture < 0.35f) return BiomeType::Desert;
        if (moisture > 0.6f) return BiomeType::Forest;
        return BiomeType::Grassland;
    }
}

TileType generate_tile(int world_x, int world_y, BiomeType biome, uint32_t seed) {
    float seed_x = static_cast<float>((seed >> 16) & 0xFFFF) * 0.1f;
    float seed_y = static_cast<float>(seed & 0xFFFF) * 0.1f;
    float wx = static_cast<float>(world_x);
    float wy = static_cast<float>(world_y);

    float detail = noise::fractal2d(
        (wx + seed_x + 3000) * 0.05f,
        (wy + seed_y + 3000) * 0.05f,
        2, 0.5f);

    float height = noise::fractal2d(
        (wx + seed_x + 4000) * 0.02f,
        (wy + seed_y + 4000) * 0.02f,
        3, 0.5f);

    float moisture = noise::fractal2d(
        (wx + seed_x + 5000) * 0.03f,
        (wy + seed_y + 5000) * 0.03f,
        2, 0.5f);

    switch (biome) {
        case BiomeType::Grassland:
            if (height > 0.4f) return TileType::Stone;
            if (detail > 0.55f) return TileType::Tree;
            if (detail > 0.3f && detail < 0.4f) return TileType::TallGrass;
            if (moisture > 0.5f && detail > 0.2f) return TileType::TallGrass;
            return TileType::Grass;

        case BiomeType::Forest:
            if (height > 0.5f) return TileType::Stone;
            if (detail > 0.15f) return TileType::Tree;
            if (detail < -0.1f) return TileType::Grass;
            if (moisture > 0.3f) return TileType::Dirt;
            return TileType::Grass;

        case BiomeType::Desert:
            if (height > 0.5f) return TileType::Stone;
            if (height > 0.35f) return TileType::Sand;
            if (detail > 0.6f) return TileType::Stone;
            if (moisture > 0.4f && detail > 0.2f) return TileType::TallGrass;
            return TileType::Sand;

        case BiomeType::Swamp:
            if (height < -0.2f || moisture > 0.6f) return TileType::Water;
            if (height < 0.0f) return TileType::DeepWater;
            if (detail > 0.5f) return TileType::Tree;
            if (detail > 0.3f) return TileType::TallGrass;
            if (moisture > 0.2f) return TileType::Dirt;
            return TileType::Grass;

        case BiomeType::Mountain:
            if (height > 0.6f) return TileType::Mountain;
            if (height > 0.4f) return TileType::Stone;
            if (height < -0.1f) return TileType::Dirt;
            if (detail > 0.5f) return TileType::Tree;
            return TileType::Stone;

        case BiomeType::Tundra:
            if (height > 0.5f) return TileType::Stone;
            if (detail > 0.6f) return TileType::Tree;
            if (moisture > 0.5f && detail > 0.3f) return TileType::Snow;
            return TileType::Snow;

        default:
            return TileType::Grass;
    }
}
