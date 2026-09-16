#include "ui/trade_view.h"

#include "game/player.h"
#include "ui/draw.h"

#include <algorithm>
#include <string>

namespace ui {

namespace {

const Colour PANEL_BG{15, 15, 35};
const Colour SELECTED_BG{40, 40, 60};
const Colour CONFIRM_BG{30, 30, 50};
const Colour LABEL{180, 180, 180};
const Colour ROW_FG{200, 200, 200};

// One side of the trade screen. Both panels list stacks the same way and
// differ only in which price they quote and which cursor selects them.
struct Panel {
    int x = 0;
    int width = 0;
    const std::vector<std::pair<Item, int>>* stock = nullptr;
    int cursor = 0;
    bool focused = false;
    std::string heading;
    const char* empty_text = "";
    Colour price_fg;
    int (*price_of)(const Item&) = nullptr;
    bool show_stock_count = false;
};

void draw_panel(Renderer& r, const Panel& panel, int top, int max_visible) {
    draw_string(r, panel.x + 1, top - 1, panel.heading, Style{LABEL, PANEL_BG});

    const auto& stock = *panel.stock;
    if (stock.empty()) {
        draw_string(r, panel.x + 2, top, panel.empty_text,
                    Style{Colour{100, 100, 100}, PANEL_BG});
        return;
    }

    // Keep the cursor on screen once the list is longer than the panel.
    int scroll = std::max(0, panel.cursor - max_visible + 1);

    for (int row = 0; row < max_visible; ++row) {
        int idx = row + scroll;
        if (idx >= static_cast<int>(stock.size())) break;

        const auto& [item, qty] = stock[idx];
        int y = top + row;
        bool selected = (idx == panel.cursor && panel.focused);
        Style style{ROW_FG, selected ? SELECTED_BG : PANEL_BG};

        // The merchant panel shows stock counts; the player panel shows stack
        // sizes. draw_item_row hides a count of one, which reads wrong for
        // "the last Health Potion in the shop", so that side is always shown.
        if (panel.show_stock_count) {
            draw_glyph(r, panel.x + 1, y, item.glyph(),
                       Colour{item.fg_r(), item.fg_g(), item.fg_b()}, style.bg);
            draw_string(r, panel.x + 3, y,
                        item.name() + " (" + std::to_string(qty) + ")", style);
        } else {
            draw_item_row(r, panel.x + 1, y, item, qty, style);
        }

        std::string price = std::to_string(panel.price_of(item)) + "g";
        draw_right_aligned(r, panel.x + panel.width - 1, y, price,
                           Style{panel.price_fg, style.bg});
    }
}

void draw_confirm(Renderer& r, const TradeState& state, int x, int y) {
    const int w = 16;
    draw_box(r, x, y - 1, w, 3, CONFIRM_BG);

    draw_string(r, x + 1, y - 1, state.selling ? "Sell this item?" : "Buy this item?",
                Style{Colour{255, 255, 255}, CONFIRM_BG});

    Colour yes_bg = state.confirm_cursor == 0 ? Colour{60, 60, 50} : CONFIRM_BG;
    Colour no_bg = state.confirm_cursor == 1 ? Colour{60, 60, 50} : CONFIRM_BG;
    draw_string(r, x + 2, y + 1, " Yes ", Style{Colour{100, 255, 100}, yes_bg});
    draw_string(r, x + 9, y + 1, " No ", Style{Colour{255, 100, 100}, no_bg});
}

}

void draw_trade(Renderer& r, const TradeState& state, const Player& player,
                const Viewport& view) {
    if (!state.active || !state.merchant) return;

    const Npc& merchant = *state.merchant;

    int panel_w = (view.cols - 8) / 2;
    int box_h = std::min(view.rows - 4, 24);
    int box_y = 2;
    int left_x = 2;
    int right_x = left_x + panel_w + 2;

    draw_box(r, left_x, box_y, right_x + panel_w - left_x, box_h, PANEL_BG);

    draw_string(r, left_x + 1, box_y, "TRADE with " + merchant.name(),
                Style{Colour{255, 255, 255}, PANEL_BG});
    draw_right_aligned(r, right_x + panel_w - 1, box_y,
                       "Gold: " + std::to_string(player.gold()),
                       Style{Colour{255, 215, 0}, PANEL_BG});

    int rows_top = box_y + 2;
    int max_visible = box_h - 4;

    Panel mine;
    mine.x = left_x;
    mine.width = panel_w;
    mine.stock = &player.inventory();
    mine.cursor = state.player_cursor;
    mine.focused = state.on_player_panel;
    mine.heading = "Your Items:";
    mine.empty_text = "(empty)";
    mine.price_fg = Colour{180, 180, 80};
    mine.price_of = trade_sell_price;

    Panel theirs;
    theirs.x = right_x;
    theirs.width = panel_w;
    theirs.stock = &merchant.shop_inventory();
    theirs.cursor = state.shop_cursor;
    theirs.focused = !state.on_player_panel;
    theirs.heading = merchant.name() + "'s Wares:";
    theirs.empty_text = "(no stock)";
    theirs.price_fg = Colour{255, 180, 80};
    theirs.price_of = trade_buy_price;
    theirs.show_stock_count = true;

    draw_panel(r, mine, rows_top, max_visible);
    draw_panel(r, theirs, rows_top, max_visible);

    draw_string(r, left_x + 1, box_y + box_h - 1,
                "[TAB] switch  [ENTER] select  [ESC] close",
                Style{Colour{100, 100, 100}, PANEL_BG});

    if (!state.message.empty()) {
        draw_string(r, left_x + 1, box_y + box_h - 2, state.message,
                    Style{Colour{255, 255, 100}, PANEL_BG});
    }

    if (state.confirming) {
        draw_confirm(r, state, left_x + panel_w / 2 - 8, box_y + box_h / 2);
    }
}

}
