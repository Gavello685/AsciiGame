#include "test_framework.h"

#include "game/player.h"
#include "game/save.h"
#include "game/time_system.h"
#include "game/world.h"

namespace {

// A world with a player-built wall, a campfire, an explored tile, a zone and
// a wounded enemy: everything the save format has to carry.
struct Fixture {
    World world;
    Player player;
    TimeSystem time;
    uint32_t seed = 31337;

    Fixture() {
        world.init(seed);
        world.update_loaded_chunks(0, 0);
        player.spawn(8, 8);
        world.update_loaded_chunks(player.x(), player.y());

        world.place_tile(10, 10, TileType::WoodWall);
        world.place_tile(11, 10, TileType::WoodFloor);
        world.set_explored(12, 12, true);

        PlacedObject campfire;
        campfire.glyph = '*';
        campfire.name = "Campfire";
        campfire.fg_r = 255; campfire.fg_g = 160; campfire.fg_b = 40;
        campfire.is_light = true;
        campfire.light_radius = 4;
        world.add_placed_object(13, 13, campfire);

        Zone farm;
        farm.x0 = 4; farm.y0 = 4; farm.x1 = 9; farm.y1 = 9;
        farm.type = ZoneType::Farm;
        world.add_zone(farm);

        if (const Item* sword = find_item("Iron Sword")) player.add_item(*sword, 1);
        if (const Item* bread = find_item("Bread")) player.add_item(*bread, 3);
        if (const Item* armor = find_item("Leather Armor")) {
            player.add_item(*armor, 1);
            player.equip_to_slot(EquipSlot::Torso, *armor);
        }
        player.set_gold(123);
        player.set_xp(45);
        player.set_level(3);
        player.set_hp(7);

        time.set_day(4);
        time.set_turn_of_day(420);
    }
};

GameState round_trip(const Fixture& fixture, uint64_t rng_state) {
    GameState captured = capture_state(fixture.player, fixture.world, fixture.seed,
                                       600, fixture.time, rng_state);
    std::string json = serialize_state(captured);

    GameState restored;
    bool ok = deserialize_state(json, restored);
    CHECK_EQ(ok, true);
    return restored;
}

}

TEST(save_round_trip_preserves_player) {
    Fixture fixture;
    GameState restored = round_trip(fixture, 0xABCDEF0123456789ull);

    CHECK_EQ(restored.player_x, 8);
    CHECK_EQ(restored.player_y, 8);
    CHECK_EQ(restored.gold, 123);
    CHECK_EQ(restored.xp, 45);
    CHECK_EQ(restored.level, 3);
    CHECK_EQ(restored.hp, 7);
    CHECK_EQ(restored.map_seed, fixture.seed);
    CHECK_EQ(restored.rng_state, 0xABCDEF0123456789ull);
    CHECK_EQ(restored.day, 4);
    CHECK_EQ(restored.turn_of_day, 420);
    CHECK_EQ(restored.meta.play_time_seconds, 600);
}

TEST(save_round_trip_preserves_inventory_and_equipment) {
    Fixture fixture;
    GameState restored = round_trip(fixture, 1);

    int bread = 0;
    bool has_sword = false;
    for (const auto& [item, qty] : restored.inventory) {
        if (item.name() == "Bread") bread = qty;
        if (item.name() == "Iron Sword") has_sword = true;
    }
    CHECK_EQ(bread, 3);
    CHECK_EQ(has_sword, true);

    auto torso = restored.equipment.find(EquipSlot::Torso);
    CHECK(torso != restored.equipment.end());
    if (torso != restored.equipment.end()) {
        CHECK_EQ(torso->second.name(), std::string("Leather Armor"));
    }
}

TEST(save_round_trip_preserves_zones) {
    Fixture fixture;
    GameState restored = round_trip(fixture, 1);

    CHECK_EQ(restored.zones.size(), size_t{1});
    if (!restored.zones.empty()) {
        CHECK_EQ(restored.zones[0].x0, 4);
        CHECK_EQ(restored.zones[0].y1, 9);
        CHECK(restored.zones[0].type == ZoneType::Farm);
    }
}

TEST(save_round_trip_preserves_built_tiles_and_objects) {
    Fixture fixture;
    GameState restored = round_trip(fixture, 1);

    bool wall = false, floor = false, campfire = false;
    for (const auto& chunk : restored.chunks) {
        for (const auto& mod : chunk.modifications) {
            if (mod.local_x == 10 && mod.local_y == 10 && mod.type == TileType::WoodWall) wall = true;
            if (mod.local_x == 11 && mod.local_y == 10 && mod.type == TileType::WoodFloor) floor = true;
        }
        for (const auto& obj : chunk.placed_objects) {
            if (obj.name == "Campfire") {
                campfire = true;
                CHECK_EQ(obj.is_light, true);
                CHECK_EQ(obj.light_radius, 4);
            }
        }
    }
    CHECK_EQ(wall, true);
    CHECK_EQ(floor, true);
    CHECK_EQ(campfire, true);
}

TEST(save_round_trip_rebuilds_world_state) {
    Fixture fixture;
    GameState captured = capture_state(fixture.player, fixture.world, fixture.seed,
                                       600, fixture.time, 7);
    std::string json = serialize_state(captured);

    GameState restored;
    CHECK_EQ(deserialize_state(json, restored), true);

    // Rebuild the way the load path does: stage the saved data first, then
    // generate chunks so replay wins over fresh spawning.
    World reloaded;
    reloaded.init(restored.map_seed);
    for (const auto& zone : restored.zones) reloaded.add_zone(zone);
    apply_chunk_data(reloaded, restored.chunks);
    reloaded.update_loaded_chunks(restored.player_x, restored.player_y);

    CHECK(reloaded.get_tile(10, 10).type == TileType::WoodWall);
    CHECK(reloaded.get_tile(11, 10).type == TileType::WoodFloor);
    CHECK_EQ(reloaded.get_tile(12, 12).explored, true);

    const PlacedObject* campfire = reloaded.placed_object_at(13, 13);
    CHECK(campfire != nullptr);
    if (campfire) CHECK_EQ(campfire->name, std::string("Campfire"));

    const Zone* zone = reloaded.zone_at(5, 5);
    CHECK(zone != nullptr);
    if (zone) CHECK(zone->type == ZoneType::Farm);
}

TEST(save_round_trip_preserves_wounded_enemy) {
    Fixture fixture;
    fixture.world.spawn_enemy(make_enemy("Ogre", 20, 20));
    Enemy* ogre = fixture.world.enemy_at(20, 20);
    CHECK(ogre != nullptr);
    if (ogre) ogre->set_hp(11);

    GameState captured = capture_state(fixture.player, fixture.world, fixture.seed,
                                       0, fixture.time, 7);
    GameState restored;
    CHECK_EQ(deserialize_state(serialize_state(captured), restored), true);

    World reloaded;
    reloaded.init(restored.map_seed);
    apply_chunk_data(reloaded, restored.chunks);
    reloaded.update_loaded_chunks(restored.player_x, restored.player_y);

    Enemy* reloaded_ogre = reloaded.enemy_at(20, 20);
    CHECK(reloaded_ogre != nullptr);
    if (reloaded_ogre) {
        CHECK_EQ(reloaded_ogre->hp(), 11);
        // Derived stats come back from the archetype, not from the file.
        CHECK_EQ(reloaded_ogre->max_hp(), 25);
        CHECK_EQ(reloaded_ogre->xp_value(), 20);
        CHECK_EQ(reloaded_ogre->damage_variance(), 2);
        CHECK_EQ(reloaded_ogre->attack(), 7);
        CHECK(!reloaded_ogre->drops().empty());
    }
}

TEST(save_round_trip_preserves_npc_affinity) {
    Fixture fixture;
    fixture.world.spawn_npc(make_npc("Blacksmith", 25, 25));
    Npc* smith = fixture.world.npc_at(25, 25);
    CHECK(smith != nullptr);
    if (smith) smith->set_affinity(83);

    GameState captured = capture_state(fixture.player, fixture.world, fixture.seed,
                                       0, fixture.time, 7);
    GameState restored;
    CHECK_EQ(deserialize_state(serialize_state(captured), restored), true);

    World reloaded;
    reloaded.init(restored.map_seed);
    apply_chunk_data(reloaded, restored.chunks);
    reloaded.update_loaded_chunks(restored.player_x, restored.player_y);

    Npc* reloaded_smith = reloaded.npc_at(25, 25);
    CHECK(reloaded_smith != nullptr);
    if (reloaded_smith) {
        CHECK_EQ(reloaded_smith->affinity(), 83);
        CHECK_EQ(reloaded_smith->is_merchant(), true);
        CHECK(!reloaded_smith->dialogue_tree().empty());
        CHECK(!reloaded_smith->shop_inventory().empty());
    }
}

TEST(save_and_load_through_disk) {
    Fixture fixture;
    GameState captured = capture_state(fixture.player, fixture.world, fixture.seed,
                                       42, fixture.time, 99);

    CHECK_EQ(save_game(2, captured), true);

    auto saves = list_saves();
    bool listed = false;
    for (const auto& entry : saves) {
        if (entry.slot != 2) continue;
        listed = true;
        CHECK_EQ(entry.play_time_seconds, 42);
        CHECK_EQ(entry.level, 3);
        CHECK_EQ(entry.day, 4);
    }
    CHECK_EQ(listed, true);

    GameState loaded;
    CHECK_EQ(load_game(2, loaded), true);
    CHECK_EQ(loaded.gold, 123);
    CHECK_EQ(loaded.rng_state, uint64_t{99});

    CHECK_EQ(delete_save(2), true);
    CHECK_EQ(load_game(2, loaded), false);
}

TEST(save_rejects_out_of_range_slots) {
    GameState state;
    CHECK_EQ(save_game(-1, state), false);
    CHECK_EQ(save_game(MAX_SAVE_SLOTS, state), false);
    CHECK_EQ(load_game(-1, state), false);
    CHECK_EQ(load_game(MAX_SAVE_SLOTS, state), false);
}

TEST(player_hp_restore_does_not_apply_armour) {
    // Routing the saved HP through take_damage() applied defence reduction, so
    // a player who saved at 7 HP came back with more.
    Player player;
    if (const Item* armor = find_item("Steel Armor")) {
        player.add_item(*armor, 1);
        player.equip_to_slot(EquipSlot::Torso, *armor);
    }
    player.set_hp(7);
    CHECK_EQ(player.hp(), 7);

    player.set_hp(player.max_hp() + 50);
    CHECK_EQ(player.hp(), player.max_hp());

    player.set_hp(-5);
    CHECK_EQ(player.hp(), 0);
}
