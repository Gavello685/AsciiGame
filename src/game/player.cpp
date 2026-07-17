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

bool Player::can_move(int dx, int dy, const Map& map) const {
    return map.is_passable(x_ + dx, y_ + dy);
}
