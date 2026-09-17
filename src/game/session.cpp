#include "game/session.h"

#include "game/chunk.h"
#include "game/enemy.h"
#include "game/entity.h"
#include "game/item.h"
#include "game/npc.h"
#include "game/structure.h"

#include <algorithm>
#include <cmath>

namespace {

// Derive the action RNG's seed from the map seed so that one number in the
// save file reproduces both the terrain and everything rolled on top of it.
uint64_t rng_seed_from(uint32_t map_seed) {
    return 0x9E3779B97F4A7C15ull ^
           (static_cast<uint64_t>(map_seed) * 6364136223846793005ull);
}

// Scan a chunk for somewhere a villager would stand. Village interiors are
// Floor, so this lands the player inside the settlement rather than in the
// grass outside it.
bool find_floor_in_chunk(const World& world, int cx, int cy, int& out_x, int& out_y) {
    int base_x = cx * CHUNK_SIZE;
    int base_y = cy * CHUNK_SIZE;
    for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            int wx = base_x + lx;
            int wy = base_y + ly;
            if (world.get_tile(wx, wy).type != TileType::Floor) continue;
            if (!world.is_passable(wx, wy)) continue;
            out_x = wx;
            out_y = wy;
            return true;
        }
    }
    return false;
}

// Walk the loaded chunks outward from the origin looking for a village to
// start in. The origin chunk always holds one, so the search normally stops
// on its first step.
bool find_village_spawn(World& world, int& out_x, int& out_y) {
    for (int ring = 0; ring <= world.load_radius(); ++ring) {
        for (int cy = -ring; cy <= ring; ++cy) {
            for (int cx = -ring; cx <= ring; ++cx) {
                // Only the newly added edge of each ring.
                if (std::abs(cx) != ring && std::abs(cy) != ring) continue;
                const Chunk* chunk = world.get_chunk(cx, cy);
                if (!chunk) continue;
                if (chunk->structure_id() != static_cast<int>(StructureType::Village)) continue;
                if (find_floor_in_chunk(world, cx, cy, out_x, out_y)) return true;
            }
        }
    }
    return false;
}

// Last resort when there is no village: the nearest passable tile to the
// origin, so the player never starts inside a mountain.
bool find_any_passable(const World& world, int& out_x, int& out_y) {
    for (int ring = 0; ring < 100; ++ring) {
        for (int dy = -ring; dy <= ring; ++dy) {
            for (int dx = -ring; dx <= ring; ++dx) {
                if (std::abs(dx) != ring && std::abs(dy) != ring) continue;
                if (!world.is_passable(dx, dy)) continue;
                out_x = dx;
                out_y = dy;
                return true;
            }
        }
    }
    return false;
}

// Pick a free tile within reach of the player, or report failure. Used to
// scatter the starting items, NPCs and enemies without stacking them.
bool find_open_tile(Session& s, int reach, int& out_x, int& out_y) {
    for (int attempt = 0; attempt < 200; ++attempt) {
        int x = s.player.x() + s.rng.range(-reach, reach);
        int y = s.player.y() + s.rng.range(-reach, reach);
        if (!s.world.is_passable(x, y)) continue;
        if (s.world.has_npc_at(x, y) || s.world.has_enemy_at(x, y)) continue;
        out_x = x;
        out_y = y;
        return true;
    }
    return false;
}

// The world clamps each radius against the others, so the order here matters
// only in that a fresh World starts from a valid configuration.
void apply_radii(World& world, const Settings& settings) {
    world.set_load_radius(settings.load_radius);
    world.set_render_radius(settings.render_radius);
    world.set_simulation_radius(settings.sim_radius);
}

void give_starting_gear(Player& player) {
    const std::pair<const char*, int> kit[] = {
        {"Bread", 3}, {"Health Potion", 1}, {"Iron Sword", 1},
        {"Leather Armor", 1}, {"Torch", 1},
    };
    for (const auto& [name, count] : kit) {
        if (const Item* item = find_item(name)) player.add_item(*item, count);
    }
}

void scatter_ground_items(Session& s) {
    const char* names[] = {"Gold Coin", "Health Potion", "Old Scroll",
                           "Bread", "Rusty Key", "Apple"};
    const int name_count = static_cast<int>(sizeof(names) / sizeof(names[0]));

    int count = 10 + s.rng.below(6);
    for (int i = 0; i < count; ++i) {
        int x, y;
        if (!find_open_tile(s, 30, x, y)) continue;
        const Item* item = find_item(names[s.rng.below(name_count)]);
        if (!item) continue;
        Entity e(x, y, item->glyph(), item->name(),
                 item->fg_r(), item->fg_g(), item->fg_b());
        e.set_is_item(true);
        s.world.spawn_item(std::move(e));
    }
}

void scatter_wanderers(Session& s) {
    // The origin village supplies the six settlement NPCs (see
    // village_npc_names), so only the roaming archetypes are placed here.
    for (const char* name : {"Old Sage", "Wanderer", "Child"}) {
        int x, y;
        if (!find_open_tile(s, 20, x, y)) continue;
        s.world.spawn_npc(make_npc(name, x, y));
    }
}

void scatter_enemies(Session& s) {
    // Far enough out that the player is not attacked on their first turn.
    const int safe_distance = 15;
    for (const char* name : {"Rat", "Rat", "Goblin", "Goblin", "Spider", "Bat"}) {
        int x, y;
        if (!find_open_tile(s, 25, x, y)) continue;
        if (std::abs(x - s.player.x()) + std::abs(y - s.player.y()) < safe_distance) continue;
        s.world.spawn_enemy(make_enemy(name, x, y));
    }
}

}

void session_apply_settings(Session& s, Settings& settings) {
    apply_radii(s.world, settings);

    settings.load_radius = s.world.load_radius();
    settings.render_radius = s.world.render_radius();
    settings.sim_radius = s.world.simulation_radius();
}

std::string session_start(Session& s, const Settings& settings, uint32_t seed) {
    s.world = World();
    s.player = Player();
    s.time = TimeSystem();
    s.map_seed = seed;
    s.rng.seed(rng_seed_from(seed));
    s.turn_counter = 0;
    s.play_time_seconds = 0;

    s.world.init(s.map_seed);
    apply_radii(s.world, settings);

    // The origin chunk always holds a village, so generate it before looking
    // for a doorstep to stand on.
    s.world.update_loaded_chunks(0, 0);

    int spawn_x = 0, spawn_y = 0;
    if (!find_village_spawn(s.world, spawn_x, spawn_y) &&
        !find_any_passable(s.world, spawn_x, spawn_y)) {
        spawn_x = 0;
        spawn_y = 0;
    }
    s.player.spawn(spawn_x, spawn_y);
    s.world.update_loaded_chunks(s.player.x(), s.player.y());

    give_starting_gear(s.player);
    scatter_ground_items(s);
    scatter_wanderers(s);
    scatter_enemies(s);

    s.started = true;
    return "You arrive at the village. The wilds await.";
}

void session_restore(Session& s, const GameState& state, const Settings& settings) {
    s.world = World();
    s.player = Player();
    s.time = TimeSystem();
    s.map_seed = state.map_seed;
    s.turn_counter = 0;
    s.play_time_seconds = state.meta.play_time_seconds;

    s.world.init(s.map_seed);
    apply_radii(s.world, settings);

    s.rng.set_state(state.rng_state);

    // Stats and equipment go in before HP so max_hp is final when the saved
    // value is clamped against it.
    s.player.spawn(state.player_x, state.player_y);
    s.player.stats() = state.stats;
    s.player.set_xp(state.xp);
    s.player.set_level(state.level);
    s.player.set_gold(state.gold);
    for (const auto& [item, qty] : state.inventory) s.player.add_item(item, qty);
    for (const auto& [slot, item] : state.equipment) s.player.equip_to_slot(slot, item);
    s.player.set_hp(state.hp);

    s.world.clear_zones();
    for (const Zone& zone : state.zones) s.world.add_zone(zone);

    // Terrain deltas and entities are staged before any chunk is generated,
    // so loading a chunk replays the save instead of spawning a fresh
    // population that then has to be cleared.
    apply_chunk_data(s.world, state.chunks);
    s.world.update_loaded_chunks(s.player.x(), s.player.y());

    s.time.set_turn_of_day(state.turn_of_day);
    s.time.set_day(state.day);

    s.started = true;
}

GameState session_capture(const Session& s) {
    return capture_state(s.player, s.world, s.map_seed, s.play_time_seconds,
                         s.time, s.rng.state());
}
