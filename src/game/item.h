#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class ItemType : uint8_t {
    Consumable,
    Equipment,
    Material,
    Key,
    Misc,
};

enum class EquipSlot : uint8_t {
    None,
    Head,
    Shoulder_L,
    Shoulder_R,
    Torso,
    Arm_L,
    Arm_R,
    Hand_L,
    Hand_R,
    Leg_L,
    Leg_R,
    Foot_L,
    Foot_R,
};

enum class UseEffect : uint8_t {
    None,
    Heal,
    Feed,
    Drink,
};

class Item {
public:
    Item() = default;

    Item(const std::string& name, uint32_t glyph,
         uint8_t fg_r, uint8_t fg_g, uint8_t fg_b,
         ItemType type, int weight, int value,
         const std::string& description,
         EquipSlot equip_slot = EquipSlot::None,
         UseEffect use_effect = UseEffect::None,
         int use_amount = 0,
         int attack = 0, int defense = 0,
         int damage_variance = 0);

    const std::string& name() const { return name_; }
    uint32_t glyph() const { return glyph_; }

    uint8_t fg_r() const { return fg_r_; }
    uint8_t fg_g() const { return fg_g_; }
    uint8_t fg_b() const { return fg_b_; }

    ItemType type() const { return type_; }
    int weight() const { return weight_; }
    int value() const { return value_; }
    const std::string& description() const { return description_; }

    EquipSlot equip_slot() const { return equip_slot_; }
    bool is_equippable() const { return equip_slot_ != EquipSlot::None; }

    UseEffect use_effect() const { return use_effect_; }
    int use_amount() const { return use_amount_; }
    bool is_usable() const { return use_effect_ != UseEffect::None; }

    int attack() const { return attack_; }
    int defense() const { return defense_; }
    int damage_variance() const { return damage_variance_; }

    bool stacks_with(const Item& other) const {
        return name_ == other.name_ && type_ == other.type_;
    }

    static bool is_two_handed_slot(EquipSlot s) {
        return s == EquipSlot::Head || s == EquipSlot::Torso ||
               s == EquipSlot::Shoulder_L || s == EquipSlot::Shoulder_R;
    }

    static bool is_one_handed_slot(EquipSlot s) {
        return s == EquipSlot::Hand_L || s == EquipSlot::Hand_R ||
               s == EquipSlot::Arm_L || s == EquipSlot::Arm_R ||
               s == EquipSlot::Leg_L || s == EquipSlot::Leg_R ||
               s == EquipSlot::Foot_L || s == EquipSlot::Foot_R;
    }

    static EquipSlot partner_slot(EquipSlot s) {
        switch (s) {
            case EquipSlot::Shoulder_L: return EquipSlot::Shoulder_R;
            case EquipSlot::Shoulder_R: return EquipSlot::Shoulder_L;
            case EquipSlot::Arm_L: return EquipSlot::Arm_R;
            case EquipSlot::Arm_R: return EquipSlot::Arm_L;
            case EquipSlot::Hand_L: return EquipSlot::Hand_R;
            case EquipSlot::Hand_R: return EquipSlot::Hand_L;
            case EquipSlot::Leg_L: return EquipSlot::Leg_R;
            case EquipSlot::Leg_R: return EquipSlot::Leg_L;
            case EquipSlot::Foot_L: return EquipSlot::Foot_R;
            case EquipSlot::Foot_R: return EquipSlot::Foot_L;
            default: return EquipSlot::None;
        }
    }

private:
    std::string name_;
    uint32_t glyph_ = ' ';
    uint8_t fg_r_ = 255, fg_g_ = 255, fg_b_ = 255;
    ItemType type_ = ItemType::Misc;
    int weight_ = 0;
    int value_ = 0;
    std::string description_;
    EquipSlot equip_slot_ = EquipSlot::None;
    UseEffect use_effect_ = UseEffect::None;
    int use_amount_ = 0;
    int attack_ = 0;
    int defense_ = 0;
    int damage_variance_ = 0;
};

const std::vector<Item>& item_db();
const Item* find_item(const std::string& name);
