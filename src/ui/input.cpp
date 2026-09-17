#include "ui/input.h"

#include "game/building.h"
#include "game/chunk.h"
#include "game/crafting.h"
#include "game/dialogue.h"
#include "game/enemy.h"
#include "game/entity.h"
#include "game/item.h"
#include "game/npc.h"
#include "game/structure.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace ui {

namespace {

// Arrow keys and WASD are interchangeable everywhere something moves: the
// player, the build cursor, the zone cursor and the world map cursor.
bool movement_delta(Key key, int& dx, int& dy) {
    switch (key) {
        case Key::W: case Key::Up:    dx =  0; dy = -1; return true;
        case Key::S: case Key::Down:  dx =  0; dy =  1; return true;
        case Key::A: case Key::Left:  dx = -1; dy =  0; return true;
        case Key::D: case Key::Right: dx =  1; dy =  0; return true;
        default: return false;
    }
}

bool confirms(Key key) { return key == Key::Enter || key == Key::Space; }

// Vertical list navigation, clamped at both ends. A count of zero leaves the
// cursor where it is.
void navigate_list(Key key, int& cursor, int count) {
    if (key == Key::Up && cursor > 0) cursor--;
    if (key == Key::Down && cursor < count - 1) cursor++;
}

// Wrap a selection index forward or backward through a fixed set.
void cycle(int& selection, int count, int delta) {
    if (count <= 0) return;
    selection = (selection + delta % count + count) % count;
}

bool has_equipped(const Player& player, const std::string& name) {
    for (const auto& [slot, item] : player.equipment()) {
        if (item.name() == name) return true;
    }
    return false;
}

int slot_order(EquipSlot slot) {
    for (int i = 0; i < NUM_EQUIP_SLOTS; ++i) {
        if (EQUIP_SLOTS[i] == slot) return i;
    }
    return NUM_EQUIP_SLOTS;
}

// Fill EquipSelect with the two paired slots, listed in paper-doll order
// (left before right). Cursor starts on an empty slot if one is free.
void begin_equip_select(UiState& ui, const Player& player, const Item& item) {
    EquipSlot primary = item.equip_slot();
    EquipSlot partner = Item::partner_slot(primary);
    if (slot_order(primary) <= slot_order(partner)) {
        ui.equip_options[0] = primary;
        ui.equip_options[1] = partner;
    } else {
        ui.equip_options[0] = partner;
        ui.equip_options[1] = primary;
    }

    bool first_free = !player.is_slot_occupied(ui.equip_options[0]);
    bool second_free = !player.is_slot_occupied(ui.equip_options[1]);
    if (first_free && !second_free) ui.equip_choice = 0;
    else if (!first_free && second_free) ui.equip_choice = 1;
    else ui.equip_choice = (ui.equip_options[0] == primary) ? 0 : 1;

    ui.mode = GameMode::EquipSelect;
}

// Announce a structure the first time the player stands in its chunk.
void check_discovery(InputContext& ctx) {
    int cx = World::world_to_chunk_x(ctx.session.player.x());
    int cy = World::world_to_chunk_y(ctx.session.player.y());

    const Chunk* chunk = ctx.session.world.get_chunk(cx, cy);
    if (!chunk) return;

    int structure_id = chunk->structure_id();
    if (structure_id == static_cast<int>(StructureType::None)) return;
    if (!ctx.ui.discovered_structures.insert({cx, cy}).second) return;

    const StructureDef& def = structure_def(static_cast<StructureType>(structure_id));
    ctx.ui.messages.push(std::string("You discover a ") + def.name + "!", 150);
}

void begin_new_game(InputContext& ctx) {
    std::string arrival = session_start(ctx.session, ctx.settings, ctx.new_game_seed);
    ctx.ui.reset_for_new_run();
    ctx.ui.messages.push(arrival, 150);
}

void return_to_main_menu(InputContext& ctx) {
    ctx.session.started = false;
    ctx.ui.mode = GameMode::MainMenu;
    ctx.ui.menu_cursor = 0;
}

// ── Menus ─────────────────────────────────────────────────────────────

InputResult handle_main_menu(InputContext& ctx, Key key) {
    InputResult out;
    navigate_list(key, ctx.ui.menu_cursor, MAIN_MENU_OPTION_COUNT);

    if (key == Key::Escape) {
        out.quit = true;
        return out;
    }
    if (!confirms(key)) return out;

    switch (static_cast<MainMenuItem>(ctx.ui.menu_cursor)) {
        case MainMenuItem::NewGame:
            begin_new_game(ctx);
            out.fov_dirty = true;
            break;
        case MainMenuItem::LoadGame:
            ctx.ui.load_from_menu = true;
            ctx.ui.pause_cursor = 0;
            ctx.ui.mode = GameMode::LoadGame;
            break;
        case MainMenuItem::Quit:
            out.quit = true;
            break;
    }
    return out;
}

InputResult handle_dead(InputContext& ctx, Key key) {
    InputResult out;
    if (key == Key::R) {
        begin_new_game(ctx);
        out.fov_dirty = true;
    } else if (key == Key::Escape) {
        return_to_main_menu(ctx);
    }
    return out;
}

InputResult handle_pause_menu(InputContext& ctx, Key key) {
    InputResult out;
    if (key == Key::Escape) {
        ctx.ui.mode = GameMode::Normal;
        return out;
    }

    navigate_list(key, ctx.ui.pause_cursor, PAUSE_MENU_OPTION_COUNT);
    if (!confirms(key)) return out;

    switch (static_cast<PauseMenuItem>(ctx.ui.pause_cursor)) {
        case PauseMenuItem::Save:
            ctx.ui.pause_cursor = 0;
            ctx.ui.mode = GameMode::SaveGame;
            break;
        case PauseMenuItem::Load:
            ctx.ui.load_from_menu = false;
            ctx.ui.pause_cursor = 0;
            ctx.ui.mode = GameMode::LoadGame;
            break;
        case PauseMenuItem::Settings:
            ctx.ui.settings_cursor = 0;
            ctx.ui.mode = GameMode::Settings;
            break;
        case PauseMenuItem::MainMenu:
            return_to_main_menu(ctx);
            break;
        case PauseMenuItem::Quit:
            out.quit = true;
            break;
    }
    return out;
}

InputResult handle_settings(InputContext& ctx, Key key) {
    InputResult out;
    if (key == Key::Escape || key == Key::Enter) {
        save_settings(ctx.settings);
        ctx.ui.mode = GameMode::PauseMenu;
        return out;
    }

    navigate_list(key, ctx.ui.settings_cursor, SETTINGS_OPTION_COUNT);

    int delta = 0;
    if (key == Key::Left) delta = -1;
    if (key == Key::Right) delta = 1;
    if (delta == 0) return out;

    switch (ctx.ui.settings_cursor) {
        case 0: ctx.settings.load_radius += delta; break;
        case 1: ctx.settings.render_radius += delta; break;
        case 2: ctx.settings.sim_radius += delta; break;
        default: break;
    }

    // The world rejects combinations that break the nesting rules, so the
    // sliders show what it accepted rather than what was asked for.
    session_apply_settings(ctx.session, ctx.settings);
    ctx.session.world.update_loaded_chunks(ctx.session.player.x(), ctx.session.player.y());
    out.fov_dirty = true;
    return out;
}

InputResult handle_save_game(InputContext& ctx, Key key) {
    InputResult out;
    if (key == Key::Escape) {
        ctx.ui.pause_cursor = 0;
        ctx.ui.mode = GameMode::PauseMenu;
        return out;
    }

    navigate_list(key, ctx.ui.pause_cursor, MAX_SAVE_SLOTS);
    if (!confirms(key)) return out;

    int slot = ctx.ui.pause_cursor;
    if (save_game(slot, session_capture(ctx.session))) {
        ctx.ui.messages.push("Game saved to slot " + std::to_string(slot + 1));
    } else {
        ctx.ui.messages.push("Save failed!");
    }
    ctx.ui.mode = GameMode::Normal;
    return out;
}

InputResult handle_load_game(InputContext& ctx, Key key) {
    InputResult out;
    if (key == Key::Escape) {
        if (!ctx.ui.load_from_menu) ctx.ui.pause_cursor = 0;
        ctx.ui.mode = ctx.ui.load_from_menu ? GameMode::MainMenu : GameMode::PauseMenu;
        return out;
    }

    navigate_list(key, ctx.ui.pause_cursor, MAX_SAVE_SLOTS);
    if (!confirms(key)) return out;

    int slot = ctx.ui.pause_cursor;
    GameState state;
    if (!load_game(slot, state)) {
        ctx.ui.messages.push("No save in slot " + std::to_string(slot + 1));
        ctx.ui.mode = ctx.ui.load_from_menu ? GameMode::MainMenu : GameMode::Normal;
        return out;
    }

    session_restore(ctx.session, state, ctx.settings);
    ctx.ui.reset_for_new_run();
    ctx.ui.messages.push("Game loaded from slot " + std::to_string(slot + 1));
    out.fov_dirty = true;
    return out;
}

// ── Conversation ──────────────────────────────────────────────────────

// Apply a gift to the NPC and report their reaction.
void give_gift(InputContext& ctx, Npc& npc, int inventory_index) {
    Player& player = ctx.session.player;

    // A copy: remove_item erases the stack when it empties, which would leave
    // a reference into the inventory dangling.
    Item item = player.inventory_item(inventory_index);
    GiftReaction reaction = npc.react_to_gift(item);

    int change = 0;
    switch (reaction) {
        case GiftReaction::Love:    change =  15; break;
        case GiftReaction::Like:    change =   8; break;
        case GiftReaction::Neutral: change =   2; break;
        case GiftReaction::Dislike: change =  -5; break;
        case GiftReaction::Hate:    change = -12; break;
    }
    npc.adjust_affinity(change);
    player.remove_item(inventory_index, 1);

    ctx.ui.messages.push(npc.name() + ": \"" + npc.gift_reaction_text(reaction) + "\"", 150);
}

// The NPC on or next to the player's tile, preferring the closest.
Npc* npc_within_reach(World& world, const Player& player) {
    const int offsets[][2] = {{0, 0}, {0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    for (const auto& offset : offsets) {
        if (Npc* npc = world.npc_at(player.x() + offset[0], player.y() + offset[1])) {
            return npc;
        }
    }
    return nullptr;
}

InputResult handle_dialogue(InputContext& ctx, Key key) {
    InputResult out;
    UiState& ui = ctx.ui;

    // The NPC is addressed by position, so it may have moved or been unloaded
    // since the overlay opened.
    Npc* speaker = ctx.session.world.npc_at(ui.dialogue_npc_x, ui.dialogue_npc_y);
    if (!speaker) {
        ui.mode = GameMode::Normal;
        return out;
    }

    const auto& tree = speaker->dialogue_tree();
    if (ui.dialogue_node < 0 || ui.dialogue_node >= static_cast<int>(tree.size())) {
        ui.mode = GameMode::Normal;
        return out;
    }

    if (key == Key::Escape) {
        ui.mode = GameMode::Normal;
        return out;
    }

    const DialogueNode& node = tree[ui.dialogue_node];
    navigate_list(key, ui.dialogue_option, static_cast<int>(node.options.size()));

    if (!confirms(key) || node.options.empty()) return out;

    const DialogueOption& option = node.options[ui.dialogue_option];
    switch (option.action) {
        case DialogueAction::Exit:
            ui.mode = GameMode::Normal;
            break;
        case DialogueAction::Talk:
            ui.dialogue_node = option.next_node;
            ui.dialogue_option = 0;
            break;
        // DESIGN.md affinity thresholds: gifts need more than Wary, trade
        // needs more than Friendly on top of being a merchant.
        case DialogueAction::Trade:
            if (speaker->can_trade()) {
                trade_open(ctx.trade, speaker);
            } else if (speaker->is_merchant()) {
                ui.messages.push(speaker->name() + " doesn't trust you enough to trade.");
            }
            break;
        case DialogueAction::Gift:
            if (speaker->can_gift()) {
                ui.gift_cursor = 0;
                ui.gift_return = GameMode::Dialogue;
                ui.mode = GameMode::GiftSelect;
            } else {
                ui.messages.push(speaker->name() + " doesn't want your gifts yet.");
            }
            break;
        default:
            break;
    }
    return out;
}

InputResult handle_gift_select(InputContext& ctx, Key key) {
    InputResult out;
    UiState& ui = ctx.ui;
    Player& player = ctx.session.player;

    if (key == Key::Escape) {
        ui.mode = ui.gift_return;
        return out;
    }

    int count = static_cast<int>(player.inventory().size());
    navigate_list(key, ui.gift_cursor, count);
    if (!confirms(key)) return out;
    if (ui.gift_cursor < 0 || ui.gift_cursor >= count) return out;

    Npc* target = npc_within_reach(ctx.session.world, player);
    if (!target) {
        ui.messages.push("No one nearby to gift to.");
        ui.mode = GameMode::Normal;
        return out;
    }

    give_gift(ctx, *target, ui.gift_cursor);
    ui.mode = GameMode::Normal;
    out.took_turn = true;
    return out;
}

// ── Crafting, building, zoning ────────────────────────────────────────

InputResult handle_craft(InputContext& ctx, Key key) {
    InputResult out;
    if (key == Key::Escape || key == Key::C) {
        ctx.ui.mode = GameMode::Normal;
        return out;
    }

    const auto& recipes = recipe_db();
    navigate_list(key, ctx.ui.craft_cursor, static_cast<int>(recipes.size()));
    if (!confirms(key)) return out;

    const Recipe& recipe = recipes[ctx.ui.craft_cursor];
    if (!craft_can_make(recipe, ctx.session.player)) {
        ctx.ui.messages.push("Missing materials.");
        return out;
    }

    ctx.ui.messages.push(craft_make(recipe, ctx.session.player));
    out.took_turn = true;
    return out;
}

InputResult handle_build(InputContext& ctx, Key key) {
    InputResult out;
    UiState& ui = ctx.ui;
    Player& player = ctx.session.player;

    if (key == Key::Escape || key == Key::B) {
        ui.mode = GameMode::Normal;
        return out;
    }

    int dx = 0, dy = 0;
    if (movement_delta(key, dx, dy)) {
        // The cursor is tethered to the player so building stays a local act.
        int nx = ui.build_cx + dx;
        int ny = ui.build_cy + dy;
        if (std::abs(nx - player.x()) <= BUILD_RANGE &&
            std::abs(ny - player.y()) <= BUILD_RANGE) {
            ui.build_cx = nx;
            ui.build_cy = ny;
        }
        return out;
    }

    const int buildable_count = static_cast<int>(BuildTile::Count);
    if (key == Key::Tab || key == Key::BracketRight) cycle(ui.build_sel, buildable_count, 1);
    if (key == Key::BracketLeft) cycle(ui.build_sel, buildable_count, -1);

    if (!confirms(key)) return out;

    BuildTile selected = static_cast<BuildTile>(ui.build_sel);
    const BuildDef& def = build_def(selected);

    bool walling_self_in = !def.is_light_object && tile_blocks_movement(def.tile) &&
                           ui.build_cx == player.x() && ui.build_cy == player.y();
    if (walling_self_in) {
        ui.messages.push("Can't build a wall under yourself.");
        return out;
    }

    std::string result = build_place(ctx.session.world, player, selected,
                                    ui.build_cx, ui.build_cy);
    ui.messages.push(result);

    // build_place reports refusals through the same string, so only a
    // successful placement costs a turn.
    if (result.rfind("Built", 0) == 0) {
        out.took_turn = true;
        out.fov_dirty = true;
    }
    return out;
}

InputResult handle_zone(InputContext& ctx, Key key) {
    InputResult out;
    UiState& ui = ctx.ui;

    if (key == Key::Escape || key == Key::Z) {
        // Back out one step at a time: type picker, then rectangle, then mode.
        if (ui.zone_pick_type) {
            ui.zone_pick_type = false;
            ui.zone_anchored = false;
        } else if (ui.zone_anchored) {
            ui.zone_anchored = false;
        } else {
            ui.mode = GameMode::Normal;
        }
        return out;
    }

    const int type_count = static_cast<int>(ZoneType::Count);

    if (ui.zone_pick_type) {
        if (key == Key::Tab || key == Key::Down || key == Key::Right) {
            cycle(ui.zone_type_sel, type_count, 1);
        }
        if (key == Key::Up || key == Key::Left) cycle(ui.zone_type_sel, type_count, -1);

        if (confirms(key)) {
            Zone zone;
            zone.x0 = std::min(ui.zone_ax, ui.zone_cx);
            zone.y0 = std::min(ui.zone_ay, ui.zone_cy);
            zone.x1 = std::max(ui.zone_ax, ui.zone_cx);
            zone.y1 = std::max(ui.zone_ay, ui.zone_cy);
            zone.type = static_cast<ZoneType>(ui.zone_type_sel);
            ctx.session.world.add_zone(zone);
            ui.messages.push(std::string("Designated ") + zone_type_name(zone.type) + " zone.");
            ui.zone_anchored = false;
            ui.zone_pick_type = false;
        }
        return out;
    }

    int dx = 0, dy = 0;
    if (movement_delta(key, dx, dy)) {
        ui.zone_cx += dx;
        ui.zone_cy += dy;
        return out;
    }

    if (key == Key::X && ctx.session.world.remove_zone_at(ui.zone_cx, ui.zone_cy)) {
        ui.messages.push("Zone removed.");
        ui.zone_anchored = false;
        return out;
    }

    if (confirms(key)) {
        if (!ui.zone_anchored) {
            ui.zone_ax = ui.zone_cx;
            ui.zone_ay = ui.zone_cy;
            ui.zone_anchored = true;
        } else {
            ui.zone_pick_type = true;
            ui.zone_type_sel = 0;
        }
    }
    return out;
}

// ── Inventory ─────────────────────────────────────────────────────────

// Keep the cursor on a real row after a stack has been consumed.
void clamp_inventory_cursor(UiState& ui, const Player& player) {
    int last = static_cast<int>(player.inventory().size()) - 1;
    if (ui.inv_cursor > last) ui.inv_cursor = std::max(0, last);
}

InputResult apply_item_action(InputContext& ctx, ItemAction action) {
    InputResult out;
    UiState& ui = ctx.ui;
    Player& player = ctx.session.player;

    // A copy, not a reference: using, equipping and dropping all remove the
    // stack from the inventory, and the item is still read afterwards for the
    // status message.
    Item item = player.inventory_item(ui.inv_cursor);

    switch (action) {
        case ItemAction::Use:
            switch (item.use_effect()) {
                case UseEffect::Heal:
                    player.heal(item.use_amount());
                    ui.messages.push("Used " + item.name() + ". Healed " +
                                     std::to_string(item.use_amount()) + " HP.");
                    break;
                case UseEffect::Feed:
                    ui.messages.push("Ate " + item.name() + ".");
                    break;
                case UseEffect::Drink:
                    ui.messages.push("Drank " + item.name() + ".");
                    break;
                default:
                    break;
            }
            player.remove_item(ui.inv_cursor, 1);
            ui.mode = GameMode::Inventory;
            clamp_inventory_cursor(ui, player);
            break;

        case ItemAction::Equip: {
            if (Item::partner_slot(item.equip_slot()) != EquipSlot::None) {
                begin_equip_select(ui, player, item);
                break;
            }
            player.equip(ui.inv_cursor);
            ui.messages.push("Equipped " + item.name() + ".", 60);
            ui.mode = GameMode::Inventory;
            clamp_inventory_cursor(ui, player);
            break;
        }

        case ItemAction::Drop: {
            Entity dropped(player.x(), player.y(), item.glyph(), item.name(),
                           item.fg_r(), item.fg_g(), item.fg_b());
            dropped.set_is_item(true);
            ctx.session.world.spawn_item(std::move(dropped));
            player.remove_item(ui.inv_cursor, 1);
            ui.messages.push("Dropped " + item.name() + ".", 60);
            ui.mode = GameMode::Inventory;
            clamp_inventory_cursor(ui, player);
            break;
        }

        case ItemAction::Examine:
            ui.examine_item = item;
            ui.mode = GameMode::InventoryExamine;
            break;

        case ItemAction::Gift:
            ui.gift_cursor = 0;
            ui.gift_return = GameMode::Inventory;
            ui.mode = GameMode::GiftSelect;
            break;
    }
    return out;
}

InputResult handle_inventory(InputContext& ctx, Key key) {
    InputResult out;
    UiState& ui = ctx.ui;
    Player& player = ctx.session.player;

    if (ui.mode == GameMode::InventoryExamine) {
        if (confirms(key) || key == Key::Escape) ui.mode = GameMode::Inventory;
        return out;
    }

    if (ui.mode == GameMode::EquipSelect) {
        if (key == Key::Escape) {
            ui.mode = GameMode::InventoryAction;
            return out;
        }
        if (key == Key::Up || key == Key::Left) ui.equip_choice = 0;
        if (key == Key::Down || key == Key::Right) ui.equip_choice = 1;
        if (!confirms(key)) return out;

        if (ui.inv_cursor < 0 ||
            ui.inv_cursor >= static_cast<int>(player.inventory().size())) {
            ui.mode = GameMode::Inventory;
            return out;
        }

        EquipSlot slot = ui.equip_options[ui.equip_choice];
        Item item = player.inventory_item(ui.inv_cursor);
        if (!player.equip(ui.inv_cursor, slot)) {
            ui.mode = GameMode::Inventory;
            return out;
        }
        ui.messages.push("Equipped " + item.name() + " (" +
                         equip_slot_name(slot) + ").", 60);
        ui.mode = GameMode::Inventory;
        clamp_inventory_cursor(ui, player);
        return out;
    }

    if (ui.mode == GameMode::InventoryAction) {
        // The action bar describes one stack, so it cannot outlive it. Using
        // the last of something drops straight back to the list.
        if (key == Key::Escape || ui.inv_cursor < 0 ||
            ui.inv_cursor >= static_cast<int>(player.inventory().size())) {
            ui.mode = GameMode::Inventory;
            clamp_inventory_cursor(ui, player);
            return out;
        }

        std::vector<ItemAction> actions = item_actions(player.inventory_item(ui.inv_cursor));
        int count = static_cast<int>(actions.size());
        if (key == Key::Left && ui.inv_action_cursor > 0) ui.inv_action_cursor--;
        if (key == Key::Right && ui.inv_action_cursor < count - 1) ui.inv_action_cursor++;

        if (confirms(key) && count > 0) {
            return apply_item_action(ctx, actions[ui.inv_action_cursor]);
        }
        return out;
    }

    if (key == Key::Escape || key == Key::I) {
        ui.mode = GameMode::Normal;
        return out;
    }

    // A negative item cursor means the equipment panel has focus.
    if (key == Key::Tab) {
        ui.equip_slot_cursor = 0;
        ui.inv_cursor = ui.inv_cursor >= 0 ? -1 : 0;
        return out;
    }

    if (ui.inv_cursor >= 0) {
        int count = static_cast<int>(player.inventory().size());
        navigate_list(key, ui.inv_cursor, count);
        if (confirms(key) && count > 0) {
            ui.mode = GameMode::InventoryAction;
            ui.inv_action_cursor = 0;
        }
        return out;
    }

    navigate_list(key, ui.equip_slot_cursor, NUM_EQUIP_SLOTS);
    if (confirms(key)) {
        EquipSlot slot = EQUIP_SLOTS[ui.equip_slot_cursor];
        if (player.is_slot_occupied(slot)) {
            player.unequip(slot);
            ui.messages.push("Unequipped.", 60);
        }
    }
    return out;
}

// ── World map ─────────────────────────────────────────────────────────

InputResult handle_map(InputContext& ctx, Key key) {
    InputResult out;
    if (key == Key::Escape || key == Key::M) {
        ctx.ui.mode = GameMode::Normal;
        return out;
    }

    int dx = 0, dy = 0;
    if (!movement_delta(key, dx, dy)) return out;

    // The cursor cannot leave the window of chunks the map draws.
    int player_cx = World::world_to_chunk_x(ctx.session.player.x());
    int player_cy = World::world_to_chunk_y(ctx.session.player.y());
    int cx = ctx.ui.map_cursor_cx + dx;
    int cy = ctx.ui.map_cursor_cy + dy;
    if (std::abs(cx - player_cx) <= MAP_RADIUS && std::abs(cy - player_cy) <= MAP_RADIUS) {
        ctx.ui.map_cursor_cx = cx;
        ctx.ui.map_cursor_cy = cy;
    }
    return out;
}

// ── Normal play ───────────────────────────────────────────────────────

// Resolve the player's attack on an enemy they walked into. Loot and XP are
// handled here because a dying enemy invalidates the pointer.
void attack_enemy(InputContext& ctx, Enemy& target, int tx, int ty) {
    Player& player = ctx.session.player;
    World& world = ctx.session.world;

    int roll = player.total_attack() + ctx.session.rng.variance(player.total_damage_variance());
    int damage = std::max(1, roll - target.defense());
    target.take_damage(damage);

    std::string message = "You hit " + target.name() + " for " +
                          std::to_string(damage) + " damage!";

    if (!target.is_alive()) {
        message += " " + target.name() + " killed! +" +
                   std::to_string(target.xp_value()) + " XP";
        player.add_xp(target.xp_value());

        // Read everything off the enemy before the spawn/remove calls
        // invalidate the pointer.
        int drop_x = target.x();
        int drop_y = target.y();
        auto drops = target.drops();
        world.remove_enemy_at(tx, ty);

        for (const auto& [item, qty] : drops) {
            for (int i = 0; i < qty; ++i) {
                Entity loot(drop_x, drop_y, item.glyph(), item.name(),
                            item.fg_r(), item.fg_g(), item.fg_b());
                loot.set_is_item(true);
                world.spawn_item(std::move(loot));
            }
        }
    }

    ctx.ui.messages.push(message, 60);
}

// Harvest the terrain the player bumped into. Returns true if something was
// gathered, which costs a turn.
bool gather_terrain(InputContext& ctx, int tx, int ty) {
    Player& player = ctx.session.player;
    World& world = ctx.session.world;

    switch (world.get_tile(tx, ty).type) {
        case TileType::Tree: {
            int yield = has_equipped(player, "Woodcutter's Axe") ? 2 : 1;
            if (const Item* wood = find_item("Wood")) player.add_item(*wood, yield);
            world.place_tile(tx, ty, TileType::Grass);
            ctx.ui.messages.push("You chop down the tree. +" + std::to_string(yield) + " Wood");
            return true;
        }
        case TileType::Mountain: {
            bool has_pick = has_equipped(player, "Pickaxe");
            int yield = has_pick ? 2 : 1;
            if (const Item* stone = find_item("Stone")) player.add_item(*stone, yield);

            std::string message = "You mine the rock face. +" + std::to_string(yield) + " Stone";
            if (ctx.session.rng.percent(has_pick ? 30 : 15)) {
                if (const Item* ore = find_item("Iron Ore")) {
                    player.add_item(*ore, 1);
                    message += ", +1 Iron Ore";
                }
            }
            world.place_tile(tx, ty, TileType::Stone);
            ctx.ui.messages.push(message);
            return true;
        }
        default:
            return false;
    }
}

// Collect everything stacked on the player's tile, stopping when the next
// item would exceed carry capacity.
void pick_up_here(InputContext& ctx) {
    Player& player = ctx.session.player;
    World& world = ctx.session.world;

    std::vector<std::string> picked;
    while (Entity* ground = world.item_at(player.x(), player.y())) {
        const Item* known = find_item(ground->name());
        if (!known) {
            // An item whose definition has since disappeared; drop it rather
            // than spin on the same tile forever.
            world.remove_item_at(player.x(), player.y());
            continue;
        }
        if (!player.can_carry(known->weight())) {
            ctx.ui.messages.push("Too heavy to pick up " + std::string(ground->name()) + "!");
            break;
        }

        if (ground->name() == "Gold Coin") {
            player.add_gold(known->value());
            picked.push_back("1 Gold");
        } else {
            player.add_item(*known, 1);
            picked.push_back(ground->name());
        }
        world.remove_item_at(player.x(), player.y());
    }

    if (picked.empty()) return;
    if (picked.size() == 1) ctx.ui.messages.push("Picked up " + picked[0], 60);
    else ctx.ui.messages.push("Picked up " + std::to_string(picked.size()) + " items", 60);
}

// Start a conversation with an adjacent NPC, if one will have you.
void talk_to_neighbour(InputContext& ctx) {
    const int offsets[][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    for (const auto& offset : offsets) {
        Npc* npc = ctx.session.world.npc_at(ctx.session.player.x() + offset[0],
                                            ctx.session.player.y() + offset[1]);
        if (!npc) continue;

        // DESIGN.md: hostile NPCs refuse to talk at all.
        if (!npc->can_talk()) {
            ctx.ui.messages.push(npc->name() + " turns away from you.");
            return;
        }

        ctx.ui.dialogue_npc_x = npc->x();
        ctx.ui.dialogue_npc_y = npc->y();
        ctx.ui.dialogue_node = 0;
        ctx.ui.dialogue_option = 0;
        ctx.ui.mode = GameMode::Dialogue;
        return;
    }
}

// Walking into something is how the player interacts with it: an enemy is
// attacked, harvestable terrain is gathered, anything else is stepped onto.
InputResult step(InputContext& ctx, int dx, int dy) {
    InputResult out;
    Player& player = ctx.session.player;
    World& world = ctx.session.world;

    int tx = player.x() + dx;
    int ty = player.y() + dy;

    if (!player.can_move(dx, dy, world)) {
        if (gather_terrain(ctx, tx, ty)) {
            out.took_turn = true;
            out.fov_dirty = true;
        }
        return out;
    }

    if (Enemy* target = world.enemy_at(tx, ty)) {
        attack_enemy(ctx, *target, tx, ty);
    } else {
        player.move(dx, dy, world);
        world.update_loaded_chunks(player.x(), player.y());
        check_discovery(ctx);
        out.fov_dirty = true;
    }
    out.took_turn = true;
    return out;
}

InputResult handle_normal(InputContext& ctx, Key key) {
    InputResult out;
    UiState& ui = ctx.ui;
    const Player& player = ctx.session.player;

    switch (key) {
        case Key::Escape:
            ui.mode = GameMode::PauseMenu;
            ui.pause_cursor = 0;
            return out;
        case Key::I:
            ui.mode = GameMode::Inventory;
            ui.inv_cursor = 0;
            ui.inv_action_cursor = 0;
            return out;
        case Key::C:
            ui.mode = GameMode::Craft;
            ui.craft_cursor = 0;
            return out;
        case Key::B:
            ui.mode = GameMode::Build;
            ui.build_cx = player.x();
            ui.build_cy = player.y();
            return out;
        case Key::Z:
            ui.mode = GameMode::Zone;
            ui.zone_cx = player.x();
            ui.zone_cy = player.y();
            ui.zone_anchored = false;
            ui.zone_pick_type = false;
            return out;
        case Key::X:
            ui.mode = GameMode::Character;
            return out;
        case Key::M:
            ui.mode = GameMode::Map;
            ui.map_cursor_cx = World::world_to_chunk_x(player.x());
            ui.map_cursor_cy = World::world_to_chunk_y(player.y());
            return out;
        case Key::Period:
            out.took_turn = true;
            out.fov_dirty = true;
            return out;
        case Key::G:
            pick_up_here(ctx);
            return out;
        case Key::Enter:
        case Key::Space:
            talk_to_neighbour(ctx);
            return out;
        default:
            break;
    }

    int dx = 0, dy = 0;
    if (movement_delta(key, dx, dy)) return step(ctx, dx, dy);
    return out;
}

}

InputResult handle_key(InputContext& ctx, Key key) {
    if (key == Key::None) return InputResult{};

    // The title and death screens own the keyboard outright; everything else
    // yields to an open trade first.
    switch (ctx.ui.mode) {
        case GameMode::MainMenu: return handle_main_menu(ctx, key);
        case GameMode::Dead:     return handle_dead(ctx, key);
        default: break;
    }

    if (ctx.trade.active) {
        trade_handle_input(ctx.trade, key, ctx.session.player);
        return InputResult{};
    }

    switch (ctx.ui.mode) {
        case GameMode::PauseMenu: return handle_pause_menu(ctx, key);
        case GameMode::Settings:  return handle_settings(ctx, key);
        case GameMode::SaveGame:  return handle_save_game(ctx, key);
        case GameMode::LoadGame:  return handle_load_game(ctx, key);
        case GameMode::Dialogue:  return handle_dialogue(ctx, key);
        case GameMode::GiftSelect: return handle_gift_select(ctx, key);
        case GameMode::Craft:     return handle_craft(ctx, key);
        case GameMode::Build:     return handle_build(ctx, key);
        case GameMode::Zone:      return handle_zone(ctx, key);
        case GameMode::Map:       return handle_map(ctx, key);
        case GameMode::Character:
            if (key == Key::Escape || key == Key::X || confirms(key)) {
                ctx.ui.mode = GameMode::Normal;
            }
            return InputResult{};
        case GameMode::Inventory:
        case GameMode::InventoryAction:
        case GameMode::InventoryExamine:
        case GameMode::EquipSelect:
            return handle_inventory(ctx, key);
        case GameMode::Normal:    return handle_normal(ctx, key);
        default:
            return InputResult{};
    }
}

}
