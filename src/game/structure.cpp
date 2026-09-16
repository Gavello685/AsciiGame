#include "game/structure.h"
#include "game/chunk.h"
#include "game/world.h"
#include "game/noise.h"
#include <cstdlib>

// ── Structure tile templates ──────────────────────────────────────
// Legend: # = Wall, . = Floor, + = Door, T = Torch, ~ = Water
//         ^ = Trap, > = Stairs_Down, < = Stairs_Up, ' ' = empty (skip)

static const char* village_tiles[] = {
    "#####.# #####",
    "#...T.#.#...#",
    "#.....+.....#",
    "#...T.#.#...#",
    "###+###.##+##",
    "............ ",
    "###+###.##+##",
    "#.....+.....#",
    "#...........#",
    "#.....T.....#",
    "#####.# #####",
};

static const char* cave_tiles[] = {
    "  #####      ",
    " ##...##     ",
    "#...T...#    ",
    "#.......#### ",
    "##..T.......#",
    " ###........#",
    "   ##...T...#",
    "    #.......#",
    "    ##..#### ",
    "     ####    ",
};

static const char* ruins_tiles[] = {
    "# #   # #",
    "        .",
    "# . # . #",
    ".. .   ..",
    "   . #   ",
    "# .   . #",
    ".. .   ..",
    "# . # . #",
    "        .",
    "# #   # #",
};

static const char* dungeon_tiles[] = {
    "##########",
    "#........#",
    "#.T....T.#",
    "#........#",
    "#..####..#",
    "#..#..#..#",
    "#..#..#..#",
    "#..####..#",
    "#........#",
    "#.T....+.T#",
    "#........#",
    "##########",
};

static const StructureDef structure_defs[] = {
    // Village
    {
        "Village", 13, 11, village_tiles,
        {
            {StructureEntityKind::Npc, "Villager"},
            {StructureEntityKind::Npc, "Merchant"},
            {StructureEntityKind::Npc, "Guard"},
            {StructureEntityKind::Npc, "Blacksmith"},
            {StructureEntityKind::Npc, "Farmer"},
            {StructureEntityKind::Npc, "Herbalist"},
        }
    },
    // Cave
    {
        "Cave", 13, 10, cave_tiles,
        {
            {StructureEntityKind::Enemy, "Goblin"},
            {StructureEntityKind::Enemy, "Rat"},
            {StructureEntityKind::Enemy, "Bat"},
        }
    },
    // Ruins
    {
        "Ruins", 9, 10, ruins_tiles,
        {
            {StructureEntityKind::Enemy, "Spider"},
            {StructureEntityKind::Enemy, "Zombie"},
        }
    },
    // Dungeon
    {
        "Dungeon", 11, 12, dungeon_tiles,
        {
            {StructureEntityKind::Enemy, "Goblin"},
            {StructureEntityKind::Enemy, "Ogre"},
            {StructureEntityKind::Enemy, "Giant Spider"},
            {StructureEntityKind::Enemy, "Zombie"},
        }
    },
};

const StructureDef& structure_def(StructureType type) {
    int idx = static_cast<int>(type);
    if (idx <= 0 || idx >= static_cast<int>(StructureType::StructureCount))
        return structure_defs[0];
    return structure_defs[idx - 1];
}

void stamp_structure(Chunk& chunk, int origin_lx, int origin_ly, StructureType type) {
    const StructureDef& def = structure_def(type);

    for (int y = 0; y < def.height; ++y) {
        for (int x = 0; x < def.width; ++x) {
            int lx = origin_lx + x;
            int ly = origin_ly + y;
            if (!chunk.in_bounds(lx, ly)) continue;

            char c = def.tiles[y][x];
            TileType t;
            switch (c) {
                case '#': t = TileType::Wall_Dungeon; break;
                case '.': t = TileType::Floor; break;
                case '+': t = TileType::Door_Stone; break;
                case 'T': t = TileType::Floor; break;  // Floor under torch
                case '^': t = TileType::Trap; break;
                case '>': t = TileType::Stairs_Down; break;
                case '<': t = TileType::Stairs_Up; break;
                case '~': t = TileType::Water; break;
                case ' ': continue;  // Skip empty
                default:  continue;
            }
            chunk.set_generated(lx, ly, t);
        }
    }
}

// Uniform deterministic hash of (cx, cy, seed) -> [0, 1).
// Used for structure chance rolls: noise values are bell-shaped around 0.5 and
// never reach the low end, so thresholding noise made structures impossible
// to spawn (only the force-placed origin village ever existed).
static float hash01(int cx, int cy, uint32_t seed) {
    uint32_t h = seed;
    h ^= static_cast<uint32_t>(cx) * 0x9E3779B1u;
    h ^= static_cast<uint32_t>(cy) * 0x85EBCA77u;
    h ^= h >> 16; h *= 0x85EBCA77u; h ^= h >> 13;
    h *= 0xC2B2AE35u; h ^= h >> 16;
    return static_cast<float>(h) / 4294967296.0f;
}

StructureType decide_structure(int cx, int cy, BiomeType biome, uint32_t seed) {
    const BiomeConfig& bc = biome_config(biome);

    if (!bc.has_structures) return StructureType::None;

    // Deterministic chance roll per chunk (true uniform probability)
    if (hash01(cx, cy, seed) >= bc.structure_chance) return StructureType::None;

    // Determine structure type from biome
    float type_noise = noise::fractal2d(
        static_cast<float>(cx) * 1.3f + 6000.0f,
        static_cast<float>(cy) * 1.3f + 6000.0f,
        1, 0.5f);
    float type_val = noise::normalize(type_noise);

    switch (biome) {
        case BiomeType::Grassland:
            return type_val < 0.6f ? StructureType::Village : StructureType::Ruins;
        case BiomeType::Forest:
            return type_val < 0.5f ? StructureType::Ruins : StructureType::Cave;
        case BiomeType::Desert:
            return StructureType::Ruins;
        case BiomeType::Swamp:
            return StructureType::Ruins;
        case BiomeType::Mountain:
            return type_val < 0.6f ? StructureType::Cave : StructureType::Dungeon;
        case BiomeType::Tundra:
            return StructureType::Ruins;
        default:
            return StructureType::None;
    }
}

// Wall torches sit on the 'T' cells of a template. They are seed-derived, so
// they are re-added on every generation and never persisted.
static void place_template_torches(Chunk& chunk, int origin_lx, int origin_ly,
                                   const StructureDef& def) {
    for (int y = 0; y < def.height; ++y) {
        for (int x = 0; x < def.width; ++x) {
            if (def.tiles[y][x] != 'T') continue;
            int lx = origin_lx + x;
            int ly = origin_ly + y;
            if (!chunk.in_bounds(lx, ly)) continue;

            PlacedObject torch;
            torch.local_x = lx;
            torch.local_y = ly;
            torch.glyph = '*';
            torch.name = "Wall Torch";
            torch.fg_r = 255; torch.fg_g = 200; torch.fg_b = 50;
            torch.is_light = true;
            torch.light_radius = 5;
            chunk.add_placed_object(torch);
        }
    }
}

StructureType try_place_structure(Chunk& chunk, int cx, int cy, uint32_t seed) {
    BiomeType biome = static_cast<BiomeType>(chunk.biome_id());
    StructureType stype = decide_structure(cx, cy, biome, seed);
    if (stype == StructureType::None) return StructureType::None;

    const StructureDef& def = structure_def(stype);

    // Random position within chunk (with margin)
    int margin = 2;
    float pos_x_noise = noise::fractal2d(
        static_cast<float>(cx) * 2.1f + 7000.0f,
        static_cast<float>(cy) * 2.1f + 7000.0f,
        1, 0.5f);
    float pos_y_noise = noise::fractal2d(
        static_cast<float>(cx) * 2.1f + 8000.0f,
        static_cast<float>(cy) * 2.1f + 8000.0f,
        1, 0.5f);

    int origin_lx = margin + static_cast<int>(noise::normalize(pos_x_noise) * (CHUNK_SIZE - def.width - margin * 2));
    int origin_ly = margin + static_cast<int>(noise::normalize(pos_y_noise) * (CHUNK_SIZE - def.height - margin * 2));

    // Clamp
    if (origin_lx < 0) origin_lx = 0;
    if (origin_ly < 0) origin_ly = 0;
    if (origin_lx + def.width > CHUNK_SIZE) origin_lx = CHUNK_SIZE - def.width;
    if (origin_ly + def.height > CHUNK_SIZE) origin_ly = CHUNK_SIZE - def.height;

    stamp_structure(chunk, origin_lx, origin_ly, stype);
    chunk.set_structure_id(static_cast<int>(stype));
    place_template_torches(chunk, origin_lx, origin_ly, def);

    return stype;
}

void force_place_structure(Chunk& chunk, StructureType type) {
    const StructureDef& def = structure_def(type);

    // Center the structure in the chunk
    int origin_lx = (CHUNK_SIZE - def.width) / 2;
    int origin_ly = (CHUNK_SIZE - def.height) / 2;
    if (origin_lx < 0) origin_lx = 0;
    if (origin_ly < 0) origin_ly = 0;

    stamp_structure(chunk, origin_lx, origin_ly, type);
    chunk.set_structure_id(static_cast<int>(type));

    // Clear terrain around the structure to Grass (removes trees, etc.)
    int margin = 4;
    int clear_x0 = origin_lx - margin;
    int clear_y0 = origin_ly - margin;
    int clear_x1 = origin_lx + def.width + margin;
    int clear_y1 = origin_ly + def.height + margin;
    for (int ly = clear_y0; ly < clear_y1; ++ly) {
        for (int lx = clear_x0; lx < clear_x1; ++lx) {
            if (!chunk.in_bounds(lx, ly)) continue;
            Tile t = chunk.get(lx, ly);
            // Only clear tiles that block movement (trees, mountains, etc.)
            // Don't overwrite structure tiles or already-passable terrain
            if (tile_blocks_movement(t.type) && t.type != TileType::Wall_Dungeon) {
                chunk.set_generated(lx, ly, TileType::Grass);
            }
        }
    }

    place_template_torches(chunk, origin_lx, origin_ly, def);
}
