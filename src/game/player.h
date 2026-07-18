#pragma once

#include "game/item.h"
#include "game/stats.h"
#include <vector>
#include <utility>
#include <unordered_map>

class World;

class Player {
public:
    Player() = default;

    void spawn(int x, int y);
    void move(int dx, int dy, const World& world);
    bool can_move(int dx, int dy, const World& world) const;

    int x() const { return x_; }
    int y() const { return y_; }

    PlayerStats& stats() { return stats_; }
    const PlayerStats& stats() const { return stats_; }

    int gold() const { return gold_; }
    void add_gold(int amount) { gold_ += amount; }
    void set_gold(int v) { gold_ = v; }
    bool spend_gold(int amount) {
        if (gold_ < amount) return false;
        gold_ -= amount;
        return true;
    }

    int xp() const { return xp_; }
    int level() const { return level_; }
    void add_xp(int amount) { xp_ += amount; check_level_up(); }
    void set_xp(int v) { xp_ = v; }
    void set_level(int v) { level_ = v; }
    int xp_to_next_level() const { return level_ * 25; }
    bool is_full_hp() const { return hp_ >= max_hp(); }
    int hp() const { return hp_; }
    int max_hp() const { return stats_.max_hp(); }
    void take_damage(int damage) {
        int actual = damage - total_defense() / 2;
        if (actual < 1) actual = 1;
        hp_ -= actual;
        if (hp_ < 0) hp_ = 0;
    }
    int calc_damage(int damage) const {
        int actual = damage - total_defense() / 2;
        if (actual < 1) actual = 1;
        return actual;
    }
    void heal(int amount) {
        hp_ += amount;
        if (hp_ > max_hp()) hp_ = max_hp();
    }
    bool is_dead() const { return hp_ <= 0; }

    int current_weight() const { return current_weight_; }
    int carry_capacity() const { return stats_.carry_capacity(); }
    bool is_over_encumbered() const { return current_weight_ >= carry_capacity(); }
    bool can_carry(int additional_weight) const {
        return current_weight_ + additional_weight <= carry_capacity();
    }

    // Inventory: vector of (item, count)
    const std::vector<std::pair<Item, int>>& inventory() const { return inventory_; }

    int inventory_count(int index) const {
        if (index < 0 || index >= static_cast<int>(inventory_.size())) return 0;
        return inventory_[index].second;
    }

    const Item& inventory_item(int index) const {
        return inventory_[index].first;
    }

    void add_item(const Item& item, int count = 1);
    bool remove_item(int index, int count = 1);
    bool has_item(const std::string& name) const;
    int find_item(const std::string& name) const;

    // Equipment
    const std::unordered_map<EquipSlot, Item>& equipment() const { return equipment_; }

    bool equip(int inventory_index);
    bool unequip(EquipSlot slot);
    void equip_to_slot(EquipSlot slot, const Item& item) { equipment_[slot] = item; }
    bool is_slot_occupied(EquipSlot slot) const { return equipment_.count(slot) > 0; }
    const Item* equipped_at(EquipSlot slot) const;

    int total_attack() const;
    int total_defense() const;
    int total_damage_variance() const;

    // Light source
    bool has_light_source() const;
    int torch_radius() const;

private:
    void recalc_weight();
    void check_level_up();

    int x_ = 0;
    int y_ = 0;
    PlayerStats stats_;
    int gold_ = 50;
    int hp_ = 20;
    int xp_ = 0;
    int level_ = 1;
    int current_weight_ = 0;
    std::vector<std::pair<Item, int>> inventory_;
    std::unordered_map<EquipSlot, Item> equipment_;
};
