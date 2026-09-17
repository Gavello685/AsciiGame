#include "test_framework.h"

#include "game/session.h"

#include <string>

namespace {

Settings default_settings() { return Settings{}; }

int inventory_count(const Player& player, const std::string& name) {
    int total = 0;
    for (const auto& [item, qty] : player.inventory()) {
        if (item.name() == name) total += qty;
    }
    return total;
}

}

TEST(a_new_run_starts_the_player_somewhere_they_can_stand) {
    Session session;
    Settings settings = default_settings();
    std::string arrival = session_start(session, settings, 4242);

    CHECK(session.started);
    CHECK(!arrival.empty());
    CHECK(session.world.is_passable(session.player.x(), session.player.y()));
    CHECK_EQ(session.map_seed, 4242u);
}

TEST(a_new_run_hands_out_the_starting_kit) {
    Session session;
    Settings settings = default_settings();
    session_start(session, settings, 99);

    CHECK_EQ(inventory_count(session.player, "Bread"), 3);
    CHECK_EQ(inventory_count(session.player, "Health Potion"), 1);
    CHECK_EQ(inventory_count(session.player, "Iron Sword"), 1);
    CHECK_EQ(inventory_count(session.player, "Leather Armor"), 1);
    CHECK_EQ(inventory_count(session.player, "Torch"), 1);
}

TEST(the_same_seed_produces_the_same_run) {
    Settings settings = default_settings();

    Session first;
    session_start(first, settings, 777);
    Session second;
    session_start(second, settings, 777);

    CHECK_EQ(first.player.x(), second.player.x());
    CHECK_EQ(first.player.y(), second.player.y());
    CHECK_EQ(first.rng.state(), second.rng.state());
}

TEST(different_seeds_produce_different_runs) {
    Settings settings = default_settings();

    Session first;
    session_start(first, settings, 1);
    Session second;
    session_start(second, settings, 2);

    CHECK_NE(first.rng.state(), second.rng.state());
}

TEST(starting_a_second_run_clears_the_first) {
    Session session;
    Settings settings = default_settings();
    session_start(session, settings, 5);

    session.turn_counter = 120;
    session.play_time_seconds = 900;
    session.player.add_gold(1000);

    session_start(session, settings, 6);

    CHECK_EQ(session.turn_counter, 0);
    CHECK_EQ(session.play_time_seconds, 0);
    CHECK_EQ(session.player.gold(), 50); // back to the Player default
}

TEST(capture_and_restore_round_trip_a_run) {
    Session original;
    Settings settings = default_settings();
    session_start(original, settings, 31337);

    original.player.add_gold(275);
    original.player.add_xp(40);
    original.play_time_seconds = 612;
    original.time.set_day(4);

    GameState state = session_capture(original);

    Session reloaded;
    session_restore(reloaded, state, settings);

    CHECK(reloaded.started);
    CHECK_EQ(reloaded.map_seed, original.map_seed);
    CHECK_EQ(reloaded.rng.state(), original.rng.state());
    CHECK_EQ(reloaded.player.x(), original.player.x());
    CHECK_EQ(reloaded.player.y(), original.player.y());
    CHECK_EQ(reloaded.player.gold(), original.player.gold());
    CHECK_EQ(reloaded.player.xp(), original.player.xp());
    CHECK_EQ(reloaded.player.hp(), original.player.hp());
    CHECK_EQ(reloaded.player.inventory().size(), original.player.inventory().size());
    CHECK_EQ(reloaded.play_time_seconds, 612);
    CHECK_EQ(reloaded.time.day(), 4);
}

TEST(a_restored_run_keeps_terrain_the_player_changed) {
    Session original;
    Settings settings = default_settings();
    session_start(original, settings, 8080);

    int wx = original.player.x() + 2;
    int wy = original.player.y();
    original.world.place_tile(wx, wy, TileType::WoodWall);

    GameState state = session_capture(original);

    Session reloaded;
    session_restore(reloaded, state, settings);
    CHECK_EQ(static_cast<int>(reloaded.world.get_tile(wx, wy).type),
             static_cast<int>(TileType::WoodWall));
}

TEST(applying_settings_reports_what_the_world_accepted) {
    Session session;
    Settings settings = default_settings();
    session_start(session, settings, 12);

    // Render distance cannot reach the load radius, so asking for equal
    // values has to come back corrected rather than silently ignored.
    settings.load_radius = 3;
    settings.render_radius = 3;
    settings.sim_radius = 3;
    session_apply_settings(session, settings);

    CHECK_EQ(settings.load_radius, session.world.load_radius());
    CHECK_EQ(settings.render_radius, session.world.render_radius());
    CHECK_EQ(settings.sim_radius, session.world.simulation_radius());
    CHECK(settings.render_radius <= settings.load_radius - 1);
    CHECK(settings.sim_radius <= settings.render_radius);
    CHECK(settings.sim_radius >= 1);
}

TEST(a_run_started_from_invalid_settings_is_still_playable) {
    Session session;
    Settings settings;
    settings.load_radius = 99;
    settings.render_radius = 0;
    settings.sim_radius = -4;

    session_start(session, settings, 64);

    CHECK(session.world.load_radius() >= World::MIN_LOAD_RADIUS);
    CHECK(session.world.load_radius() <= World::MAX_LOAD_RADIUS);
    CHECK(session.world.render_radius() >= 1);
    CHECK(session.world.simulation_radius() >= 1);
    CHECK(session.world.is_passable(session.player.x(), session.player.y()));
}
