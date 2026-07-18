#include "game/light.h"
#include "game/world.h"
#include "game/chunk.h"
#include <queue>
#include <algorithm>

namespace light {

struct LightNode {
    int x, y, level;
};

void compute(World& world, int player_x, int player_y,
             uint8_t ambient_light, int player_torch_radius) {
    // Clear light levels for all loaded chunks near the player
    int player_cx = World::world_to_chunk_x(player_x);
    int player_cy = World::world_to_chunk_y(player_y);
    int clear_radius = world.render_radius() + 1;

    for (int dy = -clear_radius; dy <= clear_radius; ++dy) {
        for (int dx = -clear_radius; dx <= clear_radius; ++dx) {
            Chunk* chunk = world.get_chunk(player_cx + dx, player_cy + dy);
            if (!chunk) continue;
            for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
                for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
                    chunk->set_light(lx, ly, ambient_light);
                }
            }
        }
    }

    // BFS from player torch
    if (player_torch_radius > 0) {
        int source_light = 15;
        int step = std::max(1, source_light / player_torch_radius);

        std::queue<LightNode> bfs;
        bfs.push({player_x, player_y, source_light});

        // Track visited to avoid re-processing
        // Use a simple set via world queries
        const int dirs[][2] = {{0,-1},{0,1},{-1,0},{1,0},{-1,-1},{-1,1},{1,-1},{1,1}};

        while (!bfs.empty()) {
            auto [cx, cy, level] = bfs.front();
            bfs.pop();

            if (level <= 0) continue;

            // Update tile light (take max of current and new)
            Tile t = world.get_tile(cx, cy);
            if (level > t.light_level) {
                world.set_light_level(cx, cy, static_cast<uint8_t>(level));
            }

            for (auto [ddx, ddy] : dirs) {
                int nx = cx + ddx;
                int ny = cy + ddy;
                if (!world.in_bounds(nx, ny)) continue;

                int next_level = level - step;
                if (next_level <= 0) continue;

                // Diagonal costs double
                if (ddx != 0 && ddy != 0) next_level -= 1;
                if (next_level <= 0) continue;

                Tile nt = world.get_tile(nx, ny);
                // Opaque tiles get the light level but don't propagate
                if (world.is_opaque(nx, ny)) {
                    if (next_level > nt.light_level) {
                        world.set_light_level(nx, ny, static_cast<uint8_t>(next_level));
                    }
                    continue;
                }

                // Only propagate if this is brighter than what's already there
                if (next_level > nt.light_level) {
                    bfs.push({nx, ny, next_level});
                }
            }
        }
    }

    // BFS from placed light sources (torches, campfires, etc.)
    for (int dy = -clear_radius; dy <= clear_radius; ++dy) {
        for (int dx = -clear_radius; dx <= clear_radius; ++dx) {
            Chunk* chunk = world.get_chunk(player_cx + dx, player_cy + dy);
            if (!chunk) continue;
            for (const auto& obj : chunk->placed_objects()) {
                if (!obj.is_light || obj.light_radius <= 0) continue;

                int obj_wx = chunk->cx() * CHUNK_SIZE + obj.local_x;
                int obj_wy = chunk->cy() * CHUNK_SIZE + obj.local_y;

                int source_light = 15;
                int step = std::max(1, source_light / obj.light_radius);

                std::queue<LightNode> bfs;
                bfs.push({obj_wx, obj_wy, source_light});

                const int dirs[][2] = {{0,-1},{0,1},{-1,0},{1,0},{-1,-1},{-1,1},{1,-1},{1,1}};

                while (!bfs.empty()) {
                    auto [cx, cy, level] = bfs.front();
                    bfs.pop();

                    if (level <= 0) continue;

                    Tile t = world.get_tile(cx, cy);
                    if (level > t.light_level) {
                        world.set_light_level(cx, cy, static_cast<uint8_t>(level));
                    }

                    for (auto [ddx, ddy] : dirs) {
                        int nx = cx + ddx;
                        int ny = cy + ddy;
                        if (!world.in_bounds(nx, ny)) continue;

                        int next_level = level - step;
                        if (next_level <= 0) continue;
                        if (ddx != 0 && ddy != 0) next_level -= 1;
                        if (next_level <= 0) continue;

                        Tile nt = world.get_tile(nx, ny);
                        if (world.is_opaque(nx, ny)) {
                            if (next_level > nt.light_level) {
                                world.set_light_level(nx, ny, static_cast<uint8_t>(next_level));
                            }
                            continue;
                        }

                        if (next_level > nt.light_level) {
                            bfs.push({nx, ny, next_level});
                        }
                    }
                }
            }
        }
    }
}

}
