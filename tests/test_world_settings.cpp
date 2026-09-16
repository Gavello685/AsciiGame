#include "test_framework.h"

#include "game/settings.h"
#include "game/world.h"

namespace {

// DESIGN.md: 2 <= load <= 4, render <= load - 1, sim <= render, all >= 1.
void check_invariants(const World& world, const std::string& where) {
    if (world.load_radius() < World::MIN_LOAD_RADIUS ||
        world.load_radius() > World::MAX_LOAD_RADIUS) {
        ::testing::fail(where, "load radius out of range: " +
                               std::to_string(world.load_radius()));
    }
    if (world.render_radius() < 1) {
        ::testing::fail(where, "render radius below 1: " +
                               std::to_string(world.render_radius()));
    }
    if (world.simulation_radius() < 1) {
        ::testing::fail(where, "simulation radius below 1: " +
                               std::to_string(world.simulation_radius()));
    }
    if (world.render_radius() > world.load_radius() - 1) {
        ::testing::fail(where, "render radius not enclosed by load radius");
    }
    if (world.simulation_radius() > world.render_radius()) {
        ::testing::fail(where, "simulation radius exceeds render radius");
    }
}

}

TEST(default_radii_satisfy_invariants) {
    World world;
    world.init(1);
    check_invariants(world, TEST_LOCATION);

    // Shipped defaults must be a legal configuration too.
    Settings defaults;
    World configured;
    configured.init(1);
    configured.set_load_radius(defaults.load_radius);
    configured.set_render_radius(defaults.render_radius);
    configured.set_simulation_radius(defaults.sim_radius);
    check_invariants(configured, TEST_LOCATION);
    CHECK_EQ(configured.load_radius(), defaults.load_radius);
    CHECK_EQ(configured.render_radius(), defaults.render_radius);
    CHECK_EQ(configured.simulation_radius(), defaults.sim_radius);
}

TEST(radii_never_go_below_one) {
    // The settings sliders call these setters with the current value +/- 1.
    // The old cascading clamps could drive render to 0 and sim to -1, which
    // stopped anything simulating at all.
    for (int value = -5; value <= 10; ++value) {
        World world;
        world.init(1);

        world.set_load_radius(value);
        check_invariants(world, TEST_LOCATION);

        world.set_render_radius(value);
        check_invariants(world, TEST_LOCATION);

        world.set_simulation_radius(value);
        check_invariants(world, TEST_LOCATION);
    }
}

TEST(repeated_slider_decrements_stay_valid) {
    World world;
    world.init(1);
    for (int i = 0; i < 12; ++i) {
        world.set_load_radius(world.load_radius() - 1);
        world.set_render_radius(world.render_radius() - 1);
        world.set_simulation_radius(world.simulation_radius() - 1);
        check_invariants(world, TEST_LOCATION);
    }
    CHECK_EQ(world.load_radius(), World::MIN_LOAD_RADIUS);
    CHECK_EQ(world.render_radius(), 1);
    CHECK_EQ(world.simulation_radius(), 1);
}

TEST(repeated_slider_increments_stay_valid) {
    World world;
    world.init(1);
    for (int i = 0; i < 12; ++i) {
        world.set_load_radius(world.load_radius() + 1);
        world.set_render_radius(world.render_radius() + 1);
        world.set_simulation_radius(world.simulation_radius() + 1);
        check_invariants(world, TEST_LOCATION);
    }
    CHECK_EQ(world.load_radius(), World::MAX_LOAD_RADIUS);
}

TEST(raising_render_radius_grows_load_radius) {
    World world;
    world.init(1);
    world.set_load_radius(2);
    world.set_render_radius(3);
    check_invariants(world, TEST_LOCATION);
    CHECK_EQ(world.render_radius(), 3);
    CHECK_EQ(world.load_radius(), 4);
}

TEST(raising_simulation_radius_grows_the_rings_around_it) {
    World world;
    world.init(1);
    world.set_load_radius(2);
    world.set_render_radius(1);
    world.set_simulation_radius(3);
    check_invariants(world, TEST_LOCATION);
    CHECK_EQ(world.simulation_radius(), 3);
    CHECK(world.render_radius() >= 3);
    CHECK_EQ(world.load_radius(), 4);
}

TEST(settings_round_trip_clamps_to_valid_values) {
    Settings out_of_range;
    out_of_range.load_radius = 99;
    out_of_range.render_radius = -4;
    out_of_range.sim_radius = 0;

    World world;
    world.init(1);
    world.set_load_radius(out_of_range.load_radius);
    world.set_render_radius(out_of_range.render_radius);
    world.set_simulation_radius(out_of_range.sim_radius);
    check_invariants(world, TEST_LOCATION);
}

TEST(chunks_load_and_unload_around_the_player) {
    World world;
    world.init(2);
    world.update_loaded_chunks(0, 0);

    int expected = (2 * world.load_radius() + 1) * (2 * world.load_radius() + 1);
    CHECK_EQ(static_cast<int>(world.loaded_chunks().size()), expected);
    CHECK(world.get_chunk(0, 0) != nullptr);

    int far = CHUNK_SIZE * (world.load_radius() + 5);
    world.update_loaded_chunks(far, far);
    CHECK(world.get_chunk(0, 0) == nullptr);
    CHECK_EQ(static_cast<int>(world.loaded_chunks().size()), expected);
}
