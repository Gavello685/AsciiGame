#pragma once

#include "engine/renderer.h"
#include "game/item.h"
#include <string>
#include <vector>

namespace ui {

// A foreground/background colour pair, so call sites read as one argument
// instead of six loose bytes.
struct Colour {
    uint8_t r = 255, g = 255, b = 255;
};

struct Style {
    Colour fg;
    Colour bg;
};

// Write a string left-to-right. Out-of-range cells are dropped by set_cell.
void draw_string(Renderer& r, int x, int y, const std::string& s, const Style& style);

// Convenience overload for the common "default background" case.
void draw_string(Renderer& r, int x, int y, const std::string& s, Colour fg,
                 Colour bg = Colour{0, 0, 0});

// Centre a string within a width, clamping to x = 0 when it does not fit.
void draw_centered(Renderer& r, int x, int width, int y, const std::string& s,
                   Colour fg, Colour bg = Colour{0, 0, 0});

// Right-align a string so it ends at the given column.
void draw_right_aligned(Renderer& r, int right_x, int y, const std::string& s,
                        const Style& style);

// Fill a rectangle with blanks on the given background.
void draw_box(Renderer& r, int x, int y, int w, int h, Colour bg);

// Write a single glyph in its own colour, leaving the background alone.
void draw_glyph(Renderer& r, int x, int y, uint32_t glyph, Colour fg,
                Colour bg = Colour{0, 0, 0});

// The item's glyph in its own colour followed by its name, suffixed with the
// stack count when there is more than one. Shared by the inventory, gift and
// trade lists so they stay visually consistent.
void draw_item_row(Renderer& r, int x, int y, const Item& item, int qty,
                   const Style& style);

// Scale a colour's brightness, clamped to the 0-255 range.
Colour shade(Colour c, float factor);

// Break text into lines no wider than width, splitting on spaces where it can.
std::vector<std::string> wrap_text(const std::string& text, int width);

}
