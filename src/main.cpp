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
#include "game/building.h"
#include "game/crafting.h"
#include "game/settings.h"
#include "game/rng.h"
#include "game/turn.h"
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>

static const int FONT_SIZE = 20;
static const int BUILD_RANGE = 6;

// PLAN.md 4B: base FOV radius at WIS 10, before the perception bonus and the
// ambient-light scaling applied in the FOV update.
static const int FOV_BASE_RADIUS = 8;

// World map view: chunks shown in each direction from the player's chunk
static const int MAP_RADIUS = 10;

static const char* slot_names[] = {
    "None", "Head", "L Shoulder", "R Shoulder", "Torso",
    "L Arm", "R Arm", "L Hand", "R Hand",
    "L Leg", "R Leg", "L Foot", "R Foot"
};

enum class GameMode {
    MainMenu,
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
    Settings,
    Craft,
    Build,
    Zone,
    Character,
    Map,
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

// Draw a filled overlay box
static void draw_box(Renderer& r, int x, int y, int w, int h,
                     uint8_t br, uint8_t bg, uint8_t bb) {
    for (int yy = y; yy < y + h; ++yy)
        for (int xx = x; xx < x + w; ++xx) {
            Cell c; c.glyph = ' ';
            c.bg_r = br; c.bg_g = bg; c.bg_b = bb;
            r.set_cell(xx, yy, c);
        }
}

int main(int argc, char* argv[]) {
    // Dev tool: --demo runs a scripted UI walkthrough, writing BMP screenshots
    bool demo_mode = (argc > 1 && std::string(argv[1]) == "--demo");

    Window window;
    if (!window.init({"ASCII Game", 1024, 768})) return 1;

    Renderer renderer;
    std::string font_path = "C:/Windows/Fonts/cour.ttf";
    if (!renderer.init(window.get(), font_path, FONT_SIZE)) {
        std::cerr << "Failed to initialize renderer.\n";
        return 1;
    }

    // --- Persistent settings ---
    Settings settings;
    load_settings(settings);

    // --- Game state ---
    uint32_t map_seed = 0;
    World world;
    Player player;
    TimeSystem time_system;
    // Seeded per run and round-tripped through the save file, so a reloaded
    // game continues the same random sequence.
    Rng rng;

    bool running = true;
    bool game_started = false;
    SDL_Event event;
    bool need_fov_update = false;
    bool player_took_turn = false;

    GameMode mode = GameMode::MainMenu;
    int menu_cursor = 0;
    bool load_from_menu = false;

    int inv_cursor = 0;
    int inv_action_cursor = 0;
    int gift_cursor = 0;
    GameMode gift_return = GameMode::Normal;
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

    Item examine_item;

    // Message log (newest at back)
    struct Msg { std::string text; int timer; };
    std::vector<Msg> msg_log;
    auto log_msg = [&](const std::string& s, int timer = 90) {
        msg_log.push_back({s, timer});
        if (msg_log.size() > 4) msg_log.erase(msg_log.begin());
    };

    int turn_counter = 0;
    int play_time_seconds = 0;
    int play_time_timer = 0;

    // Pause menu state
    int pause_cursor = 0;

    // Settings screen state
    int settings_cursor = 0;

    // Build mode state
    int build_cx = 0, build_cy = 0;
    int build_sel = 0;

    // Zone mode state
    int zone_cx = 0, zone_cy = 0;
    int zone_ax = 0, zone_ay = 0;
    bool zone_anchored = false;
    bool zone_pick_type = false;
    int zone_type_sel = 0;

    // Craft mode state
    int craft_cursor = 0;

    // World map view state
    int map_cursor_cx = 0, map_cursor_cy = 0;
    int map_cache_cx = 0, map_cache_cy = 0;
    std::vector<World::ChunkMapInfo> map_cache;

    // Structure discovery (chunk coords of discovered structures)
    std::set<std::pair<int,int>> discovered_structures;

    // Check equipment by name (any slot)
    auto has_equipped = [&](const char* name) {
        for (const auto& [slot, item] : player.equipment()) {
            if (item.name() == name) return true;
        }
        return false;
    };

    // ── New game initialization ────────────────────────────────────
    auto start_new_game = [&]() {
        // Demo mode uses a fixed seed so screenshots are reproducible
        map_seed = demo_mode ? 20260801u : static_cast<uint32_t>(std::time(nullptr));
        rng.seed(0x9E3779B97F4A7C15ull ^ (static_cast<uint64_t>(map_seed) * 6364136223846793005ull));
        world = World();
        world.init(map_seed);
        world.set_load_radius(settings.load_radius);
        world.set_render_radius(settings.render_radius);
        world.set_simulation_radius(settings.sim_radius);

        player = Player();
        time_system = TimeSystem();

        // Load chunks around origin (origin chunk always holds a village)
        world.update_loaded_chunks(0, 0);

        // Find spawn point: prefer village floor, fall back to any passable tile
        bool found = false;
        for (int r = 0; r <= world.load_radius() && !found; ++r) {
            for (int dy = -r; dy <= r && !found; ++dy) {
                for (int dx = -r; dx <= r && !found; ++dx) {
                    if (std::abs(dx) != r && std::abs(dy) != r) continue;
                    int cx = World::world_to_chunk_x(dx);
                    int cy = World::world_to_chunk_y(dy);
                    Chunk* chunk = world.get_chunk(cx, cy);
                    if (chunk && chunk->structure_id() == static_cast<int>(StructureType::Village)) {
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
        if (!found) player.spawn(0, 0);
        world.update_loaded_chunks(player.x(), player.y());

        // Starting items
        {
            const Item* bread = find_item("Bread");
            const Item* potion = find_item("Health Potion");
            const Item* sword = find_item("Iron Sword");
            const Item* armor = find_item("Leather Armor");
            const Item* torch = find_item("Torch");
            if (bread) player.add_item(*bread, 3);
            if (potion) player.add_item(*potion, 1);
            if (sword) player.add_item(*sword, 1);
            if (armor) player.add_item(*armor, 1);
            if (torch) player.add_item(*torch, 1);
        }

        // Pick a free tile in a box around the player, or report failure.
        auto find_open_tile = [&](int reach, int& out_x, int& out_y) {
            for (int attempt = 0; attempt < 200; ++attempt) {
                int tx = player.x() + rng.range(-reach, reach);
                int ty = player.y() + rng.range(-reach, reach);
                if (!world.is_passable(tx, ty)) continue;
                if (world.has_npc_at(tx, ty) || world.has_enemy_at(tx, ty)) continue;
                out_x = tx;
                out_y = ty;
                return true;
            }
            return false;
        };

        // World items — spawn near player in loaded chunks
        {
            const char* names[] = {"Gold Coin", "Health Potion", "Old Scroll",
                                   "Bread", "Rusty Key", "Apple"};
            int num = 10 + rng.below(6);
            for (int i = 0; i < num; ++i) {
                int ix, iy;
                if (!find_open_tile(30, ix, iy)) continue;
                const Item* item = find_item(names[rng.below(6)]);
                if (!item) continue;
                Entity e(ix, iy, item->glyph(), item->name(),
                         item->fg_r(), item->fg_g(), item->fg_b());
                e.set_is_item(true);
                world.spawn_item(std::move(e));
            }
        }

        // Wandering NPCs near the player. The origin village supplies the six
        // settlement NPCs (see make_npc / village_npc_names), so spawning them
        // here too would duplicate the merchant and villagers.
        for (const char* name : {"Old Sage", "Wanderer", "Child"}) {
            int nx, ny;
            if (!find_open_tile(20, nx, ny)) continue;
            world.spawn_npc(make_npc(name, nx, ny));
        }

        // Enemies — spawn near player, but not right on top of them
        for (const char* name : {"Rat", "Rat", "Goblin", "Goblin", "Spider", "Bat"}) {
            int ex, ey;
            if (!find_open_tile(25, ex, ey)) continue;
            if (std::abs(ex - player.x()) + std::abs(ey - player.y()) < 15) continue;
            world.spawn_enemy(make_enemy(name, ex, ey));
        }

        // Reset UI/run state
        turn_counter = 0;
        play_time_seconds = 0;
        play_time_timer = 0;
        msg_log.clear();
        discovered_structures.clear();
        zone_anchored = false;
        zone_pick_type = false;
        need_fov_update = true;
        game_started = true;
        mode = GameMode::Normal;
        log_msg("You arrive at the village. The wilds await.", 150);
    };

    // Demo mode: skip the menu and jump straight into a new game
    int demo_frame = 0;
    if (demo_mode) {
        start_new_game();
    }
    auto demo_push_key = [](SDL_Keycode k) {
        SDL_Event e;
        SDL_zero(e);
        e.type = SDL_KEYDOWN;
        e.key.keysym.sym = k;
        e.key.keysym.mod = KMOD_NONE;
        SDL_PushEvent(&e);
    };

    // ── Structure discovery check (after movement) ─────────────────
    auto check_discovery = [&]() {
        int cx = World::world_to_chunk_x(player.x());
        int cy = World::world_to_chunk_y(player.y());
        Chunk* chunk = world.get_chunk(cx, cy);
        if (!chunk) return;
        int sid = chunk->structure_id();
        if (sid == 0) return;
        if (discovered_structures.count({cx, cy})) return;
        discovered_structures.insert({cx, cy});
        const StructureDef& sd = structure_def(static_cast<StructureType>(sid));
        log_msg(std::string("You discover a ") + sd.name + "!", 150);
    };

    while (running) {
        player_took_turn = false;

        // Demo mode scripted timeline
        std::string demo_shot;
        if (demo_mode) {
            switch (demo_frame) {
                case 10:  demo_push_key(SDLK_d); break;
                case 12:  demo_push_key(SDLK_s); break;
                case 20:  demo_shot = "demo1_game.bmp"; break;
                case 30:  demo_push_key(SDLK_i); break;
                case 40:  demo_shot = "demo2_inv.bmp"; break;
                case 50:  demo_push_key(SDLK_ESCAPE); break;
                case 60:  demo_push_key(SDLK_c); break;
                case 70:  demo_shot = "demo3_craft.bmp"; break;
                case 80:  demo_push_key(SDLK_ESCAPE); break;
                case 90:  demo_push_key(SDLK_b); break;
                case 100: demo_push_key(SDLK_d); break;
                case 110: demo_shot = "demo4_build.bmp"; break;
                case 120: demo_push_key(SDLK_ESCAPE); break;
                case 130: demo_push_key(SDLK_z); break;
                case 140: demo_shot = "demo5_zone.bmp"; break;
                case 150: demo_push_key(SDLK_ESCAPE); break;
                case 160: demo_push_key(SDLK_x); break;
                case 170: demo_shot = "demo6_char.bmp"; break;
                case 180: demo_push_key(SDLK_ESCAPE); break;
                case 190: demo_push_key(SDLK_m); break;
                case 210: demo_push_key(SDLK_d); break;
                case 220: demo_shot = "demo7_map.bmp"; break;
                case 230: demo_push_key(SDLK_ESCAPE); break;
                case 240: demo_push_key(SDLK_ESCAPE); break;
                case 250: demo_shot = "demo8_pause.bmp"; break;
                case 260: running = false; break;
                default: break;
            }
            demo_frame++;
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) { running = false; break; }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                window.on_resized(event.window.data1, event.window.data2);
                continue;
            }
            if (event.type != SDL_KEYDOWN) continue;
            SDL_Keycode key = event.key.keysym.sym;

            // --- Main menu ---
            if (mode == GameMode::MainMenu) {
                if (key == SDLK_UP && menu_cursor > 0) menu_cursor--;
                if (key == SDLK_DOWN && menu_cursor < 2) menu_cursor++;
                if (key == SDLK_RETURN || key == SDLK_SPACE) {
                    if (menu_cursor == 0) {
                        start_new_game();
                    } else if (menu_cursor == 1) {
                        load_from_menu = true;
                        pause_cursor = 0;
                        mode = GameMode::LoadGame;
                    } else {
                        running = false;
                    }
                }
                if (key == SDLK_ESCAPE) running = false;
                continue;
            }

            // --- Dead mode ---
            if (mode == GameMode::Dead) {
                if (key == SDLK_r) start_new_game();
                if (key == SDLK_ESCAPE) { game_started = false; mode = GameMode::MainMenu; menu_cursor = 0; }
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
                if (key == SDLK_DOWN && pause_cursor < 4) pause_cursor++;
                if (key == SDLK_RETURN || key == SDLK_SPACE) {
                    if (pause_cursor == 0) { pause_cursor = 0; mode = GameMode::SaveGame; }
                    else if (pause_cursor == 1) { load_from_menu = false; pause_cursor = 0; mode = GameMode::LoadGame; }
                    else if (pause_cursor == 2) { settings_cursor = 0; mode = GameMode::Settings; }
                    else if (pause_cursor == 3) { game_started = false; mode = GameMode::MainMenu; menu_cursor = 0; }
                    else { running = false; }
                }
                continue;
            }

            // --- Settings screen ---
            if (mode == GameMode::Settings) {
                if (key == SDLK_ESCAPE || key == SDLK_RETURN) {
                    save_settings(settings);
                    mode = GameMode::PauseMenu;
                    continue;
                }
                if (key == SDLK_UP && settings_cursor > 0) settings_cursor--;
                if (key == SDLK_DOWN && settings_cursor < 2) settings_cursor++;
                int delta = 0;
                if (key == SDLK_LEFT) delta = -1;
                if (key == SDLK_RIGHT) delta = 1;
                if (delta != 0) {
                    if (settings_cursor == 0) world.set_load_radius(settings.load_radius + delta);
                    if (settings_cursor == 1) world.set_render_radius(settings.render_radius + delta);
                    if (settings_cursor == 2) world.set_simulation_radius(settings.sim_radius + delta);
                    // World setters enforce constraints; sync back
                    settings.load_radius = world.load_radius();
                    settings.render_radius = world.render_radius();
                    settings.sim_radius = world.simulation_radius();
                    // Reload chunks under new radius
                    world.update_loaded_chunks(player.x(), player.y());
                    need_fov_update = true;
                }
                continue;
            }

            // --- Save game screen ---
            if (mode == GameMode::SaveGame) {
                if (key == SDLK_ESCAPE) { pause_cursor = 0; mode = GameMode::PauseMenu; continue; }
                int max_slot = MAX_SAVE_SLOTS - 1;
                if (key == SDLK_UP && pause_cursor > 0) pause_cursor--;
                if (key == SDLK_DOWN && pause_cursor < max_slot) pause_cursor++;
                if (key == SDLK_RETURN || key == SDLK_SPACE) {
                    GameState state = capture_state(player, world, map_seed,
                                                    play_time_seconds, time_system,
                                                    rng.state());
                    if (save_game(pause_cursor, state)) {
                        log_msg("Game saved to slot " + std::to_string(pause_cursor + 1));
                    } else {
                        log_msg("Save failed!");
                    }
                    mode = GameMode::Normal;
                }
                continue;
            }

            // --- Load game screen ---
            if (mode == GameMode::LoadGame) {
                if (key == SDLK_ESCAPE) {
                    if (!load_from_menu) pause_cursor = 0;
                    mode = load_from_menu ? GameMode::MainMenu : GameMode::PauseMenu;
                    continue;
                }
                int max_slot = MAX_SAVE_SLOTS - 1;
                if (key == SDLK_UP && pause_cursor > 0) pause_cursor--;
                if (key == SDLK_DOWN && pause_cursor < max_slot) pause_cursor++;
                if (key == SDLK_RETURN || key == SDLK_SPACE) {
                    GameState state;
                    if (load_game(pause_cursor, state)) {
                        // Restore world with saved seed
                        map_seed = state.map_seed;
                        world = World();
                        world.init(map_seed);
                        world.set_load_radius(settings.load_radius);
                        world.set_render_radius(settings.render_radius);
                        world.set_simulation_radius(settings.sim_radius);

                        rng.set_state(state.rng_state);

                        // Restore player. Stats and equipment go in before HP
                        // so max_hp is final when the saved HP is clamped.
                        player = Player();
                        player.spawn(state.player_x, state.player_y);
                        player.stats() = state.stats;
                        player.set_xp(state.xp);
                        player.set_level(state.level);
                        player.set_gold(state.gold);

                        for (const auto& [item, qty] : state.inventory) {
                            player.add_item(item, qty);
                        }
                        for (const auto& [slot, item] : state.equipment) {
                            player.equip_to_slot(slot, item);
                        }
                        player.set_hp(state.hp);

                        world.clear_zones();
                        for (const auto& z : state.zones) world.add_zone(z);

                        // Stage saved terrain deltas and entities before any
                        // chunk is generated, so loading a chunk replays the
                        // save instead of spawning a fresh population that
                        // then has to be cleared.
                        apply_chunk_data(world, state.chunks);
                        world.update_loaded_chunks(player.x(), player.y());

                        // Reset UI state
                        turn_counter = 0;
                        play_time_seconds = state.meta.play_time_seconds;
                        time_system.set_turn_of_day(state.turn_of_day);
                        time_system.set_day(state.day);
                        msg_log.clear();
                        discovered_structures.clear();
                        zone_anchored = false;
                        zone_pick_type = false;
                        need_fov_update = true;
                        game_started = true;
                        log_msg("Game loaded from slot " + std::to_string(pause_cursor + 1));
                        mode = GameMode::Normal;
                    } else {
                        log_msg("No save in slot " + std::to_string(pause_cursor + 1));
                        mode = load_from_menu ? GameMode::MainMenu : GameMode::Normal;
                    }
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
                        // DESIGN.md affinity thresholds: gifts need > Wary,
                        // trade needs > Friendly on top of being a merchant.
                        case DialogueAction::Trade:
                            if (npc.can_trade()) {
                                trade_open(trade_state, &npc);
                            } else if (npc.is_merchant()) {
                                log_msg(npc.name() + " doesn't trust you enough to trade.");
                            }
                            break;
                        case DialogueAction::Gift:
                            if (npc.can_gift()) {
                                gift_cursor = 0;
                                gift_return = GameMode::Dialogue;
                                mode = GameMode::GiftSelect;
                            } else {
                                log_msg(npc.name() + " doesn't want your gifts yet.");
                            }
                            break;
                        default: break;
                    }
                }
                continue;
            }

            // --- Gift select ---
            if (mode == GameMode::GiftSelect) {
                if (key == SDLK_ESCAPE) { mode = gift_return; continue; }
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
                        log_msg("No one nearby to gift to.");
                        mode = GameMode::Normal;
                    } else {
                        Item item = player.inventory_item(gift_cursor);
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
                        log_msg(target_npc->name() + ": \"" + target_npc->gift_reaction_text(reaction) + "\"", 150);
                        mode = GameMode::Normal;
                        player_took_turn = true;
                    }
                }
                continue;
            }

            // --- Craft mode ---
            if (mode == GameMode::Craft) {
                if (key == SDLK_ESCAPE || key == SDLK_c) { mode = GameMode::Normal; continue; }
                const auto& recipes = recipe_db();
                int max_idx = static_cast<int>(recipes.size()) - 1;
                if (key == SDLK_UP && craft_cursor > 0) craft_cursor--;
                if (key == SDLK_DOWN && craft_cursor < max_idx) craft_cursor++;
                if (key == SDLK_RETURN || key == SDLK_SPACE) {
                    const Recipe& r = recipes[craft_cursor];
                    if (craft_can_make(r, player)) {
                        log_msg(craft_make(r, player));
                        player_took_turn = true;
                    } else {
                        log_msg("Missing materials.");
                    }
                }
                continue;
            }

            // --- Build mode ---
            if (mode == GameMode::Build) {
                if (key == SDLK_ESCAPE || key == SDLK_b) { mode = GameMode::Normal; continue; }
                int dx = 0, dy = 0;
                switch (key) {
                    case SDLK_w: case SDLK_UP:    dy = -1; break;
                    case SDLK_s: case SDLK_DOWN:  dy =  1; break;
                    case SDLK_a: case SDLK_LEFT:  dx = -1; break;
                    case SDLK_d: case SDLK_RIGHT: dx =  1; break;
                    default: break;
                }
                if (dx != 0 || dy != 0) {
                    int nx = build_cx + dx;
                    int ny = build_cy + dy;
                    if (std::abs(nx - player.x()) <= BUILD_RANGE && std::abs(ny - player.y()) <= BUILD_RANGE) {
                        build_cx = nx;
                        build_cy = ny;
                    }
                }
                if (key == SDLK_TAB || key == SDLK_RIGHTBRACKET) {
                    build_sel = (build_sel + 1) % static_cast<int>(BuildTile::Count);
                }
                if (key == SDLK_LEFTBRACKET) {
                    build_sel = (build_sel + static_cast<int>(BuildTile::Count) - 1) % static_cast<int>(BuildTile::Count);
                }
                if (key == SDLK_RETURN || key == SDLK_SPACE) {
                    BuildTile bt = static_cast<BuildTile>(build_sel);
                    // Don't wall yourself in
                    const BuildDef& def = build_def(bt);
                    if (!def.is_light_object && tile_blocks_movement(def.tile) &&
                        build_cx == player.x() && build_cy == player.y()) {
                        log_msg("Can't build a wall under yourself.");
                    } else {
                        std::string result = build_place(world, player, bt, build_cx, build_cy);
                        log_msg(result);
                        if (result.rfind("Built", 0) == 0) {
                            player_took_turn = true;
                            need_fov_update = true;
                        }
                    }
                }
                continue;
            }

            // --- Zone mode ---
            if (mode == GameMode::Zone) {
                if (key == SDLK_ESCAPE || key == SDLK_z) {
                    if (zone_pick_type) { zone_pick_type = false; zone_anchored = false; }
                    else if (zone_anchored) { zone_anchored = false; }
                    else { mode = GameMode::Normal; }
                    continue;
                }
                if (zone_pick_type) {
                    int nt = static_cast<int>(ZoneType::Count);
                    if (key == SDLK_TAB || key == SDLK_DOWN || key == SDLK_RIGHT) zone_type_sel = (zone_type_sel + 1) % nt;
                    if (key == SDLK_UP || key == SDLK_LEFT) zone_type_sel = (zone_type_sel + nt - 1) % nt;
                    if (key == SDLK_RETURN || key == SDLK_SPACE) {
                        Zone z;
                        z.x0 = std::min(zone_ax, zone_cx);
                        z.y0 = std::min(zone_ay, zone_cy);
                        z.x1 = std::max(zone_ax, zone_cx);
                        z.y1 = std::max(zone_ay, zone_cy);
                        z.type = static_cast<ZoneType>(zone_type_sel);
                        world.add_zone(z);
                        log_msg(std::string("Designated ") + zone_type_name(z.type) + " zone.");
                        zone_anchored = false;
                        zone_pick_type = false;
                    }
                    continue;
                }
                int dx = 0, dy = 0;
                switch (key) {
                    case SDLK_w: case SDLK_UP:    dy = -1; break;
                    case SDLK_s: case SDLK_DOWN:  dy =  1; break;
                    case SDLK_a: case SDLK_LEFT:  dx = -1; break;
                    case SDLK_d: case SDLK_RIGHT: dx =  1; break;
                    default: break;
                }
                if (dx != 0 || dy != 0) { zone_cx += dx; zone_cy += dy; }
                if (key == SDLK_x) {
                    if (world.remove_zone_at(zone_cx, zone_cy)) {
                        log_msg("Zone removed.");
                        if (zone_anchored) { zone_anchored = false; }
                    }
                }
                if (key == SDLK_RETURN || key == SDLK_SPACE) {
                    if (!zone_anchored) {
                        zone_ax = zone_cx;
                        zone_ay = zone_cy;
                        zone_anchored = true;
                    } else {
                        zone_pick_type = true;
                        zone_type_sel = 0;
                    }
                }
                continue;
            }

            // --- Character sheet ---
            if (mode == GameMode::Character) {
                if (key == SDLK_ESCAPE || key == SDLK_x || key == SDLK_RETURN || key == SDLK_SPACE) {
                    mode = GameMode::Normal;
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
                    // A copy, not a reference: equipping, dropping and using
                    // all remove the stack from the inventory, and the item is
                    // still read for the status message afterwards.
                    Item item = player.inventory_item(inv_cursor);
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
                                log_msg("Used " + item.name() + ". Healed " + std::to_string(item.use_amount()) + " HP.");
                            } else if (item.use_effect() == UseEffect::Feed) {
                                log_msg("Ate " + item.name() + ".");
                            } else if (item.use_effect() == UseEffect::Drink) {
                                log_msg("Drank " + item.name() + ".");
                            }
                            player.remove_item(inv_cursor, 1);
                            mode = GameMode::Inventory;
                            if (inv_cursor >= static_cast<int>(player.inventory().size()))
                                inv_cursor = std::max(0, static_cast<int>(player.inventory().size()) - 1);
                        } else if (action == 1) {
                            // Equip
                            EquipSlot slot = item.equip_slot();
                            if (player.is_slot_occupied(slot)) player.unequip(slot);
                            player.equip(inv_cursor);
                            log_msg("Equipped " + item.name() + ".", 60);
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
                            log_msg("Dropped " + item.name() + ".", 60);
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
                            gift_return = GameMode::Inventory;
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
                            log_msg("Unequipped.", 60);
                        }
                    }
                }
                continue;
            }

            // --- World map mode ---
            if (mode == GameMode::Map) {
                if (key == SDLK_ESCAPE || key == SDLK_m) {
                    mode = GameMode::Normal;
                    continue;
                }
                int mdx = 0, mdy = 0;
                switch (key) {
                    case SDLK_w: case SDLK_UP:    mdy = -1; break;
                    case SDLK_s: case SDLK_DOWN:  mdy =  1; break;
                    case SDLK_a: case SDLK_LEFT:  mdx = -1; break;
                    case SDLK_d: case SDLK_RIGHT: mdx =  1; break;
                    default: break;
                }
                if (mdx != 0 || mdy != 0) {
                    int pcx = World::world_to_chunk_x(player.x());
                    int pcy = World::world_to_chunk_y(player.y());
                    int ncx = map_cursor_cx + mdx;
                    int ncy = map_cursor_cy + mdy;
                    // Keep cursor within the visible map radius
                    if (std::abs(ncx - pcx) <= MAP_RADIUS && std::abs(ncy - pcy) <= MAP_RADIUS) {
                        map_cursor_cx = ncx;
                        map_cursor_cy = ncy;
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
                if (key == SDLK_c) {
                    mode = GameMode::Craft;
                    craft_cursor = 0;
                    continue;
                }
                if (key == SDLK_b) {
                    mode = GameMode::Build;
                    build_cx = player.x();
                    build_cy = player.y();
                    continue;
                }
                if (key == SDLK_z) {
                    mode = GameMode::Zone;
                    zone_cx = player.x();
                    zone_cy = player.y();
                    zone_anchored = false;
                    zone_pick_type = false;
                    continue;
                }
                if (key == SDLK_x) {
                    mode = GameMode::Character;
                    continue;
                }
                if (key == SDLK_m) {
                    mode = GameMode::Map;
                    map_cursor_cx = World::world_to_chunk_x(player.x());
                    map_cursor_cy = World::world_to_chunk_y(player.y());
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
                                log_msg("Too heavy to pick up " + std::string(e->name()) + "!");
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
                                log_msg("Picked up " + picked[0], 60);
                            } else {
                                log_msg("Picked up " + std::to_string(picked.size()) + " items", 60);
                            }
                        }
                        break;
                    }
                    case SDLK_RETURN: case SDLK_SPACE: {
                        const int dirs[][2] = {{0,-1},{0,1},{-1,0},{1,0}};
                        for (auto [ddx, ddy] : dirs) {
                            Npc* n = world.npc_at(player.x() + ddx, player.y() + ddy);
                            if (!n) continue;
                            // DESIGN.md: hostile NPCs (affinity <= 20) refuse
                            // to talk at all.
                            if (!n->can_talk()) {
                                log_msg(n->name() + " turns away from you.");
                                break;
                            }
                            dialogue_npc_x = n->x();
                            dialogue_npc_y = n->y();
                            dialogue_node = 0;
                            dialogue_option = 0;
                            mode = GameMode::Dialogue;
                            break;
                        }
                        break;
                    }
                    default: break;
                }

                // Movement / combat / gathering
                if (dx != 0 || dy != 0) {
                    int nx = player.x() + dx;
                    int ny = player.y() + dy;

                    if (player.can_move(dx, dy, world)) {
                        // Check for enemy at destination (combat)
                        bool fought = false;
                        Enemy* target = world.enemy_at(nx, ny);
                        if (target) {
                            int raw_atk = player.total_attack() +
                                          rng.variance(player.total_damage_variance());
                            int dmg = std::max(1, raw_atk - target->defense());
                            target->take_damage(dmg);
                            std::string combat_msg = "You hit " + target->name() +
                                                     " for " + std::to_string(dmg) + " damage!";
                            fought = true;

                            if (!target->is_alive()) {
                                combat_msg += " " + target->name() + " killed! +" +
                                              std::to_string(target->xp_value()) + " XP";
                                player.add_xp(target->xp_value());
                                // Read everything off the enemy before the
                                // spawn/remove calls invalidate the pointer.
                                int drop_x = target->x(), drop_y = target->y();
                                auto drops = target->drops();
                                world.remove_enemy_at(nx, ny);
                                target = nullptr;
                                for (const auto& [item, qty] : drops) {
                                    for (int n = 0; n < qty; ++n) {
                                        Entity e(drop_x, drop_y, item.glyph(), item.name(),
                                                 item.fg_r(), item.fg_g(), item.fg_b());
                                        e.set_is_item(true);
                                        world.spawn_item(std::move(e));
                                    }
                                }
                            }
                            log_msg(combat_msg, 60);
                        }

                        if (!fought) {
                            player.move(dx, dy, world);
                            world.update_loaded_chunks(player.x(), player.y());
                            need_fov_update = true;
                            check_discovery();
                        }
                        player_took_turn = true;
                    } else {
                        // Blocked — try resource gathering
                        Tile dest = world.get_tile(nx, ny);
                        if (dest.type == TileType::Tree) {
                            int yield = has_equipped("Woodcutter's Axe") ? 2 : 1;
                            const Item* wood = find_item("Wood");
                            if (wood) player.add_item(*wood, yield);
                            world.place_tile(nx, ny, TileType::Grass);
                            log_msg("You chop down the tree. +" + std::to_string(yield) + " Wood");
                            player_took_turn = true;
                            need_fov_update = true;
                        } else if (dest.type == TileType::Mountain) {
                            bool pick = has_equipped("Pickaxe");
                            int yield = pick ? 2 : 1;
                            int ore_chance = pick ? 30 : 15;
                            const Item* stone = find_item("Stone");
                            if (stone) player.add_item(*stone, yield);
                            std::string m = "You mine the rock face. +" + std::to_string(yield) + " Stone";
                            if (rng.percent(ore_chance)) {
                                const Item* ore = find_item("Iron Ore");
                                if (ore) { player.add_item(*ore, 1); m += ", +1 Iron Ore"; }
                            }
                            world.place_tile(nx, ny, TileType::Stone);
                            log_msg(m);
                            player_took_turn = true;
                            need_fov_update = true;
                        }
                    }
                }
            }
        }

        if (!game_started) {
            // ── Main menu render ───────────────────────────────────
            renderer.clear();
            int view_cols = window.width() / renderer.cell_width();
            int view_rows = window.height() / renderer.cell_height();
            renderer.begin_frame(view_cols, view_rows);

            // Decorative border
            for (int x = 0; x < view_cols; ++x) {
                Cell c; c.glyph = '='; c.fg_r = 60; c.fg_g = 60; c.fg_b = 80;
                renderer.set_cell(x, 1, c);
                renderer.set_cell(x, view_rows - 2, c);
            }

            std::string title = "A S C I I   G A M E";
            draw_string(renderer, (view_cols - title.size()) / 2, view_rows / 2 - 8,
                        title, 220, 200, 120);

            std::string sub = "A fantasy life in letters";
            draw_string(renderer, (view_cols - sub.size()) / 2, view_rows / 2 - 6,
                        sub, 120, 120, 140);

            std::string opts[] = {"New Game", "Load Game", "Quit"};
            for (int i = 0; i < 3; ++i) {
                bool sel = (i == menu_cursor);
                std::string s = (sel ? "> " : "  ") + opts[i];
                draw_string(renderer, (view_cols - 12) / 2, view_rows / 2 - 2 + i * 2,
                            s, sel ? 255 : 160, sel ? 255 : 160, sel ? 100 : 160);
            }

            // Message flash (e.g. failed load)
            if (!msg_log.empty()) {
                const Msg& m = msg_log.back();
                draw_string(renderer, (view_cols - m.text.size()) / 2, view_rows / 2 + 6,
                            m.text, 255, 200, 100);
            }

            std::string hint1 = "WASD move  |  I inventory  |  C craft  |  B build";
            std::string hint2 = "Z zones  |  X stats  |  G grab  |  . wait  |  ESC pause";
            draw_string(renderer, (view_cols - hint1.size()) / 2, view_rows - 5,
                        hint1, 80, 80, 100);
            draw_string(renderer, (view_cols - hint2.size()) / 2, view_rows - 4,
                        hint2, 80, 80, 100);

            renderer.render_grid();
            renderer.present();
            SDL_Delay(16);
            continue;
        }

        if (need_fov_update) {
            // Compute light levels first so we know torch radius
            uint8_t ambient = time_system.ambient_light();
            int torch_r = player.has_light_source() ? player.torch_radius() : 0;
            light::compute(world, player.x(), player.y(), ambient, torch_r);

            // PLAN.md 4B: base radius 8, WIS bonus (wis - 10) / 2, then scaled
            // by ambient light so sight shrinks at dusk and night.
            int base_radius = FOV_BASE_RADIUS + (player.stats().perception() - 10) / 2;
            base_radius = std::max(3, base_radius);
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
            TurnOutcome outcome = resolve_turn(world, player, time_system, rng);
            for (const auto& message : outcome.messages) log_msg(message);
            if (outcome.player_died) mode = GameMode::Dead;
            // Time advanced, so ambient light and every actor position may
            // have changed. Recompute rather than relying on each action
            // remembering to ask for it.
            need_fov_update = true;
        }

        // Play time tracking
        play_time_timer++;
        if (play_time_timer >= 60) { play_time_timer = 0; play_time_seconds++; }
        // Message timers
        for (auto& m : msg_log) m.timer--;
        while (!msg_log.empty() && msg_log.front().timer <= 0)
            msg_log.erase(msg_log.begin());

        // --- Render ---
        renderer.clear();
        int view_cols = window.width() / renderer.cell_width();
        int view_rows = window.height() / renderer.cell_height();
        renderer.begin_frame(view_cols, view_rows);

        int cam_x = player.x() - view_cols / 2;
        int cam_y = player.y() - (view_rows - 2) / 2; // 2 rows reserved for HUD
        // Unbounded camera — no map edge clamping

        // Tiles
        for (int vy = 0; vy < view_rows - 2; ++vy) {
            for (int vx = 0; vx < view_cols; ++vx) {
                int mx = cam_x + vx;
                int my = cam_y + vy;
                Cell cell;
                cell.glyph = ' ';

                if (world.in_bounds(mx, my)) {
                    Tile tile = world.get_tile(mx, my);
                    // PLAN.md 4B: visible tiles are shaded by light level, so
                    // a lit room reads differently from one lit only by the
                    // player's torch. Explored-but-unseen stays dim.
                    float shade = 1.0f;
                    if (tile.visible) {
                        cell.glyph = tile_glyph(tile.type);
                        tile_color(tile.type, cell.fg_r, cell.fg_g, cell.fg_b);
                        shade = light_shade(tile.light_level);
                    } else if (tile.explored) {
                        cell.glyph = tile_glyph(tile.type);
                        tile_color(tile.type, cell.fg_r, cell.fg_g, cell.fg_b);
                        shade = 0.3f;
                    }
                    if (shade < 1.0f) {
                        cell.fg_r = static_cast<uint8_t>(cell.fg_r * shade);
                        cell.fg_g = static_cast<uint8_t>(cell.fg_g * shade);
                        cell.fg_b = static_cast<uint8_t>(cell.fg_b * shade);
                    }

                    // Zone tint (subtle, always shown on seen tiles)
                    if (tile.visible || tile.explored) {
                        const Zone* zone = world.zone_at(mx, my);
                        if (zone) {
                            uint8_t zr, zg, zb;
                            zone_type_color(zone->type, zr, zg, zb);
                            float f = tile.visible ? 0.35f : 0.15f;
                            cell.bg_r = static_cast<uint8_t>(zr * f);
                            cell.bg_g = static_cast<uint8_t>(zg * f);
                            cell.bg_b = static_cast<uint8_t>(zb * f);
                        }
                    }

                    if (tile.visible) {
                        // Occupants draw over terrain, in increasing priority:
                        // placed object, ground item, NPC, enemy. Actors keep
                        // their full colour so they stay readable in the dark.
                        const PlacedObject* po = world.placed_object_at(mx, my);
                        if (po) {
                            cell.glyph = po->glyph;
                            cell.fg_r = po->fg_r; cell.fg_g = po->fg_g; cell.fg_b = po->fg_b;
                        }
                        Entity* item = world.item_at(mx, my);
                        if (item && !item->in_inventory()) {
                            cell.glyph = item->glyph();
                            cell.fg_r = item->fg_r(); cell.fg_g = item->fg_g(); cell.fg_b = item->fg_b();
                        }
                        Npc* npc = world.npc_at(mx, my);
                        if (npc) {
                            cell.glyph = npc->glyph();
                            cell.fg_r = npc->fg_r(); cell.fg_g = npc->fg_g(); cell.fg_b = npc->fg_b();
                        }
                        Enemy* enemy = world.enemy_at(mx, my);
                        if (enemy) {
                            cell.glyph = enemy->glyph();
                            cell.fg_r = enemy->fg_r(); cell.fg_g = enemy->fg_g(); cell.fg_b = enemy->fg_b();
                        }
                    }
                }

                if (mx == player.x() && my == player.y()) {
                    cell.glyph = '@';
                    cell.fg_r = 0; cell.fg_g = 255; cell.fg_b = 0;
                }

                renderer.set_cell(vx, vy, cell);
            }
        }

        // Build mode cursor + selection preview
        if (mode == GameMode::Build) {
            int sx = build_cx - cam_x;
            int sy = build_cy - cam_y;
            if (sx >= 0 && sx < view_cols && sy >= 0 && sy < view_rows - 2) {
                BuildTile bt = static_cast<BuildTile>(build_sel);
                const BuildDef& def = build_def(bt);
                Cell c;
                c.glyph = def.glyph;
                bool ok = build_can_afford(bt, player);
                std::string reason;
                ok = ok && build_can_place(bt, world, build_cx, build_cy, reason);
                if (ok) { c.fg_r = 120; c.fg_g = 255; c.fg_b = 120; }
                else    { c.fg_r = 255; c.fg_g = 80;  c.fg_b = 80; }
                c.bg_r = 60; c.bg_g = 60; c.bg_b = 20;
                renderer.set_cell(sx, sy, c);
            }
        }

        // Zone mode selection preview
        if (mode == GameMode::Zone) {
            uint8_t zr, zg, zb;
            zone_type_color(static_cast<ZoneType>(zone_type_sel), zr, zg, zb);
            if (zone_anchored) {
                int x0 = std::min(zone_ax, zone_cx);
                int y0 = std::min(zone_ay, zone_cy);
                int x1 = std::max(zone_ax, zone_cx);
                int y1 = std::max(zone_ay, zone_cy);
                for (int yy = y0; yy <= y1; ++yy) {
                    for (int xx = x0; xx <= x1; ++xx) {
                        int sx = xx - cam_x;
                        int sy = yy - cam_y;
                        if (sx < 0 || sx >= view_cols || sy < 0 || sy >= view_rows - 2) continue;
                        Cell c;
                        c.glyph = ' ';
                        c.bg_r = static_cast<uint8_t>(zr * 0.5f);
                        c.bg_g = static_cast<uint8_t>(zg * 0.5f);
                        c.bg_b = static_cast<uint8_t>(zb * 0.5f);
                        renderer.set_cell(sx, sy, c);
                    }
                }
            }
            int sx = zone_cx - cam_x;
            int sy = zone_cy - cam_y;
            if (sx >= 0 && sx < view_cols && sy >= 0 && sy < view_rows - 2) {
                Cell c;
                c.glyph = '+';
                c.fg_r = 255; c.fg_g = 255; c.fg_b = 255;
                c.bg_r = static_cast<uint8_t>(zr * 0.6f);
                c.bg_g = static_cast<uint8_t>(zg * 0.6f);
                c.bg_b = static_cast<uint8_t>(zb * 0.6f);
                renderer.set_cell(sx, sy, c);
            }
        }

        // ── HUD (2 rows) ───────────────────────────────────────────
        {
            int hud_y1 = view_rows - 2;
            int hud_y2 = view_rows - 1;
            for (int x = 0; x < view_cols; ++x) {
                Cell c; c.glyph = ' ';
                c.bg_r = 20; c.bg_g = 20; c.bg_b = 30;
                renderer.set_cell(x, hud_y1, c);
                renderer.set_cell(x, hud_y2, c);
            }
            std::string hud1 = "HP:" + std::to_string(player.hp()) + "/" + std::to_string(player.max_hp()) +
                               " ATK:" + std::to_string(player.total_attack() - player.total_damage_variance()) + "-" + std::to_string(player.total_attack() + player.total_damage_variance()) +
                               " DEF:" + std::to_string(player.total_defense()) +
                               " G:" + std::to_string(player.gold()) +
                               " Wt:" + std::to_string(player.current_weight()) + "/" + std::to_string(player.carry_capacity()) +
                               " Lv:" + std::to_string(player.level()) +
                               " XP:" + std::to_string(player.xp()) + "/" + std::to_string(player.xp_to_next_level());
            std::string hud2 = "Day " + std::to_string(time_system.day()) +
                               " " + time_system.time_string() + " " + time_system.time_period_string();
            // Biome indicator
            BiomeType biome = world.get_biome_at(player.x(), player.y());
            hud2 += " [" + std::string(biome_name(biome)) + "]";
            // Structure indicator
            StructureType structure = world.get_structure_at(player.x(), player.y());
            if (structure != StructureType::None) {
                const StructureDef& sd = structure_def(structure);
                hud2 += " " + std::string(sd.name);
            }
            if (player.has_light_source()) hud2 += " [Torch]";
            draw_string(renderer, 1, hud_y1, hud1, 180, 180, 180, 20, 20, 30);
            draw_string(renderer, 1, hud_y2, hud2, 140, 140, 160, 20, 20, 30);
        }

        // ── Message log (above HUD, newest at bottom) ──────────────
        {
            int base_y = view_rows - 3;
            int i = 0;
            for (auto it = msg_log.rbegin(); it != msg_log.rend() && i < 4; ++it, ++i) {
                int y = base_y - i;
                uint8_t bright = static_cast<uint8_t>(255 - i * 50);
                uint8_t bgv = static_cast<uint8_t>(40 - i * 8);
                for (int x = 0; x < static_cast<int>(it->text.size()) + 2 && x < view_cols; ++x) {
                    Cell c; c.glyph = ' ';
                    c.bg_r = bgv; c.bg_g = bgv; c.bg_b = static_cast<uint8_t>(bgv / 2);
                    renderer.set_cell(x, y, c);
                }
                draw_string(renderer, 1, y, it->text, bright, bright, static_cast<uint8_t>(100 + i * 10),
                            bgv, bgv, static_cast<uint8_t>(bgv / 2));
            }
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

            draw_box(renderer, box_x, box_y, box_w, box_h, 15, 15, 35);

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
                draw_box(renderer, ex_x, ex_y, ex_w, ex_h, 25, 25, 45);
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
                draw_box(renderer, gx, gy, gw, gh, 30, 20, 20);
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

        // Craft overlay
        if (mode == GameMode::Craft) {
            const auto& recipes = recipe_db();
            int box_w = 56;
            if (box_w > view_cols - 4) box_w = view_cols - 4;
            int box_h = static_cast<int>(recipes.size()) + 5;
            if (box_h > view_rows - 6) box_h = view_rows - 6;
            int box_x = (view_cols - box_w) / 2;
            int box_y = 2;
            draw_box(renderer, box_x, box_y, box_w, box_h, 15, 25, 20);

            draw_string(renderer, box_x + 1, box_y, "CRAFTING", 255, 255, 255, 15, 25, 20);
            int max_visible = box_h - 4;
            int scroll = 0;
            if (craft_cursor >= max_visible) scroll = craft_cursor - max_visible + 1;

            for (int i = 0; i < max_visible && (i + scroll) < static_cast<int>(recipes.size()); ++i) {
                int idx = i + scroll;
                const Recipe& r = recipes[idx];
                int y = box_y + 2 + i;
                bool selected = (idx == craft_cursor);
                bool can = craft_can_make(r, player);
                uint8_t bgr = 15, bgg = 25, bgb = 20;
                if (selected) { bgr = 35; bgg = 55; bgb = 40; }

                std::string name = std::string(r.result);
                if (r.result_qty > 1) name += " x" + std::to_string(r.result_qty);
                uint8_t fr = can ? 200 : 90, fg = can ? 255 : 90, fb = can ? 200 : 90;
                if (!can) { fr = 110; fg = 110; fb = 110; }
                draw_string(renderer, box_x + 2, y, name, fr, fg, fb, bgr, bgg, bgb);

                // Ingredients: "Wood 1/2, Iron Ore 0/2"
                std::string ing;
                for (size_t j = 0; j < r.ingredients.size(); ++j) {
                    if (j > 0) ing += ", ";
                    ing += std::string(r.ingredients[j].first) + " " +
                           std::to_string(craft_count_of(player, r.ingredients[j].first)) + "/" +
                           std::to_string(r.ingredients[j].second);
                }
                draw_string(renderer, box_x + 24, y, ing, 160, 160, 140, bgr, bgg, bgb);
            }
            draw_string(renderer, box_x + 1, box_y + box_h - 1, "[ENTER] craft  [ESC] close", 100, 100, 100, 15, 25, 20);
        }

        // Build overlay
        if (mode == GameMode::Build) {
            int box_w = 34;
            int box_h = static_cast<int>(BuildTile::Count) + 4;
            int box_x = 1, box_y = 1;
            draw_box(renderer, box_x, box_y, box_w, box_h, 25, 20, 15);
            draw_string(renderer, box_x + 1, box_y, "BUILD", 255, 255, 255, 25, 20, 15);

            for (int i = 0; i < static_cast<int>(BuildTile::Count); ++i) {
                BuildTile bt = static_cast<BuildTile>(i);
                const BuildDef& def = build_def(bt);
                int y = box_y + 2 + i;
                bool selected = (i == build_sel);
                bool can = build_can_afford(bt, player);
                uint8_t bgr = 25, bgg = 20, bgb = 15;
                if (selected) { bgr = 55; bgg = 45; bgb = 30; }

                Cell gc; gc.glyph = def.glyph;
                gc.fg_r = can ? 220 : 100; gc.fg_g = can ? 180 : 100; gc.fg_b = can ? 100 : 100;
                gc.bg_r = bgr; gc.bg_g = bgg; gc.bg_b = bgb;
                renderer.set_cell(box_x + 1, y, gc);

                draw_string(renderer, box_x + 3, y, def.name, can ? 220 : 110, can ? 220 : 110, can ? 220 : 110, bgr, bgg, bgb);

                // Material cost "2/1"
                std::string cost;
                for (size_t j = 0; j < def.materials.size(); ++j) {
                    if (j > 0) cost += ",";
                    int have = 0;
                    for (const auto& [item, qty] : player.inventory())
                        if (item.name() == def.materials[j].first) have += qty;
                    cost += " " + std::to_string(have) + "/" + std::to_string(def.materials[j].second);
                }
                draw_string(renderer, box_x + 17, y, cost, 150, 150, 130, bgr, bgg, bgb);
            }
            draw_string(renderer, box_x + 1, box_y + box_h - 1, "[TAB] pick [ENT] build", 100, 100, 100, 25, 20, 15);
        }

        // Zone overlay
        if (mode == GameMode::Zone) {
            int box_w = 30;
            int box_h = 6 + static_cast<int>(world.zones().size());
            if (box_h > 14) box_h = 14;
            int box_x = 1, box_y = 1;
            draw_box(renderer, box_x, box_y, box_w, box_h, 20, 20, 30);
            draw_string(renderer, box_x + 1, box_y, "ZONES", 255, 255, 255, 20, 20, 30);

            if (zone_pick_type) {
                draw_string(renderer, box_x + 1, box_y + 2, "Zone type:", 180, 180, 180, 20, 20, 30);
                for (int i = 0; i < static_cast<int>(ZoneType::Count); ++i) {
                    uint8_t zr, zg, zb;
                    zone_type_color(static_cast<ZoneType>(i), zr, zg, zb);
                    bool sel = (i == zone_type_sel);
                    std::string s = (sel ? "> " : "  ") + std::string(zone_type_name(static_cast<ZoneType>(i)));
                    draw_string(renderer, box_x + 2, box_y + 3 + i, s,
                                sel ? 255 : zr + 80, sel ? 255 : zg + 80, sel ? 100 : zb + 80,
                                20, 20, 30);
                }
            } else {
                draw_string(renderer, box_x + 1, box_y + 2,
                            zone_anchored ? "Pick second corner" : "Pick first corner",
                            150, 150, 170, 20, 20, 30);
                int y = box_y + 4;
                for (const auto& z : world.zones()) {
                    if (y >= box_y + box_h - 1) break;
                    uint8_t zr, zg, zb;
                    zone_type_color(z.type, zr, zg, zb);
                    std::string s = std::string(zone_type_name(z.type)) + " (" +
                                    std::to_string(z.x1 - z.x0 + 1) + "x" +
                                    std::to_string(z.y1 - z.y0 + 1) + ")";
                    draw_string(renderer, box_x + 2, y, s, zr + 100, zg + 100, zb + 100, 20, 20, 30);
                    ++y;
                }
                if (world.zones().empty())
                    draw_string(renderer, box_x + 2, y, "(none)", 90, 90, 90, 20, 20, 30);
            }
            draw_string(renderer, box_x + 1, box_y + box_h - 1, "[ENT] mark [X] del", 100, 100, 100, 20, 20, 30);
        }

        // Character sheet overlay
        if (mode == GameMode::Character) {
            int box_w = 34, box_h = 20;
            int box_x = (view_cols - box_w) / 2;
            int box_y = (view_rows - box_h) / 2;
            draw_box(renderer, box_x, box_y, box_w, box_h, 20, 20, 40);
            draw_string(renderer, box_x + 1, box_y, "CHARACTER", 255, 255, 255, 20, 20, 40);
            std::string lv = "Level " + std::to_string(player.level()) +
                             "  XP " + std::to_string(player.xp()) + "/" + std::to_string(player.xp_to_next_level());
            draw_string(renderer, box_x + 1, box_y + 1, lv, 180, 180, 180, 20, 20, 40);

            const PlayerStats& st = player.stats();
            auto row = [&](int i, const char* name, int val, const std::string& note) {
                std::string s = std::string(name) + " " + std::to_string(val);
                draw_string(renderer, box_x + 2, box_y + 3 + i, s, 200, 200, 200, 20, 20, 40);
                draw_string(renderer, box_x + 12, box_y + 3 + i, note, 120, 120, 150, 20, 20, 40);
            };
            row(0, "STR", st.str, "carry " + std::to_string(st.carry_capacity()));
            row(1, "DEX", st.dex, "defense +" + std::to_string(st.defense_bonus()));
            row(2, "CON", st.con, "max HP " + std::to_string(st.max_hp()));
            row(3, "INT", st.intel, "arcana");
            row(4, "WIS", st.wis, "perception " + std::to_string(st.perception()));
            row(5, "CHA", st.cha, "social " + std::to_string(st.social_modifier()));

            std::string atk = "Attack:  " + std::to_string(player.total_attack() - player.total_damage_variance()) +
                              "-" + std::to_string(player.total_attack() + player.total_damage_variance());
            std::string def = "Defense: " + std::to_string(player.total_defense());
            std::string hp  = "HP:      " + std::to_string(player.hp()) + "/" + std::to_string(player.max_hp());
            std::string au  = "Gold:    " + std::to_string(player.gold());
            std::string wt  = "Weight:  " + std::to_string(player.current_weight()) + "/" + std::to_string(player.carry_capacity());
            draw_string(renderer, box_x + 2, box_y + 10, atk, 220, 180, 140, 20, 20, 40);
            draw_string(renderer, box_x + 2, box_y + 11, def, 160, 200, 160, 20, 20, 40);
            draw_string(renderer, box_x + 2, box_y + 12, hp, 220, 140, 140, 20, 20, 40);
            draw_string(renderer, box_x + 2, box_y + 13, au, 255, 215, 0, 20, 20, 40);
            draw_string(renderer, box_x + 2, box_y + 14, wt, 160, 160, 160, 20, 20, 40);

            draw_string(renderer, box_x + 1, box_y + box_h - 1, "[X/ESC] close", 100, 100, 100, 20, 20, 40);
        }

        // World map overlay
        if (mode == GameMode::Map) {
            const int R = MAP_RADIUS;
            const int grid = 2 * R + 1;   // 21x21 chunks
            const int legend_w = 24;
            int box_w = grid + 2 + legend_w;
            int box_h = grid + 2 + 2;     // title + grid + status
            int box_x = (view_cols - box_w) / 2;
            int box_y = (view_rows - box_h) / 2;
            if (box_x < 0) box_x = 0;
            if (box_y < 1) box_y = 1;

            int pcx = World::world_to_chunk_x(player.x());
            int pcy = World::world_to_chunk_y(player.y());

            // Rebuild the chunk cache when the player moves to a new chunk
            if (map_cache.size() != static_cast<size_t>(grid * grid) ||
                map_cache_cx != pcx || map_cache_cy != pcy) {
                map_cache.resize(grid * grid);
                map_cache_cx = pcx;
                map_cache_cy = pcy;
                for (int dy = -R; dy <= R; ++dy)
                    for (int dx = -R; dx <= R; ++dx)
                        map_cache[(dy + R) * grid + (dx + R)] = world.chunk_map_info(pcx + dx, pcy + dy);
            }

            draw_box(renderer, box_x, box_y, box_w, box_h, 15, 15, 30);

            // Title + hint
            draw_string(renderer, box_x + 1, box_y, " WORLD MAP", 255, 255, 255, 15, 15, 30);
            std::string hint = "[WASD move  M/ESC close]";
            draw_string(renderer, box_x + box_w - static_cast<int>(hint.size()) - 1, box_y,
                        hint, 110, 110, 130, 15, 15, 30);

            // Border around the grid
            for (int i = 0; i < grid + 2; ++i) {
                Cell b; b.glyph = '#';
                b.fg_r = 90; b.fg_g = 90; b.fg_b = 120;
                b.bg_r = 15; b.bg_g = 15; b.bg_b = 30;
                renderer.set_cell(box_x + i, box_y + 1, b);
                renderer.set_cell(box_x + i, box_y + 2 + grid, b);
            }
            for (int i = 0; i < grid; ++i) {
                Cell b; b.glyph = '#';
                b.fg_r = 90; b.fg_g = 90; b.fg_b = 120;
                b.bg_r = 15; b.bg_g = 15; b.bg_b = 30;
                renderer.set_cell(box_x, box_y + 2 + i, b);
                renderer.set_cell(box_x + grid + 1, box_y + 2 + i, b);
            }

            // Structure glyph colors
            auto struct_color = [](StructureType st, uint8_t& r, uint8_t& g, uint8_t& b) {
                switch (st) {
                    case StructureType::Village: r = 255; g = 215; b =  90; break;
                    case StructureType::Cave:    r = 210; g = 140; b =  90; break;
                    case StructureType::Ruins:   r = 170; g = 170; b = 190; break;
                    case StructureType::Dungeon: r = 195; g =  70; b =  70; break;
                    default:                     r = 255; g = 255; b = 255; break;
                }
            };

            // Chunk cells
            for (int dy = -R; dy <= R; ++dy) {
                for (int dx = -R; dx <= R; ++dx) {
                    const World::ChunkMapInfo& info = map_cache[(dy + R) * grid + (dx + R)];
                    const BiomeConfig& bc = biome_config(info.biome);
                    int sx = box_x + 1 + (dx + R);
                    int sy = box_y + 2 + (dy + R);

                    Cell c;
                    c.glyph = '.';
                    c.fg_r = bc.hud_r; c.fg_g = bc.hud_g; c.fg_b = bc.hud_b;
                    c.bg_r = bc.hud_r / 3; c.bg_g = bc.hud_g / 3; c.bg_b = bc.hud_b / 3;

                    if (info.structure != StructureType::None) {
                        const StructureDef& sd = structure_def(info.structure);
                        c.glyph = sd.name[0];
                        struct_color(info.structure, c.fg_r, c.fg_g, c.fg_b);
                    }

                    bool is_player = (dx == 0 && dy == 0);
                    bool is_cursor = (pcx + dx == map_cursor_cx && pcy + dy == map_cursor_cy);

                    if (is_player) {
                        c.glyph = '@';
                        c.fg_r = 0; c.fg_g = 255; c.fg_b = 0;
                        c.bg_r = 20; c.bg_g = 50; c.bg_b = 20;
                    }
                    if (is_cursor) {
                        // Cursor highlight: light background, dark glyph
                        c.bg_r = 230; c.bg_g = 230; c.bg_b = 205;
                        if (is_player) { c.fg_r = 0; c.fg_g = 130; c.fg_b = 0; }
                        else           { c.fg_r = 20; c.fg_g = 20; c.fg_b = 20; }
                    }
                    renderer.set_cell(sx, sy, c);
                }
            }

            // Status line: chunk under the cursor
            int relx = map_cursor_cx - pcx;
            int rely = map_cursor_cy - pcy;
            const World::ChunkMapInfo& ci = map_cache[(rely + R) * grid + (relx + R)];
            std::string status = biome_name(ci.biome);
            if (ci.structure != StructureType::None)
                status += std::string(" | ") + structure_def(ci.structure).name;
            status += "  (" + std::to_string(map_cursor_cx) + "," + std::to_string(map_cursor_cy) + ")";
            if (relx == 0 && rely == 0) {
                status += "  [you]";
            } else {
                std::string dir;
                if (rely < 0) dir += std::to_string(-rely) + "N";
                if (rely > 0) dir += std::to_string(rely) + "S";
                if (relx < 0) dir += std::to_string(-relx) + "W";
                if (relx > 0) dir += std::to_string(relx) + "E";
                status += "  " + dir;
            }
            draw_string(renderer, box_x + 1, box_y + box_h - 1, status,
                        200, 200, 200, 15, 15, 30);

            // Legend (right side of the box)
            int lx = box_x + grid + 3;
            draw_string(renderer, lx, box_y + 1, "BIOMES", 150, 150, 170, 15, 15, 30);
            for (int i = 0; i < static_cast<int>(BiomeType::BiomeCount); ++i) {
                BiomeType bt = static_cast<BiomeType>(i);
                const BiomeConfig& bc = biome_config(bt);
                Cell sw; sw.glyph = '#';
                sw.fg_r = bc.hud_r; sw.fg_g = bc.hud_g; sw.fg_b = bc.hud_b;
                sw.bg_r = 15; sw.bg_g = 15; sw.bg_b = 30;
                renderer.set_cell(lx, box_y + 2 + i, sw);
                draw_string(renderer, lx + 2, box_y + 2 + i, biome_name(bt), 180, 180, 180, 15, 15, 30);
            }
            int ly = box_y + 2 + static_cast<int>(BiomeType::BiomeCount) + 1;
            draw_string(renderer, lx, ly, "STRUCTURES", 150, 150, 170, 15, 15, 30);
            for (int i = 1; i < static_cast<int>(StructureType::StructureCount); ++i) {
                StructureType st = static_cast<StructureType>(i);
                const StructureDef& sd = structure_def(st);
                uint8_t cr, cg, cb;
                struct_color(st, cr, cg, cb);
                Cell sw; sw.glyph = sd.name[0];
                sw.fg_r = cr; sw.fg_g = cg; sw.fg_b = cb;
                sw.bg_r = 15; sw.bg_g = 15; sw.bg_b = 30;
                renderer.set_cell(lx, ly + 1 + i, sw);
                draw_string(renderer, lx + 2, ly + 1 + i, sd.name, 180, 180, 180, 15, 15, 30);
            }
            draw_string(renderer, lx, box_y + box_h - 1, "@ = you", 0, 255, 0, 15, 15, 30);
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
                int dx = 2, dy = view_rows - dh - 3;

                draw_box(renderer, dx, dy, dw, dh, 20, 20, 40);

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
            int pw = 24, ph = 7;
            int px = (view_cols - pw) / 2, py = (view_rows - ph) / 2;
            draw_box(renderer, px, py, pw, ph, 20, 20, 40);
            draw_string(renderer, px + 1, py, "PAUSED", 255, 255, 255, 20, 20, 40);
            std::string opts[] = {"Save Game", "Load Game", "Settings", "Main Menu", "Quit"};
            for (int i = 0; i < 5; ++i) {
                uint8_t bg = (i == pause_cursor) ? 60 : 20;
                std::string prefix = (i == pause_cursor) ? "> " : "  ";
                draw_string(renderer, px + 2, py + 1 + i, prefix + opts[i], 200, 200, 200, bg, bg, 40);
            }
        }

        // Settings overlay
        if (mode == GameMode::Settings) {
            int sw = 40, sh = 7;
            int sx = (view_cols - sw) / 2, sy = (view_rows - sh) / 2;
            draw_box(renderer, sx, sy, sw, sh, 20, 30, 20);
            draw_string(renderer, sx + 1, sy, "SETTINGS", 255, 255, 255, 20, 30, 20);

            std::string names[] = {"Chunk Load Radius", "Render Distance", "Simulation Distance"};
            int vals[] = {settings.load_radius, settings.render_radius, settings.sim_radius};
            for (int i = 0; i < 3; ++i) {
                bool sel = (i == settings_cursor);
                uint8_t bg = sel ? 50 : 20;
                std::string s = (sel ? "> " : "  ") + names[i];
                draw_string(renderer, sx + 2, sy + 2 + i, s, 200, 200, 200, bg, bg + 10, bg);
                // Slider
                int bx = sx + 24;
                for (int v = 1; v <= 4; ++v) {
                    Cell c;
                    c.glyph = (v <= vals[i]) ? '#' : '-';
                    c.fg_r = (v <= vals[i]) ? 120 : 60;
                    c.fg_g = (v <= vals[i]) ? 220 : 60;
                    c.fg_b = (v <= vals[i]) ? 120 : 60;
                    c.bg_r = bg; c.bg_g = bg + 10; c.bg_b = bg;
                    renderer.set_cell(bx + v, sy + 2 + i, c);
                }
            }
            draw_string(renderer, sx + 1, sy + sh - 1, "[</>] adjust  [ESC] done", 100, 100, 100, 20, 30, 20);
        }

        // Save/Load screen
        if (mode == GameMode::SaveGame || mode == GameMode::LoadGame) {
            int sw = 50, sh = MAX_SAVE_SLOTS + 4;
            int sx = (view_cols - sw) / 2, sy = (view_rows - sh) / 2;
            draw_box(renderer, sx, sy, sw, sh, 15, 15, 35);

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
            std::string prompt = "Press R to restart, ESC for main menu";
            draw_string(renderer, (view_cols - prompt.size()) / 2, view_rows / 2, prompt, 180, 180, 180, 30, 0, 0);
        }

        renderer.render_grid();
        if (!demo_shot.empty()) {
            renderer.save_screenshot(demo_shot);
            std::cout << "saved " << demo_shot << "\n";
        }
        renderer.present();
        SDL_Delay(16);
    }

    renderer.shutdown();
    window.shutdown();
    return 0;
}
