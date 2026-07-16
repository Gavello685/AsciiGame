#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include <vector>
#include <unordered_map>

struct Cell {
    uint32_t glyph = ' ';
    uint8_t fg_r = 255, fg_g = 255, fg_b = 255;
    uint8_t bg_r = 0, bg_g = 0, bg_b = 0;
};

struct GlyphKey {
    uint32_t glyph;
    uint8_t r, g, b;
    bool operator==(const GlyphKey& o) const {
        return glyph == o.glyph && r == o.r && g == o.g && b == o.b;
    }
};

struct GlyphHash {
    size_t operator()(const GlyphKey& k) const {
        size_t h = std::hash<uint32_t>()(k.glyph);
        h ^= std::hash<uint8_t>()(k.r) << 1;
        h ^= std::hash<uint8_t>()(k.g) << 2;
        h ^= std::hash<uint8_t>()(k.b) << 3;
        return h;
    }
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(SDL_Window* window, const std::string& font_path, int font_size);
    void shutdown();

    void clear();
    void present();

    void begin_frame(int view_cols, int view_rows);
    void set_cell(int x, int y, const Cell& cell);
    void render_grid();

    int cell_width() const { return cell_w_; }
    int cell_height() const { return cell_h_; }

private:
    SDL_Texture* get_glyph_texture(uint32_t glyph, uint8_t r, uint8_t g, uint8_t b);

    SDL_Renderer* renderer_ = nullptr;
    TTF_Font* font_ = nullptr;
    int cell_w_ = 0;
    int cell_h_ = 0;
    int grid_w_ = 0;
    int grid_h_ = 0;
    std::vector<Cell> cells_;
    std::unordered_map<GlyphKey, SDL_Texture*, GlyphHash> glyph_cache_;
};
