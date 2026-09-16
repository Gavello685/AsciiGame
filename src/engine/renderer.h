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

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Owns an SDL_Renderer, a TTF_Font and a texture cache.
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    bool init(SDL_Window* window, const std::string& font_path, int font_size);
    void shutdown();

    void clear();
    void present();

    void begin_frame(int view_cols, int view_rows);
    void set_cell(int x, int y, const Cell& cell);
    void render_grid();

    // Dev tool: dump the current framebuffer to a BMP file (call after render_grid)
    bool save_screenshot(const std::string& path);

    int cell_width() const { return cell_w_; }
    int cell_height() const { return cell_h_; }

private:
    // Glyphs are cached white and tinted per draw with SDL_SetTextureColorMod.
    // Keying the cache on colour as well meant one texture per glyph/colour
    // pair, which grows without bound once cells are shaded by light level.
    SDL_Texture* get_glyph_texture(uint32_t glyph);

    SDL_Renderer* renderer_ = nullptr;
    TTF_Font* font_ = nullptr;
    int cell_w_ = 0;
    int cell_h_ = 0;
    int grid_w_ = 0;
    int grid_h_ = 0;
    std::vector<Cell> cells_;
    std::unordered_map<uint32_t, SDL_Texture*> glyph_cache_;
};
