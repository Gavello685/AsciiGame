#pragma once

#include "game/item.h"
#include "game/npc.h"
#include "game/player.h"
#include "ui/key.h"
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

void trade_handle_input(TradeState& state, ui::Key key, Player& player);

// Age the transaction message by a frame. Called from the frame loop rather
// than from the input handler, which only runs on a key press and so left
// messages on screen indefinitely.
void trade_tick(TradeState& state);

// Prices are shown in the panels and charged on confirmation, so the view and
// the transaction have to agree on them.
int trade_buy_price(const Item& item);
int trade_sell_price(const Item& item);
