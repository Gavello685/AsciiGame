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

    // Generate/load chunks within load radius
    for (int dy = -load_radius_; dy <= load_radius_; ++dy) {
        for (int dx = -load_radius_; dx <= load_radius_; ++dx) {
            int cx = player_cx + dx;
            int cy = player_cy + dy;
            ChunkCoord key{cx, cy};
            if (chunks_.find(key) == chunks_.end()) {
                chunks_.emplace(key, Chunk(cx, cy, seed_));
                Chunk& chunk = chunks_[key];
                chunk.mark_visited();
                // Try to place a structure in this chunk
                if (try_place_structure(chunk, cx, cy, seed_)) {
                    // Determine structure type from biome for entity spawning
                    BiomeType biome = static_cast<BiomeType>(chunk.biome_id());
                    float type_noise = noise::fractal2d(
                        static_cast<float>(cx) * 1.3f + 6000.0f,
                        static_cast<float>(cy) * 1.3f + 6000.0f,
                        1, 0.5f);
                    float type_val = noise::normalize(type_noise);
                    StructureType stype;
                    switch (biome) {
                        case BiomeType::Grassland:
                            stype = type_val < 0.6f ? StructureType::Village : StructureType::Ruins;
                            break;
                        case BiomeType::Forest:
                            stype = type_val < 0.5f ? StructureType::Ruins : StructureType::Cave;
                            break;
                        case BiomeType::Desert: stype = StructureType::Ruins; break;
                        case BiomeType::Swamp:  stype = StructureType::Ruins; break;
                        case BiomeType::Mountain:
                            stype = type_val < 0.6f ? StructureType::Cave : StructureType::Dungeon;
                            break;
                        case BiomeType::Tundra: stype = StructureType::Ruins; break;
                        default: stype = StructureType::None; break;
                    }
                    chunk.set_structure_id(static_cast<int>(stype));
                    spawn_structure_entities(chunk, cx, cy, stype);
                }
                // Spawn biome enemies
                spawn_biome_entities(chunk, cx, cy);
            }
        }
    }

    // Unload chunks beyond load radius + 1 buffer
    int unload_radius = load_radius_ + 1;
    std::vector<ChunkCoord> to_remove;
    for (const auto& [key, chunk] : chunks_) {
        int dx = key.x - player_cx;
        int dy = key.y - player_cy;
        if (std::abs(dx) > unload_radius || std::abs(dy) > unload_radius) {
            to_remove.push_back(key);
        }
    }
    for (const auto& key : to_remove) {
        clear_entities_in_chunk(key.x, key.y);
        chunks_.erase(key);
    }
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

void World::set_tile(int world_x, int world_y, TileType type) {
    int cx, cy, lx, ly;
    world_to_chunk(world_x, world_y, cx, cy, lx, ly);
    Chunk* chunk = get_chunk(cx, cy);
    if (!chunk) return;
    chunk->set(lx, ly, type);
    chunk->mark_dirty();
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

void World::set_load_radius(int r) {
    load_radius_ = std::max(1, std::min(4, r));
    // Ensure render and sim fit within load
    if (render_radius_ >= load_radius_) render_radius_ = load_radius_ - 1;
    if (sim_radius_ >= render_radius_) sim_radius_ = render_radius_ - 1;
}

void World::set_render_radius(int r) {
    render_radius_ = std::max(1, std::min(4, r));
    // Ensure load has room, and sim fits
    if (load_radius_ <= render_radius_) load_radius_ = render_radius_ + 1;
    if (sim_radius_ >= render_radius_) sim_radius_ = render_radius_ - 1;
}

void World::set_simulation_radius(int r) {
    sim_radius_ = std::max(1, std::min(3, r));
    // Ensure render is at least sim + 1
    if (render_radius_ <= sim_radius_) render_radius_ = sim_radius_ + 1;
    if (load_radius_ <= render_radius_) load_radius_ = render_radius_ + 1;
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
    set_tile(world_x, world_y, type);
}

void World::add_placed_object(int world_x, int world_y, const PlacedObject& obj) {
    int cx, cy, lx, ly;
    world_to_chunk(world_x, world_y, cx, cy, lx, ly);
    Chunk* chunk = get_chunk(cx, cy);
    if (!chunk) return;
    PlacedObject local_obj = obj;
    local_obj.local_x = lx;
    local_obj.local_y = ly;
    chunk->add_placed_object(local_obj);
}

std::vector<World::ChunkSaveData> World::gather_save_data() const {
    std::vector<ChunkSaveData> result;
    for (const auto& [key, chunk] : chunks_) {
        if (!chunk.is_visited()) continue;
        ChunkSaveData data;
        data.cx = key.x;
        data.cy = key.y;
        data.explored = chunk.explored_bits();
        data.modifications = chunk.modifications();
        data.placed_objects = chunk.placed_objects();
        result.push_back(std::move(data));
    }
    return result;
}

void World::apply_save_data(const ChunkSaveData& data) {
    ChunkCoord key{data.cx, data.cy};
    auto it = chunks_.find(key);
    if (it == chunks_.end()) {
        // Create chunk first, then apply
        chunks_.emplace(key, Chunk(data.cx, data.cy, seed_));
        it = chunks_.find(key);
    }
    Chunk& chunk = it->second;
    chunk.mark_visited();
    chunk.apply_explored(data.explored);
    if (!data.modifications.empty()) {
        chunk.apply_modifications(data.modifications);
    }
    if (!data.placed_objects.empty()) {
        chunk.apply_placed_objects(data.placed_objects);
    }
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

std::vector<Enemy*> World::get_all_enemies() {
    std::vector<Enemy*> result;
    for (auto& [key, ce] : entities_) {
        for (auto& e : ce.enemies) result.push_back(&e);
    }
    return result;
}

std::vector<Npc*> World::get_all_npcs() {
    std::vector<Npc*> result;
    for (auto& [key, ce] : entities_) {
        for (auto& n : ce.npcs) result.push_back(&n);
    }
    return result;
}

Npc* World::get_npc_mut(int wx, int wy) {
    return npc_at(wx, wy);
}

void World::clear_all_entities() {
    entities_.clear();
}

void World::clear_entities_in_chunk(int cx, int cy) {
    ChunkCoord key{cx, cy};
    entities_.erase(key);
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
    for (const auto& [key, ce] : entities_) {
        if (ce.items.empty() && ce.npcs.empty() && ce.enemies.empty()) continue;
        ChunkEntitySaveData data;
        data.cx = key.x;
        data.cy = key.y;
        data.items = ce.items;
        data.npcs = ce.npcs;
        data.enemies = ce.enemies;
        result.push_back(std::move(data));
    }
    return result;
}

void World::apply_entity_save_data(const std::vector<ChunkEntitySaveData>& data) {
    for (const auto& d : data) {
        ChunkCoord key{d.cx, d.cy};
        ChunkEntities& ce = entities_[key];
        ce.items = d.items;
        ce.npcs = d.npcs;
        ce.enemies = d.enemies;
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

// ── Entity spawning for new chunks ────────────────────────────────

void World::spawn_biome_entities(Chunk& chunk, int cx, int cy) {
    BiomeType biome = static_cast<BiomeType>(chunk.biome_id());
    int base_wx = cx * CHUNK_SIZE;
    int base_wy = cy * CHUNK_SIZE;

    // Spawn 2-4 enemies per chunk based on biome
    int count = 2 + static_cast<int>(noise::normalize(
        noise::fractal2d(static_cast<float>(cx) * 3.0f, static_cast<float>(cy) * 3.0f, 1, 0.5f)) * 3);

    for (int i = 0; i < count; ++i) {
        // Find passable position
        int ex, ey;
        int attempts = 0;
        do {
            float rx = noise::fractal2d(static_cast<float>(cx) * 5.0f + i * 100.0f + 9000.0f,
                                         static_cast<float>(cy) * 5.0f + i * 100.0f + 9000.0f, 1, 0.5f);
            float ry = noise::fractal2d(static_cast<float>(cx) * 5.0f + i * 100.0f + 9500.0f,
                                         static_cast<float>(cy) * 5.0f + i * 100.0f + 9500.0f, 1, 0.5f);
            ex = base_wx + static_cast<int>(noise::normalize(rx) * (CHUNK_SIZE - 4)) + 2;
            ey = base_wy + static_cast<int>(noise::normalize(ry) * (CHUNK_SIZE - 4)) + 2;
            attempts++;
        } while ((!is_passable(ex, ey) || has_enemy_at(ex, ey)) && attempts < 20);

        if (attempts >= 20) continue;

        // Pick enemy type based on biome
        Enemy* e = nullptr;
        switch (biome) {
            case BiomeType::Grassland:
                e = new Enemy(ex, ey, 'r', "Rat", 160, 120, 80, 5, 5, 2, 0, 3, 0, {});
                break;
            case BiomeType::Forest:
                e = new Enemy(ex, ey, 's', "Spider", 120, 120, 120, 8, 8, 3, 0, 5, 1, {});
                break;
            case BiomeType::Desert:
                e = new Enemy(ex, ey, 's', "Scorpion", 180, 140, 60, 6, 6, 3, 0, 4, 1, {});
                break;
            case BiomeType::Swamp:
                e = new Enemy(ex, ey, 'f', "Frog", 60, 160, 60, 4, 4, 1, 0, 2, 0, {});
                break;
            case BiomeType::Mountain:
                e = new Enemy(ex, ey, 'g', "Goblin", 80, 180, 80, 12, 12, 4, 1, 8, 1, {});
                break;
            case BiomeType::Tundra:
                e = new Enemy(ex, ey, 'w', "Wolf", 160, 160, 160, 10, 10, 4, 1, 7, 1, {});
                break;
            default:
                e = new Enemy(ex, ey, 'r', "Rat", 160, 120, 80, 5, 5, 2, 0, 3, 0, {});
                break;
        }
        if (e) {
            spawn_enemy(std::move(*e));
            delete e;
        }
    }
}

void World::spawn_structure_entities(Chunk& chunk, int cx, int cy, StructureType stype) {
    int base_wx = cx * CHUNK_SIZE;
    int base_wy = cy * CHUNK_SIZE;

    const StructureDef& def = structure_def(stype);

    for (const auto& se : def.entities) {
        int ex, ey;
        int attempts = 0;
        do {
            float rx = noise::fractal2d(static_cast<float>(cx) * 7.0f + 11000.0f + attempts,
                                         static_cast<float>(cy) * 7.0f + 11000.0f + attempts, 1, 0.5f);
            float ry = noise::fractal2d(static_cast<float>(cx) * 7.0f + 12000.0f + attempts,
                                         static_cast<float>(cy) * 7.0f + 12000.0f + attempts, 1, 0.5f);
            ex = base_wx + static_cast<int>(noise::normalize(rx) * (CHUNK_SIZE - 4)) + 2;
            ey = base_wy + static_cast<int>(noise::normalize(ry) * (CHUNK_SIZE - 4)) + 2;
            attempts++;
        } while ((!is_passable(ex, ey) || has_enemy_at(ex, ey) || has_npc_at(ex, ey)) && attempts < 30);

        if (attempts >= 30) continue;

        if (se.type == "npc") {
            if (se.name == "Merchant") {
                std::vector<std::pair<Item, int>> shop;
                auto add = [&](const char* n, int qty) {
                    const Item* it = find_item(n);
                    if (it) shop.emplace_back(*it, qty);
                };
                add("Bread", 10); add("Health Potion", 5); add("Iron Sword", 3);
                add("Leather Armor", 2); add("Hood", 3);
                spawn_npc(Npc(ex, ey, 'm', "Merchant", 220, 180, 60, true, 60,
                              build_merchant_dialogue(), shop));
            } else if (se.name == "Villager") {
                spawn_npc(Npc(ex, ey, 'v', "Villager", 140, 200, 140, false, 30,
                              build_villager_dialogue(), {}));
            }
        } else if (se.type == "enemy") {
            Enemy* e = nullptr;
            if (se.name == "Goblin") {
                e = new Enemy(ex, ey, 'g', "Goblin", 80, 180, 80, 12, 12, 4, 1, 8, 1, {});
            } else if (se.name == "Rat") {
                e = new Enemy(ex, ey, 'r', "Rat", 160, 120, 80, 5, 5, 2, 0, 3, 0, {});
            } else if (se.name == "Ogre") {
                e = new Enemy(ex, ey, 'o', "Ogre", 200, 100, 60, 25, 25, 7, 3, 20, 2, {});
            } else if (se.name == "Spider" || se.name == "Giant Spider") {
                e = new Enemy(ex, ey, 's', "Spider", 120, 120, 120, 8, 8, 3, 0, 5, 1, {});
            }
            if (e) {
                spawn_enemy(std::move(*e));
                delete e;
            }
        }
    }
}
