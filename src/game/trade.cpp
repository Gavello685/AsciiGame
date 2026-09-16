#include "game/trade.h"
#include <string>
#include <algorithm>
#include <cmath>

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
}

static int buy_price(const Item& item) {
    return static_cast<int>(std::ceil(item.value() * 1.5));
}

static int sell_price(const Item& item) {
    return item.value() / 2;
}

bool trade_handle_input(TradeState& state, SDL_Keycode key,
                        Player& player) {
    if (!state.active || !state.merchant)
        return false;

    Npc& merchant = *state.merchant;

    // Clear message on any key
    if (state.message_timer > 0) {
        state.message_timer--;
        if (state.message_timer <= 0) state.message.clear();
    }

    // Confirm dialog
    if (state.confirming) {
        if (key == SDLK_LEFT || key == SDLK_RIGHT) {
            state.confirm_cursor = 1 - state.confirm_cursor;
        } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
            if (state.confirm_cursor == 0) {
                // Both branches copy the Item before mutating its container:
                // remove_item and buy_from_shop erase the stack when it
                // empties, which would leave a reference dangling.
                if (state.selling) {
                    int idx = state.player_cursor;
                    if (idx >= 0 && idx < static_cast<int>(player.inventory().size())) {
                        Item item = player.inventory_item(idx);
                        int price = sell_price(item);
                        player.remove_item(idx, 1);
                        merchant.sell_to_shop(item, 1);
                        player.add_gold(price);
                        state.message = "Sold " + item.name() + " for " +
                                        std::to_string(price) + " gold.";
                        state.message_timer = 90;
                    }
                } else {
                    int idx = state.shop_cursor;
                    if (idx >= 0 && idx < static_cast<int>(merchant.shop_inventory().size())) {
                        Item item = merchant.shop_item_at(idx);
                        int price = buy_price(item);
                        if (!player.can_carry(item.weight())) {
                            state.message = "Too heavy!";
                        } else if (!player.spend_gold(price)) {
                            state.message = "Not enough gold!";
                        } else {
                            merchant.buy_from_shop(idx, 1);
                            player.add_item(item, 1);
                            state.message = "Bought " + item.name() + " for " +
                                            std::to_string(price) + " gold.";
                        }
                        state.message_timer = 90;
                    }
                }
            }
            state.confirming = false;

            // A stack that emptied shrinks its list, so pull the cursors back
            // into range.
            int player_last = static_cast<int>(player.inventory().size()) - 1;
            int shop_last = static_cast<int>(merchant.shop_inventory().size()) - 1;
            state.player_cursor = std::max(0, std::min(state.player_cursor, player_last));
            state.shop_cursor = std::max(0, std::min(state.shop_cursor, shop_last));
        } else if (key == SDLK_ESCAPE) {
            state.confirming = false;
        }
        return false;
    }

    // Navigation
    if (key == SDLK_ESCAPE) {
        trade_close(state);
        return false;
    }

    if (key == SDLK_TAB || key == SDLK_LEFT || key == SDLK_RIGHT) {
        state.on_player_panel = !state.on_player_panel;
        return false;
    }

    if (state.on_player_panel) {
        int max_idx = static_cast<int>(player.inventory().size()) - 1;
        if (key == SDLK_UP && state.player_cursor > 0) state.player_cursor--;
        if (key == SDLK_DOWN && state.player_cursor < max_idx) state.player_cursor++;
        if ((key == SDLK_RETURN || key == SDLK_SPACE) && max_idx >= 0) {
            state.selling = true;
            state.confirming = true;
            state.confirm_cursor = 1; // default to No
        }
    } else {
        int max_idx = static_cast<int>(merchant.shop_inventory().size()) - 1;
        if (key == SDLK_UP && state.shop_cursor > 0) state.shop_cursor--;
        if (key == SDLK_DOWN && state.shop_cursor < max_idx) state.shop_cursor++;
        if ((key == SDLK_RETURN || key == SDLK_SPACE) && max_idx >= 0) {
            state.selling = false;
            state.confirming = true;
            state.confirm_cursor = 1; // default to No
        }
    }

    return false;
}

static void draw_string(Renderer& renderer, int x, int y,
                        const std::string& s, uint8_t r, uint8_t g, uint8_t b,
                        uint8_t bg_r = 0, uint8_t bg_g = 0, uint8_t bg_b = 0) {
    Cell c;
    c.bg_r = bg_r; c.bg_g = bg_g; c.bg_b = bg_b;
    c.fg_r = r; c.fg_g = g; c.fg_b = b;
    for (int i = 0; i < static_cast<int>(s.size()); ++i) {
        c.glyph = s[i];
        renderer.set_cell(x + i, y, c);
    }
}

void trade_render(TradeState& state, const Player& player,
                  Renderer& renderer, int view_cols, int view_rows) {
    if (!state.active || !state.merchant)
        return;

    const Npc& merchant = *state.merchant;

    int panel_w = (view_cols - 8) / 2;
    int box_h = view_rows - 4;
    if (box_h > 24) box_h = 24;
    int box_y = 2;
    int left_x = 2;
    int right_x = left_x + panel_w + 2;

    // Background
    for (int y = box_y; y < box_y + box_h; ++y) {
        for (int x = left_x; x < right_x + panel_w; ++x) {
            Cell c; c.glyph = ' ';
            c.bg_r = 15; c.bg_g = 15; c.bg_b = 35;
            renderer.set_cell(x, y, c);
        }
    }

    // Title
    std::string title = "TRADE with " + merchant.name();
    draw_string(renderer, left_x + 1, box_y, title, 255, 255, 255, 15, 15, 35);

    // Gold
    std::string gold = "Gold: " + std::to_string(player.gold());
    draw_string(renderer, right_x + panel_w - gold.size() - 1, box_y, gold, 255, 215, 0, 15, 15, 35);

    // Player panel
    draw_string(renderer, left_x + 1, box_y + 1, "Your Items:", 180, 180, 180, 15, 15, 35);
    int item_area_y = box_y + 2;
    int max_visible = box_h - 4;

    const auto& inv = player.inventory();
    int scroll = 0;
    if (state.player_cursor >= max_visible) scroll = state.player_cursor - max_visible + 1;

    for (int i = 0; i < max_visible && (i + scroll) < static_cast<int>(inv.size()); ++i) {
        int idx = i + scroll;
        const auto& [item, qty] = inv[idx];
        int y = item_area_y + i;
        bool selected = (idx == state.player_cursor && state.on_player_panel);

        uint8_t r = 200, g = 200, b = 200;
        uint8_t bg_r = 15, bg_g = 15, bg_b = 35;
        if (selected) { bg_r = 40; bg_g = 40; bg_b = 60; }

        // Glyph
        Cell gc;
        gc.glyph = item.glyph();
        gc.fg_r = item.fg_r(); gc.fg_g = item.fg_g(); gc.fg_b = item.fg_b();
        gc.bg_r = bg_r; gc.bg_g = bg_g; gc.bg_b = bg_b;
        renderer.set_cell(left_x + 1, y, gc);

        // Name and count
        std::string label = item.name();
        if (qty > 1) label += " (" + std::to_string(qty) + ")";
        draw_string(renderer, left_x + 3, y, label, r, g, b, bg_r, bg_g, bg_b);

        // Sell price
        int sp = sell_price(item);
        std::string price_str = std::to_string(sp) + "g";
        draw_string(renderer, left_x + panel_w - price_str.size() - 1, y,
                    price_str, 180, 180, 80, bg_r, bg_g, bg_b);
    }

    if (inv.empty()) {
        draw_string(renderer, left_x + 2, item_area_y, "(empty)", 100, 100, 100, 15, 15, 35);
    }

    // Merchant panel
    draw_string(renderer, right_x + 1, box_y + 1, merchant.name() + "'s Wares:", 180, 180, 180, 15, 15, 35);

    const auto& shop = merchant.shop_inventory();
    int shop_scroll = 0;
    if (state.shop_cursor >= max_visible) shop_scroll = state.shop_cursor - max_visible + 1;

    for (int i = 0; i < max_visible && (i + shop_scroll) < static_cast<int>(shop.size()); ++i) {
        int idx = i + shop_scroll;
        const auto& [item, stock] = shop[idx];
        int y = item_area_y + i;
        bool selected = (idx == state.shop_cursor && !state.on_player_panel);

        uint8_t r = 200, g = 200, b = 200;
        uint8_t bg_r = 15, bg_g = 15, bg_b = 35;
        if (selected) { bg_r = 40; bg_g = 40; bg_b = 60; }

        // Glyph
        Cell gc;
        gc.glyph = item.glyph();
        gc.fg_r = item.fg_r(); gc.fg_g = item.fg_g(); gc.fg_b = item.fg_b();
        gc.bg_r = bg_r; gc.bg_g = bg_g; gc.bg_b = bg_b;
        renderer.set_cell(right_x + 1, y, gc);

        // Name and stock
        std::string label = item.name() + " (" + std::to_string(stock) + ")";
        draw_string(renderer, right_x + 3, y, label, r, g, b, bg_r, bg_g, bg_b);

        // Buy price
        int bp = buy_price(item);
        std::string price_str = std::to_string(bp) + "g";
        draw_string(renderer, right_x + panel_w - price_str.size() - 1, y,
                    price_str, 255, 180, 80, bg_r, bg_g, bg_b);
    }

    if (shop.empty()) {
        draw_string(renderer, right_x + 2, item_area_y, "(no stock)", 100, 100, 100, 15, 15, 35);
    }

    // Instructions
    int instr_y = box_y + box_h - 1;
    std::string instr = "[TAB] switch  [ENTER] select  [ESC] close";
    draw_string(renderer, left_x + 1, instr_y, instr, 100, 100, 100, 15, 15, 35);

    // Message
    if (!state.message.empty()) {
        draw_string(renderer, left_x + 1, box_y + 3, state.message, 255, 255, 100, 15, 15, 35);
    }

    // Confirm dialog
    if (state.confirming) {
        int cx = left_x + panel_w / 2 - 8;
        int cy = box_y + box_h / 2;

        for (int y = cy - 1; y <= cy + 1; ++y) {
            for (int x = cx; x < cx + 16; ++x) {
                Cell c; c.glyph = ' ';
                c.bg_r = 30; c.bg_g = 30; c.bg_b = 50;
                renderer.set_cell(x, y, c);
            }
        }

        std::string q = state.selling ? "Sell this item?" : "Buy this item?";
        draw_string(renderer, cx + 1, cy - 1, q, 255, 255, 255, 30, 30, 50);

        std::string yes = " Yes ";
        std::string no = " No ";
        uint8_t y_bg = (state.confirm_cursor == 0) ? 60 : 30;
        uint8_t n_bg = (state.confirm_cursor == 1) ? 60 : 30;
        draw_string(renderer, cx + 2, cy + 1, yes, 100, 255, 100, y_bg, y_bg, 50);
        draw_string(renderer, cx + 9, cy + 1, no, 255, 100, 100, n_bg, n_bg, 50);
    }
}
