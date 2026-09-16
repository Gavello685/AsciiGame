#include "game/turn.h"

#include "game/enemy.h"
#include "game/entity.h"
#include "game/npc.h"
#include "game/player.h"
#include "game/rng.h"
#include "game/time_system.h"
#include "game/world.h"

#include <cmath>
#include <cstdlib>

namespace {

int manhattan(int ax, int ay, int bx, int by) {
    return std::abs(ax - bx) + std::abs(ay - by);
}

// Drop an enemy's loot on the ground where it fell.
void spill_drops(World& world, const Enemy& enemy) {
    for (const auto& [item, qty] : enemy.drops()) {
        for (int n = 0; n < qty; ++n) {
            Entity dropped(enemy.x(), enemy.y(), item.glyph(), item.name(),
                           item.fg_r(), item.fg_g(), item.fg_b());
            dropped.set_is_item(true);
            world.spawn_item(std::move(dropped));
        }
    }
}

// One enemy's turn: move, then trade blows with the player if adjacent.
// Returns true if the enemy died and should be removed.
bool step_enemy(Enemy& enemy, World& world, Player& player,
                const TimeSystem& time, Rng& rng, std::vector<std::string>& messages) {
    enemy.update(world, player.x(), player.y(), rng);

    if (manhattan(enemy.x(), enemy.y(), player.x(), player.y()) > 1) return false;

    int raw_attack = enemy.attack() + rng.variance(enemy.damage_variance());
    // Night predators hit harder after dark.
    if (enemy.night_predator() && time.is_night()) raw_attack += 2;

    int dealt = player.calc_damage(raw_attack);
    player.take_damage(raw_attack);
    std::string message = enemy.name() + " hits you for " + std::to_string(dealt) + " damage!";

    // The player always counter-attacks.
    int raw_counter = player.total_attack() + rng.variance(player.total_damage_variance());
    int counter = std::max(1, raw_counter - enemy.defense());
    enemy.take_damage(counter);
    message += " You hit back for " + std::to_string(counter) + "!";

    if (!enemy.is_alive()) {
        message += " " + enemy.name() + " killed! +" +
                   std::to_string(enemy.xp_value()) + " XP";
        player.add_xp(enemy.xp_value());
        spill_drops(world, enemy);
        messages.push_back(std::move(message));
        return true;
    }

    messages.push_back(std::move(message));
    return false;
}

}

TurnOutcome resolve_turn(World& world, Player& player, TimeSystem& time, Rng& rng) {
    TurnOutcome outcome;

    time.advance();

    int player_cx = World::world_to_chunk_x(player.x());
    int player_cy = World::world_to_chunk_y(player.y());
    int sim = world.simulation_radius();

    // Snapshot the keys: spawning loot can insert new chunk entries, which
    // would invalidate iterators over the entity map.
    std::vector<ChunkCoord> keys = world.entity_chunk_keys();

    for (const ChunkCoord& key : keys) {
        if (std::abs(key.x - player_cx) > sim || std::abs(key.y - player_cy) > sim) continue;

        // Index-based walk with a fresh pointer each step: step_enemy can
        // spawn loot and the erase below shifts later elements, so a cached
        // pointer or a bulk pointer list would dangle.
        for (size_t i = 0; i < world.enemy_count_in_chunk(key);) {
            Enemy* enemy = world.enemy_in_chunk(key, i);
            if (!enemy) break;
            if (!enemy->is_alive()) {
                world.erase_enemy_in_chunk(key, i);
                continue;
            }
            if (step_enemy(*enemy, world, player, time, rng, outcome.messages)) {
                world.erase_enemy_in_chunk(key, i);
            } else {
                ++i;
            }
        }

        for (size_t i = 0; i < world.npc_count_in_chunk(key); ++i) {
            Npc* npc = world.npc_in_chunk(key, i);
            if (npc) npc->update(world, player.x(), player.y(), rng);
        }
    }

    // Entities that crossed a chunk boundary are re-filed under their new key.
    world.rekey_entities();

    outcome.player_died = player.is_dead();
    return outcome;
}
