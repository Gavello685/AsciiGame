#pragma once

#include "game/tile.h"
#include "game/biome.h"
#include <cstdint>
#include <string>
#include <vector>

class Chunk;
class World;

enum class StructureType : uint8_t {
    None,
    Village,
    Cave,
    Ruins,
    Dungeon,
    StructureCount
};

struct StructureEntity {
    char symbol;
    const char* type;
    const char* name;
};

struct StructureDef {
    const char* name;
    int width, height;
    const char** tiles;
    std::vector<StructureEntity> entities;
};

// Get structure definition
const StructureDef& structure_def(StructureType type);

// Decide which structure (if any) belongs in chunk (cx, cy) for a biome.
// Pure/deterministic: used by chunk generation AND the world map view so
// ungenerated chunks show the same structures they will have when generated.
StructureType decide_structure(int cx, int cy, BiomeType biome, uint32_t seed);

// Try to place a structure in a chunk (returns true if placed)
bool try_place_structure(Chunk& chunk, int cx, int cy, uint32_t seed);

// Force-place a structure in a chunk (always succeeds)
void force_place_structure(Chunk& chunk, int cx, int cy, StructureType type);

// Stamp structure tiles into chunk at local origin
void stamp_structure(Chunk& chunk, int origin_lx, int origin_ly, StructureType type);
