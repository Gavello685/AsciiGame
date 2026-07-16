#pragma once

#include "game/tile.h"
#include <vector>
#include <cstdint>

class Map {
public:
    Map() = default;

    void generate(int width, int height, uint32_t seed = 0);

    int width() const { return width_; }
    int height() const { return height_; }

    Tile get(int x, int y) const;
    void set(int x, int y, TileType type);

    bool in_bounds(int x, int y) const;
    bool is_passable(int x, int y) const;

private:
    void carve_room(int rx, int ry, int rw, int rh);
    void carve_hallway(int x1, int y1, int x2, int y2);

    int width_ = 0;
    int height_ = 0;
    std::vector<Tile> tiles_;
};
