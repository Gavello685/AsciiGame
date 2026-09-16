#include "engine/renderer.h"
#include <iostream>

Renderer::Renderer() {}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::init(SDL_Window* window, const std::string& font_path, int font_size) {
    renderer_ = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer_) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        return false;
    }

    if (TTF_Init() == -1) {
        std::cerr << "TTF_Init failed: " << TTF_GetError() << "\n";
        return false;
    }

    font_ = TTF_OpenFont(font_path.c_str(), font_size);
    if (!font_) {
        std::cerr << "TTF_OpenFont failed: " << TTF_GetError() << "\n";
        std::cerr << "  Tried: " << font_path << "\n";
        return false;
    }

    TTF_SizeText(font_, " ", &cell_w_, &cell_h_);
    return true;
}

void Renderer::shutdown() {
    for (auto& [key, tex] : glyph_cache_) {
        SDL_DestroyTexture(tex);
    }
    glyph_cache_.clear();

    if (font_) {
        TTF_CloseFont(font_);
        font_ = nullptr;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    TTF_Quit();
}

SDL_Texture* Renderer::get_glyph_texture(uint32_t glyph) {
    auto it = glyph_cache_.find(glyph);
    if (it != glyph_cache_.end()) return it->second;

    SDL_Color white = {255, 255, 255, 255};
    std::string s(1, static_cast<char>(glyph));
    SDL_Surface* surf = TTF_RenderText_Blended(font_, s.c_str(), white);
    if (!surf) return nullptr;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_FreeSurface(surf);
    if (!tex) return nullptr;

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    glyph_cache_[glyph] = tex;
    return tex;
}

void Renderer::clear() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
}

void Renderer::present() {
    SDL_RenderPresent(renderer_);
}

void Renderer::set_cell(int x, int y, const Cell& cell) {
    if (x < 0 || x >= grid_w_ || y < 0 || y >= grid_h_) return;
    cells_[y * grid_w_ + x] = cell;
}

void Renderer::begin_frame(int view_cols, int view_rows) {
    grid_w_ = view_cols;
    grid_h_ = view_rows;
    cells_.assign(grid_w_ * grid_h_, Cell{});
}

bool Renderer::save_screenshot(const std::string& path) {
    int w, h;
    if (SDL_GetRendererOutputSize(renderer_, &w, &h) != 0) return false;
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surf) return false;
    bool ok = SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888,
                                   surf->pixels, surf->pitch) == 0;
    if (ok) ok = SDL_SaveBMP(surf, path.c_str()) == 0;
    SDL_FreeSurface(surf);
    return ok;
}

void Renderer::render_grid() {
    SDL_Rect dest;
    dest.w = cell_w_;
    dest.h = cell_h_;

    for (int y = 0; y < grid_h_; ++y) {
        for (int x = 0; x < grid_w_; ++x) {
            const Cell& cell = cells_[y * grid_w_ + x];

            dest.x = x * cell_w_;
            dest.y = y * cell_h_;

            // Draw background
            SDL_SetRenderDrawColor(renderer_, cell.bg_r, cell.bg_g, cell.bg_b, 255);
            SDL_RenderFillRect(renderer_, &dest);

            // Draw glyph from cache, tinted to the cell's foreground colour
            if (cell.glyph != ' ') {
                SDL_Texture* tex = get_glyph_texture(cell.glyph);
                if (tex) {
                    SDL_SetTextureColorMod(tex, cell.fg_r, cell.fg_g, cell.fg_b);
                    SDL_RenderCopy(renderer_, tex, nullptr, &dest);
                }
            }
        }
    }
}
