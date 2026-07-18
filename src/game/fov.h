#pragma once

#include <cstdint>

class World;

namespace fov {

// Compute field of view from player position using world-space coordinates
// radius is the effective FOV radius (scales with ambient light)
void compute(World& world, int cx, int cy, int radius);

}
