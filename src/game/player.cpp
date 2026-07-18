#include "game/player.h"
#include "game/world.h"
#include <algorithm>

void Player::spawn(int x, int y) {
    x_ = x;
    y_ = y;
    hp_ = stats_.max_hp();
}

void Player::move(int dx, int dy, const World& world) {
    int nx = x_ + dx;
    int ny = y_ + dy;
    if (world.is_passable(nx, ny)) {
        x_ = nx;
        y_ = ny;
    }
}

bool Player::can_move(int dx, int dy, const World& world) const {
    return world.is_passable(x_ + dx, y_ + dy);
}

void Player::add_item(const Item& item, int count) {
    // Try to stack with existing
    for (auto& [existing, qty] : inventory_) {
        if (existing.stacks_with(item)) {
            qty += count;
            current_weight_ += item.weight() * count;
            return;
        }
    }
    // New stack
    inventory_.emplace_back(item, count);
    current_weight_ += item.weight() * count;
}

bool Player::remove_item(int index, int count) {
    if (index < 0 || index >= static_cast<int>(inventory_.size())) return false;
    auto& [item, qty] = inventory_[index];
    if (qty < count) return false;

    current_weight_ -= item.weight() * count;
    qty -= count;
    if (qty <= 0) {
        inventory_.erase(inventory_.begin() + index);
    }
    return true;
}

bool Player::has_item(const std::string& name) const {
    for (const auto& [item, qty] : inventory_) {
        if (item.name() == name && qty > 0) return true;
    }
    return false;
}

int Player::find_item(const std::string& name) const {
    for (int i = 0; i < static_cast<int>(inventory_.size()); ++i) {
        if (inventory_[i].first.name() == name && inventory_[i].second > 0) return i;
    }
    return -1;
}

bool Player::equip(int inventory_index) {
    if (inventory_index < 0 || inventory_index >= static_cast<int>(inventory_.size())) return false;
    const Item& item = inventory_[inventory_index].first;
    if (!item.is_equippable()) return false;

    EquipSlot slot = item.equip_slot();

    // If slot is occupied, unequip first
    if (equipment_.count(slot)) {
        unequip(slot);
    }

    // Equip: move from inventory to equipment
    equipment_[slot] = item;
    remove_item(inventory_index, 1);
    return true;
}

bool Player::unequip(EquipSlot slot) {
    auto it = equipment_.find(slot);
    if (it == equipment_.end()) return false;

    // Add back to inventory
    add_item(it->second, 1);
    equipment_.erase(it);
    return true;
}

const Item* Player::equipped_at(EquipSlot slot) const {
    auto it = equipment_.find(slot);
    if (it == equipment_.end()) return nullptr;
    return &it->second;
}

int Player::total_attack() const {
    int atk = stats_.melee_attack();
    for (const auto& [slot, item] : equipment_) {
        atk += item.attack();
    }
    return atk;
}

int Player::total_defense() const {
    int def = stats_.defense_bonus();
    for (const auto& [slot, item] : equipment_) {
        def += item.defense();
    }
    return def;
}

int Player::total_damage_variance() const {
    int var = 0;
    for (const auto& [slot, item] : equipment_) {
        var += item.damage_variance();
    }
    return var;
}

bool Player::has_light_source() const {
    for (const auto& [slot, item] : equipment_) {
        if ((slot == EquipSlot::Hand_L || slot == EquipSlot::Hand_R) &&
            (item.name() == "Torch" || item.name() == "Lantern")) {
            return true;
        }
    }
    return false;
}

int Player::torch_radius() const {
    for (const auto& [slot, item] : equipment_) {
        if ((slot == EquipSlot::Hand_L || slot == EquipSlot::Hand_R)) {
            if (item.name() == "Torch") return 5;
            if (item.name() == "Lantern") return 7;
        }
    }
    return 0;
}

void Player::recalc_weight() {
    current_weight_ = 0;
    for (const auto& [item, qty] : inventory_) {
        current_weight_ += item.weight() * qty;
    }
}

void Player::check_level_up() {
    while (xp_ >= xp_to_next_level()) {
        xp_ -= xp_to_next_level();
        level_++;
        stats_.str += 1;
        stats_.dex += 1;
        stats_.con += 1;
        hp_ = stats_.max_hp(); // Full heal on level up
    }
}
