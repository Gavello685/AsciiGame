#pragma once

#include <cstdint>

class World;
struct PlacedObject;

namespace light {

// Maximum tile light level.
inline constexpr uint8_t MAX_LEVEL = 15;

// Compute light levels for all loaded tiles near the player.
// Sets tile.light_level for each tile based on ambient + local sources.
void compute(World& world, int player_x, int player_y,
             uint8_t ambient_light, int player_torch_radius);

}

// Brightness multiplier for a tile's light level, per PLAN.md 4B: unlit tiles
// are near-black, below 4 is dim, and brightness ramps to full above that.
inline float light_shade(uint8_t level) {
    if (level == 0) return 0.10f;
    if (level < 4) return 0.35f + 0.05f * static_cast<float>(level);
    float t = static_cast<float>(level) / static_cast<float>(light::MAX_LEVEL);
    return 0.55f + 0.45f * t;
}
