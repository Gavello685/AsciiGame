#pragma once

#include "game/item.h"
#include "game/player.h"
#include "game/npc.h"
#include "engine/renderer.h"
#include <string>

struct TradeState {
    bool active = false;
    Npc* merchant = nullptr;
    int player_cursor = 0;
    int shop_cursor = 0;
    bool on_player_panel = true;
    bool confirming = false;
    bool selling = false;
    int confirm_cursor = 0;
    std::string message;
    int message_timer = 0;
};

void trade_open(TradeState& state, Npc* merchant);
void trade_close(TradeState& state);

// Returns true if an action was taken (consumes a turn)
bool trade_handle_input(TradeState& state, SDL_Keycode key, Player& player);

void trade_render(TradeState& state, const Player& player,
                  Renderer& renderer, int view_cols, int view_rows);
