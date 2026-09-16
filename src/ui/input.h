#pragma once

#include "game/session.h"
#include "game/settings.h"
#include "game/trade.h"
#include "ui/key.h"
#include "ui/ui_state.h"

namespace ui {

// Everything a key press may touch, bundled so handle_key stays a single
// entry point rather than a six-argument call.
struct InputContext {
    Session& session;
    UiState& ui;
    Settings& settings;
    TradeState& trade;

    // Seed handed to the next new game. The caller supplies it so a demo run
    // can pin it and get reproducible screenshots.
    uint32_t new_game_seed = 0;
};

// What the frame loop still has to do once a key has been handled.
struct InputResult {
    // The player asked to leave the game entirely.
    bool quit = false;

    // The action consumed a game turn, so the world should be stepped.
    bool took_turn = false;

    // Visibility or lighting changed and must be recomputed before drawing.
    bool fov_dirty = false;
};

// Dispatch one key press to whichever screen currently owns input.
InputResult handle_key(InputContext& ctx, Key key);

}
