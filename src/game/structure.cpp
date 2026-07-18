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
            {'V', "npc", "Villager"},
            {'M', "npc", "Merchant"},
        }
    },
    // Cave
    {
        "Cave", 13, 10, cave_tiles,
        {
            {'G', "enemy", "Goblin"},
            {'R', "enemy", "Rat"},
        }
    },
    // Ruins
    {
        "Ruins", 9, 10, ruins_tiles,
        {
            {'S', "enemy", "Spider"},
        }
    },
    // Dungeon
    {
        "Dungeon", 11, 12, dungeon_tiles,
        {
            {'G', "enemy", "Goblin"},
            {'O', "enemy", "Ogre"},
            {'T', "enemy", "Giant Spider"},
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
            chunk.set(lx, ly, t);
        }
    }
    chunk.mark_dirty();
}

bool try_place_structure(Chunk& chunk, int cx, int cy, uint32_t seed) {
    BiomeType biome = static_cast<BiomeType>(chunk.biome_id());
    const BiomeConfig& bc = biome_config(biome);

    if (!bc.has_structures) return false;

    // Deterministic chance check per chunk
    float chance_noise = noise::fractal2d(
        static_cast<float>(cx) * 0.7f + 5000.0f,
        static_cast<float>(cy) * 0.7f + 5000.0f,
        1, 0.5f);
    float chance = noise::normalize(chance_noise);

    if (chance > bc.structure_chance) return false;

    // Determine structure type from biome
    StructureType stype;
    float type_noise = noise::fractal2d(
        static_cast<float>(cx) * 1.3f + 6000.0f,
        static_cast<float>(cy) * 1.3f + 6000.0f,
        1, 0.5f);
    float type_val = noise::normalize(type_noise);

    switch (biome) {
        case BiomeType::Grassland:
            stype = type_val < 0.6f ? StructureType::Village : StructureType::Ruins;
            break;
        case BiomeType::Forest:
            stype = type_val < 0.5f ? StructureType::Ruins : StructureType::Cave;
            break;
        case BiomeType::Desert:
            stype = StructureType::Ruins;
            break;
        case BiomeType::Swamp:
            stype = StructureType::Ruins;
            break;
        case BiomeType::Mountain:
            stype = type_val < 0.6f ? StructureType::Cave : StructureType::Dungeon;
            break;
        case BiomeType::Tundra:
            stype = StructureType::Ruins;
            break;
        default:
            return false;
    }

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

    // Place torches where 'T' tiles are
    for (int y = 0; y < def.height; ++y) {
        for (int x = 0; x < def.width; ++x) {
            if (def.tiles[y][x] == 'T') {
                int lx = origin_lx + x;
                int ly = origin_ly + y;
                if (chunk.in_bounds(lx, ly)) {
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
    }

    return true;
}
