#include "test_framework.h"

#include "game/enemy.h"
#include "game/item.h"
#include "game/npc.h"
#include "game/session.h"
#include "game/trade.h"
#include "ui/input.h"
#include "ui/key.h"
#include "ui/ui_state.h"

#include <string>
#include <vector>

using ui::GameMode;
using ui::InputContext;
using ui::InputResult;
using ui::Key;

namespace {

// A started run plus the interface state around it, so a test can press keys
// the way the frame loop does.
struct Harness {
    Session session;
    ui::UiState ui;
    Settings settings;
    TradeState trade;
    InputContext ctx{session, ui, settings, trade, 0};

    explicit Harness(uint32_t seed = 4242) {
        ctx.new_game_seed = seed;
    }

    // Begin a run and drop straight into normal play.
    void start() {
        session_start(session, settings, ctx.new_game_seed);
        ui.reset_for_new_run();
    }

    InputResult press(Key key) { return ui::handle_key(ctx, key); }

    // The most recent status line, or empty if the log is clear.
    std::string last_message() const {
        return ui.messages.empty() ? std::string() : ui.messages.newest().text;
    }

    bool logged(const std::string& fragment) const {
        for (const ui::Message& message : ui.messages.entries()) {
            if (message.text.find(fragment) != std::string::npos) return true;
        }
        return false;
    }
};

// A passable tile next to the player, or false when they are boxed in.
bool open_neighbour(const Harness& h, int& out_x, int& out_y) {
    const int offsets[][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& offset : offsets) {
        int x = h.session.player.x() + offset[0];
        int y = h.session.player.y() + offset[1];
        if (!h.session.world.is_passable(x, y)) continue;
        if (h.session.world.has_npc_at(x, y) || h.session.world.has_enemy_at(x, y)) continue;
        out_x = x;
        out_y = y;
        return true;
    }
    return false;
}

int inventory_count(const Player& player, const std::string& name) {
    int total = 0;
    for (const auto& [item, qty] : player.inventory()) {
        if (item.name() == name) total += qty;
    }
    return total;
}

int index_of(const Player& player, const std::string& name) {
    for (int i = 0; i < static_cast<int>(player.inventory().size()); ++i) {
        if (player.inventory_item(i).name() == name) return i;
    }
    return -1;
}

// Open the action bar for an inventory row and select one action. Which
// column an action sits in depends on the item, so the position is looked up
// rather than assumed.
bool choose_action(Harness& h, int inventory_index, ui::ItemAction wanted) {
    // I toggles the inventory closed, so a second call from an already-open
    // overlay would drop back to play and the following Enter would talk
    // rather than pick an action. Walk back to the item list first.
    if (h.ui.mode == GameMode::EquipSelect) h.press(Key::Escape);
    if (h.ui.mode == GameMode::InventoryAction) h.press(Key::Escape);
    if (h.ui.mode == GameMode::InventoryExamine) h.press(Key::Escape);
    if (h.ui.mode != GameMode::Inventory) h.press(Key::I);
    if (h.ui.mode != GameMode::Inventory) return false;

    h.ui.inv_cursor = inventory_index;
    h.press(Key::Enter);
    if (h.ui.mode != GameMode::InventoryAction) return false;

    std::vector<ui::ItemAction> actions =
        ui::item_actions(h.session.player.inventory_item(inventory_index));
    for (int column = 0; column < static_cast<int>(actions.size()); ++column) {
        if (actions[column] != wanted) continue;
        for (int i = 0; i < column; ++i) h.press(Key::Right);
        h.press(Key::Enter);
        return true;
    }
    return false;
}

}

// ── Menus ─────────────────────────────────────────────────────────────

TEST(new_game_from_the_title_screen_starts_a_run) {
    Harness h;
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::MainMenu));
    CHECK(!h.session.started);

    InputResult result = h.press(Key::Enter);

    CHECK(h.session.started);
    CHECK(result.fov_dirty);
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Normal));
}

TEST(quit_from_the_title_screen_asks_to_exit) {
    Harness h;
    h.press(Key::Down);
    h.press(Key::Down);
    CHECK_EQ(h.ui.menu_cursor, 2);

    CHECK(h.press(Key::Enter).quit);
}

TEST(the_title_cursor_cannot_leave_the_option_list) {
    Harness h;
    for (int i = 0; i < 10; ++i) h.press(Key::Up);
    CHECK_EQ(h.ui.menu_cursor, 0);
    for (int i = 0; i < 10; ++i) h.press(Key::Down);
    CHECK_EQ(h.ui.menu_cursor, ui::MAIN_MENU_OPTION_COUNT - 1);
}

TEST(escape_opens_and_closes_the_pause_menu) {
    Harness h;
    h.start();

    h.press(Key::Escape);
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::PauseMenu));
    h.press(Key::Escape);
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Normal));
}

TEST(the_pause_menu_can_return_to_the_title_screen) {
    Harness h;
    h.start();
    h.press(Key::Escape);

    // "Main Menu" is the fourth row.
    for (int i = 0; i < 3; ++i) h.press(Key::Down);
    h.press(Key::Enter);

    CHECK(!h.session.started);
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::MainMenu));
}

TEST(the_settings_screen_only_offers_valid_radius_combinations) {
    Harness h;
    h.start();
    h.press(Key::Escape);
    for (int i = 0; i < 2; ++i) h.press(Key::Down); // Settings
    h.press(Key::Enter);
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Settings));

    // Drive every slider hard in both directions; the world's invariants must
    // hold at every step, not just at the end.
    for (int row = 0; row < ui::SETTINGS_OPTION_COUNT; ++row) {
        h.ui.settings_cursor = row;
        for (int i = 0; i < 8; ++i) {
            h.press(i % 2 == 0 ? Key::Right : Key::Left);
            CHECK(h.settings.load_radius >= World::MIN_LOAD_RADIUS);
            CHECK(h.settings.load_radius <= World::MAX_LOAD_RADIUS);
            CHECK(h.settings.render_radius >= 1);
            CHECK(h.settings.render_radius <= h.settings.load_radius - 1);
            CHECK(h.settings.sim_radius >= 1);
            CHECK(h.settings.sim_radius <= h.settings.render_radius);
        }
    }
}

TEST(saving_and_loading_through_the_menus_restores_the_run) {
    Harness h;
    h.start();
    h.session.player.add_gold(321);
    int gold = h.session.player.gold();
    int x = h.session.player.x();

    h.press(Key::Escape);
    h.press(Key::Enter);   // Save Game
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::SaveGame));
    h.press(Key::Enter);   // slot 1
    CHECK(h.logged("saved"));

    h.session.player.add_gold(-gold);
    CHECK_EQ(h.session.player.gold(), 0);

    h.press(Key::Escape);
    h.press(Key::Down);
    h.press(Key::Enter);   // Load Game
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::LoadGame));
    h.press(Key::Enter);   // slot 1

    CHECK_EQ(h.session.player.gold(), gold);
    CHECK_EQ(h.session.player.x(), x);
}

TEST(loading_an_empty_slot_reports_it_instead_of_wiping_the_run) {
    Harness h;
    h.start();
    int gold = h.session.player.gold();

    h.press(Key::Escape);
    h.press(Key::Down);
    h.press(Key::Enter);        // Load Game
    h.ui.pause_cursor = MAX_SAVE_SLOTS - 1;
    h.press(Key::Enter);

    CHECK(h.logged("No save in slot"));
    CHECK(h.session.started);
    CHECK_EQ(h.session.player.gold(), gold);
}

// ── Movement, combat and gathering ────────────────────────────────────

TEST(walking_moves_the_player_and_costs_a_turn) {
    Harness h;
    h.start();

    int x, y;
    if (!open_neighbour(h, x, y)) return;
    Key direction = (x > h.session.player.x()) ? Key::D
                  : (x < h.session.player.x()) ? Key::A
                  : (y > h.session.player.y()) ? Key::S : Key::W;

    InputResult result = h.press(direction);

    CHECK(result.took_turn);
    CHECK(result.fov_dirty);
    CHECK_EQ(h.session.player.x(), x);
    CHECK_EQ(h.session.player.y(), y);
}

TEST(walking_into_an_enemy_attacks_instead_of_moving) {
    Harness h;
    h.start();

    int x, y;
    if (!open_neighbour(h, x, y)) return;
    int from_x = h.session.player.x(), from_y = h.session.player.y();

    Enemy rat = make_enemy("Rat", x, y);
    rat.set_hp(1);
    h.session.world.spawn_enemy(std::move(rat));

    Key direction = (x > from_x) ? Key::D : (x < from_x) ? Key::A
                  : (y > from_y) ? Key::S : Key::W;
    InputResult result = h.press(direction);

    CHECK(result.took_turn);
    CHECK_EQ(h.session.player.x(), from_x);
    CHECK_EQ(h.session.player.y(), from_y);
    CHECK(h.logged("You hit Rat"));
    CHECK(h.session.world.enemy_at(x, y) == nullptr);
    CHECK(h.session.player.xp() > 0);
}

TEST(a_kill_leaves_loot_on_the_ground) {
    Harness h;
    h.start();

    int x, y;
    if (!open_neighbour(h, x, y)) return;
    int from_x = h.session.player.x(), from_y = h.session.player.y();

    Enemy rat = make_enemy("Rat", x, y);
    rat.set_hp(1);
    h.session.world.spawn_enemy(std::move(rat));

    Key direction = (x > from_x) ? Key::D : (x < from_x) ? Key::A
                  : (y > from_y) ? Key::S : Key::W;
    h.press(direction);

    Entity* loot = h.session.world.item_at(x, y);
    CHECK(loot != nullptr);
}

TEST(bumping_a_tree_chops_it_down_for_wood) {
    Harness h;
    h.start();

    int x = h.session.player.x() + 1;
    int y = h.session.player.y();
    h.session.world.place_tile(x, y, TileType::Tree);

    int wood_before = inventory_count(h.session.player, "Wood");
    InputResult result = h.press(Key::D);

    CHECK(result.took_turn);
    CHECK(result.fov_dirty);
    CHECK(inventory_count(h.session.player, "Wood") > wood_before);
    CHECK_EQ(h.session.player.x(), x - 1); // did not step onto the tile
    CHECK_EQ(static_cast<int>(h.session.world.get_tile(x, y).type),
             static_cast<int>(TileType::Grass));
}

TEST(bumping_a_mountain_mines_it_for_stone) {
    Harness h;
    h.start();

    int x = h.session.player.x() + 1;
    int y = h.session.player.y();
    h.session.world.place_tile(x, y, TileType::Mountain);

    int stone_before = inventory_count(h.session.player, "Stone");
    CHECK(h.press(Key::D).took_turn);
    CHECK(inventory_count(h.session.player, "Stone") > stone_before);
}

TEST(walking_into_a_wall_costs_nothing) {
    Harness h;
    h.start();

    int x = h.session.player.x() + 1;
    int y = h.session.player.y();
    h.session.world.place_tile(x, y, TileType::WoodWall);

    InputResult result = h.press(Key::D);

    CHECK(!result.took_turn);
    CHECK_EQ(h.session.player.x(), x - 1);
}

TEST(waiting_costs_a_turn_without_moving) {
    Harness h;
    h.start();
    int x = h.session.player.x(), y = h.session.player.y();

    InputResult result = h.press(Key::Period);

    CHECK(result.took_turn);
    CHECK_EQ(h.session.player.x(), x);
    CHECK_EQ(h.session.player.y(), y);
}

TEST(grabbing_collects_every_item_underfoot) {
    Harness h;
    h.start();

    // Clear whatever the run scattered here, then place two known items.
    while (h.session.world.item_at(h.session.player.x(), h.session.player.y())) {
        h.session.world.remove_item_at(h.session.player.x(), h.session.player.y());
    }

    for (const char* name : {"Apple", "Old Scroll"}) {
        const Item* item = find_item(name);
        CHECK(item != nullptr);
        if (!item) continue;
        Entity e(h.session.player.x(), h.session.player.y(), item->glyph(),
                 item->name(), item->fg_r(), item->fg_g(), item->fg_b());
        e.set_is_item(true);
        h.session.world.spawn_item(std::move(e));
    }

    h.press(Key::G);

    CHECK_EQ(inventory_count(h.session.player, "Apple"), 1);
    CHECK_EQ(inventory_count(h.session.player, "Old Scroll"), 1);
    CHECK(h.session.world.item_at(h.session.player.x(), h.session.player.y()) == nullptr);
}

TEST(a_gold_coin_underfoot_becomes_currency_not_cargo) {
    Harness h;
    h.start();

    while (h.session.world.item_at(h.session.player.x(), h.session.player.y())) {
        h.session.world.remove_item_at(h.session.player.x(), h.session.player.y());
    }

    const Item* coin = find_item("Gold Coin");
    CHECK(coin != nullptr);
    if (!coin) return;
    Entity e(h.session.player.x(), h.session.player.y(), coin->glyph(), coin->name(),
             coin->fg_r(), coin->fg_g(), coin->fg_b());
    e.set_is_item(true);
    h.session.world.spawn_item(std::move(e));

    int gold_before = h.session.player.gold();
    h.press(Key::G);

    CHECK(h.session.player.gold() > gold_before);
    CHECK_EQ(inventory_count(h.session.player, "Gold Coin"), 0);
}

// ── Inventory ─────────────────────────────────────────────────────────

TEST(using_the_last_of_a_stack_closes_the_action_bar) {
    // The action bar used to index the inventory unconditionally, so
    // consuming the selected stack left it describing a row that was gone.
    Harness h;
    h.start();

    int potion = index_of(h.session.player, "Health Potion");
    CHECK(potion >= 0);
    if (potion < 0) return;

    h.session.player.take_damage(10);
    int hp = h.session.player.hp();
    CHECK(choose_action(h, potion, ui::ItemAction::Use));

    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Inventory));
    CHECK_EQ(inventory_count(h.session.player, "Health Potion"), 0);
    CHECK(h.session.player.hp() > hp);
    CHECK(h.ui.inv_cursor < static_cast<int>(h.session.player.inventory().size()));

    // Reopening must not read past the end of the shortened list.
    h.press(Key::Enter);
    h.press(Key::Enter);
}

TEST(equipping_from_the_inventory_fills_the_slot) {
    Harness h;
    h.start();

    int sword = index_of(h.session.player, "Iron Sword");
    CHECK(sword >= 0);
    if (sword < 0) return;

    int attack_before = h.session.player.total_attack();

    CHECK(choose_action(h, sword, ui::ItemAction::Equip));
    // A weapon can go in either hand, so Equip opens the slot picker.
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::EquipSelect));
    h.press(Key::Enter);

    CHECK(h.session.player.total_attack() > attack_before);
    CHECK_EQ(inventory_count(h.session.player, "Iron Sword"), 0);
    CHECK(h.session.player.equipped_at(EquipSlot::Hand_L) != nullptr);
    CHECK(h.logged("Equipped Iron Sword"));
}

TEST(a_one_slot_item_equips_without_asking) {
    Harness h;
    h.start();

    int armor = index_of(h.session.player, "Leather Armor");
    CHECK(armor >= 0);
    if (armor < 0) return;

    CHECK(choose_action(h, armor, ui::ItemAction::Equip));

    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Inventory));
    CHECK(h.session.player.equipped_at(EquipSlot::Torso) != nullptr);
    CHECK_EQ(inventory_count(h.session.player, "Leather Armor"), 0);
}

TEST(equip_select_puts_a_paired_item_in_the_other_hand) {
    Harness h;
    h.start();

    int sword = index_of(h.session.player, "Iron Sword");
    int torch = index_of(h.session.player, "Torch");
    CHECK(sword >= 0 && torch >= 0);
    if (sword < 0 || torch < 0) return;

    CHECK(choose_action(h, sword, ui::ItemAction::Equip));
    h.press(Key::Enter);
    CHECK(h.session.player.equipped_at(EquipSlot::Hand_L) != nullptr);

    // Torch also names Hand_L, which is occupied, so the picker should land
    // on the empty right hand.
    torch = index_of(h.session.player, "Torch");
    CHECK(choose_action(h, torch, ui::ItemAction::Equip));
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::EquipSelect));
    CHECK_EQ(static_cast<int>(h.ui.equip_options[h.ui.equip_choice]),
             static_cast<int>(EquipSlot::Hand_R));
    h.press(Key::Enter);

    const Item* left = h.session.player.equipped_at(EquipSlot::Hand_L);
    const Item* right = h.session.player.equipped_at(EquipSlot::Hand_R);
    CHECK(left != nullptr && right != nullptr);
    if (!left || !right) return;
    CHECK_EQ(left->name(), std::string("Iron Sword"));
    CHECK_EQ(right->name(), std::string("Torch"));
}

TEST(equip_select_escape_leaves_the_item_in_inventory) {
    Harness h;
    h.start();

    int sword = index_of(h.session.player, "Iron Sword");
    CHECK(sword >= 0);
    if (sword < 0) return;

    CHECK(choose_action(h, sword, ui::ItemAction::Equip));
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::EquipSelect));
    h.press(Key::Escape);

    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::InventoryAction));
    CHECK_EQ(inventory_count(h.session.player, "Iron Sword"), 1);
    CHECK(h.session.player.equipped_at(EquipSlot::Hand_L) == nullptr);
}

TEST(a_gold_ring_can_go_on_either_hand) {
    Harness h;
    h.start();

    const Item* ring = find_item("Gold Ring");
    CHECK(ring != nullptr);
    if (!ring) return;
    h.session.player.add_item(*ring);

    int index = index_of(h.session.player, "Gold Ring");
    CHECK(choose_action(h, index, ui::ItemAction::Equip));
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::EquipSelect));
    // Named slot is Hand_R; listed after L Hand, so Down then Enter.
    h.press(Key::Up);   // L Hand
    h.press(Key::Enter);

    const Item* left = h.session.player.equipped_at(EquipSlot::Hand_L);
    CHECK(left != nullptr);
    if (!left) return;
    CHECK_EQ(left->name(), std::string("Gold Ring"));
    CHECK_EQ(inventory_count(h.session.player, "Gold Ring"), 0);
}

TEST(dropping_an_item_leaves_it_on_the_floor) {
    Harness h;
    h.start();

    int bread = index_of(h.session.player, "Bread");
    CHECK(bread >= 0);
    if (bread < 0) return;

    CHECK(choose_action(h, bread, ui::ItemAction::Drop));

    CHECK_EQ(inventory_count(h.session.player, "Bread"), 2);
    CHECK(h.logged("Dropped Bread"));
    CHECK(h.session.world.item_at(h.session.player.x(), h.session.player.y()) != nullptr);
}

TEST(tab_moves_focus_between_the_item_list_and_the_paper_doll) {
    Harness h;
    h.start();
    h.press(Key::I);
    CHECK(h.ui.inv_cursor >= 0);

    h.press(Key::Tab);
    CHECK(h.ui.inv_cursor < 0);
    for (int i = 0; i < 30; ++i) h.press(Key::Down);
    CHECK_EQ(h.ui.equip_slot_cursor, ui::NUM_EQUIP_SLOTS - 1);

    h.press(Key::Tab);
    CHECK(h.ui.inv_cursor >= 0);
}

TEST(the_inventory_cursor_cannot_run_off_the_list) {
    Harness h;
    h.start();
    h.press(Key::I);

    for (int i = 0; i < 50; ++i) h.press(Key::Down);
    CHECK_EQ(h.ui.inv_cursor,
             static_cast<int>(h.session.player.inventory().size()) - 1);
    for (int i = 0; i < 50; ++i) h.press(Key::Up);
    CHECK_EQ(h.ui.inv_cursor, 0);
}

// ── Building and zoning ───────────────────────────────────────────────

TEST(the_build_cursor_stays_within_reach_of_the_player) {
    Harness h;
    h.start();
    h.press(Key::B);
    CHECK_EQ(h.ui.build_cx, h.session.player.x());

    for (int i = 0; i < 40; ++i) h.press(Key::D);
    CHECK_EQ(h.ui.build_cx, h.session.player.x() + ui::BUILD_RANGE);

    for (int i = 0; i < 80; ++i) h.press(Key::A);
    CHECK_EQ(h.ui.build_cx, h.session.player.x() - ui::BUILD_RANGE);
}

TEST(the_buildable_picker_wraps_in_both_directions) {
    Harness h;
    h.start();
    h.press(Key::B);

    int count = static_cast<int>(BuildTile::Count);
    for (int i = 0; i < count; ++i) h.press(Key::Tab);
    CHECK_EQ(h.ui.build_sel, 0);

    h.press(Key::BracketLeft);
    CHECK_EQ(h.ui.build_sel, count - 1);
}

TEST(building_without_materials_is_refused) {
    Harness h;
    h.start();
    h.press(Key::B);
    h.press(Key::D);   // step the cursor off the player

    InputResult result = h.press(Key::Enter);

    CHECK(!result.took_turn);
    CHECK(!h.logged("Built"));
}

TEST(building_with_materials_places_the_tile_and_costs_a_turn) {
    Harness h;
    h.start();

    const Item* plank = find_item("Wood Plank");
    CHECK(plank != nullptr);
    if (!plank) return;
    h.session.player.add_item(*plank, 4);

    h.press(Key::B);
    h.press(Key::D);
    int x = h.ui.build_cx, y = h.ui.build_cy;
    // A wall cannot go on water or a tree, so clear the ground first.
    h.session.world.place_tile(x, y, TileType::Grass);

    InputResult result = h.press(Key::Enter);

    CHECK(result.took_turn);
    CHECK(result.fov_dirty);
    CHECK(h.logged("Built"));
    CHECK_EQ(static_cast<int>(h.session.world.get_tile(x, y).type),
             static_cast<int>(TileType::WoodWall));
}

TEST(you_cannot_wall_yourself_in) {
    Harness h;
    h.start();

    const Item* plank = find_item("Wood Plank");
    if (!plank) return;
    h.session.player.add_item(*plank, 4);

    h.press(Key::B);   // cursor starts on the player
    InputResult result = h.press(Key::Enter);

    CHECK(!result.took_turn);
    CHECK(h.logged("under yourself"));
}

TEST(marking_two_corners_and_a_type_designates_a_zone) {
    Harness h;
    h.start();
    CHECK(h.session.world.zones().empty());

    h.press(Key::Z);
    h.press(Key::Enter);                 // first corner
    CHECK(h.ui.zone_anchored);
    for (int i = 0; i < 3; ++i) h.press(Key::D);
    for (int i = 0; i < 2; ++i) h.press(Key::S);
    h.press(Key::Enter);                 // second corner, opens the type picker
    CHECK(h.ui.zone_pick_type);
    h.press(Key::Enter);                 // accept Farm

    CHECK_EQ(h.session.world.zones().size(), size_t{1});
    CHECK(h.logged("Designated"));

    const Zone& zone = h.session.world.zones().front();
    CHECK_EQ(zone.x1 - zone.x0 + 1, 4);
    CHECK_EQ(zone.y1 - zone.y0 + 1, 3);
    CHECK(!h.ui.zone_anchored);
    CHECK(!h.ui.zone_pick_type);
}

TEST(escape_backs_out_of_zone_mode_one_step_at_a_time) {
    Harness h;
    h.start();
    h.press(Key::Z);
    h.press(Key::Enter);
    h.press(Key::D);
    h.press(Key::Enter);
    CHECK(h.ui.zone_pick_type);

    h.press(Key::Escape);
    CHECK(!h.ui.zone_pick_type);
    CHECK(!h.ui.zone_anchored);
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Zone));

    h.press(Key::Escape);
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Normal));
}

TEST(a_designated_zone_can_be_deleted_again) {
    Harness h;
    h.start();
    h.press(Key::Z);
    h.press(Key::Enter);
    h.press(Key::Enter);
    h.press(Key::Enter);
    CHECK_EQ(h.session.world.zones().size(), size_t{1});

    h.press(Key::X);
    CHECK(h.session.world.zones().empty());
    CHECK(h.logged("Zone removed"));
}

// ── Crafting ──────────────────────────────────────────────────────────

TEST(crafting_without_ingredients_is_refused) {
    Harness h;
    h.start();
    h.press(Key::C);

    InputResult result = h.press(Key::Enter);

    CHECK(!result.took_turn);
    CHECK(h.logged("Missing materials"));
}

TEST(crafting_with_ingredients_consumes_them_and_costs_a_turn) {
    Harness h;
    h.start();

    const Item* wood = find_item("Wood");
    CHECK(wood != nullptr);
    if (!wood) return;
    h.session.player.add_item(*wood, 1);

    h.press(Key::C);
    h.ui.craft_cursor = 0;   // Wood -> 2 Wood Plank
    InputResult result = h.press(Key::Enter);

    CHECK(result.took_turn);
    CHECK_EQ(inventory_count(h.session.player, "Wood"), 0);
    CHECK_EQ(inventory_count(h.session.player, "Wood Plank"), 2);
}

// ── NPCs ──────────────────────────────────────────────────────────────

TEST(a_friendly_neighbour_can_be_talked_to) {
    Harness h;
    h.start();

    int x, y;
    if (!open_neighbour(h, x, y)) return;
    h.session.world.spawn_npc(make_npc("Villager", x, y));

    h.press(Key::Enter);

    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Dialogue));
    CHECK_EQ(h.ui.dialogue_npc_x, x);
    CHECK_EQ(h.ui.dialogue_npc_y, y);
}

TEST(a_hostile_neighbour_refuses_to_talk) {
    // DESIGN.md: affinity of 20 or less means the NPC will not engage.
    Harness h;
    h.start();

    int x, y;
    if (!open_neighbour(h, x, y)) return;
    Npc grump = make_npc("Villager", x, y);
    grump.set_affinity(Npc::HOSTILE_MAX);
    h.session.world.spawn_npc(std::move(grump));

    h.press(Key::Enter);

    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Normal));
    CHECK(h.logged("turns away from you"));
}

TEST(a_wary_npc_declines_gifts) {
    Harness h;
    h.start();

    int x, y;
    if (!open_neighbour(h, x, y)) return;
    Npc wary = make_npc("Villager", x, y);
    wary.set_affinity(Npc::WARY_MAX);
    h.session.world.spawn_npc(std::move(wary));

    h.press(Key::Enter);
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Dialogue));

    // Walk the root node's options until Gift is refused.
    Npc* npc = h.session.world.npc_at(x, y);
    CHECK(npc != nullptr);
    if (!npc) return;

    const auto& options = npc->dialogue_tree()[0].options;
    for (int i = 0; i < static_cast<int>(options.size()); ++i) {
        if (options[i].action != DialogueAction::Gift) continue;
        h.ui.dialogue_option = i;
        h.press(Key::Enter);
        CHECK(h.logged("doesn't want your gifts yet"));
        CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Dialogue));
        return;
    }
}

TEST(a_non_merchant_never_opens_a_shop) {
    Harness h;
    h.start();

    int x, y;
    if (!open_neighbour(h, x, y)) return;
    Npc villager = make_npc("Villager", x, y);
    villager.set_affinity(100);
    CHECK(!villager.is_merchant());
    CHECK(!villager.can_trade());
    h.session.world.spawn_npc(std::move(villager));

    h.press(Key::Enter);
    CHECK(!h.trade.active);
}

TEST(gifting_an_item_shifts_affinity_and_consumes_it) {
    Harness h;
    h.start();

    int x, y;
    if (!open_neighbour(h, x, y)) return;
    Npc friendly = make_npc("Villager", x, y);
    friendly.set_affinity(60);
    h.session.world.spawn_npc(std::move(friendly));

    int bread = index_of(h.session.player, "Bread");
    CHECK(bread >= 0);
    if (bread < 0) return;

    int affinity_before = h.session.world.npc_at(x, y)->affinity();
    int bread_before = inventory_count(h.session.player, "Bread");

    h.ui.mode = GameMode::GiftSelect;
    h.ui.gift_return = GameMode::Normal;
    h.ui.gift_cursor = bread;
    InputResult result = h.press(Key::Enter);

    CHECK(result.took_turn);
    CHECK_EQ(inventory_count(h.session.player, "Bread"), bread_before - 1);
    CHECK_NE(h.session.world.npc_at(x, y)->affinity(), affinity_before);
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Normal));
}

TEST(gifting_with_no_one_around_is_reported) {
    Harness h;
    h.start();

    // Clear the village NPCs so nobody is in reach.
    h.session.world.clear_all_entities();

    h.ui.mode = GameMode::GiftSelect;
    h.ui.gift_cursor = 0;
    h.press(Key::Enter);

    CHECK(h.logged("No one nearby"));
}

TEST(gifting_from_an_empty_inventory_does_nothing) {
    Harness h;
    h.start();

    while (!h.session.player.inventory().empty()) {
        h.session.player.remove_item(0, h.session.player.inventory_count(0));
    }

    h.ui.mode = GameMode::GiftSelect;
    h.ui.gift_cursor = 0;
    InputResult result = h.press(Key::Enter);

    CHECK(!result.took_turn);
}

// ── Trade ─────────────────────────────────────────────────────────────

TEST(an_open_trade_swallows_game_keys) {
    Harness h;
    h.start();

    int x, y;
    if (!open_neighbour(h, x, y)) return;
    Npc merchant = make_npc("Merchant", x, y);
    merchant.set_affinity(100);
    CHECK(merchant.can_trade());
    h.session.world.spawn_npc(std::move(merchant));

    trade_open(h.trade, h.session.world.npc_at(x, y));
    int from_x = h.session.player.x();

    InputResult result = h.press(Key::D);

    CHECK(!result.took_turn);
    CHECK_EQ(h.session.player.x(), from_x);
    CHECK(h.trade.active);

    h.press(Key::Escape);
    CHECK(!h.trade.active);
}

TEST(buying_from_a_merchant_exchanges_gold_for_goods) {
    Harness h;
    h.start();

    int x, y;
    if (!open_neighbour(h, x, y)) return;
    Npc merchant = make_npc("Merchant", x, y);
    merchant.set_affinity(100);
    h.session.world.spawn_npc(std::move(merchant));

    Npc* shop = h.session.world.npc_at(x, y);
    CHECK(shop != nullptr);
    if (!shop || shop->shop_inventory().empty()) return;

    Item wanted = shop->shop_item_at(0);
    h.session.player.add_gold(1000);
    int gold_before = h.session.player.gold();
    int held_before = inventory_count(h.session.player, wanted.name());

    trade_open(h.trade, shop);
    h.press(Key::Tab);      // focus the merchant's wares
    h.press(Key::Enter);    // ask to buy
    CHECK(h.trade.confirming);
    h.press(Key::Left);     // move off the default "No"
    h.press(Key::Enter);    // confirm

    CHECK_EQ(inventory_count(h.session.player, wanted.name()), held_before + 1);
    CHECK_EQ(h.session.player.gold(), gold_before - trade_buy_price(wanted));
}

TEST(declining_a_purchase_changes_nothing) {
    Harness h;
    h.start();

    int x, y;
    if (!open_neighbour(h, x, y)) return;
    Npc merchant = make_npc("Merchant", x, y);
    merchant.set_affinity(100);
    h.session.world.spawn_npc(std::move(merchant));

    Npc* shop = h.session.world.npc_at(x, y);
    if (!shop || shop->shop_inventory().empty()) return;

    int gold_before = h.session.player.gold();
    size_t stacks_before = h.session.player.inventory().size();

    trade_open(h.trade, shop);
    h.press(Key::Tab);
    h.press(Key::Enter);
    h.press(Key::Enter);    // "No" is selected by default

    CHECK(!h.trade.confirming);
    CHECK_EQ(h.session.player.gold(), gold_before);
    CHECK_EQ(h.session.player.inventory().size(), stacks_before);
}

TEST(selling_the_last_of_a_stack_leaves_the_cursor_in_range) {
    Harness h;
    h.start();

    int x, y;
    if (!open_neighbour(h, x, y)) return;
    Npc merchant = make_npc("Merchant", x, y);
    merchant.set_affinity(100);
    h.session.world.spawn_npc(std::move(merchant));

    Npc* shop = h.session.world.npc_at(x, y);
    if (!shop) return;

    // Sell down the last stack in the list, which is where an out-of-range
    // cursor would previously have been left pointing.
    trade_open(h.trade, shop);
    h.trade.player_cursor = static_cast<int>(h.session.player.inventory().size()) - 1;

    for (int i = 0; i < 6; ++i) {
        if (h.session.player.inventory().empty()) break;
        h.press(Key::Enter);
        h.press(Key::Left);
        h.press(Key::Enter);
        CHECK(h.trade.player_cursor >= 0);
        CHECK(h.trade.player_cursor <=
              std::max(0, static_cast<int>(h.session.player.inventory().size()) - 1));
    }
}

// ── Overlays ──────────────────────────────────────────────────────────

TEST(the_world_map_cursor_stays_inside_the_drawn_window) {
    Harness h;
    h.start();
    h.press(Key::M);

    int player_cx = World::world_to_chunk_x(h.session.player.x());
    CHECK_EQ(h.ui.map_cursor_cx, player_cx);

    for (int i = 0; i < ui::MAP_RADIUS + 10; ++i) h.press(Key::D);
    CHECK_EQ(h.ui.map_cursor_cx, player_cx + ui::MAP_RADIUS);

    h.press(Key::M);
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Normal));
}

TEST(the_character_sheet_toggles_with_the_same_key) {
    Harness h;
    h.start();

    h.press(Key::X);
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Character));
    h.press(Key::X);
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Normal));
}

TEST(dialogue_closes_if_the_speaker_disappears) {
    Harness h;
    h.start();

    int x, y;
    if (!open_neighbour(h, x, y)) return;
    h.session.world.spawn_npc(make_npc("Villager", x, y));
    h.press(Key::Enter);
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Dialogue));

    h.session.world.clear_all_entities();
    h.press(Key::Down);

    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Normal));
}

TEST(unknown_keys_are_ignored_in_every_mode) {
    Harness h;
    h.start();

    const GameMode modes[] = {
        GameMode::Normal, GameMode::Inventory, GameMode::InventoryAction,
        GameMode::InventoryExamine, GameMode::EquipSelect, GameMode::GiftSelect,
        GameMode::Dialogue,
        GameMode::PauseMenu, GameMode::SaveGame, GameMode::LoadGame,
        GameMode::Settings, GameMode::Craft, GameMode::Build, GameMode::Zone,
        GameMode::Character, GameMode::Map, GameMode::Dead, GameMode::MainMenu,
    };

    for (GameMode mode : modes) {
        h.ui.mode = mode;
        InputResult result = h.press(Key::None);
        CHECK(!result.quit);
        CHECK(!result.took_turn);
        CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(mode));
    }
}

TEST(restarting_after_death_begins_a_fresh_run) {
    Harness h;
    h.start();
    h.session.player.add_gold(500);
    h.ui.mode = GameMode::Dead;

    InputResult result = h.press(Key::R);

    CHECK(result.fov_dirty);
    CHECK(h.session.started);
    CHECK_EQ(static_cast<int>(h.ui.mode), static_cast<int>(GameMode::Normal));
    CHECK_EQ(h.session.player.gold(), 50);
}
