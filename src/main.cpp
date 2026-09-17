#include "engine/renderer.h"
#include "engine/window.h"
#include "game/fov.h"
#include "game/light.h"
#include "game/session.h"
#include "game/settings.h"
#include "game/trade.h"
#include "game/turn.h"
#include "platform/paths.h"
#include "ui/input.h"
#include "ui/key.h"
#include "ui/menus.h"
#include "ui/overlays.h"
#include "ui/trade_view.h"
#include "ui/ui_state.h"
#include "ui/world_view.h"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <string>

namespace {

constexpr int FONT_SIZE = 20;
constexpr int WINDOW_W = 1024;
constexpr int WINDOW_H = 768;

// ~60fps. Without it the loop spins a core at 100%.
constexpr uint32_t FRAME_DELAY_MS = 16;

// Frames per in-game second of recorded play time.
constexpr int FRAMES_PER_SECOND = 60;

// PLAN.md 4B: base FOV radius at WIS 10, before the perception bonus and the
// ambient-light scaling applied below.
constexpr int FOV_BASE_RADIUS = 8;

// Ambient light is graded 0-15; FOV is scaled by that fraction.
constexpr float MAX_AMBIENT = 15.0f;

// Map SDL's keycodes onto the game's own key names. Keys the game does not
// use collapse to Key::None and are ignored by the input handler.
ui::Key translate(SDL_Keycode code) {
    switch (code) {
        case SDLK_UP:           return ui::Key::Up;
        case SDLK_DOWN:         return ui::Key::Down;
        case SDLK_LEFT:         return ui::Key::Left;
        case SDLK_RIGHT:        return ui::Key::Right;
        case SDLK_RETURN:       return ui::Key::Enter;
        case SDLK_SPACE:        return ui::Key::Space;
        case SDLK_ESCAPE:       return ui::Key::Escape;
        case SDLK_TAB:          return ui::Key::Tab;
        case SDLK_PERIOD:       return ui::Key::Period;
        case SDLK_LEFTBRACKET:  return ui::Key::BracketLeft;
        case SDLK_RIGHTBRACKET: return ui::Key::BracketRight;
        case SDLK_a:            return ui::Key::A;
        case SDLK_b:            return ui::Key::B;
        case SDLK_c:            return ui::Key::C;
        case SDLK_d:            return ui::Key::D;
        case SDLK_g:            return ui::Key::G;
        case SDLK_i:            return ui::Key::I;
        case SDLK_m:            return ui::Key::M;
        case SDLK_r:            return ui::Key::R;
        case SDLK_s:            return ui::Key::S;
        case SDLK_w:            return ui::Key::W;
        case SDLK_x:            return ui::Key::X;
        case SDLK_z:            return ui::Key::Z;
        default:                return ui::Key::None;
    }
}

// Recompute lighting and visibility. Light comes first because the FOV radius
// depends on how bright it is where the player is standing.
void update_vision(Session& session) {
    uint8_t ambient = session.time.ambient_light();
    int torch_radius = session.player.has_light_source() ? session.player.torch_radius() : 0;

    light::compute(session.world, session.player.x(), session.player.y(),
                   ambient, torch_radius);

    // PLAN.md 4B: base radius 8 plus the WIS bonus, scaled by ambient light
    // so sight shrinks at dusk and collapses at night.
    int base = std::max(3, FOV_BASE_RADIUS + (session.player.stats().perception() - 10) / 2);
    int radius = static_cast<int>(base * (ambient / MAX_AMBIENT));

    // A carried light always reveals its own radius, and there is always a
    // little glimmer, so the player is never blind.
    int floor_radius = torch_radius > 0 ? torch_radius + 2 : 4;
    radius = std::max(radius, floor_radius);

    fov::compute(session.world, session.player.x(), session.player.y(), radius);
}

// Draw the world and whichever overlays the current mode calls for.
void render(Renderer& renderer, const Window& window, Session& session,
            ui::UiState& state, const TradeState& trade, const Settings& settings) {
    renderer.clear();

    ui::Viewport view = ui::make_viewport(renderer, window.width(), window.height(),
                                          session.player);
    renderer.begin_frame(view.cols, view.rows);

    if (!session.started) {
        ui::draw_main_menu(renderer, state, view);
        renderer.render_grid();
        return;
    }

    ui::draw_world(renderer, session.world, session.player, view);

    if (state.mode == ui::GameMode::Build) {
        ui::draw_build_cursor(renderer, session.world, session.player, state, view);
    }
    if (state.mode == ui::GameMode::Zone) {
        ui::draw_zone_cursor(renderer, state, view);
    }

    ui::draw_hud(renderer, session.world, session.player, session.time, view);
    ui::draw_message_log(renderer, state.messages, view);

    switch (state.mode) {
        case ui::GameMode::Inventory:
        case ui::GameMode::InventoryAction:
        case ui::GameMode::InventoryExamine:
        case ui::GameMode::GiftSelect:
            ui::draw_inventory(renderer, session.player, state, view);
            break;
        case ui::GameMode::Craft:
            ui::draw_craft(renderer, session.player, state, view);
            break;
        case ui::GameMode::Build:
            ui::draw_build_panel(renderer, session.player, state);
            break;
        case ui::GameMode::Zone:
            ui::draw_zone_panel(renderer, session.world, state);
            break;
        case ui::GameMode::Character:
            ui::draw_character_sheet(renderer, session.player, view);
            break;
        case ui::GameMode::Map:
            ui::draw_world_map(renderer, session.world, session.player, state, view);
            break;
        case ui::GameMode::Dialogue:
            ui::draw_dialogue(renderer, session.world, state, view);
            break;
        default:
            break;
    }

    if (trade.active) ui::draw_trade(renderer, trade, session.player, view);

    switch (state.mode) {
        case ui::GameMode::PauseMenu:
            ui::draw_pause_menu(renderer, state, view);
            break;
        case ui::GameMode::Settings:
            ui::draw_settings(renderer, state, settings, view);
            break;
        case ui::GameMode::SaveGame:
        case ui::GameMode::LoadGame:
            ui::draw_save_slots(renderer, state, view);
            break;
        case ui::GameMode::Dead:
            ui::draw_death_screen(renderer, view);
            break;
        default:
            break;
    }

    renderer.render_grid();
}

// A scripted walkthrough of the interface used to produce screenshots. Keys
// are pushed onto SDL's queue so they travel the same path as real input.
struct DemoScript {
    // Fixed so successive runs produce byte-identical screenshots.
    static constexpr uint32_t SEED = 20260801u;

    struct Beat {
        int frame;
        ui::Key key;          // Key::None for a screenshot-only beat
        const char* shot;     // nullptr when nothing is captured
    };

    static constexpr Beat BEATS[] = {
        { 10, ui::Key::D,      nullptr},
        { 12, ui::Key::S,      nullptr},
        { 20, ui::Key::None,   "demo1_game.bmp"},
        { 30, ui::Key::I,      nullptr},
        { 40, ui::Key::None,   "demo2_inv.bmp"},
        { 50, ui::Key::Escape, nullptr},
        { 60, ui::Key::C,      nullptr},
        { 70, ui::Key::None,   "demo3_craft.bmp"},
        { 80, ui::Key::Escape, nullptr},
        { 90, ui::Key::B,      nullptr},
        {100, ui::Key::D,      nullptr},
        {110, ui::Key::None,   "demo4_build.bmp"},
        {120, ui::Key::Escape, nullptr},
        {130, ui::Key::Z,      nullptr},
        {140, ui::Key::None,   "demo5_zone.bmp"},
        {150, ui::Key::Escape, nullptr},
        {160, ui::Key::X,      nullptr},
        {170, ui::Key::None,   "demo6_char.bmp"},
        {180, ui::Key::Escape, nullptr},
        {190, ui::Key::M,      nullptr},
        {210, ui::Key::D,      nullptr},
        {220, ui::Key::None,   "demo7_map.bmp"},
        {230, ui::Key::Escape, nullptr},
        {240, ui::Key::Escape, nullptr},
        {250, ui::Key::None,   "demo8_pause.bmp"},
    };

    static constexpr int LAST_FRAME = 260;

    int frame = 0;

    // Advance one frame, returning the screenshot to write, if any.
    const char* advance(ui::InputContext& ctx, bool& running) {
        const char* shot = nullptr;
        for (const Beat& beat : BEATS) {
            if (beat.frame != frame) continue;
            if (beat.key != ui::Key::None) ui::handle_key(ctx, beat.key);
            shot = beat.shot;
        }
        if (frame >= LAST_FRAME) running = false;
        frame++;
        return shot;
    }
};

}

int main(int argc, char* argv[]) {
    bool demo_mode = (argc > 1 && std::string(argv[1]) == "--demo");

    Window window;
    if (!window.init({"ASCII Game", WINDOW_W, WINDOW_H})) return 1;

    std::filesystem::path font_path = platform::find_monospace_font();
    if (font_path.empty()) {
        std::cerr << "No monospace font found. Put a .ttf in assets/fonts or "
                     "set ASCII_GAME_FONT.\n";
        return 1;
    }

    Renderer renderer;
    if (!renderer.init(window.get(), font_path.string(), FONT_SIZE)) {
        std::cerr << "Failed to initialise renderer with font " << font_path << ".\n";
        return 1;
    }

    Settings settings;
    load_settings(settings);

    Session session;
    ui::UiState state;
    TradeState trade;

    ui::InputContext input{session, state, settings, trade, 0};

    DemoScript demo;
    if (demo_mode) {
        input.new_game_seed = DemoScript::SEED;
        ui::handle_key(input, ui::Key::Enter);   // New Game on the title screen
    }

    bool running = true;
    bool vision_dirty = true;
    int play_time_frames = 0;

    while (running) {
        // Re-seeded every frame so each new game gets a different world,
        // except in demo mode where the seed is pinned.
        if (!demo_mode) {
            input.new_game_seed = static_cast<uint32_t>(std::time(nullptr));
        }

        bool took_turn = false;
        const char* screenshot = demo_mode ? demo.advance(input, running) : nullptr;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                break;
            }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                window.on_resized(event.window.data1, event.window.data2);
                continue;
            }
            if (event.type != SDL_KEYDOWN) continue;

            ui::InputResult result = ui::handle_key(input, translate(event.key.keysym.sym));
            if (result.quit) running = false;
            if (result.took_turn) took_turn = true;
            if (result.fov_dirty) vision_dirty = true;
        }

        if (!session.started) {
            render(renderer, window, session, state, trade, settings);
            renderer.present();
            SDL_Delay(FRAME_DELAY_MS);
            continue;
        }

        if (vision_dirty) {
            update_vision(session);
            vision_dirty = false;
        }

        if (took_turn) {
            session.turn_counter++;
            TurnOutcome outcome = resolve_turn(session.world, session.player,
                                               session.time, session.rng);
            for (const std::string& message : outcome.messages) state.messages.push(message);
            if (outcome.player_died) state.mode = ui::GameMode::Dead;

            // Time moved, so ambient light and every actor position may have
            // changed. Recompute rather than rely on each action asking.
            vision_dirty = true;
        }

        if (++play_time_frames >= FRAMES_PER_SECOND) {
            play_time_frames = 0;
            session.play_time_seconds++;
        }
        state.messages.tick();
        trade_tick(trade);

        render(renderer, window, session, state, trade, settings);

        if (screenshot) {
            renderer.save_screenshot(screenshot);
            std::cout << "saved " << screenshot << "\n";
        }
        renderer.present();
        SDL_Delay(FRAME_DELAY_MS);
    }

    renderer.shutdown();
    window.shutdown();
    return 0;
}
