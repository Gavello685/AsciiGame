#pragma once

#include "engine/renderer.h"
#include "ui/ui_state.h"

class Player;
class World;
class TimeSystem;

namespace ui {

// Centre the viewport on the player. The camera is unbounded — the world has
// no edges to clamp against.
Viewport make_viewport(const Renderer& renderer, int window_w, int window_h,
                       const Player& player);

// Terrain, zone tints, placed objects, items, NPCs, enemies and the player,
// shaded by tile light level.
void draw_world(Renderer& r, const World& world, const Player& player,
                const Viewport& view);

// Build mode's placement cursor, coloured by whether the piece can go there.
void draw_build_cursor(Renderer& r, const World& world, const Player& player,
                       const UiState& ui, const Viewport& view);

// Zone mode's rectangle-in-progress and cursor.
void draw_zone_cursor(Renderer& r, const UiState& ui, const Viewport& view);

// Two-row status bar along the bottom.
void draw_hud(Renderer& r, const World& world, const Player& player,
              const TimeSystem& time, const Viewport& view);

// Status log stacked upward from just above the HUD, newest brightest.
void draw_message_log(Renderer& r, const MessageLog& log, const Viewport& view);

}
