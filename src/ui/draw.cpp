#include "ui/draw.h"

#include <algorithm>
#include <cmath>

namespace ui {

void draw_string(Renderer& r, int x, int y, const std::string& s, const Style& style) {
    Cell cell;
    cell.fg_r = style.fg.r; cell.fg_g = style.fg.g; cell.fg_b = style.fg.b;
    cell.bg_r = style.bg.r; cell.bg_g = style.bg.g; cell.bg_b = style.bg.b;
    for (size_t i = 0; i < s.size(); ++i) {
        cell.glyph = static_cast<uint32_t>(static_cast<unsigned char>(s[i]));
        r.set_cell(x + static_cast<int>(i), y, cell);
    }
}

void draw_string(Renderer& r, int x, int y, const std::string& s, Colour fg, Colour bg) {
    draw_string(r, x, y, s, Style{fg, bg});
}

void draw_centered(Renderer& r, int x, int width, int y, const std::string& s,
                   Colour fg, Colour bg) {
    // Signed arithmetic throughout: a string wider than the box would
    // otherwise underflow to a huge unsigned offset and vanish.
    int offset = (width - static_cast<int>(s.size())) / 2;
    draw_string(r, x + std::max(0, offset), y, s, Style{fg, bg});
}

void draw_right_aligned(Renderer& r, int right_x, int y, const std::string& s,
                        const Style& style) {
    int x = right_x - static_cast<int>(s.size());
    draw_string(r, std::max(0, x), y, s, style);
}

void draw_box(Renderer& r, int x, int y, int w, int h, Colour bg) {
    Cell cell;
    cell.glyph = ' ';
    cell.bg_r = bg.r; cell.bg_g = bg.g; cell.bg_b = bg.b;
    for (int yy = y; yy < y + h; ++yy) {
        for (int xx = x; xx < x + w; ++xx) r.set_cell(xx, yy, cell);
    }
}

void draw_glyph(Renderer& r, int x, int y, uint32_t glyph, Colour fg, Colour bg) {
    Cell cell;
    cell.glyph = glyph;
    cell.fg_r = fg.r; cell.fg_g = fg.g; cell.fg_b = fg.b;
    cell.bg_r = bg.r; cell.bg_g = bg.g; cell.bg_b = bg.b;
    r.set_cell(x, y, cell);
}

void draw_item_row(Renderer& r, int x, int y, const Item& item, int qty,
                   const Style& style) {
    draw_glyph(r, x, y, item.glyph(),
               Colour{item.fg_r(), item.fg_g(), item.fg_b()}, style.bg);

    std::string label = item.name();
    if (qty > 1) label += " (" + std::to_string(qty) + ")";
    draw_string(r, x + 2, y, label, style);
}

Colour shade(Colour c, float factor) {
    auto scale = [factor](uint8_t channel) {
        float value = static_cast<float>(channel) * factor;
        if (value < 0.0f) value = 0.0f;
        if (value > 255.0f) value = 255.0f;
        return static_cast<uint8_t>(value);
    };
    return Colour{scale(c.r), scale(c.g), scale(c.b)};
}

std::vector<std::string> wrap_text(const std::string& text, int width) {
    std::vector<std::string> lines;
    if (width <= 0) return lines;

    std::string remaining = text;
    while (!remaining.empty()) {
        if (static_cast<int>(remaining.size()) <= width) {
            lines.push_back(remaining);
            break;
        }
        size_t split = remaining.rfind(' ', static_cast<size_t>(width));
        if (split == std::string::npos || split == 0) split = static_cast<size_t>(width);
        lines.push_back(remaining.substr(0, split));
        // Skip the space we broke on, but not a hard mid-word split.
        remaining = remaining.substr(split < static_cast<size_t>(width) ? split + 1 : split);
    }
    return lines;
}

}
