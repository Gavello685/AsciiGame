#include "test_framework.h"
#include "json_validator.h"

#include "game/player.h"
#include "game/save.h"
#include "game/settings.h"
#include "game/time_system.h"
#include "game/world.h"

// The validator has to actually reject malformed input, or the save-format
// tests below would pass on anything.

TEST(json_validator_accepts_valid_documents) {
    std::string error;
    CHECK_EQ(json_check::is_valid("{}", error), true);
    CHECK_EQ(json_check::is_valid("[]", error), true);
    CHECK_EQ(json_check::is_valid("{\"a\": [1, 2, {\"b\": \"c\"}], \"d\": true}", error), true);
    CHECK_EQ(json_check::is_valid("{\"n\": -12.5e+3, \"z\": null}", error), true);
    CHECK_EQ(json_check::is_valid("{\"esc\": \"a\\\"b\\\\c\\n\"}", error), true);
}

TEST(json_validator_rejects_trailing_commas) {
    std::string error;
    // The exact shape the save writer used to emit.
    CHECK_EQ(json_check::is_valid("{\"equipment\": [],}", error), false);
    CHECK_EQ(json_check::is_valid("{\"a\": [1, 2,]}", error), false);
    CHECK_EQ(json_check::is_valid("[1,]", error), false);
}

TEST(json_validator_rejects_other_malformed_input) {
    std::string error;
    CHECK_EQ(json_check::is_valid("{\"a\": }", error), false);
    CHECK_EQ(json_check::is_valid("{a: 1}", error), false);
    CHECK_EQ(json_check::is_valid("{\"a\" 1}", error), false);
    CHECK_EQ(json_check::is_valid("{\"a\": 1", error), false);
    CHECK_EQ(json_check::is_valid("{} {}", error), false);
    CHECK_EQ(json_check::is_valid("\"unterminated", error), false);
    CHECK_EQ(json_check::is_valid("", error), false);
}

namespace {

std::string populated_save_json() {
    World world;
    world.init(2024);
    world.update_loaded_chunks(0, 0);
    world.place_tile(10, 10, TileType::WoodWall);
    world.set_explored(11, 11, true);

    PlacedObject torch;
    torch.glyph = '*';
    torch.name = "Torch Post";
    torch.is_light = true;
    torch.light_radius = 3;
    world.add_placed_object(12, 12, torch);

    Zone zone;
    zone.x0 = 1; zone.y0 = 1; zone.x1 = 3; zone.y1 = 3;
    zone.type = ZoneType::Storage;
    world.add_zone(zone);

    world.spawn_enemy(make_enemy("Goblin", 14, 14));
    world.spawn_npc(make_npc("Herbalist", 15, 15));

    Player player;
    player.spawn(8, 8);
    if (const Item* sword = find_item("Iron Sword")) {
        player.add_item(*sword, 1);
        player.equip_to_slot(EquipSlot::Hand_L, *sword);
    }
    if (const Item* bread = find_item("Bread")) player.add_item(*bread, 2);

    TimeSystem time;
    GameState state = capture_state(player, world, 2024, 90, time, 5);
    return serialize_state(state);
}

}

TEST(populated_save_is_valid_json) {
    std::string json = populated_save_json();
    std::string error;
    bool valid = json_check::is_valid(json, error);
    if (!valid) ::testing::fail(TEST_LOCATION, "save JSON invalid: " + error);
    CHECK_EQ(valid, true);
}

TEST(empty_save_is_valid_json) {
    GameState state;
    std::string error;
    bool valid = json_check::is_valid(serialize_state(state), error);
    if (!valid) ::testing::fail(TEST_LOCATION, "empty save JSON invalid: " + error);
    CHECK_EQ(valid, true);
}

TEST(save_escapes_quotes_in_names) {
    // Real item names include an apostrophe ("Woodcutter's Axe"); make sure
    // the writer would also survive a quote or backslash.
    GameState state;
    state.player_name = "He said \"hi\"\\";
    state.meta.character_name = state.player_name;

    std::string json = serialize_state(state);
    std::string error;
    CHECK_EQ(json_check::is_valid(json, error), true);

    GameState restored;
    CHECK_EQ(deserialize_state(json, restored), true);
}

TEST(settings_file_is_valid_json) {
    Settings settings;
    settings.load_radius = 4;
    settings.render_radius = 3;
    settings.sim_radius = 2;
    CHECK_EQ(save_settings(settings), true);

    Settings loaded;
    CHECK_EQ(load_settings(loaded), true);
    CHECK_EQ(loaded.load_radius, 4);
    CHECK_EQ(loaded.render_radius, 3);
    CHECK_EQ(loaded.sim_radius, 2);
}
