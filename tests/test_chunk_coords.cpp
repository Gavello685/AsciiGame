#include "test_framework.h"

#include "game/chunk.h"
#include "game/world.h"

// World-to-chunk conversion has to floor, not truncate, or the chunk boundary
// at the origin is twice as wide as every other one and negative coordinates
// alias onto positive chunks.

TEST(chunk_coords_positive) {
    CHECK_EQ(World::world_to_chunk_x(0), 0);
    CHECK_EQ(World::world_to_chunk_x(CHUNK_SIZE - 1), 0);
    CHECK_EQ(World::world_to_chunk_x(CHUNK_SIZE), 1);
    CHECK_EQ(World::world_to_chunk_x(CHUNK_SIZE * 3 + 5), 3);
}

TEST(chunk_coords_negative) {
    CHECK_EQ(World::world_to_chunk_x(-1), -1);
    CHECK_EQ(World::world_to_chunk_x(-CHUNK_SIZE), -1);
    CHECK_EQ(World::world_to_chunk_x(-CHUNK_SIZE - 1), -2);
    CHECK_EQ(World::world_to_chunk_y(-CHUNK_SIZE * 2), -2);
}

TEST(chunk_coords_local_always_in_range) {
    for (int w = -CHUNK_SIZE * 3 - 7; w <= CHUNK_SIZE * 3 + 7; ++w) {
        int cx = 0, cy = 0, lx = 0, ly = 0;
        World::world_to_chunk(w, w, cx, cy, lx, ly);
        CHECK(lx >= 0 && lx < CHUNK_SIZE);
        CHECK(ly >= 0 && ly < CHUNK_SIZE);
        // The pair must reconstruct the original world coordinate.
        CHECK_EQ(cx * CHUNK_SIZE + lx, w);
        CHECK_EQ(cy * CHUNK_SIZE + ly, w);
    }
}

TEST(chunk_coords_round_trip_across_boundary) {
    const int samples[] = {-129, -128, -127, -65, -64, -63, -1, 0, 1, 63, 64, 65};
    for (int w : samples) {
        int cx = 0, cy = 0, lx = 0, ly = 0;
        World::world_to_chunk(w, 0, cx, cy, lx, ly);
        CHECK_EQ(cx, World::world_to_chunk_x(w));
        CHECK_EQ(cx * CHUNK_SIZE + lx, w);
    }
}
