#include "test_framework.h"

#include "game/chunk.h"
#include "game/world.h"

// Player-built tiles used to be written straight into the tile array without
// being recorded as a delta, so they vanished on save and on chunk unload.

TEST(generated_tiles_are_not_recorded_as_deltas) {
    Chunk chunk(0, 0, 1234);
    CHECK_EQ(chunk.modifications().size(), size_t{0});

    chunk.set_generated(5, 5, TileType::Wall);
    CHECK_EQ(chunk.modifications().size(), size_t{0});
    CHECK(chunk.get(5, 5).type == TileType::Wall);
}

TEST(player_edits_are_recorded_as_deltas) {
    Chunk chunk(0, 0, 1234);
    chunk.modify(5, 5, TileType::WoodWall);

    CHECK_EQ(chunk.modifications().size(), size_t{1});
    CHECK_EQ(chunk.modifications()[0].local_x, 5);
    CHECK_EQ(chunk.modifications()[0].local_y, 5);
    CHECK(chunk.modifications()[0].type == TileType::WoodWall);
    CHECK(chunk.is_dirty());
}

TEST(repeated_edits_to_one_tile_collapse) {
    Chunk chunk(0, 0, 1234);
    chunk.modify(7, 3, TileType::WoodWall);
    chunk.modify(7, 3, TileType::WoodFloor);
    chunk.modify(7, 3, TileType::WoodDoor);

    CHECK_EQ(chunk.modifications().size(), size_t{1});
    CHECK(chunk.modifications()[0].type == TileType::WoodDoor);
    CHECK(chunk.get(7, 3).type == TileType::WoodDoor);
}

TEST(built_tile_is_reported_in_save_data) {
    World world;
    world.init(4242);
    world.update_loaded_chunks(0, 0);

    world.place_tile(10, 10, TileType::WoodWall);
    CHECK(world.get_tile(10, 10).type == TileType::WoodWall);

    bool found = false;
    for (const auto& chunk : world.gather_save_data()) {
        for (const auto& mod : chunk.modifications) {
            if (chunk.cx == 0 && chunk.cy == 0 && mod.local_x == 10 && mod.local_y == 10) {
                found = true;
                CHECK(mod.type == TileType::WoodWall);
            }
        }
    }
    CHECK(found);
}

TEST(built_tile_survives_chunk_unload_and_reload) {
    World world;
    world.init(4242);
    world.update_loaded_chunks(0, 0);
    world.place_tile(10, 10, TileType::WoodWall);

    // Walk far enough that chunk (0,0) is unloaded, then walk back.
    int far = CHUNK_SIZE * (world.load_radius() + 4);
    world.update_loaded_chunks(far, far);
    CHECK(world.get_chunk(0, 0) == nullptr);

    world.update_loaded_chunks(0, 0);
    CHECK(world.get_tile(10, 10).type == TileType::WoodWall);
}

TEST(player_placed_objects_survive_unload_but_structure_torches_do_not) {
    World world;
    world.init(4242);
    world.update_loaded_chunks(0, 0);

    // The origin chunk always holds a village, which brings wall torches.
    size_t generated_objects = 0;
    for (const auto& chunk : world.gather_save_data()) {
        if (chunk.cx == 0 && chunk.cy == 0) generated_objects = chunk.placed_objects.size();
    }
    // Structure torches are seed-derived and so are never persisted.
    CHECK_EQ(generated_objects, size_t{0});

    PlacedObject campfire;
    campfire.glyph = '*';
    campfire.name = "Campfire";
    campfire.is_light = true;
    campfire.light_radius = 4;
    world.add_placed_object(12, 12, campfire);

    size_t saved_objects = 0;
    for (const auto& chunk : world.gather_save_data()) {
        if (chunk.cx == 0 && chunk.cy == 0) saved_objects = chunk.placed_objects.size();
    }
    CHECK_EQ(saved_objects, size_t{1});

    int far = CHUNK_SIZE * (world.load_radius() + 4);
    world.update_loaded_chunks(far, far);
    world.update_loaded_chunks(0, 0);

    const PlacedObject* restored = world.placed_object_at(12, 12);
    CHECK(restored != nullptr);
    if (restored) CHECK_EQ(restored->name, std::string("Campfire"));
}

TEST(explored_tiles_survive_chunk_unload) {
    World world;
    world.init(99);
    world.update_loaded_chunks(0, 0);
    world.set_explored(20, 20, true);
    CHECK_EQ(world.get_tile(20, 20).explored, true);

    int far = CHUNK_SIZE * (world.load_radius() + 4);
    world.update_loaded_chunks(far, far);
    world.update_loaded_chunks(0, 0);

    CHECK_EQ(world.get_tile(20, 20).explored, true);
}
