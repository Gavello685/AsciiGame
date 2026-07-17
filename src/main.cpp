#include "engine/window.h"
#include "engine/renderer.h"
#include "game/map.h"
#include "game/player.h"
#include "game/entity.h"
#include "game/fov.h"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

static const int FONT_SIZE = 20;
static const int MAP_WIDTH = 80;
static const int MAP_HEIGHT = 50;
static const int FOV_RADIUS = 8;

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Window window;
    if (!window.init({"ASCII Game", 1024, 768})) {
        return 1;
    }

    Renderer renderer;
    std::string font_path = "C:/Windows/Fonts/cour.ttf";
    if (!renderer.init(window.get(), font_path, FONT_SIZE)) {
        std::cerr << "Failed to initialize renderer. Place a monospace .ttf in assets/fonts/\n";
        return 1;
    }

    // Generate map
    Map map;
    map.generate(MAP_WIDTH, MAP_HEIGHT);

    // Spawn player in a passable tile
    Player player;
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            if (map.is_passable(x, y)) {
                player.spawn(x, y);
                y = MAP_HEIGHT;
                break;
            }
        }
    }

    // Spawn items on map
    std::vector<Entity> items;
    const char* item_names[] = {"Gold Coin", "Health Potion", "Old Scroll", "Bread", "Rusty Key"};
    const uint32_t item_glyphs[] = {'$', '$', '?', '%', '!'};
    const uint8_t item_colors[][3] = {
        {255, 215, 0},   // Gold - yellow
        {255, 50, 50},   // Potion - red
        {200, 180, 140}, // Scroll - tan
        {180, 140, 60},  // Bread - brown
        {150, 150, 150}, // Key - gray
    };

    int num_items = 10 + std::rand() % 6;
    for (int i = 0; i < num_items; ++i) {
        int ix, iy;
        do {
            ix = std::rand() % MAP_WIDTH;
            iy = std::rand() % MAP_HEIGHT;
        } while (!map.is_passable(ix, iy));

        int type = std::rand() % 5;
        Entity item(ix, iy, item_glyphs[type], item_names[type],
                    item_colors[type][0], item_colors[type][1], item_colors[type][2]);
        item.set_is_item(true);
        item.set_blocking(false);
        items.push_back(item);
    }

    // Inventory
    std::vector<Entity> inventory;

    // Game state
    bool running = true;
    SDL_Event event;
    bool show_inventory = false;
    bool need_fov_update = true;

    // Initial FOV
    fov::compute(map, player.x(), player.y(), FOV_RADIUS);

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN) {
                if (show_inventory) {
                    if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_i) {
                        show_inventory = false;
                    }
                    continue;
                }

                int dx = 0, dy = 0;
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_w: case SDLK_UP:    dy = -1; break;
                    case SDLK_s: case SDLK_DOWN:  dy =  1; break;
                    case SDLK_a: case SDLK_LEFT:  dx = -1; break;
                    case SDLK_d: case SDLK_RIGHT: dx =  1; break;
                    case SDLK_i:
                        show_inventory = true;
                        break;
                    case SDLK_g: {
                        // Pick up item underfoot
                        for (auto& item : items) {
                            if (!item.in_inventory() && item.x() == player.x() && item.y() == player.y()) {
                                item.set_in_inventory(true);
                                inventory.push_back(item);
                                // Remove from map list
                                items.erase(std::remove_if(items.begin(), items.end(),
                                    [](const Entity& e) { return e.in_inventory(); }), items.end());
                                break;
                            }
                        }
                        break;
                    }
                    default: break;
                }

                if ((dx != 0 || dy != 0) && player.can_move(dx, dy, map)) {
                    player.move(dx, dy, map);
                    need_fov_update = true;
                }
            }
        }

        if (need_fov_update) {
            fov::compute(map, player.x(), player.y(), FOV_RADIUS);
            need_fov_update = false;
        }

        // Render
        renderer.clear();

        int view_cols = window.width() / renderer.cell_width();
        int view_rows = window.height() / renderer.cell_height();

        renderer.begin_frame(view_cols, view_rows);

        // Camera: center on player
        int cam_x = player.x() - view_cols / 2;
        int cam_y = player.y() - view_rows / 2;

        if (cam_x < 0) cam_x = 0;
        if (cam_y < 0) cam_y = 0;
        if (cam_x + view_cols > MAP_WIDTH) cam_x = MAP_WIDTH - view_cols;
        if (cam_y + view_rows > MAP_HEIGHT) cam_y = MAP_HEIGHT - view_rows;

        // Render tiles
        for (int vy = 0; vy < view_rows; ++vy) {
            for (int vx = 0; vx < view_cols; ++vx) {
                int mx = cam_x + vx;
                int my = cam_y + vy;

                Cell cell;
                cell.glyph = ' ';

                if (map.in_bounds(mx, my)) {
                    Tile tile = map.get(mx, my);

                    if (tile.visible) {
                        // Fully visible
                        cell.glyph = tile_glyph(tile.type);
                        tile_color(tile.type, cell.fg_r, cell.fg_g, cell.fg_b);
                    } else if (tile.explored) {
                        // Explored but not visible — dim
                        cell.glyph = tile_glyph(tile.type);
                        cell.fg_r = 40; cell.fg_g = 40; cell.fg_b = 40;
                    }
                    // else: black (unexplored)
                }

                // Draw visible items (before player so @ overwrites item glyphs)
                if (map.in_bounds(mx, my) && map.get(mx, my).visible) {
                    for (const auto& item : items) {
                        if (!item.in_inventory() && item.x() == mx && item.y() == my) {
                            cell.glyph = item.glyph();
                            cell.fg_r = item.fg_r();
                            cell.fg_g = item.fg_g();
                            cell.fg_b = item.fg_b();
                        }
                    }
                }

                // Draw player (on top of items)
                if (mx == player.x() && my == player.y()) {
                    cell.glyph = '@';
                    cell.fg_r = 0; cell.fg_g = 255; cell.fg_b = 0;
                }

                renderer.set_cell(vx, vy, cell);
            }
        }

        // Inventory overlay (drawn into cells_ before render_grid)
        if (show_inventory) {
            int box_x = 2;
            int box_y = 2;
            int box_w = 30;
            int box_h = static_cast<int>(inventory.size()) + 4;
            if (box_h < 6) box_h = 6;
            if (box_y + box_h > view_rows) box_y = view_rows - box_h - 1;
            if (box_x + box_w > view_cols) box_w = view_cols - 4;

            // Draw box background
            for (int y = box_y; y < box_y + box_h; ++y) {
                for (int x = box_x; x < box_x + box_w; ++x) {
                    Cell c;
                    c.glyph = ' ';
                    c.bg_r = 30; c.bg_g = 30; c.bg_b = 50;
                    renderer.set_cell(x, y, c);
                }
            }

            // Draw title
            {
                Cell c;
                c.glyph = 'I';
                c.fg_r = 255; c.fg_g = 255; c.fg_b = 255;
                c.bg_r = 30; c.bg_g = 30; c.bg_b = 50;
                std::string title = "INVENTORY";
                for (int i = 0; i < static_cast<int>(title.size()) && i + box_x + 1 < box_x + box_w; ++i) {
                    c.glyph = title[i];
                    renderer.set_cell(box_x + 1 + i, box_y + 1, c);
                }
            }

            // Draw items
            if (inventory.empty()) {
                Cell c;
                c.glyph = '-';
                c.fg_r = 100; c.fg_g = 100; c.fg_b = 100;
                c.bg_r = 30; c.bg_g = 30; c.bg_b = 50;
                renderer.set_cell(box_x + 2, box_y + 3, c);
                c.glyph = ' ';
                renderer.set_cell(box_x + 3, box_y + 3, c);
                c.glyph = 'e';
                renderer.set_cell(box_x + 4, box_y + 3, c);
                c.glyph = 'm';
                renderer.set_cell(box_x + 5, box_y + 3, c);
                c.glyph = 'p';
                renderer.set_cell(box_x + 6, box_y + 3, c);
                c.glyph = 't';
                renderer.set_cell(box_x + 7, box_y + 3, c);
                c.glyph = 'y';
                renderer.set_cell(box_x + 8, box_y + 3, c);
            } else {
                for (int i = 0; i < static_cast<int>(inventory.size()); ++i) {
                    int iy = box_y + 3 + i;
                    if (iy >= box_y + box_h - 1) break;

                    Cell c;
                    c.bg_r = 30; c.bg_g = 30; c.bg_b = 50;

                    // Glyph
                    c.glyph = inventory[i].glyph();
                    c.fg_r = inventory[i].fg_r();
                    c.fg_g = inventory[i].fg_g();
                    c.fg_b = inventory[i].fg_b();
                    renderer.set_cell(box_x + 2, iy, c);

                    // Name
                    const std::string& name = inventory[i].name();
                    c.fg_r = 200; c.fg_g = 200; c.fg_b = 200;
                    for (int j = 0; j < static_cast<int>(name.size()) && box_x + 4 + j < box_x + box_w - 1; ++j) {
                        c.glyph = name[j];
                        renderer.set_cell(box_x + 4 + j, iy, c);
                    }
                }
            }
        }

        renderer.render_grid();

        renderer.present();
        SDL_Delay(16);
    }

    renderer.shutdown();
    window.shutdown();
    return 0;
}
