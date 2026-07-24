#include "game/enemy.h"
#include "game/world.h"
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>

Enemy::Enemy(int x, int y, uint32_t glyph, const std::string& name,
             uint8_t fg_r, uint8_t fg_g, uint8_t fg_b,
             int hp, int max_hp, int attack, int defense, int xp_value,
             int damage_variance,
             const std::vector<std::pair<Item, int>>& drops)
    : x_(x), y_(y), glyph_(glyph), name_(name),
      fg_r_(fg_r), fg_g_(fg_g), fg_b_(fg_b),
      hp_(hp), max_hp_(max_hp), attack_(attack), defense_(defense),
      damage_variance_(damage_variance), xp_value_(xp_value), drops_(drops) {
}

void Enemy::take_damage(int damage) {
    hp_ -= damage;
    if (hp_ < 0) hp_ = 0;
}

bool Enemy::check_death() {
    return hp_ <= 0;
}

bool Enemy::night_predator() const {
    return name_ == "Wolf" || name_ == "Bat" || name_ == "Zombie" || name_ == "Spider";
}

// ── Enemy type database ───────────────────────────────────────────

static std::vector<std::pair<Item, int>> drop_list(const char* item_name, int qty) {
    const Item* it = find_item(item_name);
    if (it) return {{*it, qty}};
    return {};
}

Enemy make_enemy(const std::string& name, int x, int y) {
    // name → (glyph, colors, hp, atk, def, xp, variance, drops)
    if (name == "Goblin")
        return Enemy(x, y, 'g', "Goblin", 80, 180, 80, 12, 12, 4, 1, 8, 1, drop_list("Gold Coin", 1));
    if (name == "Ogre")
        return Enemy(x, y, 'o', "Ogre", 200, 100, 60, 25, 25, 7, 3, 20, 2, drop_list("Gold Coin", 3));
    if (name == "Spider" || name == "Giant Spider")
        return Enemy(x, y, 's', name, 120, 120, 120, 8, 8, 3, 0, 5, 1, drop_list("Health Potion", 1));
    if (name == "Bat")
        return Enemy(x, y, 'b', "Bat", 100, 100, 160, 4, 4, 1, 0, 2, 0);
    if (name == "Scorpion")
        return Enemy(x, y, 's', "Scorpion", 180, 140, 60, 6, 6, 3, 0, 4, 1);
    if (name == "Frog")
        return Enemy(x, y, 'f', "Frog", 60, 160, 60, 4, 4, 1, 0, 2, 0);
    if (name == "Wolf")
        return Enemy(x, y, 'w', "Wolf", 160, 160, 160, 10, 10, 4, 1, 7, 1, drop_list("Cooked Meat", 1));
    if (name == "Bandit")
        return Enemy(x, y, 'B', "Bandit", 200, 160, 120, 14, 14, 5, 1, 10, 1, drop_list("Gold Coin", 2));
    if (name == "Snake")
        return Enemy(x, y, 's', "Snake", 80, 200, 80, 6, 6, 3, 0, 4, 1);
    if (name == "Zombie")
        return Enemy(x, y, 'z', "Zombie", 120, 160, 120, 16, 16, 4, 0, 8, 0, drop_list("Old Scroll", 1));
    if (name == "Troll")
        return Enemy(x, y, 'T', "Troll", 90, 140, 90, 30, 30, 8, 2, 25, 2, drop_list("Stone", 2));
    if (name == "Yeti")
        return Enemy(x, y, 'Y', "Yeti", 220, 230, 240, 28, 28, 7, 1, 22, 2, drop_list("Cooked Meat", 1));
    // Default: Rat
    return Enemy(x, y, 'r', "Rat", 160, 120, 80, 5, 5, 2, 0, 3, 0, drop_list("Bread", 1));
}

void Enemy::assign_default_drops() {
    drops_ = make_enemy(name_, x_, y_).drops();
}

bool Enemy::can_move_to(int x, int y, const World& world) const {
    if (!world.is_passable(x, y)) return false;
    if (world.has_enemy_at(x, y)) return false;
    return true;
}

void Enemy::update(World& world, int player_x, int player_y) {
    int dist = std::abs(x_ - player_x) + std::abs(y_ - player_y);

    // Chase if player is within detection range (8 tiles, requires visibility)
    // Night predators ignore darkness and hunt at longer range (12 tiles)
    bool can_sense = (dist <= 8 && world.get_tile(x_, y_).visible) ||
                     (night_predator() && dist <= 12);
    if (can_sense) {
        state_ = EnemyState::Chasing;
    } else if (state_ == EnemyState::Chasing && dist > 12) {
        state_ = EnemyState::Idle;
        idle_timer_ = 0;
    }

    switch (state_) {
        case EnemyState::Idle:
            idle_timer_++;
            if (idle_timer_ > 20 + std::rand() % 30) {
                state_ = EnemyState::Wandering;
                wander_timer_ = 5 + std::rand() % 10;
                const int dirs[][2] = {{0,1},{0,-1},{1,0},{-1,0}};
                int d = std::rand() % 4;
                wander_dx_ = dirs[d][0];
                wander_dy_ = dirs[d][1];
            }
            break;

        case EnemyState::Chasing:
            chase_player(player_x, player_y, world);
            break;

        case EnemyState::Wandering:
            wander(world);
            break;
    }
}

// Pack world coordinates into a single int64 for use as hash key
static int64_t pack_coord(int x, int y) {
    return (static_cast<int64_t>(x) << 32) | static_cast<uint32_t>(y);
}

void Enemy::chase_player(int player_x, int player_y, World& world) {
    if (x_ == player_x && y_ == player_y) return;
    if (std::abs(x_ - player_x) + std::abs(y_ - player_y) == 1) return;

    // BFS using world-space coordinates with hash map
    std::unordered_map<int64_t, int64_t> came_from;
    std::queue<std::pair<int,int>> bfs;
    bfs.push({x_, y_});
    came_from[pack_coord(x_, y_)] = pack_coord(x_, y_);

    const int dirs[][2] = {{0,-1},{0,1},{-1,0},{1,0}};
    int goal_x = -1, goal_y = -1;

    // Limit BFS to avoid infinite searches across chunks
    int max_steps = 200;
    int steps = 0;

    while (!bfs.empty() && goal_x == -1 && steps < max_steps) {
        auto [cx, cy] = bfs.front();
        bfs.pop();
        steps++;

        for (auto [ddx, ddy] : dirs) {
            int nx = cx + ddx;
            int ny = cy + ddy;

            if (!world.is_passable(nx, ny)) continue;

            // If neighbor is the player, (cx,cy) is our goal — stop adjacent
            if (nx == player_x && ny == player_y) {
                goal_x = cx;
                goal_y = cy;
                break;
            }

            // Don't walk through other enemies
            if (world.has_enemy_at(nx, ny)) continue;

            int64_t key = pack_coord(nx, ny);
            if (came_from.find(key) != came_from.end()) continue;
            came_from[key] = pack_coord(cx, cy);
            bfs.push({nx, ny});
        }
    }

    if (goal_x == -1) return;

    // Trace back from goal to find first step
    int tx = goal_x, ty = goal_y;
    int64_t my_key = pack_coord(x_, y_);
    while (came_from[pack_coord(tx, ty)] != my_key) {
        int64_t prev = came_from[pack_coord(tx, ty)];
        tx = static_cast<int>(prev >> 32);
        ty = static_cast<int>(prev & 0xFFFFFFFF);
    }

    x_ = tx;
    y_ = ty;
}

void Enemy::wander(World& world) {
    wander_timer_--;
    if (wander_timer_ <= 0) {
        state_ = EnemyState::Idle;
        idle_timer_ = 0;
        return;
    }

    int nx = x_ + wander_dx_;
    int ny = y_ + wander_dy_;

    if (can_move_to(nx, ny, world)) {
        x_ = nx;
        y_ = ny;
    } else {
        const int dirs[][2] = {{0,1},{0,-1},{1,0},{-1,0}};
        int d = std::rand() % 4;
        wander_dx_ = dirs[d][0];
        wander_dy_ = dirs[d][1];
    }
}
