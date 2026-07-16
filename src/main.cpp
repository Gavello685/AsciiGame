#include "engine/window.h"
#include "engine/renderer.h"
#include "game/map.h"
#include "game/player.h"
#include <iostream>
#include <string>

static const int FONT_SIZE = 20;
static const int MAP_WIDTH = 80;
static const int MAP_HEIGHT = 50;

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
                y = MAP_HEIGHT; // break outer
                break;
            }
        }
    }

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_w: case SDLK_UP:    player.move(0, -1, map); break;
                    case SDLK_s: case SDLK_DOWN:  player.move(0,  1, map); break;
                    case SDLK_a: case SDLK_LEFT:  player.move(-1, 0, map); break;
                    case SDLK_d: case SDLK_RIGHT: player.move( 1, 0, map); break;
                    default: break;
                }
            }
        }

        renderer.clear();

        int view_cols = window.width() / renderer.cell_width();
        int view_rows = window.height() / renderer.cell_height();

        renderer.begin_frame(view_cols, view_rows);

        // Camera: center on player
        int cam_x = player.x() - view_cols / 2;
        int cam_y = player.y() - view_rows / 2;

        // Clamp camera to map bounds
        if (cam_x < 0) cam_x = 0;
        if (cam_y < 0) cam_y = 0;
        if (cam_x + view_cols > MAP_WIDTH) cam_x = MAP_WIDTH - view_cols;
        if (cam_y + view_rows > MAP_HEIGHT) cam_y = MAP_HEIGHT - view_rows;

        // Render visible tiles
        for (int vy = 0; vy < view_rows; ++vy) {
            for (int vx = 0; vx < view_cols; ++vx) {
                int mx = cam_x + vx;
                int my = cam_y + vy;

                Cell cell;
                cell.glyph = ' ';

                if (map.in_bounds(mx, my)) {
                    Tile tile = map.get(mx, my);
                    cell.glyph = tile_glyph(tile.type);
                    tile_color(tile.type, cell.fg_r, cell.fg_g, cell.fg_b);
                }

                // Draw player on top
                if (mx == player.x() && my == player.y()) {
                    cell.glyph = '@';
                    cell.fg_r = 0; cell.fg_g = 255; cell.fg_b = 0;
                }

                renderer.set_cell(vx, vy, cell);
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
