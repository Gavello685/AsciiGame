#include "game/world.h"
#include "game/noise.h"
#include "game/biome.h"
#include "game/structure.h"
#include "game/dialogue.h"
#include "game/item.h"
#include <algorithm>
#include <cmath>

void World::init(uint32_t seed) {
    seed_ = seed;
    noise::seed(seed);
}

void World::update_loaded_chunks(int player_world_x, int player_world_y) {
    int player_cx = world_to_chunk_x(player_world_x);
    int player_cy = world_to_chunk_y(player_world_y);

    for (int dy = -load_radius_; dy <= load_radius_; ++dy) {
        for (int dx = -load_radius_; dx <= load_radius_; ++dx) {
            ChunkCoord key{player_cx + dx, player_cy + dy};
            if (chunks_.find(key) != chunks_.end()) continue;

            // Terrain always comes from the seed, so it is regenerated rather
            // than stored. Only the non-reproducible parts are replayed.
            chunks_.emplace(key, Chunk(key.x, key.y, seed_));
            Chunk& chunk = chunks_.at(key);
            chunk.mark_visited();
            generate_chunk_contents(key, chunk);

            if (archive_.find(key) != archive_.end()) {
                restore_chunk(key, chunk);
            } else {
                spawn_structure_entities(
                    key.x, key.y, static_cast<StructureType>(chunk.structure_id()));
                spawn_biome_entities(chunk, key.x, key.y);
            }
        }
    }

    // Unload beyond load radius + 1 buffer, archiving anything generation
    // cannot rebuild.
    int unload_radius = load_radius_ + 1;
    std::vector<ChunkCoord> to_remove;
    for (const auto& [key, chunk] : chunks_) {
        (void)chunk;
        if (std::abs(key.x - player_cx) > unload_radius ||
            std::abs(key.y - player_cy) > unload_radius) {
            to_remove.push_back(key);
        }
    }
    for (const auto& key : to_remove) {
        archive_chunk(key);
        entities_.erase(key);
        chunks_.erase(key);
    }
}

// Seed-derived terrain features: structure stamp, structure id, wall torches.
void World::generate_chunk_contents(ChunkCoord key, Chunk& chunk) {
    if (try_place_structure(chunk, key.x, key.y, seed_) != StructureType::None) return;

    // The origin chunk always holds a village so a new game has somewhere to
    // start. Deterministic, so it regenerates identically every load.
    if (key.x == 0 && key.y == 0) {
        force_place_structure(chunk, StructureType::Village);
    }
}

void World::archive_chunk(ChunkCoord key) {
    auto chunk_it = chunks_.find(key);
    if (chunk_it == chunks_.end()) return;
    const Chunk& chunk = chunk_it->second;

    ChunkArchive& stored = archive_[key];
    stored.explored = chunk.explored_bits();
    stored.modifications = chunk.modifications();

    stored.placed_objects.clear();
    for (const auto& obj : chunk.placed_objects()) {
        if (obj.player_placed) stored.placed_objects.push_back(obj);
    }

    auto ent_it = entities_.find(key);
    if (ent_it != entities_.end()) {
        stored.entities = ent_it->second;
    } else {
        stored.entities = ChunkEntities{};
    }
}

void World::restore_chunk_terrain(ChunkCoord key, Chunk& chunk) {
    const ChunkArchive& stored = archive_.at(key);
    chunk.mark_visited();

    if (!stored.explored.empty()) chunk.apply_explored(stored.explored);
    if (!stored.modifications.empty()) chunk.apply_modifications(stored.modifications);
    for (const auto& obj : stored.placed_objects) chunk.add_placed_object(obj);
}

void World::restore_chunk(ChunkCoord key, Chunk& chunk) {
    restore_chunk_terrain(key, chunk);
    entities_[key] = archive_.at(key).entities;
}

bool World::is_chunk_known(int cx, int cy) const {
    ChunkCoord key{cx, cy};
    return chunks_.find(key) != chunks_.end() || archive_.find(key) != archive_.end();
}

Chunk* World::get_chunk(int cx, int cy) {
    ChunkCoord key{cx, cy};
    auto it = chunks_.find(key);
    if (it == chunks_.end()) return nullptr;
    return &it->second;
}

const Chunk* World::get_chunk(int cx, int cy) const {
    ChunkCoord key{cx, cy};
    auto it = chunks_.find(key);
    if (it == chunks_.end()) return nullptr;
    return &it->second;
}

Tile World::get_tile(int world_x, int world_y) const {
    int cx, cy, lx, ly;
    world_to_chunk(world_x, world_y, cx, cy, lx, ly);
    const Chunk* chunk = get_chunk(cx, cy);
    if (!chunk) return {TileType::None, false, false, 0};
    return chunk->get(lx, ly);
}

bool World::in_bounds(int world_x, int world_y) const {
    int cx, cy, lx, ly;
    world_to_chunk(world_x, world_y, cx, cy, lx, ly);
    const Chunk* chunk = get_chunk(cx, cy);
    if (!chunk) return false;
    return chunk->in_bounds(lx, ly);
}

bool World::is_passable(int world_x, int world_y) const {
    int cx, cy, lx, ly;
    world_to_chunk(world_x, world_y, cx, cy, lx, ly);
    const Chunk* chunk = get_chunk(cx, cy);
    if (!chunk) return false;
    return chunk->is_passable(lx, ly);
}

bool World::is_opaque(int world_x, int world_y) const {
    int cx, cy, lx, ly;
    world_to_chunk(world_x, world_y, cx, cy, lx, ly);
    const Chunk* chunk = get_chunk(cx, cy);
    if (!chunk) return true;
    return chunk->is_opaque(lx, ly);
}

void World::set_visible(int world_x, int world_y, bool v) {
    int cx, cy, lx, ly;
    world_to_chunk(world_x, world_y, cx, cy, lx, ly);
    Chunk* chunk = get_chunk(cx, cy);
    if (!chunk) return;
    chunk->set_visible(lx, ly, v);
}

void World::set_explored(int world_x, int world_y, bool v) {
    int cx, cy, lx, ly;
    world_to_chunk(world_x, world_y, cx, cy, lx, ly);
    Chunk* chunk = get_chunk(cx, cy);
    if (!chunk) return;
    chunk->set_explored(lx, ly, v);
}

void World::set_light_level(int world_x, int world_y, uint8_t level) {
    int cx, cy, lx, ly;
    world_to_chunk(world_x, world_y, cx, cy, lx, ly);
    Chunk* chunk = get_chunk(cx, cy);
    if (!chunk) return;
    chunk->set_light(lx, ly, level);
}

// Coordinate conversion
void World::world_to_chunk(int world_x, int world_y, int& cx, int& cy, int& lx, int& ly) {
    // Handle negative coordinates correctly
    cx = (world_x >= 0) ? world_x / CHUNK_SIZE : (world_x - CHUNK_SIZE + 1) / CHUNK_SIZE;
    cy = (world_y >= 0) ? world_y / CHUNK_SIZE : (world_y - CHUNK_SIZE + 1) / CHUNK_SIZE;
    lx = world_x - cx * CHUNK_SIZE;
    ly = world_y - cy * CHUNK_SIZE;
}

void World::chunk_to_world(int cx, int cy, int lx, int ly, int& world_x, int& world_y) {
    world_x = cx * CHUNK_SIZE + lx;
    world_y = cy * CHUNK_SIZE + ly;
}

int World::world_to_chunk_x(int world_x) {
    return (world_x >= 0) ? world_x / CHUNK_SIZE : (world_x - CHUNK_SIZE + 1) / CHUNK_SIZE;
}

int World::world_to_chunk_y(int world_y) {
    return (world_y >= 0) ? world_y / CHUNK_SIZE : (world_y - CHUNK_SIZE + 1) / CHUNK_SIZE;
}

// Invariants (DESIGN.md): 2 <= load <= 4, 1 <= render <= load - 1,
// 1 <= sim <= render. Every radius stays at least 1; the previous cascading
// clamps could drive render to 0 and sim to -1 from the settings sliders.
void World::reconcile_radii() {
    load_radius_ = std::max(MIN_LOAD_RADIUS, std::min(MAX_LOAD_RADIUS, load_radius_));
    render_radius_ = std::max(1, std::min(load_radius_ - 1, render_radius_));
    sim_radius_ = std::max(1, std::min(render_radius_, sim_radius_));
}

void World::set_load_radius(int r) {
    load_radius_ = r;
    reconcile_radii();
}

void World::set_render_radius(int r) {
    render_radius_ = std::max(1, std::min(MAX_LOAD_RADIUS - 1, r));
    // Growing the render ring grows the load ring to keep it enclosing.
    load_radius_ = std::max(load_radius_, render_radius_ + 1);
    reconcile_radii();
}

void World::set_simulation_radius(int r) {
    sim_radius_ = std::max(1, std::min(MAX_LOAD_RADIUS - 1, r));
    render_radius_ = std::max(render_radius_, sim_radius_);
    load_radius_ = std::max(load_radius_, render_radius_ + 1);
    reconcile_radii();
}

bool World::is_in_render_range(int world_x, int world_y, int player_wx, int player_wy) const {
    int dx = world_to_chunk_x(world_x) - world_to_chunk_x(player_wx);
    int dy = world_to_chunk_y(world_y) - world_to_chunk_y(player_wy);
    return std::abs(dx) <= render_radius_ && std::abs(dy) <= render_radius_;
}

bool World::is_in_simulation_range(int world_x, int world_y, int player_wx, int player_wy) const {
    int dx = world_to_chunk_x(world_x) - world_to_chunk_x(player_wx);
    int dy = world_to_chunk_y(world_y) - world_to_chunk_y(player_wy);
    return std::abs(dx) <= sim_radius_ && std::abs(dy) <= sim_radius_;
}

std::vector<ChunkCoord> World::nearby_chunk_keys(int player_wx, int player_wy) const {
    std::vector<ChunkCoord> result;
    int pcx = world_to_chunk_x(player_wx);
    int pcy = world_to_chunk_y(player_wy);
    for (int dy = -load_radius_; dy <= load_radius_; ++dy) {
        for (int dx = -load_radius_; dx <= load_radius_; ++dx) {
            result.push_back({pcx + dx, pcy + dy});
        }
    }
    return result;
}

void World::place_tile(int world_x, int world_y, TileType type) {
    int cx, cy, lx, ly;
    world_to_chunk(world_x, world_y, cx, cy, lx, ly);
    Chunk* chunk = get_chunk(cx, cy);
    if (!chunk) return;
    chunk->modify(lx, ly, type);
}

void World::add_placed_object(int world_x, int world_y, const PlacedObject& obj) {
    int cx, cy, lx, ly;
    world_to_chunk(world_x, world_y, cx, cy, lx, ly);
    Chunk* chunk = get_chunk(cx, cy);
    if (!chunk) return;
    PlacedObject local_obj = obj;
    local_obj.local_x = lx;
    local_obj.local_y = ly;
    local_obj.player_placed = true; // only the build system reaches this path
    chunk->add_placed_object(local_obj);
}

const PlacedObject* World::placed_object_at(int world_x, int world_y) const {
    int cx, cy, lx, ly;
    world_to_chunk(world_x, world_y, cx, cy, lx, ly);
    const Chunk* chunk = get_chunk(cx, cy);
    if (!chunk) return nullptr;
    for (const auto& obj : chunk->placed_objects()) {
        if (obj.local_x == lx && obj.local_y == ly) return &obj;
    }
    return nullptr;
}

std::vector<World::ChunkSaveData> World::gather_save_data() const {
    std::vector<ChunkSaveData> result;
    result.reserve(chunks_.size() + archive_.size());

    // Loaded chunks hold the authoritative state.
    for (const auto& [key, chunk] : chunks_) {
        if (!chunk.is_visited()) continue;
        ChunkSaveData data;
        data.cx = key.x;
        data.cy = key.y;
        data.explored = chunk.explored_bits();
        data.modifications = chunk.modifications();
        for (const auto& obj : chunk.placed_objects()) {
            if (obj.player_placed) data.placed_objects.push_back(obj);
        }
        result.push_back(std::move(data));
    }

    // Chunks the player has left keep their archived state.
    for (const auto& [key, stored] : archive_) {
        if (chunks_.find(key) != chunks_.end()) continue;
        ChunkSaveData data;
        data.cx = key.x;
        data.cy = key.y;
        data.explored = stored.explored;
        data.modifications = stored.modifications;
        data.placed_objects = stored.placed_objects;
        result.push_back(std::move(data));
    }

    return result;
}

void World::apply_save_data(const ChunkSaveData& data) {
    ChunkCoord key{data.cx, data.cy};

    ChunkArchive& stored = archive_[key];
    stored.explored = data.explored;
    stored.modifications = data.modifications;
    stored.placed_objects = data.placed_objects;

    // If the chunk is already loaded, replay the terrain now; otherwise the
    // archive entry is picked up the next time it is generated. Entities are
    // staged separately by apply_entity_save_data.
    auto it = chunks_.find(key);
    if (it == chunks_.end()) return;
    restore_chunk_terrain(key, it->second);
}

// ── Entity management ──────────────────────────────────────────────

static ChunkCoord entity_chunk(int wx, int wy) {
    return {World::world_to_chunk_x(wx), World::world_to_chunk_y(wy)};
}

void World::spawn_item(Entity&& e) {
    ChunkCoord key = entity_chunk(e.x(), e.y());
    entities_[key].items.push_back(std::move(e));
}

void World::spawn_npc(Npc&& n) {
    ChunkCoord key = entity_chunk(n.x(), n.y());
    entities_[key].npcs.push_back(std::move(n));
}

void World::spawn_enemy(Enemy&& e) {
    ChunkCoord key = entity_chunk(e.x(), e.y());
    entities_[key].enemies.push_back(std::move(e));
}

Entity World::remove_item_at(int wx, int wy) {
    ChunkCoord key = entity_chunk(wx, wy);
    auto it = entities_.find(key);
    if (it == entities_.end()) return Entity();
    auto& items = it->second.items;
    for (auto eit = items.begin(); eit != items.end(); ++eit) {
        if (eit->x() == wx && eit->y() == wy) {
            Entity result = std::move(*eit);
            items.erase(eit);
            return result;
        }
    }
    return Entity();
}

bool World::remove_enemy_at(int wx, int wy) {
    ChunkCoord key = entity_chunk(wx, wy);
    auto it = entities_.find(key);
    if (it == entities_.end()) return false;
    auto& enemies = it->second.enemies;
    for (auto eit = enemies.begin(); eit != enemies.end(); ++eit) {
        if (eit->x() == wx && eit->y() == wy) {
            enemies.erase(eit);
            return true;
        }
    }
    return false;
}

Entity* World::item_at(int wx, int wy) {
    ChunkCoord key = entity_chunk(wx, wy);
    auto it = entities_.find(key);
    if (it == entities_.end()) return nullptr;
    for (auto& e : it->second.items) {
        if (e.x() == wx && e.y() == wy) return &e;
    }
    return nullptr;
}

Npc* World::npc_at(int wx, int wy) {
    ChunkCoord key = entity_chunk(wx, wy);
    auto it = entities_.find(key);
    if (it == entities_.end()) return nullptr;
    for (auto& n : it->second.npcs) {
        if (n.x() == wx && n.y() == wy) return &n;
    }
    return nullptr;
}

Enemy* World::enemy_at(int wx, int wy) {
    ChunkCoord key = entity_chunk(wx, wy);
    auto it = entities_.find(key);
    if (it == entities_.end()) return nullptr;
    for (auto& e : it->second.enemies) {
        if (e.x() == wx && e.y() == wy && e.is_alive()) return &e;
    }
    return nullptr;
}

bool World::has_enemy_at(int wx, int wy) const {
    ChunkCoord key = entity_chunk(wx, wy);
    auto it = entities_.find(key);
    if (it == entities_.end()) return false;
    for (const auto& e : it->second.enemies) {
        if (e.x() == wx && e.y() == wy && e.is_alive()) return true;
    }
    return false;
}

bool World::has_npc_at(int wx, int wy) const {
    ChunkCoord key = entity_chunk(wx, wy);
    auto it = entities_.find(key);
    if (it == entities_.end()) return false;
    for (const auto& n : it->second.npcs) {
        if (n.x() == wx && n.y() == wy) return true;
    }
    return false;
}

World::NearbyEntities World::get_nearby_entities(int center_wx, int center_wy, int radius_chunks) const {
    NearbyEntities result;
    int player_cx = world_to_chunk_x(center_wx);
    int player_cy = world_to_chunk_y(center_wy);
    for (int dy = -radius_chunks; dy <= radius_chunks; ++dy) {
        for (int dx = -radius_chunks; dx <= radius_chunks; ++dx) {
            ChunkCoord key{player_cx + dx, player_cy + dy};
            auto it = entities_.find(key);
            if (it == entities_.end()) continue;
            const auto& ce = it->second;
            for (const auto& e : ce.items) result.items.push_back(const_cast<Entity*>(&e));
            for (const auto& n : ce.npcs) result.npcs.push_back(const_cast<Npc*>(&n));
            for (const auto& e : ce.enemies) result.enemies.push_back(const_cast<Enemy*>(&e));
        }
    }
    return result;
}

std::vector<ChunkCoord> World::entity_chunk_keys() const {
    std::vector<ChunkCoord> keys;
    keys.reserve(entities_.size());
    for (const auto& [key, ce] : entities_) {
        (void)ce;
        keys.push_back(key);
    }
    return keys;
}

size_t World::enemy_count_in_chunk(ChunkCoord key) const {
    auto it = entities_.find(key);
    return it == entities_.end() ? 0 : it->second.enemies.size();
}

Enemy* World::enemy_in_chunk(ChunkCoord key, size_t index) {
    auto it = entities_.find(key);
    if (it == entities_.end() || index >= it->second.enemies.size()) return nullptr;
    return &it->second.enemies[index];
}

void World::erase_enemy_in_chunk(ChunkCoord key, size_t index) {
    auto it = entities_.find(key);
    if (it == entities_.end() || index >= it->second.enemies.size()) return;
    auto& enemies = it->second.enemies;
    enemies.erase(enemies.begin() + static_cast<long>(index));
}

size_t World::npc_count_in_chunk(ChunkCoord key) const {
    auto it = entities_.find(key);
    return it == entities_.end() ? 0 : it->second.npcs.size();
}

Npc* World::npc_in_chunk(ChunkCoord key, size_t index) {
    auto it = entities_.find(key);
    if (it == entities_.end() || index >= it->second.npcs.size()) return nullptr;
    return &it->second.npcs[index];
}

void World::clear_all_entities() {
    entities_.clear();
}

void World::rekey_entities() {
    // Collect entities that are in the wrong chunk
    std::vector<std::pair<ChunkCoord, size_t>> miskeyed_enemies; // {old_key, index}
    std::vector<std::pair<ChunkCoord, size_t>> miskeyed_npcs;
    std::vector<std::pair<ChunkCoord, size_t>> miskeyed_items;

    for (auto& [key, ce] : entities_) {
        for (size_t i = 0; i < ce.enemies.size(); ++i) {
            ChunkCoord actual = entity_chunk(ce.enemies[i].x(), ce.enemies[i].y());
            if (actual != key) miskeyed_enemies.push_back({key, i});
        }
        for (size_t i = 0; i < ce.npcs.size(); ++i) {
            ChunkCoord actual = entity_chunk(ce.npcs[i].x(), ce.npcs[i].y());
            if (actual != key) miskeyed_npcs.push_back({key, i});
        }
        for (size_t i = 0; i < ce.items.size(); ++i) {
            ChunkCoord actual = entity_chunk(ce.items[i].x(), ce.items[i].y());
            if (actual != key) miskeyed_items.push_back({key, i});
        }
    }

    // Move miskeyed enemies (iterate in reverse to preserve indices)
    for (auto it = miskeyed_enemies.rbegin(); it != miskeyed_enemies.rend(); ++it) {
        auto& ce = entities_[it->first];
        Enemy e = std::move(ce.enemies[it->second]);
        ce.enemies.erase(ce.enemies.begin() + it->second);
        ChunkCoord new_key = entity_chunk(e.x(), e.y());
        entities_[new_key].enemies.push_back(std::move(e));
    }
    for (auto it = miskeyed_npcs.rbegin(); it != miskeyed_npcs.rend(); ++it) {
        auto& ce = entities_[it->first];
        Npc n = std::move(ce.npcs[it->second]);
        ce.npcs.erase(ce.npcs.begin() + it->second);
        ChunkCoord new_key = entity_chunk(n.x(), n.y());
        entities_[new_key].npcs.push_back(std::move(n));
    }
    for (auto it = miskeyed_items.rbegin(); it != miskeyed_items.rend(); ++it) {
        auto& ce = entities_[it->first];
        Entity e = std::move(ce.items[it->second]);
        ce.items.erase(ce.items.begin() + it->second);
        ChunkCoord new_key = entity_chunk(e.x(), e.y());
        entities_[new_key].items.push_back(std::move(e));
    }
}

std::vector<World::ChunkEntitySaveData> World::gather_entity_save_data() const {
    std::vector<ChunkEntitySaveData> result;
    result.reserve(entities_.size() + archive_.size());

    auto append = [&result](ChunkCoord key, const ChunkEntities& ce) {
        if (ce.items.empty() && ce.npcs.empty() && ce.enemies.empty()) return;
        ChunkEntitySaveData data;
        data.cx = key.x;
        data.cy = key.y;
        data.items = ce.items;
        data.npcs = ce.npcs;
        data.enemies = ce.enemies;
        result.push_back(std::move(data));
    };

    for (const auto& [key, ce] : entities_) append(key, ce);

    // Entities in chunks the player has walked away from.
    for (const auto& [key, stored] : archive_) {
        if (entities_.find(key) != entities_.end()) continue;
        append(key, stored.entities);
    }

    return result;
}

void World::apply_entity_save_data(const ChunkEntitySaveData& data) {
    ChunkCoord key{data.cx, data.cy};

    ChunkEntities restored;
    restored.items = data.items;
    restored.npcs = data.npcs;
    restored.enemies = data.enemies;

    archive_[key].entities = restored;
    if (chunks_.find(key) != chunks_.end()) {
        entities_[key] = std::move(restored);
    }
}

// ── Biome/structure queries ───────────────────────────────────────

BiomeType World::get_biome_at(int world_x, int world_y) const {
    return determine_biome(world_x, world_y, seed_);
}

StructureType World::get_structure_at(int world_x, int world_y) const {
    int cx, cy, lx, ly;
    world_to_chunk(world_x, world_y, cx, cy, lx, ly);
    const Chunk* chunk = get_chunk(cx, cy);
    if (!chunk) return StructureType::None;
    return static_cast<StructureType>(chunk->structure_id());
}

World::ChunkMapInfo World::chunk_map_info(int cx, int cy) const {
    const Chunk* chunk = get_chunk(cx, cy);
    if (chunk) {
        return {static_cast<BiomeType>(chunk->biome_id()),
                static_cast<StructureType>(chunk->structure_id())};
    }

    // Ungenerated chunk: predict deterministically (same rules as generation)
    int center_wx = cx * CHUNK_SIZE + CHUNK_SIZE / 2;
    int center_wy = cy * CHUNK_SIZE + CHUNK_SIZE / 2;
    BiomeType biome = determine_biome(center_wx, center_wy, seed_);
    StructureType st = decide_structure(cx, cy, biome, seed_);
    // Origin chunk always holds a village
    if (st == StructureType::None && cx == 0 && cy == 0)
        st = StructureType::Village;
    return {biome, st};
}

// ── Zones ─────────────────────────────────────────────────────────

void World::add_zone(const Zone& z) {
    zones_.push_back(z);
}

bool World::remove_zone_at(int wx, int wy) {
    for (auto it = zones_.begin(); it != zones_.end(); ++it) {
        if (it->contains(wx, wy)) {
            zones_.erase(it);
            return true;
        }
    }
    return false;
}

const Zone* World::zone_at(int wx, int wy) const {
    for (const auto& z : zones_) {
        if (z.contains(wx, wy)) return &z;
    }
    return nullptr;
}

// ── Entity spawning for new chunks ────────────────────────────────

void World::spawn_biome_entities(Chunk& chunk, int cx, int cy) {
    BiomeType biome = static_cast<BiomeType>(chunk.biome_id());
    int base_wx = cx * CHUNK_SIZE;
    int base_wy = cy * CHUNK_SIZE;

    // Per-biome spawn tables (weighted by repetition)
    static const char* grassland_spawns[] = {"Rat", "Rat", "Snake", "Bandit"};
    static const char* forest_spawns[]    = {"Spider", "Spider", "Wolf", "Goblin"};
    static const char* desert_spawns[]    = {"Scorpion", "Scorpion", "Snake", "Bandit"};
    static const char* swamp_spawns[]     = {"Frog", "Frog", "Snake", "Zombie"};
    static const char* mountain_spawns[]  = {"Goblin", "Goblin", "Bat", "Troll"};
    static const char* tundra_spawns[]    = {"Wolf", "Wolf", "Bat", "Yeti"};

    const char** table = grassland_spawns;
    int table_size = 4;
    switch (biome) {
        case BiomeType::Grassland: table = grassland_spawns; break;
        case BiomeType::Forest:    table = forest_spawns;    break;
        case BiomeType::Desert:    table = desert_spawns;    break;
        case BiomeType::Swamp:     table = swamp_spawns;     break;
        case BiomeType::Mountain:  table = mountain_spawns;  break;
        case BiomeType::Tundra:    table = tundra_spawns;    break;
        default: break;
    }

    // Spawn 2-4 enemies per chunk based on biome
    int count = 2 + static_cast<int>(noise::normalize(
        noise::fractal2d(static_cast<float>(cx) * 3.0f, static_cast<float>(cy) * 3.0f, 1, 0.5f)) * 3);

    for (int i = 0; i < count; ++i) {
        int ex = 0, ey = 0;
        bool found = false;
        for (int attempt = 0; attempt < 20 && !found; ++attempt) {
            // Offset the sample per entity and per attempt, otherwise every
            // retry evaluates the same coordinate.
            float jitter = static_cast<float>(i) * 100.0f + static_cast<float>(attempt) * 13.0f;
            float rx = noise::fractal2d(static_cast<float>(cx) * 5.0f + 9000.0f + jitter,
                                        static_cast<float>(cy) * 5.0f + 9000.0f + jitter, 1, 0.5f);
            float ry = noise::fractal2d(static_cast<float>(cx) * 5.0f + 9500.0f + jitter,
                                        static_cast<float>(cy) * 5.0f + 9500.0f + jitter, 1, 0.5f);
            ex = base_wx + static_cast<int>(noise::normalize(rx) * (CHUNK_SIZE - 4)) + 2;
            ey = base_wy + static_cast<int>(noise::normalize(ry) * (CHUNK_SIZE - 4)) + 2;
            found = is_passable(ex, ey) && !has_enemy_at(ex, ey);
        }
        if (!found) continue;

        // Deterministic pick from the biome's spawn table
        float pick = noise::fractal2d(static_cast<float>(cx) * 6.0f + i * 77.0f + 10000.0f,
                                      static_cast<float>(cy) * 6.0f + i * 77.0f + 10000.0f, 1, 0.5f);
        int idx = static_cast<int>(noise::normalize(pick) * table_size);
        if (idx >= table_size) idx = table_size - 1;

        spawn_enemy(make_enemy(table[idx], ex, ey));
    }
}

void World::spawn_structure_entities(int cx, int cy, StructureType stype) {
    if (stype == StructureType::None) return;

    int base_wx = cx * CHUNK_SIZE;
    int base_wy = cy * CHUNK_SIZE;

    const StructureDef& def = structure_def(stype);

    for (size_t i = 0; i < def.entities.size(); ++i) {
        const StructureEntity& se = def.entities[i];

        int ex = 0, ey = 0;
        bool found = false;
        for (int attempt = 0; attempt < 30 && !found; ++attempt) {
            // Offset the sample per entity and per attempt, otherwise every
            // retry evaluates the same coordinate.
            float jitter = static_cast<float>(i) * 31.0f + static_cast<float>(attempt) * 7.0f;
            float rx = noise::fractal2d(static_cast<float>(cx) * 7.0f + 11000.0f + jitter,
                                        static_cast<float>(cy) * 7.0f + 11000.0f + jitter, 1, 0.5f);
            float ry = noise::fractal2d(static_cast<float>(cx) * 7.0f + 12000.0f + jitter,
                                        static_cast<float>(cy) * 7.0f + 12000.0f + jitter, 1, 0.5f);
            ex = base_wx + static_cast<int>(noise::normalize(rx) * (CHUNK_SIZE - 4)) + 2;
            ey = base_wy + static_cast<int>(noise::normalize(ry) * (CHUNK_SIZE - 4)) + 2;
            found = is_passable(ex, ey) && !has_enemy_at(ex, ey) && !has_npc_at(ex, ey);
        }
        if (!found) continue;

        switch (se.kind) {
            case StructureEntityKind::Npc:
                spawn_npc(make_npc(se.name, ex, ey));
                break;
            case StructureEntityKind::Enemy:
                spawn_enemy(make_enemy(se.name, ex, ey));
                break;
        }
    }
}
