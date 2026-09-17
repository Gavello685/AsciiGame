#pragma once

#include "engine/renderer.h"
#include "game/trade.h"
#include "ui/ui_state.h"

class Player;

namespace ui {

// Two-panel trade overlay: the player's stock on the left with sell prices,
// the merchant's on the right with buy prices.
void draw_trade(Renderer& r, const TradeState& state, const Player& player,
                const Viewport& view);

}
