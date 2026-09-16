#pragma once

#include <cstdint>

namespace ui {

// The keys the game reacts to, named independently of SDL so input handling
// can be exercised without a window or an event loop. main() maps SDL
// keycodes onto these before dispatching.
enum class Key : uint8_t {
    None,
    Up, Down, Left, Right,
    Enter, Space, Escape, Tab, Period,
    BracketLeft, BracketRight,
    A, B, C, D, G, I, M, R, S, W, X, Z,
};

}
