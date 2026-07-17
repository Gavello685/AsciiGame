#pragma once

#include <cstdint>

enum class TileType : uint8_t {
    None,
    Ground,
    Wall,
    Door,
    Water,
};

struct Tile {
    TileType type = TileType::None;
    bool explored = false;
    bool visible = false;
};

inline uint32_t tile_glyph(TileType t) {
    switch (t) {
        case TileType::Ground: return '.';
        case TileType::Wall:   return '#';
        case TileType::Door:   return '+';
        case TileType::Water:  return '~';
        default: return ' ';
    }
}

inline void tile_color(TileType t, uint8_t& r, uint8_t& g, uint8_t& b) {
    switch (t) {
        case TileType::Ground: r = 120; g = 110; b = 80;  break;
        case TileType::Wall:   r = 180; g = 180; b = 180; break;
        case TileType::Door:   r = 140; g = 100; b = 40;  break;
        case TileType::Water:  r = 60;  g = 120; b = 200; break;
        default:               r = 0;   g = 0;   b = 0;   break;
    }
}
