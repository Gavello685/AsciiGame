#include "ui/overlays.h"

#include "game/biome.h"
#include "game/building.h"
#include "game/crafting.h"
#include "game/dialogue.h"
#include "game/npc.h"
#include "game/player.h"
#include "game/structure.h"
#include "game/world.h"
#include "ui/draw.h"

#include <algorithm>
#include <string>
#include <vector>

namespace ui {

namespace {

const Colour HINT{100, 100, 100};
const Colour LABEL{180, 180, 180};
const Colour ROW_FG{200, 200, 200};
const Colour TITLE{255, 255, 255};
const Colour DISABLED{110, 110, 110};

const Colour INVENTORY_BG{15, 15, 35};
const Colour EXAMINE_BG{25, 25, 45};
const Colour GIFT_BG{30, 20, 20};
const Colour CRAFT_BG{15, 25, 20};
const Colour BUILD_BG{25, 20, 15};
const Colour ZONE_BG{20, 20, 30};
const Colour SHEET_BG{20, 20, 40};
const Colour MAP_BG{15, 15, 30};

// Highlight for the row or cell under a cursor, tinted to match its panel.
Colour selected_bg(const Colour& panel) {
    return Colour{static_cast<uint8_t>(std::min(255, panel.r + 25)),
                  static_cast<uint8_t>(std::min(255, panel.g + 25)),
                  static_cast<uint8_t>(std::min(255, panel.b + 25))};
}

// Offset of the first visible row so that `cursor` stays on screen.
int scroll_for(int cursor, int visible_rows) {
    return std::max(0, cursor - visible_rows + 1);
}

// Chunk markers reuse the structure's initial, so they need their own
// palette rather than the biome colour underneath them.
Colour structure_colour(StructureType type) {
    switch (type) {
        case StructureType::Village: return Colour{255, 215,  90};
        case StructureType::Cave:    return Colour{210, 140,  90};
        case StructureType::Ruins:   return Colour{170, 170, 190};
        case StructureType::Dungeon: return Colour{195,  70,  70};
        default:                     return Colour{255, 255, 255};
    }
}

Colour biome_colour(const BiomeConfig& config) {
    return Colour{config.hud_r, config.hud_g, config.hud_b};
}

// ── Inventory pieces ──────────────────────────────────────────────────

void draw_item_list(Renderer& r, const Player& player, const UiState& ui,
                    int x, int y, int visible_rows) {
    draw_string(r, x, y - 1, "Items:", Style{LABEL, INVENTORY_BG});

    const auto& inventory = player.inventory();
    if (inventory.empty()) {
        draw_string(r, x + 1, y, "(empty)", Style{HINT, INVENTORY_BG});
        return;
    }

    bool focused = (ui.inv_cursor >= 0);
    int scroll = focused ? scroll_for(ui.inv_cursor, visible_rows) : 0;

    for (int row = 0; row < visible_rows; ++row) {
        int idx = row + scroll;
        if (idx >= static_cast<int>(inventory.size())) break;

        const auto& [item, qty] = inventory[idx];
        bool selected = focused && idx == ui.inv_cursor;
        Style style{ROW_FG, selected ? selected_bg(INVENTORY_BG) : INVENTORY_BG};
        draw_item_row(r, x, y + row, item, qty, style);
    }
}

void draw_paper_doll(Renderer& r, const Player& player, const UiState& ui,
                     int x, int y) {
    draw_string(r, x, y - 1, "Equipment:", Style{LABEL, INVENTORY_BG});

    bool focused = (ui.inv_cursor < 0);
    for (int slot_index = 0; slot_index < NUM_EQUIP_SLOTS; ++slot_index) {
        int row_y = y + slot_index;
        bool selected = focused && slot_index == ui.equip_slot_cursor;
        Colour bg = selected ? selected_bg(INVENTORY_BG) : INVENTORY_BG;

        draw_string(r, x, row_y, EQUIP_SLOT_NAMES[slot_index],
                    Style{Colour{140, 140, 160}, bg});

        const Item* equipped = player.equipped_at(EQUIP_SLOTS[slot_index]);
        if (!equipped) {
            draw_string(r, x + 10, row_y, "-", Style{Colour{80, 80, 80}, bg});
            continue;
        }
        draw_glyph(r, x + 9, row_y, equipped->glyph(),
                   Colour{equipped->fg_r(), equipped->fg_g(), equipped->fg_b()}, bg);
        draw_string(r, x + 11, row_y, equipped->name(), Style{ROW_FG, bg});
    }
}

void draw_action_bar(Renderer& r, const Player& player, const UiState& ui,
                     int x, int y) {
    if (ui.inv_cursor < 0 || ui.inv_cursor >= static_cast<int>(player.inventory().size())) {
        return;
    }

    std::vector<ItemAction> actions = item_actions(player.inventory_item(ui.inv_cursor));
    const int column_width = 10;

    for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
        Colour bg = (i == ui.inv_action_cursor) ? Colour{60, 60, 50} : Colour{30, 30, 50};
        draw_string(r, x + i * column_width, y, item_action_name(actions[i]),
                    Style{ROW_FG, bg});
    }
}

void draw_examine_card(Renderer& r, const Item& item, int box_x, int box_y,
                       int box_w, int box_h) {
    const int w = 40, h = 8;
    int x = box_x + (box_w - w) / 2;
    int y = box_y + (box_h - h) / 2;

    draw_box(r, x, y, w, h, EXAMINE_BG);
    draw_string(r, x + 1, y, item.name(), Style{TITLE, EXAMINE_BG});

    // Descriptions can run past the card, so they wrap rather than bleed into
    // the panel behind it.
    std::vector<std::string> lines = wrap_text(item.description(), w - 2);
    for (int i = 0; i < static_cast<int>(lines.size()) && i < 2; ++i) {
        draw_string(r, x + 1, y + 1 + i, lines[i], Style{LABEL, EXAMINE_BG});
    }

    std::string stats = "Wt:" + std::to_string(item.weight()) +
                        "  Val:" + std::to_string(item.value());
    if (item.attack() > 0) stats += "  ATK:+" + std::to_string(item.attack());
    if (item.defense() > 0) stats += "  DEF:+" + std::to_string(item.defense());
    draw_string(r, x + 1, y + 4, stats, Style{Colour{140, 180, 140}, EXAMINE_BG});

    draw_string(r, x + 1, y + h - 1, "[ENTER] close", Style{HINT, EXAMINE_BG});
}

void draw_equip_select(Renderer& r, const Player& player, const UiState& ui,
                       int box_x, int box_y, int box_w, int box_h) {
    if (ui.inv_cursor < 0 ||
        ui.inv_cursor >= static_cast<int>(player.inventory().size())) {
        return;
    }

    const Item& item = player.inventory_item(ui.inv_cursor);
    const int w = 36, h = 7;
    int x = box_x + (box_w - w) / 2;
    int y = box_y + (box_h - h) / 2;

    draw_box(r, x, y, w, h, EXAMINE_BG);
    draw_string(r, x + 1, y, "Equip " + item.name() + " to:",
                Style{TITLE, EXAMINE_BG});

    for (int i = 0; i < 2; ++i) {
        EquipSlot slot = ui.equip_options[i];
        bool selected = (i == ui.equip_choice);
        Colour bg = selected ? Colour{60, 60, 50} : EXAMINE_BG;
        int row_y = y + 2 + i;

        std::string label = equip_slot_name(slot);
        const Item* occupant = player.equipped_at(slot);
        if (occupant) {
            label += "  (" + occupant->name() + ")";
        } else {
            label += "  -";
        }
        draw_string(r, x + 2, row_y, label, Style{ROW_FG, bg});
    }

    draw_string(r, x + 1, y + h - 1, "[ENTER] equip  [ESC] back",
                Style{HINT, EXAMINE_BG});
}

void draw_gift_picker(Renderer& r, const Player& player, const UiState& ui,
                      int box_x, int box_y, int box_h) {
    const auto& inventory = player.inventory();
    const int w = 30;
    int h = std::max(6, static_cast<int>(inventory.size()) + 4);
    int x = box_x + 2;
    int y = box_y + box_h - h - 1;

    draw_box(r, x, y, w, h, GIFT_BG);
    draw_string(r, x + 1, y, "Gift which item?", Style{Colour{255, 200, 200}, GIFT_BG});

    int visible_rows = h - 2;
    int scroll = scroll_for(ui.gift_cursor, visible_rows);
    for (int row = 0; row < visible_rows; ++row) {
        int idx = row + scroll;
        if (idx >= static_cast<int>(inventory.size())) break;

        const auto& [item, qty] = inventory[idx];
        bool selected = (idx == ui.gift_cursor);
        Style style{ROW_FG, selected ? Colour{60, 60, 20} : GIFT_BG};
        draw_item_row(r, x + 1, y + 1 + row, item, qty, style);
    }
}

}

void draw_inventory(Renderer& r, const Player& player, const UiState& ui,
                    const Viewport& view) {
    int box_w = std::min(view.cols - 4, 60);
    int box_h = std::min(view.rows - 6, 28);
    const int box_x = 2, box_y = 2;
    int panel_w = (box_w - 4) / 2;

    draw_box(r, box_x, box_y, box_w, box_h, INVENTORY_BG);
    draw_string(r, box_x + 1, box_y, "INVENTORY", Style{TITLE, INVENTORY_BG});

    std::string weight = "Wt: " + std::to_string(player.current_weight()) + "/" +
                         std::to_string(player.carry_capacity());
    std::string gold = "Gold: " + std::to_string(player.gold());
    int weight_x = box_x + box_w - 1;
    draw_right_aligned(r, weight_x, box_y, weight, Style{LABEL, INVENTORY_BG});
    draw_right_aligned(r, weight_x - static_cast<int>(weight.size()) - 2, box_y, gold,
                       Style{Colour{255, 215, 0}, INVENTORY_BG});

    int rows_top = box_y + 2;
    int visible_rows = box_h - 4;
    int doll_x = box_x + panel_w + 3;

    draw_item_list(r, player, ui, box_x + 1, rows_top, visible_rows);
    draw_paper_doll(r, player, ui, doll_x, rows_top);

    if (ui.mode == GameMode::InventoryAction) {
        draw_action_bar(r, player, ui, box_x + 2, box_y + box_h - 2);
    }
    if (ui.mode == GameMode::InventoryExamine) {
        draw_examine_card(r, ui.examine_item, box_x, box_y, box_w, box_h);
    }
    if (ui.mode == GameMode::EquipSelect) {
        draw_equip_select(r, player, ui, box_x, box_y, box_w, box_h);
    }
    if (ui.mode == GameMode::GiftSelect) {
        draw_gift_picker(r, player, ui, box_x, box_y, box_h);
    }

    draw_string(r, doll_x, box_y + box_h - 1, "[TAB] switch", Style{HINT, INVENTORY_BG});
}

void draw_craft(Renderer& r, const Player& player, const UiState& ui,
                const Viewport& view) {
    const auto& recipes = recipe_db();

    int box_w = std::min(view.cols - 4, 56);
    int box_h = std::min(view.rows - 6, static_cast<int>(recipes.size()) + 5);
    int box_x = (view.cols - box_w) / 2;
    const int box_y = 2;

    draw_box(r, box_x, box_y, box_w, box_h, CRAFT_BG);
    draw_string(r, box_x + 1, box_y, "CRAFTING", Style{TITLE, CRAFT_BG});

    int visible_rows = box_h - 4;
    int scroll = scroll_for(ui.craft_cursor, visible_rows);

    for (int row = 0; row < visible_rows; ++row) {
        int idx = row + scroll;
        if (idx >= static_cast<int>(recipes.size())) break;

        const Recipe& recipe = recipes[idx];
        int y = box_y + 2 + row;
        bool selected = (idx == ui.craft_cursor);
        bool affordable = craft_can_make(recipe, player);
        Colour bg = selected ? Colour{35, 55, 40} : CRAFT_BG;

        std::string name = recipe.result;
        if (recipe.result_qty > 1) name += " x" + std::to_string(recipe.result_qty);
        draw_string(r, box_x + 2, y, name,
                    Style{affordable ? Colour{200, 255, 200} : DISABLED, bg});

        // "Wood 1/2, Iron Ore 0/2"
        std::string ingredients;
        for (size_t i = 0; i < recipe.ingredients.size(); ++i) {
            if (i > 0) ingredients += ", ";
            const auto& [item_name, needed] = recipe.ingredients[i];
            ingredients += std::string(item_name) + " " +
                           std::to_string(craft_count_of(player, item_name)) + "/" +
                           std::to_string(needed);
        }
        draw_string(r, box_x + 24, y, ingredients, Style{Colour{160, 160, 140}, bg});
    }

    draw_string(r, box_x + 1, box_y + box_h - 1, "[ENTER] craft  [ESC] close",
                Style{HINT, CRAFT_BG});
}

void draw_build_panel(Renderer& r, const Player& player, const UiState& ui) {
    const int count = static_cast<int>(BuildTile::Count);
    const int box_w = 44;
    int box_h = count + 4;
    const int box_x = 1, box_y = 1;

    draw_box(r, box_x, box_y, box_w, box_h, BUILD_BG);
    draw_string(r, box_x + 1, box_y, "BUILD", Style{TITLE, BUILD_BG});

    for (int i = 0; i < count; ++i) {
        BuildTile buildable = static_cast<BuildTile>(i);
        const BuildDef& def = build_def(buildable);
        int y = box_y + 2 + i;
        bool selected = (i == ui.build_sel);
        bool affordable = build_can_afford(buildable, player);
        Colour bg = selected ? Colour{55, 45, 30} : BUILD_BG;

        draw_glyph(r, box_x + 1, y, def.glyph,
                   affordable ? Colour{220, 180, 100} : DISABLED, bg);
        draw_string(r, box_x + 3, y, def.name,
                    Style{affordable ? Colour{220, 220, 220} : DISABLED, bg});

        // Same "name have/need" form as the crafting panel, so two-material
        // rows (campfire) are readable rather than a bare "0/2, 0/1".
        std::string cost;
        for (size_t m = 0; m < def.materials.size(); ++m) {
            if (m > 0) cost += ", ";
            const auto& [material, needed] = def.materials[m];
            cost += std::string(material) + " " +
                    std::to_string(craft_count_of(player, material)) + "/" +
                    std::to_string(needed);
        }
        draw_string(r, box_x + 16, y, cost, Style{Colour{150, 150, 130}, bg});
    }

    draw_string(r, box_x + 1, box_y + box_h - 1, "[TAB] pick [ENT] build",
                Style{HINT, BUILD_BG});
}

void draw_zone_panel(Renderer& r, const World& world, const UiState& ui) {
    const int box_w = 30;
    int box_h = std::min(14, 6 + static_cast<int>(world.zones().size()));
    const int box_x = 1, box_y = 1;

    draw_box(r, box_x, box_y, box_w, box_h, ZONE_BG);
    draw_string(r, box_x + 1, box_y, "ZONES", Style{TITLE, ZONE_BG});

    if (ui.zone_pick_type) {
        draw_string(r, box_x + 1, box_y + 2, "Zone type:", Style{LABEL, ZONE_BG});
        for (int i = 0; i < static_cast<int>(ZoneType::Count); ++i) {
            ZoneType type = static_cast<ZoneType>(i);
            Colour colour;
            zone_type_color(type, colour.r, colour.g, colour.b);

            bool selected = (i == ui.zone_type_sel);
            std::string label = (selected ? "> " : "  ") + std::string(zone_type_name(type));
            Colour fg = selected ? Colour{255, 255, 100}
                                 : Colour{static_cast<uint8_t>(std::min(255, colour.r + 80)),
                                          static_cast<uint8_t>(std::min(255, colour.g + 80)),
                                          static_cast<uint8_t>(std::min(255, colour.b + 80))};
            draw_string(r, box_x + 2, box_y + 3 + i, label, Style{fg, ZONE_BG});
        }
        draw_string(r, box_x + 1, box_y + box_h - 1, "[ENT] confirm type",
                    Style{HINT, ZONE_BG});
        return;
    }

    draw_string(r, box_x + 1, box_y + 2,
                ui.zone_anchored ? "Pick second corner" : "Pick first corner",
                Style{Colour{150, 150, 170}, ZONE_BG});

    int y = box_y + 4;
    for (const Zone& zone : world.zones()) {
        if (y >= box_y + box_h - 1) break;
        Colour colour;
        zone_type_color(zone.type, colour.r, colour.g, colour.b);

        std::string label = std::string(zone_type_name(zone.type)) + " (" +
                            std::to_string(zone.x1 - zone.x0 + 1) + "x" +
                            std::to_string(zone.y1 - zone.y0 + 1) + ")";
        Colour fg{static_cast<uint8_t>(std::min(255, colour.r + 100)),
                  static_cast<uint8_t>(std::min(255, colour.g + 100)),
                  static_cast<uint8_t>(std::min(255, colour.b + 100))};
        draw_string(r, box_x + 2, y, label, Style{fg, ZONE_BG});
        ++y;
    }
    if (world.zones().empty()) {
        draw_string(r, box_x + 2, y, "(none)", Style{Colour{90, 90, 90}, ZONE_BG});
    }

    draw_string(r, box_x + 1, box_y + box_h - 1, "[ENT] mark [X] del",
                Style{HINT, ZONE_BG});
}

void draw_character_sheet(Renderer& r, const Player& player, const Viewport& view) {
    const int box_w = 34, box_h = 20;
    int box_x = (view.cols - box_w) / 2;
    int box_y = (view.rows - box_h) / 2;

    draw_box(r, box_x, box_y, box_w, box_h, SHEET_BG);
    draw_string(r, box_x + 1, box_y, "CHARACTER", Style{TITLE, SHEET_BG});
    draw_string(r, box_x + 1, box_y + 1,
                "Level " + std::to_string(player.level()) +
                    "  XP " + std::to_string(player.xp()) + "/" +
                    std::to_string(player.xp_to_next_level()),
                Style{LABEL, SHEET_BG});

    const PlayerStats& stats = player.stats();

    // Each attribute is listed with the derived stat it feeds, so the sheet
    // explains why the numbers matter.
    const std::pair<const char*, std::pair<int, std::string>> rows[] = {
        {"STR", {stats.str,   "carry " + std::to_string(stats.carry_capacity())}},
        {"DEX", {stats.dex,   "defense +" + std::to_string(stats.defense_bonus())}},
        {"CON", {stats.con,   "max HP " + std::to_string(stats.max_hp())}},
        {"INT", {stats.intel, "arcana"}},
        {"WIS", {stats.wis,   "perception " + std::to_string(stats.perception())}},
        {"CHA", {stats.cha,   "social " + std::to_string(stats.social_modifier())}},
    };

    for (int i = 0; i < static_cast<int>(sizeof(rows) / sizeof(rows[0])); ++i) {
        const auto& [name, detail] = rows[i];
        int y = box_y + 3 + i;
        draw_string(r, box_x + 2, y, std::string(name) + " " + std::to_string(detail.first),
                    Style{ROW_FG, SHEET_BG});
        draw_string(r, box_x + 12, y, detail.second, Style{Colour{120, 120, 150}, SHEET_BG});
    }

    int variance = player.total_damage_variance();
    const std::pair<std::string, Colour> totals[] = {
        {"Attack:  " + std::to_string(player.total_attack() - variance) + "-" +
             std::to_string(player.total_attack() + variance), Colour{220, 180, 140}},
        {"Defense: " + std::to_string(player.total_defense()), Colour{160, 200, 160}},
        {"HP:      " + std::to_string(player.hp()) + "/" + std::to_string(player.max_hp()),
         Colour{220, 140, 140}},
        {"Gold:    " + std::to_string(player.gold()), Colour{255, 215, 0}},
        {"Weight:  " + std::to_string(player.current_weight()) + "/" +
             std::to_string(player.carry_capacity()), Colour{160, 160, 160}},
    };
    for (int i = 0; i < static_cast<int>(sizeof(totals) / sizeof(totals[0])); ++i) {
        draw_string(r, box_x + 2, box_y + 10 + i, totals[i].first,
                    Style{totals[i].second, SHEET_BG});
    }

    draw_string(r, box_x + 1, box_y + box_h - 1, "[X/ESC] close", Style{HINT, SHEET_BG});
}

void draw_world_map(Renderer& r, const World& world, const Player& player,
                    UiState& ui, const Viewport& view) {
    const int radius = MAP_RADIUS;
    const int grid = 2 * radius + 1;
    const int legend_w = 24;

    int box_w = grid + 2 + legend_w;
    int box_h = grid + 4;   // title row, grid with border, status row
    int box_x = std::max(0, (view.cols - box_w) / 2);
    int box_y = std::max(1, (view.rows - box_h) / 2);

    int player_cx = World::world_to_chunk_x(player.x());
    int player_cy = World::world_to_chunk_y(player.y());

    // The summaries come from noise, so they only change when the player
    // crosses into a different chunk.
    if (ui.map_cache.size() != static_cast<size_t>(grid * grid) ||
        ui.map_cache_cx != player_cx || ui.map_cache_cy != player_cy) {
        ui.map_cache.resize(static_cast<size_t>(grid) * grid);
        ui.map_cache_cx = player_cx;
        ui.map_cache_cy = player_cy;
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                ui.map_cache[(dy + radius) * grid + (dx + radius)] =
                    world.chunk_map_info(player_cx + dx, player_cy + dy);
            }
        }
    }

    draw_box(r, box_x, box_y, box_w, box_h, MAP_BG);
    draw_string(r, box_x + 1, box_y, " WORLD MAP", Style{TITLE, MAP_BG});
    draw_right_aligned(r, box_x + box_w - 1, box_y, "[WASD move  M/ESC close]",
                       Style{Colour{110, 110, 130}, MAP_BG});

    const Colour border{90, 90, 120};
    for (int i = 0; i < grid + 2; ++i) {
        draw_glyph(r, box_x + i, box_y + 1, '#', border, MAP_BG);
        draw_glyph(r, box_x + i, box_y + 2 + grid, '#', border, MAP_BG);
    }
    for (int i = 0; i < grid; ++i) {
        draw_glyph(r, box_x, box_y + 2 + i, '#', border, MAP_BG);
        draw_glyph(r, box_x + grid + 1, box_y + 2 + i, '#', border, MAP_BG);
    }

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const World::ChunkMapInfo& info = ui.map_cache[(dy + radius) * grid + (dx + radius)];
            const BiomeConfig& config = biome_config(info.biome);

            Cell cell;
            cell.glyph = '.';
            Colour fg = biome_colour(config);
            Colour bg = shade(fg, 1.0f / 3.0f);

            if (info.structure != StructureType::None) {
                cell.glyph = structure_def(info.structure).name[0];
                fg = structure_colour(info.structure);
            }

            bool is_player = (dx == 0 && dy == 0);
            if (is_player) {
                cell.glyph = '@';
                fg = Colour{0, 255, 0};
                bg = Colour{20, 50, 20};
            }
            if (player_cx + dx == ui.map_cursor_cx && player_cy + dy == ui.map_cursor_cy) {
                // Invert the cell so the cursor reads at a glance against any
                // biome colour underneath it.
                bg = Colour{230, 230, 205};
                fg = is_player ? Colour{0, 130, 0} : Colour{20, 20, 20};
            }

            cell.fg_r = fg.r; cell.fg_g = fg.g; cell.fg_b = fg.b;
            cell.bg_r = bg.r; cell.bg_g = bg.g; cell.bg_b = bg.b;
            r.set_cell(box_x + 1 + (dx + radius), box_y + 2 + (dy + radius), cell);
        }
    }

    int rel_x = ui.map_cursor_cx - player_cx;
    int rel_y = ui.map_cursor_cy - player_cy;
    const World::ChunkMapInfo& under_cursor =
        ui.map_cache[(rel_y + radius) * grid + (rel_x + radius)];

    std::string status = biome_name(under_cursor.biome);
    if (under_cursor.structure != StructureType::None) {
        status += std::string(" | ") + structure_def(under_cursor.structure).name;
    }
    status += "  (" + std::to_string(ui.map_cursor_cx) + "," +
              std::to_string(ui.map_cursor_cy) + ")";
    if (rel_x == 0 && rel_y == 0) {
        status += "  [you]";
    } else {
        status += "  ";
        if (rel_y < 0) status += std::to_string(-rel_y) + "N";
        if (rel_y > 0) status += std::to_string(rel_y) + "S";
        if (rel_x < 0) status += std::to_string(-rel_x) + "W";
        if (rel_x > 0) status += std::to_string(rel_x) + "E";
    }
    draw_string(r, box_x + 1, box_y + box_h - 1, status, Style{ROW_FG, MAP_BG});

    int legend_x = box_x + grid + 3;
    draw_string(r, legend_x, box_y + 1, "BIOMES", Style{Colour{150, 150, 170}, MAP_BG});
    for (int i = 0; i < static_cast<int>(BiomeType::BiomeCount); ++i) {
        BiomeType biome = static_cast<BiomeType>(i);
        int y = box_y + 2 + i;
        draw_glyph(r, legend_x, y, '#', biome_colour(biome_config(biome)), MAP_BG);
        draw_string(r, legend_x + 2, y, biome_name(biome), Style{LABEL, MAP_BG});
    }

    int legend_y = box_y + 3 + static_cast<int>(BiomeType::BiomeCount);
    draw_string(r, legend_x, legend_y, "STRUCTURES", Style{Colour{150, 150, 170}, MAP_BG});
    for (int i = 1; i < static_cast<int>(StructureType::StructureCount); ++i) {
        StructureType type = static_cast<StructureType>(i);
        const StructureDef& def = structure_def(type);
        int y = legend_y + i;
        draw_glyph(r, legend_x, y, def.name[0], structure_colour(type), MAP_BG);
        draw_string(r, legend_x + 2, y, def.name, Style{LABEL, MAP_BG});
    }

    draw_string(r, legend_x, box_y + box_h - 1, "@ = you",
                Style{Colour{0, 255, 0}, MAP_BG});
}

void draw_dialogue(Renderer& r, World& world, const UiState& ui,
                   const Viewport& view) {
    Npc* speaker = world.npc_at(ui.dialogue_npc_x, ui.dialogue_npc_y);
    if (!speaker) return;

    const auto& tree = speaker->dialogue_tree();
    if (ui.dialogue_node < 0 || ui.dialogue_node >= static_cast<int>(tree.size())) return;
    const DialogueNode& node = tree[ui.dialogue_node];

    int box_w = std::min(view.cols - 4, 50);
    std::vector<std::string> lines = wrap_text(node.speaker_text, box_w - 2);

    int box_h = std::max(7, static_cast<int>(lines.size() + node.options.size()) + 4);
    const int box_x = 2;
    int box_y = std::max(0, view.rows - box_h - 3);

    draw_box(r, box_x, box_y, box_w, box_h, SHEET_BG);

    draw_string(r, box_x + 1, box_y, speaker->name(),
                Style{Colour{speaker->fg_r(), speaker->fg_g(), speaker->fg_b()}, SHEET_BG});

    // The label explains what the number gates, which is otherwise invisible
    // until an option is refused.
    std::string affinity = std::string(speaker->affinity_label()) + " " +
                           std::to_string(speaker->affinity());
    draw_right_aligned(r, box_x + box_w - 1, box_y, affinity,
                       Style{Colour{140, 140, 140}, SHEET_BG});

    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        draw_string(r, box_x + 1, box_y + 1 + i, lines[i], Style{ROW_FG, SHEET_BG});
    }

    int options_y = box_y + static_cast<int>(lines.size()) + 2;
    for (int i = 0; i < static_cast<int>(node.options.size()); ++i) {
        bool selected = (i == ui.dialogue_option);
        Colour bg = selected ? Colour{60, 60, 40} : SHEET_BG;
        std::string label = (selected ? "> " : "  ") + node.options[i].text;
        draw_string(r, box_x + 1, options_y + i, label,
                    Style{Colour{200, 200, 100}, bg});
    }
}

}
