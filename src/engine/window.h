#pragma once

#include <SDL.h>
#include <string>

struct WindowConfig {
    std::string title = "ASCII Game";
    int width = 1024;
    int height = 768;
};

class Window {
public:
    Window();
    ~Window();

    bool init(const WindowConfig& config = {});
    void shutdown();

    SDL_Window* get() const { return window_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    SDL_Window* window_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};
