#include "ui/world_view.h"

#include "game/biome.h"
#include "game/building.h"
#include "game/enemy.h"
#include "game/entity.h"
#include "game/light.h"
#include "game/npc.h"
#include "game/player.h"
#include "game/structure.h"
#include "game/time_system.h"
#include "game/world.h"
#include "ui/draw.h"

#include <algorithm>
#include <string>

namespace ui {

namespace {

const Colour HUD_BG{20, 20, 30};

// Explored-but-not-currently-visible tiles.
constexpr float MEMORY_SHADE = 0.3f;

}

Viewport make_viewport(const Renderer& renderer, int window_w, int window_h,
                       const Player& player) {
    Viewport view;
    view.cols = renderer.cell_width() > 0 ? window_w / renderer.cell_width() : 0;
    view.rows = renderer.cell_height() > 0 ? window_h / renderer.cell_height() : 0;
    view.cam_x = player.x() - view.cols / 2;
    view.cam_y = player.y() - view.map_rows() / 2;
    return view;
}

void draw_world(Renderer& r, const World& world, const Player& player,
                const Viewport& view) {
    // The spatial queries are non-const on World, but drawing does not modify
    // anything; casting here keeps the draw signature honest for callers.
    World& mutable_world = const_cast<World&>(world);

    for (int vy = 0; vy < view.map_rows(); ++vy) {
        for (int vx = 0; vx < view.cols; ++vx) {
            int mx = view.cam_x + vx;
            int my = view.cam_y + vy;

            Cell cell;
            cell.glyph = ' ';

            if (world.in_bounds(mx, my)) {
                Tile tile = world.get_tile(mx, my);

                // PLAN.md 4B: visible tiles are shaded by light level, so a lit
                // room reads differently from one lit only by the player's
                // torch. Remembered tiles stay uniformly dim.
                Colour fg{0, 0, 0};
                float brightness = 1.0f;
                if (tile.visible) {
                    cell.glyph = tile_glyph(tile.type);
                    tile_color(tile.type, fg.r, fg.g, fg.b);
                    brightness = light_shade(tile.light_level);
                } else if (tile.explored) {
                    cell.glyph = tile_glyph(tile.type);
                    tile_color(tile.type, fg.r, fg.g, fg.b);
                    brightness = MEMORY_SHADE;
                }
                fg = shade(fg, brightness);
                cell.fg_r = fg.r; cell.fg_g = fg.g; cell.fg_b = fg.b;

                // Zone tint, on anything the player has seen.
                if (tile.visible || tile.explored) {
                    if (const Zone* zone = world.zone_at(mx, my)) {
                        Colour zone_colour;
                        zone_type_color(zone->type, zone_colour.r, zone_colour.g, zone_colour.b);
                        Colour bg = shade(zone_colour, tile.visible ? 0.35f : 0.15f);
                        cell.bg_r = bg.r; cell.bg_g = bg.g; cell.bg_b = bg.b;
                    }
                }

                // Occupants draw over terrain in increasing priority. They keep
                // their full colour so they stay readable in the dark.
                if (tile.visible) {
                    if (const PlacedObject* object = world.placed_object_at(mx, my)) {
                        cell.glyph = object->glyph;
                        cell.fg_r = object->fg_r;
                        cell.fg_g = object->fg_g;
                        cell.fg_b = object->fg_b;
                    }
                    Entity* item = mutable_world.item_at(mx, my);
                    if (item && !item->in_inventory()) {
                        cell.glyph = item->glyph();
                        cell.fg_r = item->fg_r();
                        cell.fg_g = item->fg_g();
                        cell.fg_b = item->fg_b();
                    }
                    if (Npc* npc = mutable_world.npc_at(mx, my)) {
                        cell.glyph = npc->glyph();
                        cell.fg_r = npc->fg_r();
                        cell.fg_g = npc->fg_g();
                        cell.fg_b = npc->fg_b();
                    }
                    if (Enemy* enemy = mutable_world.enemy_at(mx, my)) {
                        cell.glyph = enemy->glyph();
                        cell.fg_r = enemy->fg_r();
                        cell.fg_g = enemy->fg_g();
                        cell.fg_b = enemy->fg_b();
                    }
                }
            }

            if (mx == player.x() && my == player.y()) {
                cell.glyph = '@';
                cell.fg_r = 0; cell.fg_g = 255; cell.fg_b = 0;
            }

            r.set_cell(vx, vy, cell);
        }
    }
}

void draw_build_cursor(Renderer& r, const World& world, const Player& player,
                       const UiState& ui, const Viewport& view) {
    int sx = ui.build_cx - view.cam_x;
    int sy = ui.build_cy - view.cam_y;
    if (!view.contains(sx, sy)) return;

    BuildTile selected = static_cast<BuildTile>(ui.build_sel);
    const BuildDef& def = build_def(selected);

    std::string reason;
    bool placeable = build_can_afford(selected, player) &&
                     build_can_place(selected, world, ui.build_cx, ui.build_cy, reason);

    Cell cell;
    cell.glyph = def.glyph;
    if (placeable) { cell.fg_r = 120; cell.fg_g = 255; cell.fg_b = 120; }
    else           { cell.fg_r = 255; cell.fg_g = 80;  cell.fg_b = 80; }
    cell.bg_r = 60; cell.bg_g = 60; cell.bg_b = 20;
    r.set_cell(sx, sy, cell);
}

void draw_zone_cursor(Renderer& r, const UiState& ui, const Viewport& view) {
    Colour zone_colour;
    zone_type_color(static_cast<ZoneType>(ui.zone_type_sel),
                    zone_colour.r, zone_colour.g, zone_colour.b);

    if (ui.zone_anchored) {
        int x0 = std::min(ui.zone_ax, ui.zone_cx);
        int y0 = std::min(ui.zone_ay, ui.zone_cy);
        int x1 = std::max(ui.zone_ax, ui.zone_cx);
        int y1 = std::max(ui.zone_ay, ui.zone_cy);

        Colour bg = shade(zone_colour, 0.5f);
        for (int wy = y0; wy <= y1; ++wy) {
            for (int wx = x0; wx <= x1; ++wx) {
                int sx = wx - view.cam_x;
                int sy = wy - view.cam_y;
                if (!view.contains(sx, sy)) continue;
                Cell cell;
                cell.glyph = ' ';
                cell.bg_r = bg.r; cell.bg_g = bg.g; cell.bg_b = bg.b;
                r.set_cell(sx, sy, cell);
            }
        }
    }

    int sx = ui.zone_cx - view.cam_x;
    int sy = ui.zone_cy - view.cam_y;
    if (!view.contains(sx, sy)) return;

    Colour bg = shade(zone_colour, 0.6f);
    Cell cell;
    cell.glyph = '+';
    cell.fg_r = 255; cell.fg_g = 255; cell.fg_b = 255;
    cell.bg_r = bg.r; cell.bg_g = bg.g; cell.bg_b = bg.b;
    r.set_cell(sx, sy, cell);
}

void draw_hud(Renderer& r, const World& world, const Player& player,
              const TimeSystem& time, const Viewport& view) {
    int stats_row = view.rows - 2;
    int world_row = view.rows - 1;

    draw_box(r, 0, stats_row, view.cols, Viewport::HUD_ROWS, HUD_BG);

    int variance = player.total_damage_variance();
    std::string stats =
        "HP:" + std::to_string(player.hp()) + "/" + std::to_string(player.max_hp()) +
        " ATK:" + std::to_string(player.total_attack() - variance) +
        "-" + std::to_string(player.total_attack() + variance) +
        " DEF:" + std::to_string(player.total_defense()) +
        " G:" + std::to_string(player.gold()) +
        " Wt:" + std::to_string(player.current_weight()) +
        "/" + std::to_string(player.carry_capacity()) +
        " Lv:" + std::to_string(player.level()) +
        " XP:" + std::to_string(player.xp()) + "/" + std::to_string(player.xp_to_next_level());

    std::string surroundings =
        "Day " + std::to_string(time.day()) + " " +
        time.time_string() + " " + time.time_period_string();

    surroundings += " [" + std::string(biome_name(world.get_biome_at(player.x(), player.y()))) + "]";

    StructureType structure = world.get_structure_at(player.x(), player.y());
    if (structure != StructureType::None) {
        surroundings += " " + std::string(structure_def(structure).name);
    }
    if (player.has_light_source()) surroundings += " [Torch]";

    draw_string(r, 1, stats_row, stats, Style{Colour{180, 180, 180}, HUD_BG});
    draw_string(r, 1, world_row, surroundings, Style{Colour{140, 140, 160}, HUD_BG});
}

void draw_message_log(Renderer& r, const MessageLog& log, const Viewport& view) {
    const auto& entries = log.entries();
    int base_y = view.rows - 3;

    // Walk newest first, stacking upward and fading with age.
    for (size_t age = 0; age < entries.size(); ++age) {
        const Message& message = entries[entries.size() - 1 - age];
        int y = base_y - static_cast<int>(age);
        if (y < 0) break;

        uint8_t brightness = static_cast<uint8_t>(255 - age * 50);
        uint8_t backdrop = static_cast<uint8_t>(40 - age * 8);
        Colour bg{backdrop, backdrop, static_cast<uint8_t>(backdrop / 2)};

        int width = std::min(static_cast<int>(message.text.size()) + 2, view.cols);
        draw_box(r, 0, y, width, 1, bg);
        draw_string(r, 1, y, message.text,
                    Style{Colour{brightness, brightness,
                                 static_cast<uint8_t>(100 + age * 10)}, bg});
    }
}

}
