#include "ui/menus.h"

#include "game/save.h"
#include "game/settings.h"
#include "ui/draw.h"

#include <algorithm>
#include <string>

namespace ui {

namespace {

const Colour MENU_BG{20, 20, 40};
const Colour SLOT_BG{15, 15, 35};
const Colour SETTINGS_BG{20, 30, 20};
const Colour HINT{100, 100, 100};

// Highlight the row under the cursor.
Colour row_backdrop(bool selected, const Colour& base) {
    return selected ? Colour{60, 60, base.b} : base;
}

std::string format_play_time(int seconds) {
    int minutes = seconds / 60;
    if (minutes < 60) return std::to_string(minutes) + "m";
    return std::to_string(minutes / 60) + "h" + std::to_string(minutes % 60) + "m";
}

}

void draw_main_menu(Renderer& r, const UiState& ui, const Viewport& view) {
    for (int x = 0; x < view.cols; ++x) {
        Cell rule;
        rule.glyph = '=';
        rule.fg_r = 60; rule.fg_g = 60; rule.fg_b = 80;
        r.set_cell(x, 1, rule);
        r.set_cell(x, view.rows - 2, rule);
    }

    draw_centered(r, 0, view.cols, view.rows / 2 - 8, "A S C I I   G A M E",
                  Colour{220, 200, 120});
    draw_centered(r, 0, view.cols, view.rows / 2 - 6, "A fantasy life in letters",
                  Colour{120, 120, 140});

    for (int i = 0; i < MAIN_MENU_OPTION_COUNT; ++i) {
        bool selected = (i == ui.menu_cursor);
        std::string label = (selected ? "> " : "  ") + std::string(MAIN_MENU_OPTIONS[i]);
        Colour fg = selected ? Colour{255, 255, 100} : Colour{160, 160, 160};
        draw_string(r, std::max(0, (view.cols - 12) / 2), view.rows / 2 - 2 + i * 2, label, fg);
    }

    // Surface the last status line here too, so a failed load is visible.
    if (!ui.messages.empty()) {
        draw_centered(r, 0, view.cols, view.rows / 2 + 6, ui.messages.newest().text,
                      Colour{255, 200, 100});
    }

    draw_centered(r, 0, view.cols, view.rows - 5,
                  "WASD move  |  I inventory  |  C craft  |  B build",
                  Colour{80, 80, 100});
    draw_centered(r, 0, view.cols, view.rows - 4,
                  "Z zones  |  X stats  |  M map  |  G grab  |  . wait  |  ESC pause",
                  Colour{80, 80, 100});
}

void draw_pause_menu(Renderer& r, const UiState& ui, const Viewport& view) {
    int w = 24, h = PAUSE_MENU_OPTION_COUNT + 2;
    int x = (view.cols - w) / 2, y = (view.rows - h) / 2;

    draw_box(r, x, y, w, h, MENU_BG);
    draw_string(r, x + 1, y, "PAUSED", Style{Colour{255, 255, 255}, MENU_BG});

    for (int i = 0; i < PAUSE_MENU_OPTION_COUNT; ++i) {
        bool selected = (i == ui.pause_cursor);
        std::string label = (selected ? "> " : "  ") + std::string(PAUSE_MENU_OPTIONS[i]);
        draw_string(r, x + 2, y + 1 + i, label,
                    Style{Colour{200, 200, 200}, row_backdrop(selected, MENU_BG)});
    }
}

void draw_settings(Renderer& r, const UiState& ui, const Settings& settings,
                   const Viewport& view) {
    int w = 40, h = SETTINGS_OPTION_COUNT + 4;
    int x = (view.cols - w) / 2, y = (view.rows - h) / 2;

    draw_box(r, x, y, w, h, SETTINGS_BG);
    draw_string(r, x + 1, y, "SETTINGS", Style{Colour{255, 255, 255}, SETTINGS_BG});

    const int values[SETTINGS_OPTION_COUNT] = {
        settings.load_radius, settings.render_radius, settings.sim_radius
    };

    for (int i = 0; i < SETTINGS_OPTION_COUNT; ++i) {
        bool selected = (i == ui.settings_cursor);
        Colour bg = selected ? Colour{50, 60, 50} : SETTINGS_BG;
        std::string label = (selected ? "> " : "  ") + std::string(SETTINGS_LABELS[i]);
        draw_string(r, x + 2, y + 2 + i, label, Style{Colour{200, 200, 200}, bg});

        for (int notch = 1; notch <= World::MAX_LOAD_RADIUS; ++notch) {
            bool filled = notch <= values[i];
            Cell cell;
            cell.glyph = filled ? '#' : '-';
            cell.fg_r = filled ? 120 : 60;
            cell.fg_g = filled ? 220 : 60;
            cell.fg_b = filled ? 120 : 60;
            cell.bg_r = bg.r; cell.bg_g = bg.g; cell.bg_b = bg.b;
            r.set_cell(x + 24 + notch, y + 2 + i, cell);
        }
    }

    draw_string(r, x + 1, y + h - 1, "[</>] adjust  [ESC] done", Style{HINT, SETTINGS_BG});
}

void draw_save_slots(Renderer& r, const UiState& ui, const Viewport& view) {
    int w = 52, h = MAX_SAVE_SLOTS + 4;
    int x = (view.cols - w) / 2, y = (view.rows - h) / 2;

    draw_box(r, x, y, w, h, SLOT_BG);
    draw_string(r, x + 1, y, ui.mode == GameMode::SaveGame ? "SAVE GAME" : "LOAD GAME",
                Style{Colour{255, 255, 255}, SLOT_BG});

    std::vector<SaveMeta> saves = list_saves();

    for (int slot = 0; slot < MAX_SAVE_SLOTS; ++slot) {
        bool selected = (slot == ui.pause_cursor);
        Colour bg = row_backdrop(selected, SLOT_BG);

        const SaveMeta* meta = nullptr;
        for (const auto& candidate : saves) {
            if (candidate.slot == slot) { meta = &candidate; break; }
        }

        std::string label = "Slot " + std::to_string(slot + 1) + ": ";
        if (meta) {
            label += meta->character_name +
                     "  Lv " + std::to_string(meta->level) +
                     "  Day " + std::to_string(meta->day) +
                     "  " + format_play_time(meta->play_time_seconds);
        } else {
            label += "--- empty ---";
        }

        Colour fg = meta ? Colour{200, 200, 200} : Colour{100, 100, 100};
        draw_string(r, x + 2, y + 1 + slot, label, Style{fg, bg});
    }

    draw_string(r, x + 1, y + h - 1, "[ENTER] select  [ESC] back", Style{HINT, SLOT_BG});
}

void draw_death_screen(Renderer& r, const Viewport& view) {
    draw_box(r, 0, 0, view.cols, view.rows, Colour{30, 0, 0});
    Colour bg{30, 0, 0};
    draw_centered(r, 0, view.cols, view.rows / 2 - 2, "YOU DIED", Colour{255, 50, 50}, bg);
    draw_centered(r, 0, view.cols, view.rows / 2,
                  "Press R to restart, ESC for main menu", Colour{180, 180, 180}, bg);
}

}
