#pragma once

#include "game/map.h"

class Player {
public:
    Player() = default;

    void spawn(int x, int y);
    void move(int dx, int dy, const Map& map);
    bool can_move(int dx, int dy, const Map& map) const;

    int x() const { return x_; }
    int y() const { return y_; }

private:
    int x_ = 0;
    int y_ = 0;
};
