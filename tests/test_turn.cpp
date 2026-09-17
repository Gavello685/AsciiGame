#include "test_framework.h"

#include "game/player.h"
#include "game/rng.h"
#include "game/time_system.h"
#include "game/turn.h"
#include "game/world.h"

namespace {

// Place the player on a passable tile near the origin village.
void spawn_player_somewhere_open(World& world, Player& player) {
    for (int r = 0; r < 40; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (world.is_passable(dx, dy)) {
                    player.spawn(dx, dy);
                    return;
                }
            }
        }
    }
    player.spawn(0, 0);
}

int count_enemies(const World& world) {
    int total = 0;
    World& mutable_world = const_cast<World&>(world);
    for (const auto& key : world.entity_chunk_keys()) {
        total += static_cast<int>(mutable_world.enemy_count_in_chunk(key));
    }
    return total;
}

}

TEST(turn_advances_time) {
    World world;
    world.init(11);
    world.update_loaded_chunks(0, 0);
    Player player;
    spawn_player_somewhere_open(world, player);
    TimeSystem time;
    Rng rng(1);

    int before = time.turn_of_day();
    resolve_turn(world, player, time, rng);
    CHECK_NE(time.turn_of_day(), before);
}

TEST(killing_an_adjacent_enemy_does_not_corrupt_the_entity_list) {
    // Resolving a turn used to walk a snapshot of raw enemy pointers while
    // removing dead enemies and spawning loot, which invalidated them.
    World world;
    world.init(12);
    world.update_loaded_chunks(0, 0);

    Player player;
    spawn_player_somewhere_open(world, player);
    world.update_loaded_chunks(player.x(), player.y());

    // Surround the player with weak enemies so several die in one turn.
    const int offsets[][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int placed = 0;
    for (const auto& offset : offsets) {
        int ex = player.x() + offset[0];
        int ey = player.y() + offset[1];
        if (!world.is_passable(ex, ey)) continue;
        Enemy rat = make_enemy("Rat", ex, ey);
        rat.set_hp(1); // dies to the player's counter-attack
        world.spawn_enemy(std::move(rat));
        placed++;
    }
    CHECK(placed > 0);

    int before = count_enemies(world);
    TimeSystem time;
    Rng rng(7);
    TurnOutcome outcome = resolve_turn(world, player, time, rng);

    CHECK(count_enemies(world) < before);
    CHECK(!outcome.messages.empty());

    // Every surviving enemy must still be readable through the world.
    for (const auto& key : world.entity_chunk_keys()) {
        for (size_t i = 0; i < world.enemy_count_in_chunk(key); ++i) {
            Enemy* enemy = world.enemy_in_chunk(key, i);
            CHECK(enemy != nullptr);
            if (enemy) {
                CHECK(enemy->is_alive());
                CHECK(!enemy->name().empty());
            }
        }
    }
}

TEST(dead_enemies_drop_loot_on_the_ground) {
    World world;
    world.init(13);
    world.update_loaded_chunks(0, 0);

    Player player;
    spawn_player_somewhere_open(world, player);
    world.update_loaded_chunks(player.x(), player.y());

    int ex = player.x() + 1, ey = player.y();
    if (!world.is_passable(ex, ey)) { ex = player.x(); ey = player.y() + 1; }
    if (!world.is_passable(ex, ey)) return; // boxed in; nothing to assert

    Enemy rat = make_enemy("Rat", ex, ey);
    rat.set_hp(1);
    world.spawn_enemy(std::move(rat));

    TimeSystem time;
    Rng rng(3);
    resolve_turn(world, player, time, rng);

    CHECK(world.enemy_at(ex, ey) == nullptr);
    Entity* loot = world.item_at(ex, ey);
    CHECK(loot != nullptr);
    if (loot) CHECK_EQ(loot->name(), std::string("Bread"));
}

TEST(player_gains_xp_from_a_kill_during_the_enemy_turn) {
    World world;
    world.init(14);
    world.update_loaded_chunks(0, 0);

    Player player;
    spawn_player_somewhere_open(world, player);
    world.update_loaded_chunks(player.x(), player.y());

    int ex = player.x() + 1, ey = player.y();
    if (!world.is_passable(ex, ey)) return;

    Enemy rat = make_enemy("Rat", ex, ey);
    rat.set_hp(1);
    world.spawn_enemy(std::move(rat));

    int xp_before = player.xp();
    TimeSystem time;
    Rng rng(4);
    resolve_turn(world, player, time, rng);
    CHECK(player.xp() > xp_before);
}

TEST(enemies_beyond_simulation_radius_do_not_act) {
    World world;
    world.init(15);
    world.set_simulation_radius(1);
    world.update_loaded_chunks(0, 0);

    Player player;
    spawn_player_somewhere_open(world, player);
    world.update_loaded_chunks(player.x(), player.y());

    // Two chunks out is outside a simulation radius of 1.
    int distant_x = player.x() + CHUNK_SIZE * 2 + 5;
    int distant_y = player.y();
    world.update_loaded_chunks(player.x(), player.y());
    if (!world.is_passable(distant_x, distant_y)) return;

    world.spawn_enemy(make_enemy("Rat", distant_x, distant_y));

    TimeSystem time;
    Rng rng(5);
    for (int i = 0; i < 80; ++i) resolve_turn(world, player, time, rng);

    Enemy* frozen = world.enemy_at(distant_x, distant_y);
    CHECK(frozen != nullptr);
}

TEST(many_turns_leave_the_world_consistent) {
    World world;
    world.init(16);
    world.update_loaded_chunks(0, 0);

    Player player;
    spawn_player_somewhere_open(world, player);
    world.update_loaded_chunks(player.x(), player.y());

    TimeSystem time;
    Rng rng(6);
    for (int i = 0; i < 300; ++i) {
        resolve_turn(world, player, time, rng);
        if (player.is_dead()) break;
    }

    // Every entity must sit in the chunk its coordinates say it does.
    for (const auto& key : world.entity_chunk_keys()) {
        for (size_t i = 0; i < world.enemy_count_in_chunk(key); ++i) {
            Enemy* enemy = world.enemy_in_chunk(key, i);
            CHECK(enemy != nullptr);
            if (!enemy) continue;
            CHECK_EQ(World::world_to_chunk_x(enemy->x()), key.x);
            CHECK_EQ(World::world_to_chunk_y(enemy->y()), key.y);
        }
        for (size_t i = 0; i < world.npc_count_in_chunk(key); ++i) {
            Npc* npc = world.npc_in_chunk(key, i);
            CHECK(npc != nullptr);
            if (!npc) continue;
            CHECK_EQ(World::world_to_chunk_x(npc->x()), key.x);
            CHECK_EQ(World::world_to_chunk_y(npc->y()), key.y);
        }
    }
}
