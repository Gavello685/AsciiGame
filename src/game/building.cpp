#include "game/building.h"
#include "game/world.h"
#include "game/player.h"
#include "game/item.h"

static const BuildDef build_defs[] = {
    // name, tile, glyph, is_light_object, light_radius, materials
    { "Wood Wall",   TileType::WoodWall,   '#', false, 0, {{"Wood Plank", 1}} },
    { "Wood Floor",  TileType::WoodFloor,  '.', false, 0, {{"Wood Plank", 1}} },
    { "Wood Door",   TileType::WoodDoor,   '+', false, 0, {{"Wood Plank", 2}} },
    { "Stone Wall",  TileType::Wall,       '#', false, 0, {{"Stone Block", 1}} },
    { "Stone Floor", TileType::StoneFloor, '.', false, 0, {{"Stone Block", 1}} },
    { "Campfire",    TileType::None,       '*', true,  4, {{"Wood", 2}, {"Stone", 1}} },
    { "Torch Post",  TileType::None,       '*', true,  3, {{"Wood", 1}} },
};

const BuildDef& build_def(BuildTile t) {
    int idx = static_cast<int>(t);
    if (idx < 0 || idx >= static_cast<int>(BuildTile::Count)) idx = 0;
    return build_defs[idx];
}

bool build_can_afford(BuildTile t, const Player& player) {
    const BuildDef& def = build_def(t);
    for (const auto& [mat_name, needed] : def.materials) {
        int have = 0;
        for (const auto& [item, qty] : player.inventory()) {
            if (item.name() == mat_name) have += qty;
        }
        if (have < needed) return false;
    }
    return true;
}

bool build_can_place(BuildTile t, const World& world, int wx, int wy, std::string& reason) {
    if (!world.in_bounds(wx, wy)) {
        reason = "Out of range";
        return false;
    }
    Tile tile = world.get_tile(wx, wy);
    const BuildDef& def = build_def(t);

    if (def.is_light_object) {
        // Light objects go on any passable ground
        if (tile_blocks_movement(tile.type)) {
            reason = "Blocked";
            return false;
        }
        return true;
    }

    // Tiles: can't build on water, trees, or rock faces
    switch (tile.type) {
        case TileType::Water:
        case TileType::DeepWater:
        case TileType::Tree:
        case TileType::Mountain:
            reason = "Bad terrain";
            return false;
        default:
            break;
    }

    // Walls and doors occupy the tile — nothing may stand on it
    bool solid = tile_blocks_movement(def.tile);
    if (solid) {
        if (world.has_enemy_at(wx, wy) || world.has_npc_at(wx, wy)) {
            reason = "Occupied";
            return false;
        }
    }
    return true;
}

std::string build_place(World& world, Player& player, BuildTile t, int wx, int wy) {
    const BuildDef& def = build_def(t);

    std::string reason;
    if (!build_can_place(t, world, wx, wy, reason)) {
        return "Can't build here: " + reason;
    }
    if (!build_can_afford(t, player)) {
        return "Missing materials for " + std::string(def.name);
    }

    // Consume materials
    for (const auto& [mat_name, needed] : def.materials) {
        int remaining = needed;
        for (int i = 0; i < static_cast<int>(player.inventory().size()) && remaining > 0; ++i) {
            if (player.inventory_item(i).name() == mat_name) {
                int take = player.inventory_count(i);
                if (take > remaining) take = remaining;
                player.remove_item(i, take);
                remaining -= take;
                --i; // stack may have been erased; recheck this index
            }
        }
    }

    if (def.is_light_object) {
        PlacedObject obj;
        // world.add_placed_object converts to local coords
        obj.local_x = 0; obj.local_y = 0; // set by world
        obj.glyph = def.glyph;
        obj.name = def.name;
        if (def.light_radius >= 4) { obj.fg_r = 255; obj.fg_g = 160; obj.fg_b = 60; }
        else                       { obj.fg_r = 255; obj.fg_g = 200; obj.fg_b = 50; }
        obj.is_light = true;
        obj.light_radius = def.light_radius;
        world.add_placed_object(wx, wy, obj);
    } else {
        world.place_tile(wx, wy, def.tile);
    }

    return "Built " + std::string(def.name);
}

// ── Zones ─────────────────────────────────────────────────────────

const char* zone_type_name(ZoneType t) {
    switch (t) {
        case ZoneType::Farm:     return "Farm";
        case ZoneType::Storage:  return "Storage";
        case ZoneType::Barracks: return "Barracks";
        case ZoneType::Bed:      return "Bed";
        default:                 return "?";
    }
}

void zone_type_color(ZoneType t, uint8_t& r, uint8_t& g, uint8_t& b) {
    switch (t) {
        case ZoneType::Farm:     r = 60;  g = 120; b = 40;  break;
        case ZoneType::Storage:  r = 120; g = 90;  b = 40;  break;
        case ZoneType::Barracks: r = 120; g = 50;  b = 50;  break;
        case ZoneType::Bed:      r = 60;  g = 60;  b = 120; break;
        default:                 r = 60;  g = 60;  b = 60;  break;
    }
}
