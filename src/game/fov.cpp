#include "game/fov.h"
#include "game/world.h"
#include "game/chunk.h"
#include <cmath>
#include <algorithm>

namespace fov {

void compute(World& world, int cx, int cy, int radius) {
    // Clear visibility for all loaded chunks near the player
    // (chunks further than render distance don't need clearing)
    int player_cx = World::world_to_chunk_x(cx);
    int player_cy = World::world_to_chunk_y(cy);
    int clear_radius = world.render_radius() + 1;

    for (int dy = -clear_radius; dy <= clear_radius; ++dy) {
        for (int dx = -clear_radius; dx <= clear_radius; ++dx) {
            Chunk* chunk = world.get_chunk(player_cx + dx, player_cy + dy);
            if (!chunk) continue;
            for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
                for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
                    chunk->set_visible(lx, ly, false);
                }
            }
        }
    }

    // Origin is always visible
    world.set_visible(cx, cy, true);
    world.set_explored(cx, cy, true);

    // Cast rays in a circle
    int num_rays = radius * 8;
    for (int i = 0; i < num_rays; ++i) {
        double angle = (2.0 * M_PI * i) / num_rays;
        double dx = std::cos(angle);
        double dy = std::sin(angle);

        for (int r = 1; r <= radius; ++r) {
            int x = cx + static_cast<int>(std::round(dx * r));
            int y = cy + static_cast<int>(std::round(dy * r));

            // Check if the tile exists (chunk loaded)
            if (!world.in_bounds(x, y)) break;

            world.set_visible(x, y, true);
            world.set_explored(x, y, true);

            if (world.is_opaque(x, y)) break;
        }
    }
}

}
