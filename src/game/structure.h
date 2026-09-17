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

enum class StructureEntityKind : uint8_t {
    Npc,
    Enemy,
};

struct StructureEntity {
    StructureEntityKind kind;
    // Archetype name, resolved through make_npc() / make_enemy().
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

// Place the structure that belongs in this chunk, if any. Stamps tiles, sets
// the chunk's structure id and adds wall torches. Returns the structure placed,
// or StructureType::None when the chunk has none.
StructureType try_place_structure(Chunk& chunk, int cx, int cy, uint32_t seed);

// Force-place a structure at the centre of a chunk (always succeeds).
void force_place_structure(Chunk& chunk, StructureType type);

// Stamp structure tiles into chunk at local origin
void stamp_structure(Chunk& chunk, int origin_lx, int origin_ly, StructureType type);
