#pragma once

#include "engine/renderer.h"
#include "ui/ui_state.h"

class Player;
class World;

namespace ui {

// Item list, paper doll, and — depending on the mode — the action bar, the
// examine card or the gift picker layered on top.
void draw_inventory(Renderer& r, const Player& player, const UiState& ui,
                    const Viewport& view);

// Recipe list with have/need ingredient counts; unaffordable rows dimmed.
void draw_craft(Renderer& r, const Player& player, const UiState& ui,
                const Viewport& view);

// Buildable picker with material costs, shown while the build cursor is live.
// Anchored to the top-left corner, so it needs no viewport.
void draw_build_panel(Renderer& r, const Player& player, const UiState& ui);

// Existing zones, or the type picker once a rectangle has been marked out.
void draw_zone_panel(Renderer& r, const World& world, const UiState& ui);

// Attributes with their derived effects, plus combat totals.
void draw_character_sheet(Renderer& r, const Player& player, const Viewport& view);

// Chunk-scale map with biome colours, structure markers and a legend. Takes
// UiState by reference because it refreshes the cached chunk summaries.
void draw_world_map(Renderer& r, const World& world, const Player& player,
                    UiState& ui, const Viewport& view);

// Speaker text and reply options for the NPC the player is talking to.
void draw_dialogue(Renderer& r, World& world, const UiState& ui,
                   const Viewport& view);

}
