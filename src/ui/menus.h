#pragma once

#include "engine/renderer.h"
#include "ui/ui_state.h"

struct Settings;

namespace ui {

// Title screen, drawn instead of the world before a game starts.
void draw_main_menu(Renderer& r, const UiState& ui, const Viewport& view);

// Overlays drawn on top of a paused game.
void draw_pause_menu(Renderer& r, const UiState& ui, const Viewport& view);
void draw_settings(Renderer& r, const UiState& ui, const Settings& settings,
                   const Viewport& view);
void draw_save_slots(Renderer& r, const UiState& ui, const Viewport& view);
void draw_death_screen(Renderer& r, const Viewport& view);

}
