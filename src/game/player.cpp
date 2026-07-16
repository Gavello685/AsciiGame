#include "game/player.h"

void Player::spawn(int x, int y) {
    x_ = x;
    y_ = y;
}

void Player::move(int dx, int dy, const Map& map) {
    int nx = x_ + dx;
    int ny = y_ + dy;
    if (map.is_passable(nx, ny)) {
        x_ = nx;
        y_ = ny;
    }
}
