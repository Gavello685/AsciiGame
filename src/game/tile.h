#pragma once

#include <cstdint>

enum class TileType : uint8_t {
    None,
    Ground,     // Generic walkable floor (dungeons, structures)
    Wall,       // Generic wall (blocks movement + light)
    Door,       // Walkable door
    Water,      // Shallow water (blocks movement)
    // Overworld terrain
    Grass,      // Default walkable overworld terrain
    Dirt,       // Bare earth (walkable)
    Sand,       // Desert terrain (walkable)
    Snow,       // Tundra terrain (walkable)
    Stone,      // Rocky ground (walkable, mountain floors)
    Tree,       // Blocks movement, blocks light
    TallGrass,  // Walkable, slight cover
    Mountain,   // Impassable rock face (blocks movement + light)
    DeepWater,  // Deep water, impassable
    // Structure tiles (4C)
    Floor,      // Interior floor (walkable, distinct from Ground)
    Wall_Dungeon, // Dungeon wall (blocks movement + light)
    Door_Stone, // Stone door (walkable)
    Trap,       // Walkable but triggers effect
    Stairs_Down, // Cosmetic depth marker (walkable)
    Stairs_Up,  // Cosmetic depth marker (walkable)
    // Player-built tiles (4D)
    WoodWall,   // Player-built wooden wall (blocks movement + light)
    WoodFloor,  // Player-built plank floor (walkable)
    StoneFloor, // Player-built stone floor (walkable)
    WoodDoor,   // Player-built wooden door (walkable)
};

struct Tile {
    TileType type = TileType::None;
    bool explored = false;
    bool visible = false;
    uint8_t light_level = 15; // 0 (dark) to 15 (full brightness)
};

inline bool tile_blocks_movement(TileType t) {
    switch (t) {
        case TileType::Wall:
        case TileType::Water:
        case TileType::Tree:
        case TileType::Mountain:
        case TileType::DeepWater:
        case TileType::Wall_Dungeon:
        case TileType::WoodWall:
            return true;
        default:
            return false;
    }
}

inline bool tile_blocks_light(TileType t) {
    switch (t) {
        case TileType::Wall:
        case TileType::Tree:
        case TileType::Mountain:
        case TileType::Wall_Dungeon:
        case TileType::DeepWater:
        case TileType::WoodWall:
            return true;
        default:
            return false;
    }
}

inline uint32_t tile_glyph(TileType t) {
    switch (t) {
        case TileType::Ground:       return '.';
        case TileType::Wall:         return '#';
        case TileType::Door:         return '+';
        case TileType::Water:        return '~';
        case TileType::Grass:        return '.';
        case TileType::Dirt:         return '.';
        case TileType::Sand:         return '.';
        case TileType::Snow:         return '.';
        case TileType::Stone:        return '.';
        case TileType::Tree:         return 'T';
        case TileType::TallGrass:    return '"';
        case TileType::Mountain:     return '%';
        case TileType::DeepWater:    return '~';
        case TileType::Floor:        return '.';
        case TileType::Wall_Dungeon: return '#';
        case TileType::Door_Stone:   return '+';
        case TileType::Trap:         return '^';
        case TileType::Stairs_Down:  return '>';
        case TileType::Stairs_Up:    return '<';
        case TileType::WoodWall:     return '#';
        case TileType::WoodFloor:    return '.';
        case TileType::StoneFloor:   return '.';
        case TileType::WoodDoor:     return '+';
        default: return ' ';
    }
}

inline void tile_color(TileType t, uint8_t& r, uint8_t& g, uint8_t& b) {
    switch (t) {
        case TileType::Ground:       r = 120; g = 110; b = 80;  break;
        case TileType::Wall:         r = 180; g = 180; b = 180; break;
        case TileType::Door:         r = 140; g = 100; b = 40;  break;
        case TileType::Water:        r = 60;  g = 120; b = 200; break;
        case TileType::Grass:        r = 60;  g = 140; b = 50;  break;
        case TileType::Dirt:         r = 130; g = 100; b = 60;  break;
        case TileType::Sand:         r = 200; g = 180; b = 120; break;
        case TileType::Snow:         r = 220; g = 220; b = 240; break;
        case TileType::Stone:        r = 140; g = 140; b = 140; break;
        case TileType::Tree:         r = 30;  g = 100; b = 30;  break;
        case TileType::TallGrass:    r = 80;  g = 160; b = 60;  break;
        case TileType::Mountain:     r = 100; g = 90;  b = 80;  break;
        case TileType::DeepWater:    r = 30;  g = 60;  b = 160; break;
        case TileType::Floor:        r = 100; g = 90;  b = 70;  break;
        case TileType::Wall_Dungeon: r = 90;  g = 80;  b = 70;  break;
        case TileType::Door_Stone:   r = 120; g = 110; b = 90;  break;
        case TileType::Trap:         r = 180; g = 50;  b = 50;  break;
        case TileType::Stairs_Down:  r = 160; g = 160; b = 80;  break;
        case TileType::Stairs_Up:    r = 160; g = 160; b = 80;  break;
        case TileType::WoodWall:     r = 150; g = 110; b = 60;  break;
        case TileType::WoodFloor:    r = 170; g = 130; b = 80;  break;
        case TileType::StoneFloor:   r = 150; g = 150; b = 150; break;
        case TileType::WoodDoor:     r = 170; g = 120; b = 60;  break;
        default:                     r = 0;   g = 0;   b = 0;   break;
    }
}
