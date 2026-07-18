#include "engine/window.h"
#include "engine/renderer.h"
#include "game/world.h"
#include "game/chunk.h"
#include "game/player.h"
#include "game/entity.h"
#include "game/npc.h"
#include "game/enemy.h"
#include "game/item.h"
#include "game/fov.h"
#include "game/trade.h"
#include "game/save.h"
#include "game/time_system.h"
#include "game/light.h"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>

static const int FONT_SIZE = 20;
static const int FOV_RADIUS = 8;

static const char* slot_names[] = {
    "None", "Head", "L Shoulder", "R Shoulder", "Torso",
    "L Arm", "R Arm", "L Hand", "R Hand",
    "L Leg", "R Leg", "L Foot", "R Foot"
};

enum class GameMode {
    Normal,
    Inventory,
    InventoryAction,
    InventoryExamine,
    GiftSelect,
    Dialogue,
    Dead,
    PauseMenu,
    SaveGame,
    LoadGame,
};

static void draw_string(Renderer& r, int x, int y, const std::string& s,
                        uint8_t fr, uint8_t fg, uint8_t fb,
                        uint8_t br = 0, uint8_t bg = 0, uint8_t bb = 0) {
    Cell c;
    c.fg_r = fr; c.fg_g = fg; c.fg_b = fb;
    c.bg_r = br; c.bg_g = bg; c.bg_b = bb;
    for (int i = 0; i < static_cast<int>(s.size()); ++i) {
        c.glyph = s[i];
        r.set_cell(x + i, y, c);
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Window window;
    if (!window.init({"ASCII Game", 1024, 768})) return 1;

    Renderer renderer;
    std::string font_path = "C:/Windows/Fonts/cour.ttf";
    if (!renderer.init(window.get(), font_path, FONT_SIZE)) {
        std::cerr << "Failed to initialize renderer.\n";
        return 1;
    }

    // --- Game state ---
    uint32_t map_seed = static_cast<uint32_t>(std::time(nullptr));
    World world;
    world.init(map_seed);

    Player player;
    // Load chunks around origin first so spawn search has terrain to check
    world.update_loaded_chunks(0, 0);
    // Guarantee a village exists at origin
    {
        Chunk* origin_chunk = world.get_chunk(0, 0);
        if (origin_chunk && origin_chunk->structure_id() == 0) {
            force_place_structure(*origin_chunk, 0, 0, StructureType::Village);
            // Spawn village entities
            world.spawn_structure_entities(*origin_chunk, 0, 0, StructureType::Village);
        }
    }
    // Find spawn point: prefer a village, fall back to any passable tile
    {
        bool found = false;

        // Pass 1: search for a village structure within load radius
        for (int r = 0; r <= world.load_radius() && !found; ++r) {
            for (int dy = -r; dy <= r && !found; ++dy) {
                for (int dx = -r; dx <= r && !found; ++dx) {
                    if (std::abs(dx) != r && std::abs(dy) != r) continue;
                    int cx = World::world_to_chunk_x(dx);
                    int cy = World::world_to_chunk_y(dy);
                    Chunk* chunk = world.get_chunk(cx, cy);
                    if (chunk && chunk->structure_id() == static_cast<int>(StructureType::Village)) {
                        // Find a passable floor tile inside this chunk
                        int base_wx = cx * CHUNK_SIZE;
                        int base_wy = cy * CHUNK_SIZE;
                        for (int ly = 0; ly < CHUNK_SIZE && !found; ++ly) {
                            for (int lx = 0; lx < CHUNK_SIZE && !found; ++lx) {
                                int wx = base_wx + lx;
                                int wy = base_wy + ly;
                                Tile t = world.get_tile(wx, wy);
                                if (t.type == TileType::Floor && world.is_passable(wx, wy)) {
                                    player.spawn(wx, wy);
                                    found = true;
                                }
                            }
                        }
                    }
                }
            }
        }

        // Pass 2: any passable tile near origin
        if (!found) {
            for (int r = 0; r < 100 && !found; ++r) {
                for (int dy = -r; dy <= r && !found; ++dy) {
                    for (int dx = -r; dx <= r && !found; ++dx) {
                        if (std::abs(dx) != r && std::abs(dy) != r) continue;
                        if (world.is_passable(dx, dy)) {
                            player.spawn(dx, dy);
                            found = true;
                        }
                    }
                }
            }
        }

        // Guaranteed fallback
        if (!found) player.spawn(0, 0);
    }
    world.update_loaded_chunks(player.x(), player.y());

    // Starting items
    {
        const Item* bread = find_item("Bread");
        const Item* potion = find_item("Health Potion");
        const Item* sword = find_item("Iron Sword");
        const Item* armor = find_item("Leather Armor");
        if (bread) player.add_item(*bread, 3);
        if (potion) player.add_item(*potion, 1);
        if (sword) player.add_item(*sword, 1);
        if (armor) player.add_item(*armor, 1);
    }

    // World items — spawn near player in loaded chunks
    {
        const char* names[] = {"Gold Coin", "Health Potion", "Old Scroll", "Bread", "Rusty Key"};
        const uint32_t glyphs[] = {'$', '!', '?', '%', '!'};
        const uint8_t colors[][3] = {
            {255, 215, 0}, {255, 50, 50}, {200, 180, 140}, {180, 140, 60}, {150, 150, 150}};
        int num = 10 + std::rand() % 6;
        for (int i = 0; i < num; ++i) {
            int ix, iy;
            int attempts = 0;
            do {
                ix = player.x() - 30 + std::rand() % 60;
                iy = player.y() - 30 + std::rand() % 60;
                attempts++;
            } while (!world.is_passable(ix, iy) && attempts < 200);
            if (attempts >= 200) continue;
            int t = std::rand() % 5;
            Entity e(ix, iy, glyphs[t], names[t], colors[t][0], colors[t][1], colors[t][2]);
            e.set_is_item(true);
            world.spawn_item(std::move(e));
        }
    }

    // NPCs — spawn near player in loaded chunks
    {
        std::vector<std::pair<Item, int>> merchant_stock;
        {
            auto add = [&](const char* name, int qty) {
                const Item* it = find_item(name);
                if (it) merchant_stock.emplace_back(*it, qty);
            };
            add("Bread", 10); add("Health Potion", 5); add("Water Skin", 5);
            add("Iron Sword", 3); add("Iron Shield", 2); add("Leather Armor", 2);
            add("Wooden Shield", 4); add("Hood", 3); add("Leather Boots", 3); add("Gold Ring", 1);
        }

        struct NpcDef {
            uint32_t glyph; const char* name; uint8_t r, g, b;
            bool merchant; int affinity;
            std::vector<DialogueNode> (*dialogue_fn)();
            std::vector<std::pair<Item, int>> shop;
        };
        NpcDef defs[] = {
            {'a', "Old Sage", 180, 160, 220, false, 40, build_sage_dialogue, {}},
            {'v', "Villager", 140, 200, 140, false, 30, build_villager_dialogue, {}},
            {'m', "Merchant", 220, 180, 60, true, 60, build_merchant_dialogue, merchant_stock},
            {'a', "Wanderer", 100, 160, 200, false, 20, build_wanderer_dialogue, {}},
            {'v', "Child", 200, 200, 120, false, 50, build_child_dialogue, {}},
        };
        for (const auto& def : defs) {
            int nx, ny;
            int attempts = 0;
            do {
                nx = player.x() - 20 + std::rand() % 40;
                ny = player.y() - 20 + std::rand() % 40;
                attempts++;
            } while (!world.is_passable(nx, ny) && attempts < 200);
            if (attempts >= 200) continue;
            world.spawn_npc(Npc(nx, ny, def.glyph, def.name, def.r, def.g, def.b,
                                def.merchant, def.affinity, def.dialogue_fn(), def.shop));
        }
    }

    // Enemies — spawn near player in loaded chunks
    {
        struct EnemyDef {
            uint32_t glyph; const char* name; uint8_t r, g, b;
            int hp, atk, def, xp, variance;
            std::vector<std::pair<Item, int>> drops;
        };
        auto make_drops = [](const char* name, int qty) -> std::vector<std::pair<Item, int>> {
            const Item* it = find_item(name);
            if (it) return {{*it, qty}};
            return {};
        };

        EnemyDef defs[] = {
            {'r', "Rat", 160, 120, 80, 5, 2, 0, 3, 0, make_drops("Bread", 1)},
            {'r', "Rat", 160, 120, 80, 5, 2, 0, 3, 0, {}},
            {'g', "Goblin", 80, 180, 80, 12, 4, 1, 8, 1, make_drops("Gold Coin", 1)},
            {'g', "Goblin", 80, 180, 80, 12, 4, 1, 8, 1, make_drops("Rusty Key", 1)},
            {'s', "Spider", 120, 120, 120, 8, 3, 0, 5, 1, make_drops("Health Potion", 1)},
            {'s', "Spider", 120, 120, 120, 8, 3, 0, 5, 1, {}},
            {'b', "Bat", 100, 100, 160, 4, 1, 0, 2, 0, {}},
            {'b', "Bat", 100, 100, 160, 4, 1, 0, 2, 0, {}},
        };

        for (const auto& def : defs) {
            int ex, ey;
            int attempts = 0;
            do {
                ex = player.x() - 25 + std::rand() % 50;
                ey = player.y() - 25 + std::rand() % 50;
                attempts++;
            } while (!world.is_passable(ex, ey) && attempts < 200);
            if (attempts >= 200) continue;
            // Don't spawn too close to player
            if (std::abs(ex - player.x()) + std::abs(ey - player.y()) < 15) continue;
            world.spawn_enemy(Enemy(ex, ey, def.glyph, def.name, def.r, def.g, def.b,
                                   def.hp, def.hp, def.atk, def.def, def.xp, def.variance, def.drops));
        }
    }

    // Game state
    bool running = true;
    SDL_Event event;
    bool need_fov_update = true;
    bool player_took_turn = false;

    GameMode mode = GameMode::Normal;
    int inv_cursor = 0;
    int inv_action_cursor = 0;
    int gift_cursor = 0;
    int dialogue_npc_x = -1, dialogue_npc_y = -1;
    int dialogue_node = 0;
    int dialogue_option = 0;
    TradeState trade_state;
    int equip_slot_cursor = 0;

    static const EquipSlot equip_slots[] = {
        EquipSlot::Head, EquipSlot::Shoulder_L, EquipSlot::Shoulder_R,
        EquipSlot::Torso, EquipSlot::Arm_L, EquipSlot::Arm_R,
        EquipSlot::Hand_L, EquipSlot::Hand_R, EquipSlot::Leg_L,
        EquipSlot::Leg_R, EquipSlot::Foot_L, EquipSlot::Foot_R,
    };
    static const int NUM_EQUIP_SLOTS = 12;

    int examine_item_type = 0;
    Item examine_item;

    std::string status_msg;
    int status_timer = 0;
    int turn_counter = 0;
    int play_time_seconds = 0;
    int play_time_timer = 0;

    // Time and lighting
    TimeSystem time_system;

    // Pause menu state
    int pause_cursor = 0;

            {
                uint8_t ambient = time_system.ambient_light();
                int torch_r = player.has_light_source() ? player.torch_radius() : 0;
                light::compute(world, player.x(), player.y(), ambient, torch_r);

                int perception = player.stats().perception();
                int base_radius = perception * 2;
                if (base_radius < 6) base_radius = 6;
                float light_frac = ambient / 15.0f;
                int init_fov_radius = static_cast<int>(base_radius * light_frac);
                int min_radius = (torch_r > 0) ? torch_r + 2 : 4;
                if (init_fov_radius < min_radius) init_fov_radius = min_radius;
                fov::compute(world, player.x(), player.y(), init_fov_radius);
            }

    while (running) {
        player_took_turn = false;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) { running = false; break; }
            if (event.type != SDL_KEYDOWN) continue;
            SDL_Keycode key = event.key.keysym.sym;

            // --- Dead mode ---
            if (mode == GameMode::Dead) {
                if (key == SDLK_r) {
                    // Restart
                    mode = GameMode::Normal;
                    map_seed = static_cast<uint32_t>(std::time(nullptr));
                    world.init(map_seed);
                    time_system = TimeSystem();
                    player = Player();
                    world.update_loaded_chunks(0, 0);
                    // Find spawn point
                    {
                        bool found = false;
                        for (int r = 0; r < 100 && !found; ++r) {
                            for (int dy = -r; dy <= r && !found; ++dy) {
                                for (int dx = -r; dx <= r && !found; ++dx) {
                                    if (std::abs(dx) != r && std::abs(dy) != r) continue;
                                    if (world.is_passable(dx, dy)) {
                                        player.spawn(dx, dy);
                                        found = true;
                                    }
                                }
                            }
                        }
        if (!found) {
            for (int r = 0; r < 200 && !found; ++r) {
                for (int dy = -r; dy <= r && !found; ++dy) {
                    for (int dx = -r; dx <= r && !found; ++dx) {
                        if (std::abs(dx) != r && std::abs(dy) != r) continue;
                        if (world.is_passable(dx, dy)) {
                            player.spawn(dx, dy);
                            found = true;
                        }
                    }
                }
            }
        }
        if (!found) player.spawn(0, 0);
                    }
                    world.update_loaded_chunks(player.x(), player.y());
                    player.add_item(*find_item("Bread"), 3);
                    player.add_item(*find_item("Health Potion"), 1);
                    player.add_item(*find_item("Iron Sword"), 1);
                    player.add_item(*find_item("Leather Armor"), 1);
                    world.clear_all_entities();
                    turn_counter = 0;
                    need_fov_update = true;

                    // Respawn enemies near player
                    {
                        struct EDef { uint32_t glyph; const char* n; uint8_t r,g,b; int hp,atk,def,xp; };
                        EDef ed[] = {
                            {'r',"Rat",160,120,80,5,2,0,3},
                            {'r',"Rat",160,120,80,5,2,0,3},
                            {'g',"Goblin",80,180,80,12,4,1,8},
                            {'g',"Goblin",80,180,80,12,4,1,8},
                            {'s',"Spider",120,120,120,8,3,0,5},
                            {'b',"Bat",100,100,160,4,1,0,2},
                        };
                        for (const auto& d : ed) {
                            int ex,ey;
                            int attempts = 0;
                            do {
                                ex = player.x() - 25 + std::rand() % 50;
                                ey = player.y() - 25 + std::rand() % 50;
                                attempts++;
                            } while (!world.is_passable(ex,ey) && attempts < 200);
                            if (attempts >= 200) continue;
                            if (std::abs(ex-player.x())+std::abs(ey-player.y())<15) continue;
                            world.spawn_enemy(Enemy(ex,ey,d.glyph,d.n,d.r,d.g,d.b,d.hp,d.hp,d.atk,d.def,d.xp));
                        }
                    }
                }
                if (key == SDLK_ESCAPE) running = false;
                continue;
            }

            // --- Trade mode ---
            if (trade_state.active) {
                trade_handle_input(trade_state, key, player);
                continue;
            }

            // --- Pause menu ---
            if (mode == GameMode::PauseMenu) {
                if (key == SDLK_ESCAPE) { mode = GameMode::Normal; continue; }
                if (key == SDLK_UP && pause_cursor > 0) pause_cursor--;
                if (key == SDLK_DOWN && pause_cursor < 2) pause_cursor++;
                if (key == SDLK_RETURN || key == SDLK_SPACE) {
                    if (pause_cursor == 0) { mode = GameMode::SaveGame; }
                    else if (pause_cursor == 1) { mode = GameMode::LoadGame; }
                    else { running = false; }
                }
                continue;
            }

            // --- Save game screen ---
            if (mode == GameMode::SaveGame) {
                if (key == SDLK_ESCAPE) { mode = GameMode::PauseMenu; continue; }
                auto saves = list_saves();
                int max_slot = MAX_SAVE_SLOTS - 1;
                if (key == SDLK_UP && pause_cursor > 0) pause_cursor--;
                if (key == SDLK_DOWN && pause_cursor < max_slot) pause_cursor++;
                if (key == SDLK_RETURN || key == SDLK_SPACE) {
                    GameState state = capture_state(player, world, map_seed, play_time_seconds, time_system);
                    if (save_game(pause_cursor, state)) {
                        status_msg = "Game saved to slot " + std::to_string(pause_cursor + 1);
                        status_timer = 90;
                    } else {
                        status_msg = "Save failed!";
                        status_timer = 90;
                    }
                    mode = GameMode::Normal;
                }
                continue;
            }

            // --- Load game screen ---
            if (mode == GameMode::LoadGame) {
                if (key == SDLK_ESCAPE) { mode = GameMode::PauseMenu; continue; }
                auto saves = list_saves();
                int max_slot = MAX_SAVE_SLOTS - 1;
                if (key == SDLK_UP && pause_cursor > 0) pause_cursor--;
                if (key == SDLK_DOWN && pause_cursor < max_slot) pause_cursor++;
                if (key == SDLK_RETURN || key == SDLK_SPACE) {
                    GameState state;
                    if (load_game(pause_cursor, state)) {
                        // Restore world with saved seed
                        map_seed = state.map_seed;
                        world.init(map_seed);

                        // Restore player
                        player = Player();
                        player.spawn(state.player_x, state.player_y);
                        player.stats() = state.stats;
                        int hp_target = state.hp;
                        int hp_max = player.max_hp();
                        if (hp_target < hp_max) {
                            player.take_damage(hp_max - hp_target);
                        }
                        player.set_xp(state.xp);
                        player.set_level(state.level);
                        player.set_gold(state.gold);

                        // Restore inventory
                        for (const auto& [item, qty] : state.inventory) {
                            player.add_item(item, qty);
                        }

                        // Restore equipment
                        for (const auto& [slot, item] : state.equipment) {
                            player.equip_to_slot(slot, item);
                        }

                        // Load chunks around player
                        world.update_loaded_chunks(player.x(), player.y());

                        // Clear existing entities before restoring per-chunk
                        world.clear_all_entities();

                        // Restore chunk data (terrain, explored tiles, modifications, placed objects, entities)
                        apply_chunk_data(world, state.chunks);

                        // Reset UI state
                        turn_counter = 0;
                        play_time_seconds = state.meta.play_time_seconds;
                        time_system.set_turn_of_day(state.turn_of_day);
                        need_fov_update = true;
                        status_msg = "Game loaded from slot " + std::to_string(pause_cursor + 1);
                        status_timer = 90;
                    } else {
                        status_msg = "No save in slot " + std::to_string(pause_cursor + 1);
                        status_timer = 90;
                    }
                    mode = GameMode::Normal;
                }
                continue;
            }

            // --- Dialogue mode ---
            if (mode == GameMode::Dialogue) {
                Npc* dialogue_npc = world.npc_at(dialogue_npc_x, dialogue_npc_y);
                if (!dialogue_npc) {
                    mode = GameMode::Normal; continue;
                }
                Npc& npc = *dialogue_npc;
                const auto& tree = npc.dialogue_tree();
                if (dialogue_node < 0 || dialogue_node >= static_cast<int>(tree.size())) {
                    mode = GameMode::Normal; continue;
                }

                const DialogueNode& node = tree[dialogue_node];
                int num_options = static_cast<int>(node.options.size());

                if (key == SDLK_ESCAPE) { mode = GameMode::Normal; continue; }
                if (key == SDLK_UP && dialogue_option > 0) dialogue_option--;
                if (key == SDLK_DOWN && dialogue_option < num_options - 1) dialogue_option++;

                if ((key == SDLK_RETURN || key == SDLK_SPACE) && num_options > 0) {
                    const DialogueOption& opt = node.options[dialogue_option];
                    switch (opt.action) {
                        case DialogueAction::Exit: mode = GameMode::Normal; break;
                        case DialogueAction::Talk:
                            dialogue_node = opt.next_node;
                            dialogue_option = 0;
                            break;
                        case DialogueAction::Trade:
                            if (npc.is_merchant()) trade_open(trade_state, &npc);
                            break;
                        case DialogueAction::Gift:
                            if (npc.can_gift() || npc.is_merchant()) {
                                gift_cursor = 0;
                                mode = GameMode::GiftSelect;
                            } else {
                                status_msg = npc.name() + " doesn't want your gifts yet.";
                                status_timer = 90;
                            }
                            break;
                        default: break;
                    }
                }
                continue;
            }

            // --- Gift select ---
            if (mode == GameMode::GiftSelect) {
                if (key == SDLK_ESCAPE) { mode = GameMode::Inventory; continue; }
                int max_idx = static_cast<int>(player.inventory().size()) - 1;
                if (key == SDLK_UP && gift_cursor > 0) gift_cursor--;
                if (key == SDLK_DOWN && gift_cursor < max_idx) gift_cursor++;
                if ((key == SDLK_RETURN || key == SDLK_SPACE) && max_idx >= 0) {
                    // Find nearest NPC
                    Npc* target_npc = nullptr;
                    int best_dist = 999;
                    const int dirs[][2] = {{0,0},{0,-1},{0,1},{-1,0},{1,0}};
                    for (auto [ddx, ddy] : dirs) {
                        int nx = player.x() + ddx;
                        int ny = player.y() + ddy;
                        Npc* n = world.npc_at(nx, ny);
                        if (n) {
                            int dist = std::abs(ddx) + std::abs(ddy);
                            if (dist < best_dist) { best_dist = dist; target_npc = n; }
                        }
                    }
                    if (!target_npc) {
                        status_msg = "No one nearby to gift to.";
                        status_timer = 90;
                        mode = GameMode::Normal;
                    } else {
                        const Item& item = player.inventory_item(gift_cursor);
                        GiftReaction reaction = target_npc->react_to_gift(item);
                        int aff_change = 0;
                        switch (reaction) {
                            case GiftReaction::Love:    aff_change = 15; break;
                            case GiftReaction::Like:    aff_change = 8; break;
                            case GiftReaction::Neutral: aff_change = 2; break;
                            case GiftReaction::Dislike: aff_change = -5; break;
                            case GiftReaction::Hate:    aff_change = -12; break;
                        }
                        target_npc->adjust_affinity(aff_change);
                        player.remove_item(gift_cursor, 1);
                        status_msg = target_npc->name() + ": \"" + target_npc->gift_reaction_text(reaction) + "\"";
                        status_timer = 150;
                        mode = GameMode::Normal;
                        player_took_turn = true;
                    }
                }
                continue;
            }

            // --- Inventory modes ---
            if (mode == GameMode::Inventory || mode == GameMode::InventoryAction ||
                mode == GameMode::InventoryExamine) {

                if (key == SDLK_ESCAPE || (mode == GameMode::Inventory && key == SDLK_i)) {
                    mode = GameMode::Normal; continue;
                }

                if (mode == GameMode::InventoryExamine) {
                    if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_ESCAPE) {
                        mode = GameMode::Inventory;
                    }
                    continue;
                }

                if (mode == GameMode::InventoryAction) {
                    const Item& item = player.inventory_item(inv_cursor);
                    int max_actions = 0;
                    std::vector<int> actions;
                    if (item.is_usable()) { actions.push_back(0); max_actions++; }
                    if (item.is_equippable()) { actions.push_back(1); max_actions++; }
                    actions.push_back(2); max_actions++; // Drop
                    actions.push_back(3); max_actions++; // Examine
                    actions.push_back(4); max_actions++; // Gift

                    if (key == SDLK_LEFT && inv_action_cursor > 0) inv_action_cursor--;
                    if (key == SDLK_RIGHT && inv_action_cursor < max_actions - 1) inv_action_cursor++;

                    if ((key == SDLK_RETURN || key == SDLK_SPACE) && max_actions > 0) {
                        int action = actions[inv_action_cursor];

                        if (action == 0) {
                            // Use
                            if (item.use_effect() == UseEffect::Heal) {
                                player.heal(item.use_amount());
                                status_msg = "Used " + item.name() + ". Healed " + std::to_string(item.use_amount()) + " HP.";
                            } else if (item.use_effect() == UseEffect::Feed) {
                                status_msg = "Ate " + item.name() + ". Restored hunger.";
                            } else if (item.use_effect() == UseEffect::Drink) {
                                status_msg = "Drank " + item.name() + ". Restored thirst.";
                            }
                            status_timer = 90;
                            player.remove_item(inv_cursor, 1);
                            mode = GameMode::Inventory;
                            if (inv_cursor >= static_cast<int>(player.inventory().size()))
                                inv_cursor = std::max(0, static_cast<int>(player.inventory().size()) - 1);
                        } else if (action == 1) {
                            // Equip
                            EquipSlot slot = item.equip_slot();
                            if (player.is_slot_occupied(slot)) player.unequip(slot);
                            player.equip(inv_cursor);
                            status_msg = "Equipped " + item.name() + ".";
                            status_timer = 60;
                            mode = GameMode::Inventory;
                            if (inv_cursor >= static_cast<int>(player.inventory().size()))
                                inv_cursor = std::max(0, static_cast<int>(player.inventory().size()) - 1);
                        } else if (action == 2) {
                            // Drop
                            Entity e(player.x(), player.y(), item.glyph(), item.name(),
                                     item.fg_r(), item.fg_g(), item.fg_b());
                            e.set_is_item(true);
                            world.spawn_item(std::move(e));
                            player.remove_item(inv_cursor, 1);
                            status_msg = "Dropped " + item.name() + ".";
                            status_timer = 60;
                            mode = GameMode::Inventory;
                            if (inv_cursor >= static_cast<int>(player.inventory().size()))
                                inv_cursor = std::max(0, static_cast<int>(player.inventory().size()) - 1);
                        } else if (action == 3) {
                            // Examine
                            examine_item = item;
                            mode = GameMode::InventoryExamine;
                        } else if (action == 4) {
                            // Gift
                            gift_cursor = 0;
                            mode = GameMode::GiftSelect;
                        }
                    }
                    if (key == SDLK_ESCAPE) { mode = GameMode::Inventory; continue; }
                    continue;
                }

                // Browse mode
                if (key == SDLK_TAB) {
                    equip_slot_cursor = 0;
                    if (inv_cursor >= 0) { inv_cursor = -1; }
                    else { inv_cursor = 0; }
                    continue;
                }

                if (inv_cursor >= 0) {
                    int max_idx = static_cast<int>(player.inventory().size()) - 1;
                    if (key == SDLK_UP && inv_cursor > 0) inv_cursor--;
                    if (key == SDLK_DOWN && inv_cursor < max_idx) inv_cursor++;
                    if ((key == SDLK_RETURN || key == SDLK_SPACE) && max_idx >= 0) {
                        mode = GameMode::InventoryAction;
                        inv_action_cursor = 0;
                    }
                } else {
                    if (key == SDLK_UP && equip_slot_cursor > 0) equip_slot_cursor--;
                    if (key == SDLK_DOWN && equip_slot_cursor < NUM_EQUIP_SLOTS - 1) equip_slot_cursor++;
                    if (key == SDLK_RETURN || key == SDLK_SPACE) {
                        EquipSlot slot = equip_slots[equip_slot_cursor];
                        if (player.is_slot_occupied(slot)) {
                            player.unequip(slot);
                            status_msg = "Unequipped.";
                            status_timer = 60;
                        }
                    }
                }
                continue;
            }

            // --- Normal mode ---
            if (mode == GameMode::Normal) {
                if (key == SDLK_ESCAPE) {
                    mode = GameMode::PauseMenu;
                    pause_cursor = 0;
                    continue;
                }
                if (key == SDLK_i) {
                    mode = GameMode::Inventory;
                    inv_cursor = 0;
                    inv_action_cursor = 0;
                    continue;
                }

                int dx = 0, dy = 0;
                switch (key) {
                    case SDLK_w: case SDLK_UP:    dy = -1; break;
                    case SDLK_s: case SDLK_DOWN:  dy =  1; break;
                    case SDLK_a: case SDLK_LEFT:  dx = -1; break;
                    case SDLK_d: case SDLK_RIGHT: dx =  1; break;
                    case SDLK_PERIOD: {
                        // Wait / skip turn
                        player_took_turn = true;
                        need_fov_update = true;
                        break;
                    }
                    case SDLK_g: {
                        // Pick up all items on the tile
                        std::vector<std::string> picked;
                        while (true) {
                            Entity* e = world.item_at(player.x(), player.y());
                            if (!e) break;
                            const Item* db_item = find_item(e->name());
                            if (!db_item) {
                                // Remove orphaned item
                                world.remove_item_at(player.x(), player.y());
                                continue;
                            }
                            if (!player.can_carry(db_item->weight())) {
                                status_msg = "Too heavy to pick up " + std::string(e->name()) + "!";
                                status_timer = 90;
                                break;
                            }
                            if (e->name() == "Gold Coin") {
                                player.add_gold(db_item->value());
                                picked.push_back("1 Gold");
                            } else {
                                player.add_item(*db_item, 1);
                                picked.push_back(e->name());
                            }
                            world.remove_item_at(player.x(), player.y());
                        }
                        if (!picked.empty()) {
                            if (picked.size() == 1) {
                                status_msg = "Picked up " + picked[0];
                            } else {
                                status_msg = "Picked up " + std::to_string(picked.size()) + " items";
                            }
                            status_timer = 60;
                        }
                        break;
                    }
                    case SDLK_RETURN: case SDLK_SPACE: {
                        const int dirs[][2] = {{0,-1},{0,1},{-1,0},{1,0}};
                        for (auto [ddx, ddy] : dirs) {
                            Npc* n = world.npc_at(player.x() + ddx, player.y() + ddy);
                            if (n) {
                                dialogue_npc_x = n->x();
                                dialogue_npc_y = n->y();
                                dialogue_node = 0;
                                dialogue_option = 0;
                                mode = GameMode::Dialogue;
                                break;
                            }
                        }
                        break;
                    }
                    default: break;
                }

                // Movement / combat
                if ((dx != 0 || dy != 0) && player.can_move(dx, dy, world)) {
                    int nx = player.x() + dx;
                    int ny = player.y() + dy;

                    // Check for enemy at destination (combat)
                    bool fought = false;
                    Enemy* target = world.enemy_at(nx, ny);
                    if (target) {
                        // Player attacks enemy
                        int atk_var = player.total_damage_variance();
                        int raw_atk = player.total_attack() + (atk_var > 0 ? std::rand() % (2 * atk_var + 1) - atk_var : 0);
                        int dmg = raw_atk - target->defense();
                        if (dmg < 1) dmg = 1;
                        target->take_damage(dmg);
                        status_msg = "You hit " + target->name() + " for " + std::to_string(dmg) + " damage!";
                        status_timer = 60;
                        fought = true;

                        if (target->check_death()) {
                            status_msg += " " + target->name() + " killed! +" + std::to_string(target->xp_value()) + " XP";
                            player.add_xp(target->xp_value());
                            // Drop items on the ground
                            int drop_x = target->x(), drop_y = target->y();
                            for (const auto& [item, qty] : target->drops()) {
                                Entity e(drop_x, drop_y, item.glyph(), item.name(),
                                         item.fg_r(), item.fg_g(), item.fg_b());
                                e.set_is_item(true);
                                world.spawn_item(std::move(e));
                            }
                            world.remove_enemy_at(nx, ny);
                        }
                    }

                    if (!fought) {
                        player.move(dx, dy, world);
                        world.update_loaded_chunks(player.x(), player.y());
                        need_fov_update = true;
                    }
                    player_took_turn = true;
                }
            }
        }

        if (need_fov_update) {
            // Compute light levels first so we know torch radius
            uint8_t ambient = time_system.ambient_light();
            int torch_r = player.has_light_source() ? player.torch_radius() : 0;
            light::compute(world, player.x(), player.y(), ambient, torch_r);

            // FOV radius scales with ambient light: full radius at day, shrinks at dusk/night
            int perception = player.stats().perception();
            int base_radius = perception * 2;
            if (base_radius < 6) base_radius = 6;
            float light_frac = ambient / 15.0f;
            int fov_radius = static_cast<int>(base_radius * light_frac);
            // Always see at least torch range + 2, or minimum 4
            int min_radius = (torch_r > 0) ? torch_r + 2 : 4;
            if (fov_radius < min_radius) fov_radius = min_radius;
            fov::compute(world, player.x(), player.y(), fov_radius);

            need_fov_update = false;
        }

        if (player_took_turn) {
            turn_counter++;
            time_system.advance();
            // Enemy turns — collect pointers first since enemies may die
            auto all_enemies = world.get_all_enemies();
            for (auto* ep : all_enemies) {
                if (!ep->is_alive()) continue;
                int dist_before = std::abs(ep->x() - player.x()) + std::abs(ep->y() - player.y());
                ep->update(world, player.x(), player.y());

                // Enemy attacks if adjacent and chasing
                int dist_after = std::abs(ep->x() - player.x()) + std::abs(ep->y() - player.y());
                if (dist_after <= 1 && ep->is_alive()) {
                    int e_var = ep->damage_variance();
                    int raw_e_atk = ep->attack() + (e_var > 0 ? std::rand() % (2 * e_var + 1) - e_var : 0);
                    int actual = player.calc_damage(raw_e_atk);
                    player.take_damage(raw_e_atk);
                    status_msg = ep->name() + " hits you for " + std::to_string(actual) + " damage!";

                    // Player counter-attacks
                    int c_var = player.total_damage_variance();
                    int raw_c_atk = player.total_attack() + (c_var > 0 ? std::rand() % (2 * c_var + 1) - c_var : 0);
                    int counter_dmg = raw_c_atk - ep->defense();
                    if (counter_dmg < 1) counter_dmg = 1;
                    ep->take_damage(counter_dmg);
                    status_msg += " You hit back for " + std::to_string(counter_dmg) + "!";

                    if (ep->check_death()) {
                        status_msg += " " + ep->name() + " killed! +" + std::to_string(ep->xp_value()) + " XP";
                        player.add_xp(ep->xp_value());
                        for (const auto& [item, qty] : ep->drops()) {
                            Entity de(ep->x(), ep->y(), item.glyph(), item.name(),
                                     item.fg_r(), item.fg_g(), item.fg_b());
                            de.set_is_item(true);
                            world.spawn_item(std::move(de));
                        }
                        world.remove_enemy_at(ep->x(), ep->y());
                    }
                    status_timer = 90;
                }
            }
            // NPC turns
            auto all_npcs = world.get_all_npcs();
            for (auto* np : all_npcs) {
                np->update(world, player.x(), player.y());
            }
            // Fix entities that crossed chunk boundaries during movement
            world.rekey_entities();

            // Check player death
            if (player.is_dead()) {
                mode = GameMode::Dead;
            }
        }

        // Play time tracking
        play_time_timer++;
        if (play_time_timer >= 60) { play_time_timer = 0; play_time_seconds++; }
        if (status_timer > 0) status_timer--;

        // --- Render ---
        renderer.clear();
        int view_cols = window.width() / renderer.cell_width();
        int view_rows = window.height() / renderer.cell_height();
        renderer.begin_frame(view_cols, view_rows);

        int cam_x = player.x() - view_cols / 2;
        int cam_y = player.y() - view_rows / 2;
        // Unbounded camera — no map edge clamping

        // Tiles
        for (int vy = 0; vy < view_rows; ++vy) {
            for (int vx = 0; vx < view_cols; ++vx) {
                int mx = cam_x + vx;
                int my = cam_y + vy;
                Cell cell;
                cell.glyph = ' ';

                if (world.in_bounds(mx, my)) {
                    Tile tile = world.get_tile(mx, my);
                    if (tile.visible) {
                        cell.glyph = tile_glyph(tile.type);
                        tile_color(tile.type, cell.fg_r, cell.fg_g, cell.fg_b);
                    } else if (tile.explored) {
                        cell.glyph = tile_glyph(tile.type);
                        tile_color(tile.type, cell.fg_r, cell.fg_g, cell.fg_b);
                        cell.fg_r = static_cast<uint8_t>(cell.fg_r * 0.3f);
                        cell.fg_g = static_cast<uint8_t>(cell.fg_g * 0.3f);
                        cell.fg_b = static_cast<uint8_t>(cell.fg_b * 0.3f);
                    }
                }

                if (world.in_bounds(mx, my) && world.get_tile(mx, my).visible) {
                    Tile tile = world.get_tile(mx, my);

                    // Check for items
                    Entity* item = world.item_at(mx, my);
                    if (item && !item->in_inventory()) {
                        cell.glyph = item->glyph();
                        cell.fg_r = item->fg_r(); cell.fg_g = item->fg_g(); cell.fg_b = item->fg_b();
                    }
                    // Check for NPCs
                    Npc* npc = world.npc_at(mx, my);
                    if (npc) {
                        cell.glyph = npc->glyph();
                        cell.fg_r = npc->fg_r(); cell.fg_g = npc->fg_g(); cell.fg_b = npc->fg_b();
                    }
                    // Check for enemies
                    Enemy* enemy = world.enemy_at(mx, my);
                    if (enemy) {
                        cell.glyph = enemy->glyph();
                        cell.fg_r = enemy->fg_r(); cell.fg_g = enemy->fg_g(); cell.fg_b = enemy->fg_b();
                    }
                }

                if (mx == player.x() && my == player.y()) {
                    cell.glyph = '@';
                    cell.fg_r = 0; cell.fg_g = 255; cell.fg_b = 0;
                }

                renderer.set_cell(vx, vy, cell);
            }
        }

        // HUD bar
        {
            int hud_y = view_rows - 1;
            for (int x = 0; x < view_cols; ++x) {
                Cell c; c.glyph = ' ';
                c.bg_r = 20; c.bg_g = 20; c.bg_b = 30;
                renderer.set_cell(x, hud_y, c);
            }
            std::string hud = "HP:" + std::to_string(player.hp()) + "/" + std::to_string(player.max_hp()) +
                              " ATK:" + std::to_string(player.total_attack() - player.total_damage_variance()) + "-" + std::to_string(player.total_attack() + player.total_damage_variance()) +
                              " DEF:" + std::to_string(player.total_defense()) +
                              " G:" + std::to_string(player.gold()) +
                              " Wt:" + std::to_string(player.current_weight()) + "/" + std::to_string(player.carry_capacity()) +
                              " Lv:" + std::to_string(player.level()) +
                              " XP:" + std::to_string(player.xp()) + "/" + std::to_string(player.xp_to_next_level()) +
                              " " + time_system.time_string() + " " + time_system.time_period_string();
            // Biome indicator
            BiomeType biome = world.get_biome_at(player.x(), player.y());
            hud += " [" + std::string(biome_name(biome)) + "]";
            // Structure indicator
            StructureType structure = world.get_structure_at(player.x(), player.y());
            if (structure != StructureType::None) {
                const StructureDef& sd = structure_def(structure);
                hud += " " + std::string(sd.name);
            }
            if (player.has_light_source()) hud += " [Torch]";
            draw_string(renderer, 1, hud_y, hud, 180, 180, 180, 20, 20, 30);
        }

        // Status message
        if (status_timer > 0 && !status_msg.empty()) {
            int msg_y = view_rows - 2;
            for (int x = 0; x < static_cast<int>(status_msg.size()) + 2 && x < view_cols; ++x) {
                Cell c; c.glyph = ' ';
                c.bg_r = 40; c.bg_g = 40; c.bg_b = 20;
                renderer.set_cell(x, msg_y, c);
            }
            draw_string(renderer, 1, msg_y, status_msg, 255, 255, 100, 40, 40, 20);
        }

        // --- Overlays ---

        // Inventory overlay
        if (mode == GameMode::Inventory || mode == GameMode::InventoryAction ||
            mode == GameMode::InventoryExamine || mode == GameMode::GiftSelect) {
            int box_w = view_cols - 4;
            if (box_w > 60) box_w = 60;
            int box_h = view_rows - 6;
            if (box_h > 28) box_h = 28;
            int box_x = 2, box_y = 2;
            int panel_w = (box_w - 4) / 2;

            for (int y = box_y; y < box_y + box_h; ++y)
                for (int x = box_x; x < box_x + box_w; ++x) {
                    Cell c; c.glyph = ' '; c.bg_r = 15; c.bg_g = 15; c.bg_b = 35;
                    renderer.set_cell(x, y, c);
                }

            draw_string(renderer, box_x + 1, box_y, "INVENTORY", 255, 255, 255, 15, 15, 35);
            std::string wt = "Wt: " + std::to_string(player.current_weight()) + "/" + std::to_string(player.carry_capacity());
            std::string gd = "Gold: " + std::to_string(player.gold());
            draw_string(renderer, box_x + box_w - wt.size() - 1, box_y, wt, 180, 180, 180, 15, 15, 35);
            draw_string(renderer, box_x + box_w - wt.size() - gd.size() - 3, box_y, gd, 255, 215, 0, 15, 15, 35);

            int list_x = box_x + 1;
            int doll_x = box_x + panel_w + 3;
            int max_visible = box_h - 4;

            // Item list
            draw_string(renderer, list_x, box_y + 1, "Items:", 180, 180, 180, 15, 15, 35);
            const auto& inv = player.inventory();
            bool in_items = (inv_cursor >= 0);
            int scroll = 0;
            if (in_items && inv_cursor >= max_visible) scroll = inv_cursor - max_visible + 1;

            for (int i = 0; i < max_visible && (i + scroll) < static_cast<int>(inv.size()); ++i) {
                int idx = i + scroll;
                const auto& [item, qty] = inv[idx];
                int y = box_y + 2 + i;
                bool selected = (idx == inv_cursor && in_items);
                uint8_t bgr = 15, bgg = 15, bgb = 35;
                if (selected) { bgr = 40; bgg = 40; bgb = 60; }

                Cell gc; gc.glyph = item.glyph();
                gc.fg_r = item.fg_r(); gc.fg_g = item.fg_g(); gc.fg_b = item.fg_b();
                gc.bg_r = bgr; gc.bg_g = bgg; gc.bg_b = bgb;
                renderer.set_cell(list_x, y, gc);

                std::string label = item.name();
                if (qty > 1) label += " (" + std::to_string(qty) + ")";
                draw_string(renderer, list_x + 2, y, label, 200, 200, 200, bgr, bgg, bgb);
            }
            if (inv.empty()) draw_string(renderer, list_x + 1, box_y + 2, "(empty)", 100, 100, 100, 15, 15, 35);

            // Paper doll
            draw_string(renderer, doll_x, box_y + 1, "Equipment:", 180, 180, 180, 15, 15, 35);
            for (int i = 0; i < NUM_EQUIP_SLOTS; ++i) {
                int y = box_y + 2 + i;
                bool selected = (!in_items && i == equip_slot_cursor);
                uint8_t bgr = 15, bgg = 15, bgb = 35;
                if (selected) { bgr = 40; bgg = 40; bgb = 60; }

                EquipSlot slot = equip_slots[i];
                const Item* equipped = player.equipped_at(slot);
                draw_string(renderer, doll_x, y, slot_names[i + 1], 140, 140, 160, bgr, bgg, bgb);
                if (equipped) {
                    Cell gc; gc.glyph = equipped->glyph();
                    gc.fg_r = equipped->fg_r(); gc.fg_g = equipped->fg_g(); gc.fg_b = equipped->fg_b();
                    gc.bg_r = bgr; gc.bg_g = bgg; gc.bg_b = bgb;
                    renderer.set_cell(doll_x + 9, y, gc);
                    draw_string(renderer, doll_x + 11, y, equipped->name(), 200, 200, 200, bgr, bgg, bgb);
                } else {
                    draw_string(renderer, doll_x + 10, y, "-", 80, 80, 80, bgr, bgg, bgb);
                }
            }

            // Action menu
            if (mode == GameMode::InventoryAction && in_items && inv_cursor >= 0 && inv_cursor < static_cast<int>(inv.size())) {
                const Item& item = player.inventory_item(inv_cursor);
                std::vector<std::string> actions;
                if (item.is_usable()) actions.push_back("Use");
                if (item.is_equippable()) actions.push_back("Equip");
                actions.push_back("Drop");
                actions.push_back("Examine");
                actions.push_back("Gift");

                int menu_x = box_x + 2;
                int menu_y = box_y + box_h - 2;
                for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
                    uint8_t bg = (i == inv_action_cursor) ? 60 : 30;
                    draw_string(renderer, menu_x + i * 10, menu_y, actions[i], 200, 200, 200, bg, bg, 50);
                }
            }

            // Examine overlay
            if (mode == GameMode::InventoryExamine) {
                int ex_w = 40, ex_h = 8;
                int ex_x = box_x + (box_w - ex_w) / 2;
                int ex_y = box_y + (box_h - ex_h) / 2;
                for (int y = ex_y; y < ex_y + ex_h; ++y)
                    for (int x = ex_x; x < ex_x + ex_w; ++x) {
                        Cell c; c.glyph = ' '; c.bg_r = 25; c.bg_g = 25; c.bg_b = 45;
                        renderer.set_cell(x, y, c);
                    }
                draw_string(renderer, ex_x + 1, ex_y, examine_item.name(), 255, 255, 255, 25, 25, 45);
                draw_string(renderer, ex_x + 1, ex_y + 1, examine_item.description(), 180, 180, 180, 25, 25, 45);
                std::string stats = "Wt:" + std::to_string(examine_item.weight()) + "  Val:" + std::to_string(examine_item.value());
                if (examine_item.attack() > 0) stats += "  ATK:+" + std::to_string(examine_item.attack());
                if (examine_item.defense() > 0) stats += "  DEF:+" + std::to_string(examine_item.defense());
                draw_string(renderer, ex_x + 1, ex_y + 3, stats, 140, 180, 140, 25, 25, 45);
                draw_string(renderer, ex_x + 1, ex_y + 5, "[ENTER] close", 100, 100, 100, 25, 25, 45);
            }

            // Gift select overlay
            if (mode == GameMode::GiftSelect) {
                int gw = 30, gh = static_cast<int>(inv.size()) + 4;
                if (gh < 6) gh = 6;
                int gx = box_x + 2, gy = box_y + box_h - gh - 1;
                for (int y = gy; y < gy + gh; ++y)
                    for (int x = gx; x < gx + gw; ++x) {
                        Cell c; c.glyph = ' '; c.bg_r = 30; c.bg_g = 20; c.bg_b = 20;
                        renderer.set_cell(x, y, c);
                    }
                draw_string(renderer, gx + 1, gy, "Gift which item?", 255, 200, 200, 30, 20, 20);
                for (int i = 0; i < static_cast<int>(inv.size()); ++i) {
                    int y = gy + 1 + i;
                    if (y >= gy + gh - 1) break;
                    const auto& [item, qty] = inv[i];
                    bool selected = (i == gift_cursor);
                    uint8_t bg = selected ? 60 : 30;
                    Cell gc; gc.glyph = item.glyph();
                    gc.fg_r = item.fg_r(); gc.fg_g = item.fg_g(); gc.fg_b = item.fg_b();
                    gc.bg_r = bg; gc.bg_g = bg; gc.bg_b = 20;
                    renderer.set_cell(gx + 1, y, gc);
                    std::string label = item.name();
                    if (qty > 1) label += " (" + std::to_string(qty) + ")";
                    draw_string(renderer, gx + 3, y, label, 200, 200, 200, bg, bg, 20);
                }
            }

            draw_string(renderer, doll_x, box_y + box_h - 1, "[TAB] switch", 100, 100, 100, 15, 15, 35);
        }

        // Dialogue overlay
        if (mode == GameMode::Dialogue && dialogue_npc_x >= 0) {
            Npc* d_npc = world.npc_at(dialogue_npc_x, dialogue_npc_y);
            if (d_npc) {
                const Npc& npc = *d_npc;
                const auto& tree = npc.dialogue_tree();
                if (dialogue_node >= 0 && dialogue_node < static_cast<int>(tree.size())) {
                const DialogueNode& node = tree[dialogue_node];
                int dw = 50;
                if (dw > view_cols - 4) dw = view_cols - 4;
                int text_w = dw - 2;

                std::vector<std::string> wrapped;
                {
                    std::string rem = node.speaker_text;
                    while (!rem.empty()) {
                        if (static_cast<int>(rem.size()) <= text_w) { wrapped.push_back(rem); break; }
                        int split = static_cast<int>(rem.rfind(' ', text_w));
                        if (split <= 0) split = text_w;
                        wrapped.push_back(rem.substr(0, split));
                        rem = rem.substr((split < text_w) ? split + 1 : split);
                    }
                }

                int dh = static_cast<int>(wrapped.size()) + static_cast<int>(node.options.size()) + 4;
                if (dh < 7) dh = 7;
                int dx = 2, dy = view_rows - dh - 2;

                for (int y = dy; y < dy + dh; ++y)
                    for (int x = dx; x < dx + dw; ++x) {
                        Cell c; c.glyph = ' '; c.bg_r = 20; c.bg_g = 20; c.bg_b = 40;
                        renderer.set_cell(x, y, c);
                    }

                draw_string(renderer, dx + 1, dy, npc.name(), npc.fg_r(), npc.fg_g(), npc.fg_b(), 20, 20, 40);
                std::string aff = "Affinity: " + std::to_string(npc.affinity());
                draw_string(renderer, dx + dw - aff.size() - 1, dy, aff, 140, 140, 140, 20, 20, 40);

                for (int i = 0; i < static_cast<int>(wrapped.size()); ++i)
                    draw_string(renderer, dx + 1, dy + 1 + i, wrapped[i], 200, 200, 200, 20, 20, 40);

                int opt_y = dy + 1 + static_cast<int>(wrapped.size()) + 1;
                for (int i = 0; i < static_cast<int>(node.options.size()); ++i) {
                    bool selected = (i == dialogue_option);
                    uint8_t bg = selected ? 60 : 20;
                    std::string prefix = selected ? "> " : "  ";
                    draw_string(renderer, dx + 1, opt_y + i, prefix + node.options[i].text, 200, 200, 100, bg, bg, 40);
                }
                }
            }
        }

        // Trade overlay
        if (trade_state.active) {
            trade_render(trade_state, player, renderer, view_cols, view_rows);
        }

        // Pause menu overlay
        if (mode == GameMode::PauseMenu) {
            int pw = 24, ph = 5;
            int px = (view_cols - pw) / 2, py = (view_rows - ph) / 2;
            for (int y = py; y < py + ph; ++y)
                for (int x = px; x < px + pw; ++x) {
                    Cell c; c.glyph = ' '; c.bg_r = 20; c.bg_g = 20; c.bg_b = 40;
                    renderer.set_cell(x, y, c);
                }
            draw_string(renderer, px + 1, py, "PAUSED", 255, 255, 255, 20, 20, 40);
            std::string opts[] = {"Save Game", "Load Game", "Quit"};
            for (int i = 0; i < 3; ++i) {
                uint8_t bg = (i == pause_cursor) ? 60 : 20;
                std::string prefix = (i == pause_cursor) ? "> " : "  ";
                draw_string(renderer, px + 2, py + 1 + i, prefix + opts[i], 200, 200, 200, bg, bg, 40);
            }
        }

        // Save/Load screen
        if (mode == GameMode::SaveGame || mode == GameMode::LoadGame) {
            int sw = 50, sh = MAX_SAVE_SLOTS + 4;
            int sx = (view_cols - sw) / 2, sy = (view_rows - sh) / 2;
            for (int y = sy; y < sy + sh; ++y)
                for (int x = sx; x < sx + sw; ++x) {
                    Cell c; c.glyph = ' '; c.bg_r = 15; c.bg_g = 15; c.bg_b = 35;
                    renderer.set_cell(x, y, c);
                }

            std::string title = (mode == GameMode::SaveGame) ? "SAVE GAME" : "LOAD GAME";
            draw_string(renderer, sx + 1, sy, title, 255, 255, 255, 15, 15, 35);

            auto saves = list_saves();
            for (int i = 0; i < MAX_SAVE_SLOTS; ++i) {
                int y = sy + 1 + i;
                bool selected = (i == pause_cursor);
                uint8_t bg = selected ? 40 : 15;

                // Check if this slot has a save
                bool has_save = false;
                SaveMeta meta;
                for (const auto& s : saves) {
                    if (s.slot == i) { has_save = true; meta = s; break; }
                }

                std::string slot_label = "Slot " + std::to_string(i + 1) + ": ";
                if (has_save) {
                    slot_label += meta.character_name + " (Lv " + std::to_string(meta.play_time_seconds / 60) + "min)";
                } else {
                    slot_label += "--- empty ---";
                }
                draw_string(renderer, sx + 2, y, slot_label, has_save ? 200 : 100, has_save ? 200 : 100, has_save ? 200 : 100, bg, bg, 35);
            }

            draw_string(renderer, sx + 1, sy + sh - 1, "[ENTER] select  [ESC] back", 100, 100, 100, 15, 15, 35);
        }

        // Death screen
        if (mode == GameMode::Dead) {
            for (int y = 0; y < view_rows; ++y)
                for (int x = 0; x < view_cols; ++x) {
                    Cell c; c.glyph = ' '; c.bg_r = 30; c.bg_g = 0; c.bg_b = 0;
                    renderer.set_cell(x, y, c);
                }
            std::string msg = "YOU DIED";
            draw_string(renderer, (view_cols - msg.size()) / 2, view_rows / 2 - 2, msg, 255, 50, 50, 30, 0, 0);
            std::string prompt = "Press R to restart, ESC to quit";
            draw_string(renderer, (view_cols - prompt.size()) / 2, view_rows / 2, prompt, 180, 180, 180, 30, 0, 0);
        }

        renderer.render_grid();
        renderer.present();
        SDL_Delay(16);
    }

    renderer.shutdown();
    window.shutdown();
    return 0;
}
