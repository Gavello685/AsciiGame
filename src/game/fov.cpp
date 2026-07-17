#include "game/fov.h"
#include <cmath>
#include <algorithm>

namespace fov {

static bool is_opaque(const Map& map, int x, int y) {
    if (!map.in_bounds(x, y)) return true;
    Tile t = map.get(x, y);
    return t.type == TileType::Wall;
}

void compute(Map& map, int cx, int cy, int radius) {
    // Clear current visibility
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            Tile t = map.get(x, y);
            t.visible = false;
            map.set_visible(x, y, false);
        }
    }

    // Origin is always visible
    if (map.in_bounds(cx, cy)) {
        map.set_visible(cx, cy, true);
        map.set_explored(cx, cy, true);
    }

    // Cast rays in a circle
    int num_rays = radius * 8;
    for (int i = 0; i < num_rays; ++i) {
        double angle = (2.0 * M_PI * i) / num_rays;
        double dx = std::cos(angle);
        double dy = std::sin(angle);

        for (int r = 1; r <= radius; ++r) {
            int x = cx + static_cast<int>(std::round(dx * r));
            int y = cy + static_cast<int>(std::round(dy * r));

            if (!map.in_bounds(x, y)) break;

            map.set_visible(x, y, true);
            map.set_explored(x, y, true);

            if (is_opaque(map, x, y)) break;
        }
    }
}

}
