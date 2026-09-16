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

    // Owns an SDL_Window. Copying would double-destroy it.
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    bool init(const WindowConfig& config = {});
    void shutdown();

    SDL_Window* get() const { return window_; }
    int width() const { return width_; }
    int height() const { return height_; }

    // Record a new size after an SDL_WINDOWEVENT_SIZE_CHANGED. Without this
    // the cached dimensions stay at the startup size and the view is laid out
    // for a window that no longer exists.
    void on_resized(int w, int h);

private:
    SDL_Window* window_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};
