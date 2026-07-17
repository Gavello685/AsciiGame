#include "game/map.h"
#include <cstdlib>
#include <ctime>
#include <algorithm>

void Map::generate(int width, int height, uint32_t seed) {
    width_ = width;
    height_ = height;
    tiles_.resize(width * height, {TileType::Wall, false});

    if (seed != 0) std::srand(seed);
    else std::srand(static_cast<unsigned>(std::time(nullptr)));

    // Generate random rooms
    struct Room { int x, y, w, h; };
    std::vector<Room> rooms;
    int num_rooms = 8 + std::rand() % 8;

    for (int i = 0; i < num_rooms; ++i) {
        int rw = 4 + std::rand() % 8;
        int rh = 4 + std::rand() % 6;
        int rx = 1 + std::rand() % (width - rw - 2);
        int ry = 1 + std::rand() % (height - rh - 2);

        // Check overlap
        bool overlap = false;
        for (const auto& room : rooms) {
            if (rx < room.x + room.w + 1 && rx + rw + 1 > room.x &&
                ry < room.y + room.h + 1 && ry + rh + 1 > room.y) {
                overlap = true;
                break;
            }
        }
        if (overlap) continue;

        carve_room(rx, ry, rw, rh);

        if (!rooms.empty()) {
            // Connect to previous room center
            int cx1 = rooms.back().x + rooms.back().w / 2;
            int cy1 = rooms.back().y + rooms.back().h / 2;
            int cx2 = rx + rw / 2;
            int cy2 = ry + rh / 2;

            if (std::rand() % 2) {
                carve_hallway(cx1, cy1, cx2, cy1);
                carve_hallway(cx2, cy1, cx2, cy2);
            } else {
                carve_hallway(cx1, cy1, cx1, cy2);
                carve_hallway(cx1, cy2, cx2, cy2);
            }
        }

        rooms.push_back({rx, ry, rw, rh});
    }

    // Scatter some water
    int water_patches = 2 + std::rand() % 3;
    for (int i = 0; i < water_patches; ++i) {
        int cx = 2 + std::rand() % (width - 4);
        int cy = 2 + std::rand() % (height - 4);
        int size = 2 + std::rand() % 3;
        for (int dy = -size; dy <= size; ++dy) {
            for (int dx = -size; dx <= size; ++dx) {
                int nx = cx + dx, ny = cy + dy;
                if (in_bounds(nx, ny) && get(nx, ny).type == TileType::Ground) {
                    if (dx * dx + dy * dy <= size * size) {
                        set(nx, ny, TileType::Water);
                    }
                }
            }
        }
    }
}

Tile Map::get(int x, int y) const {
    if (!in_bounds(x, y)) return {TileType::Wall, false, false};
    return tiles_[y * width_ + x];
}

void Map::set(int x, int y, TileType type) {
    if (!in_bounds(x, y)) return;
    tiles_[y * width_ + x].type = type;
}

void Map::set_visible(int x, int y, bool v) {
    if (!in_bounds(x, y)) return;
    tiles_[y * width_ + x].visible = v;
}

void Map::set_explored(int x, int y, bool v) {
    if (!in_bounds(x, y)) return;
    tiles_[y * width_ + x].explored = v;
}

bool Map::in_bounds(int x, int y) const {
    return x >= 0 && x < width_ && y >= 0 && y < height_;
}

bool Map::is_passable(int x, int y) const {
    Tile t = get(x, y);
    return t.type == TileType::Ground || t.type == TileType::Door;
}

void Map::carve_room(int rx, int ry, int rw, int rh) {
    for (int y = ry; y < ry + rh; ++y) {
        for (int x = rx; x < rx + rw; ++x) {
            set(x, y, TileType::Ground);
        }
    }
}

void Map::carve_hallway(int x1, int y1, int x2, int y2) {
    int dx = (x2 > x1) ? 1 : (x2 < x1) ? -1 : 0;
    int dy = (y2 > y1) ? 1 : (y2 < y1) ? -1 : 0;

    int x = x1, y = y1;
    while (x != x2 || y != y2) {
        set(x, y, TileType::Ground);
        if (x != x2) x += dx;
        else if (y != y2) y += dy;
    }
    set(x2, y2, TileType::Ground);
}
