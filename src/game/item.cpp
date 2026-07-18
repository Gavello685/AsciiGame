#include "game/item.h"

Item::Item(const std::string& name, uint32_t glyph,
           uint8_t fg_r, uint8_t fg_g, uint8_t fg_b,
           ItemType type, int weight, int value,
           const std::string& description,
           EquipSlot equip_slot, UseEffect use_effect,
           int use_amount, int attack, int defense,
           int damage_variance)
    : name_(name), glyph_(glyph),
      fg_r_(fg_r), fg_g_(fg_g), fg_b_(fg_b),
      type_(type), weight_(weight), value_(value),
      description_(description),
      equip_slot_(equip_slot),
      use_effect_(use_effect), use_amount_(use_amount),
      attack_(attack), defense_(defense),
      damage_variance_(damage_variance) {
}

const std::vector<Item>& item_db() {
    static const std::vector<Item> db = {
        // Consumables
        Item("Bread", '%', 180, 140, 60,
             ItemType::Consumable, 1, 2,
             "A crusty loaf of bread. Restores 10 hunger.",
             EquipSlot::None, UseEffect::Feed, 10),

        Item("Health Potion", '!', 255, 50, 50,
             ItemType::Consumable, 1, 25,
             "A bubbling red liquid. Restores 20 HP.",
             EquipSlot::None, UseEffect::Heal, 20),

        Item("Water Skin", '!', 60, 120, 200,
             ItemType::Consumable, 1, 5,
             "A leather flask of clean water. Restores 15 thirst.",
             EquipSlot::None, UseEffect::Drink, 15),

        // Currency
        Item("Gold Coin", '$', 255, 215, 0,
             ItemType::Misc, 0, 1,
             "A shiny gold coin. Use it to trade with merchants."),

        // Misc / Key items
        Item("Old Scroll", '?', 200, 180, 140,
             ItemType::Misc, 0, 10,
             "A yellowed parchment covered in faded writing."),

        Item("Rusty Key", '!', 150, 150, 150,
             ItemType::Key, 0, 0,
             "An old iron key. It might unlock something."),

        // Equipment - Weapons
        Item("Iron Sword", '/', 180, 180, 200,
             ItemType::Equipment, 4, 30,
             "A sturdy iron blade. Reliable in a fight.",
             EquipSlot::Hand_L, UseEffect::None, 0, 3, 0, 1),

        Item("Iron Shield", 'O', 180, 180, 200,
             ItemType::Equipment, 8, 50,
             "A heavy iron shield. Blocks incoming attacks.",
             EquipSlot::Hand_R, UseEffect::None, 0, 0, 3),

        // Equipment - Armor
        Item("Leather Armor", '}', 140, 100, 60,
             ItemType::Equipment, 8, 40,
             "Simple leather protection. Light and flexible.",
             EquipSlot::Torso, UseEffect::None, 0, 0, 2),

        Item("Wooden Shield", 'O', 140, 100, 40,
             ItemType::Equipment, 5, 20,
             "A basic wooden shield. Better than nothing.",
             EquipSlot::Hand_R, UseEffect::None, 0, 0, 1),

        Item("Hood", '^', 100, 100, 100,
             ItemType::Equipment, 1, 10,
             "A simple cloth hood. Provides minimal head protection.",
             EquipSlot::Head, UseEffect::None, 0, 0, 1),

        Item("Leather Boots", ']', 120, 80, 40,
             ItemType::Equipment, 2, 15,
             "Sturdy leather boots. Good for long journeys.",
             EquipSlot::Foot_L, UseEffect::None, 0, 0, 1),

        Item("Gold Ring", '=', 255, 215, 0,
             ItemType::Equipment, 0, 100,
             "A simple gold ring. Perhaps it has sentimental value.",
             EquipSlot::Hand_R, UseEffect::None, 0, 0, 0),

        // Light sources
        Item("Torch", '|', 255, 180, 50,
             ItemType::Equipment, 2, 8,
             "A wooden torch. Lights the way (radius 5).",
             EquipSlot::Hand_L, UseEffect::None, 0, 1, 0),

        Item("Lantern", '*', 255, 220, 100,
             ItemType::Equipment, 3, 30,
             "A brass lantern. Bright and permanent (radius 7).",
             EquipSlot::Hand_L, UseEffect::None, 0, 0, 0),

        Item("Campfire Kit", '^', 200, 120, 40,
             ItemType::Misc, 4, 15,
             "Wood and flint for building a campfire. Can be placed."),
    };
    return db;
}

const Item* find_item(const std::string& name) {
    for (const auto& item : item_db()) {
        if (item.name() == name) return &item;
    }
    return nullptr;
}
