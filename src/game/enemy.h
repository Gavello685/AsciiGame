#pragma once

#include "game/item.h"
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

class World;

enum class EnemyState : uint8_t {
    Idle,
    Chasing,
    Wandering,
};

class Enemy {
public:
    Enemy() = default;
    Enemy(int x, int y, uint32_t glyph, const std::string& name,
          uint8_t fg_r, uint8_t fg_g, uint8_t fg_b,
          int hp, int max_hp, int attack, int defense, int xp_value,
          int damage_variance = 0,
          const std::vector<std::pair<Item, int>>& drops = {});

    int x() const { return x_; }
    int y() const { return y_; }

    uint32_t glyph() const { return glyph_; }
    const std::string& name() const { return name_; }

    uint8_t fg_r() const { return fg_r_; }
    uint8_t fg_g() const { return fg_g_; }
    uint8_t fg_b() const { return fg_b_; }

    int hp() const { return hp_; }
    int max_hp() const { return max_hp_; }
    int attack() const { return attack_; }
    int defense() const { return defense_; }
    int damage_variance() const { return damage_variance_; }
    int xp_value() const { return xp_value_; }

    bool is_alive() const { return hp_ > 0; }
    void take_damage(int damage);
    bool check_death();

    const std::vector<std::pair<Item, int>>& drops() const { return drops_; }

    void update(World& world, int player_x, int player_y);

    bool can_move_to(int x, int y, const World& world) const;

private:
    void chase_player(int player_x, int player_y, World& world);
    void wander(World& world);

    int x_ = 0;
    int y_ = 0;
    uint32_t glyph_ = ' ';
    std::string name_;
    uint8_t fg_r_ = 255, fg_g_ = 255, fg_b_ = 255;

    int hp_ = 10;
    int max_hp_ = 10;
    int attack_ = 2;
    int defense_ = 0;
    int damage_variance_ = 0;
    int xp_value_ = 5;

    std::vector<std::pair<Item, int>> drops_;
    EnemyState state_ = EnemyState::Idle;

    int idle_timer_ = 0;
    int wander_timer_ = 0;
    int wander_dx_ = 0;
    int wander_dy_ = 0;
};
