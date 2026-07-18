#pragma once

#include <cstdint>

struct PlayerStats {
    int str = 10;
    int dex = 10;
    int con = 10;
    int intel = 10;
    int wis = 10;
    int cha = 10;

    int carry_capacity() const { return str * 15; }

    int max_hp() const { return 10 + con; }

    int melee_attack() const { return str / 3; }

    int ranged_attack() const { return dex / 3; }

    int defense_bonus() const { return dex / 4; }

    int social_modifier() const { return (cha - 10) / 2; }

    int perception() const { return wis; }
};
