#pragma once

#include <cstdint>

class World;
struct PlacedObject;

namespace light {

// Compute light levels for all loaded tiles near the player.
// Sets tile.light_level for each tile based on ambient + local sources.
void compute(World& world, int player_x, int player_y,
             uint8_t ambient_light, int player_torch_radius);

}
