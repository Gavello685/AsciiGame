#include "game/trade.h"

#include <algorithm>
#include <cmath>

using ui::Key;

void trade_open(TradeState& state, Npc* merchant) {
    state.active = true;
    state.merchant = merchant;
    state.player_cursor = 0;
    state.shop_cursor = 0;
    state.on_player_panel = true;
    state.confirming = false;
    state.message.clear();
    state.message_timer = 0;
}

void trade_close(TradeState& state) {
    state.active = false;
    state.merchant = nullptr;
    state.confirming = false;
    state.message.clear();
    state.message_timer = 0;
}

int trade_buy_price(const Item& item) {
    return static_cast<int>(std::ceil(item.value() * 1.5));
}

int trade_sell_price(const Item& item) {
    return item.value() / 2;
}

void trade_tick(TradeState& state) {
    if (state.message_timer <= 0) return;
    if (--state.message_timer <= 0) state.message.clear();
}

namespace {

// Sell the stack under the player cursor. The Item is copied first: a stack
// that empties is erased from the inventory, which would leave a reference
// into it dangling before the status message is built.
void sell_selected(TradeState& state, Player& player, Npc& merchant) {
    int idx = state.player_cursor;
    if (idx < 0 || idx >= static_cast<int>(player.inventory().size())) return;

    Item item = player.inventory_item(idx);
    int price = trade_sell_price(item);
    player.remove_item(idx, 1);
    merchant.sell_to_shop(item, 1);
    player.add_gold(price);
    state.message = "Sold " + item.name() + " for " + std::to_string(price) + " gold.";
    state.message_timer = 90;
}

void buy_selected(TradeState& state, Player& player, Npc& merchant) {
    int idx = state.shop_cursor;
    if (idx < 0 || idx >= static_cast<int>(merchant.shop_inventory().size())) return;

    Item item = merchant.shop_item_at(idx);
    int price = trade_buy_price(item);
    if (!player.can_carry(item.weight())) {
        state.message = "Too heavy!";
    } else if (!player.spend_gold(price)) {
        state.message = "Not enough gold!";
    } else {
        merchant.buy_from_shop(idx, 1);
        player.add_item(item, 1);
        state.message = "Bought " + item.name() + " for " + std::to_string(price) + " gold.";
    }
    state.message_timer = 90;
}

// A transaction can empty a stack and shrink its list, so both cursors have
// to be pulled back into range afterwards.
void clamp_cursors(TradeState& state, const Player& player, const Npc& merchant) {
    int player_last = static_cast<int>(player.inventory().size()) - 1;
    int shop_last = static_cast<int>(merchant.shop_inventory().size()) - 1;
    state.player_cursor = std::max(0, std::min(state.player_cursor, player_last));
    state.shop_cursor = std::max(0, std::min(state.shop_cursor, shop_last));
}

}

void trade_handle_input(TradeState& state, Key key, Player& player) {
    if (!state.active || !state.merchant) return;

    Npc& merchant = *state.merchant;

    if (state.confirming) {
        if (key == Key::Left || key == Key::Right) {
            state.confirm_cursor = 1 - state.confirm_cursor;
        } else if (key == Key::Enter || key == Key::Space) {
            if (state.confirm_cursor == 0) {
                if (state.selling) sell_selected(state, player, merchant);
                else               buy_selected(state, player, merchant);
            }
            state.confirming = false;
            clamp_cursors(state, player, merchant);
        } else if (key == Key::Escape) {
            state.confirming = false;
        }
        return;
    }

    if (key == Key::Escape) {
        trade_close(state);
        return;
    }

    if (key == Key::Tab || key == Key::Left || key == Key::Right) {
        state.on_player_panel = !state.on_player_panel;
        return;
    }

    int& cursor = state.on_player_panel ? state.player_cursor : state.shop_cursor;
    int last = state.on_player_panel
                   ? static_cast<int>(player.inventory().size()) - 1
                   : static_cast<int>(merchant.shop_inventory().size()) - 1;

    if (key == Key::Up && cursor > 0) cursor--;
    if (key == Key::Down && cursor < last) cursor++;
    if ((key == Key::Enter || key == Key::Space) && last >= 0) {
        state.selling = state.on_player_panel;
        state.confirming = true;
        state.confirm_cursor = 1; // default to No
    }
}
