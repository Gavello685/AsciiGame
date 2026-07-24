#pragma once

#include "game/tile.h"
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

class World;
class Player;

// ── Buildables ────────────────────────────────────────────────────

enum class BuildTile : uint8_t {
    WoodWall,
    WoodFloor,
    WoodDoor,
    StoneWall,
    StoneFloor,
    Campfire,
    TorchPost,
    Count
};

struct BuildDef {
    const char* name;
    TileType tile;                 // resulting tile (None for placed objects)
    uint32_t glyph;                // preview glyph
    bool is_light_object;          // placed as PlacedObject instead of a tile
    int light_radius;
    std::vector<std::pair<const char*, int>> materials; // item name, count
};

const BuildDef& build_def(BuildTile t);

// Check whether the player has the materials for a buildable
bool build_can_afford(BuildTile t, const Player& player);

// Check whether a buildable can be placed at world coords
bool build_can_place(BuildTile t, const World& world, int wx, int wy, std::string& reason);

// Consume materials and place the buildable. Returns a status message.
std::string build_place(World& world, Player& player, BuildTile t, int wx, int wy);

// ── Zones ─────────────────────────────────────────────────────────

enum class ZoneType : uint8_t {
    Farm,
    Storage,
    Barracks,
    Bed,
    Count
};

struct Zone {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // inclusive world-space rectangle
    ZoneType type = ZoneType::Farm;

    bool contains(int wx, int wy) const {
        return wx >= x0 && wx <= x1 && wy >= y0 && wy <= y1;
    }
};

const char* zone_type_name(ZoneType t);
void zone_type_color(ZoneType t, uint8_t& r, uint8_t& g, uint8_t& b);
