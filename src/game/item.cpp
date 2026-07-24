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

        Item("Apple", '%', 220, 60, 60,
             ItemType::Consumable, 1, 1,
             "A crisp red apple. Restores 5 hunger.",
             EquipSlot::None, UseEffect::Feed, 5),

        Item("Cooked Meat", '%', 140, 90, 50,
             ItemType::Consumable, 1, 6,
             "A hearty cut of roasted meat. Restores 25 hunger.",
             EquipSlot::None, UseEffect::Feed, 25),

        Item("Cheese", '%', 230, 210, 120,
             ItemType::Consumable, 1, 4,
             "A wedge of sharp cheese. Restores 15 hunger.",
             EquipSlot::None, UseEffect::Feed, 15),

        Item("Greater Health Potion", '!', 255, 100, 120,
             ItemType::Consumable, 1, 60,
             "A vibrant crimson elixir. Restores 50 HP.",
             EquipSlot::None, UseEffect::Heal, 50),

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

        Item("Steel Sword", '/', 200, 210, 230,
             ItemType::Equipment, 5, 75,
             "A finely forged steel blade. Sharp and well-balanced.",
             EquipSlot::Hand_L, UseEffect::None, 0, 5, 0, 1),

        Item("Dagger", '/', 160, 160, 180,
             ItemType::Equipment, 1, 12,
             "A short, quick blade. Easy to conceal.",
             EquipSlot::Hand_L, UseEffect::None, 0, 2, 0, 1),

        Item("Spear", '/', 180, 150, 100,
             ItemType::Equipment, 4, 25,
             "A long shaft tipped with iron. Good reach.",
             EquipSlot::Hand_L, UseEffect::None, 0, 4, 0, 1),

        Item("Battle Axe", '/', 190, 140, 90,
             ItemType::Equipment, 7, 65,
             "A heavy double-bladed axe. Brutal but unpredictable.",
             EquipSlot::Hand_L, UseEffect::None, 0, 6, 0, 2),

        Item("War Hammer", '/', 150, 150, 160,
             ItemType::Equipment, 10, 80,
             "A massive crushing head on a stout haft. Slow but devastating.",
             EquipSlot::Hand_L, UseEffect::None, 0, 7, 0, 2),

        Item("Woodcutter's Axe", '/', 170, 120, 60,
             ItemType::Equipment, 5, 20,
             "A woodsman's tool. Doubles wood yielded when chopping trees.",
             EquipSlot::Hand_L, UseEffect::None, 0, 3, 0, 1),

        Item("Pickaxe", '/', 140, 140, 150,
             ItemType::Equipment, 6, 25,
             "A miner's pick. Doubles stone yielded when mining rock.",
             EquipSlot::Hand_L, UseEffect::None, 0, 2, 0, 1),

        Item("Iron Shield", 'O', 180, 180, 200,
             ItemType::Equipment, 8, 50,
             "A heavy iron shield. Blocks incoming attacks.",
             EquipSlot::Hand_R, UseEffect::None, 0, 0, 3),

        Item("Steel Shield", 'O', 200, 210, 230,
             ItemType::Equipment, 10, 110,
             "A polished steel shield. Superb protection.",
             EquipSlot::Hand_R, UseEffect::None, 0, 0, 5),

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

        Item("Chain Mail", '}', 170, 170, 180,
             ItemType::Equipment, 15, 90,
             "Interlocking steel rings. Strong, flexible protection.",
             EquipSlot::Torso, UseEffect::None, 0, 0, 4),

        Item("Steel Armor", '}', 200, 210, 230,
             ItemType::Equipment, 25, 180,
             "A full suit of plate steel. Heavy but nearly impenetrable.",
             EquipSlot::Torso, UseEffect::None, 0, 0, 6),

        Item("Steel Helm", '^', 200, 210, 230,
             ItemType::Equipment, 4, 60,
             "A crested steel helm. Keeps your skull intact.",
             EquipSlot::Head, UseEffect::None, 0, 0, 2),

        Item("Steel Boots", ']', 200, 210, 230,
             ItemType::Equipment, 5, 70,
             "Plated greaves and sabatons. Clanks when you walk.",
             EquipSlot::Foot_L, UseEffect::None, 0, 0, 2),

        Item("Gauntlets", ']', 160, 160, 170,
             ItemType::Equipment, 3, 35,
             "Leather and steel gloves. Protects your sword arm.",
             EquipSlot::Arm_L, UseEffect::None, 0, 0, 1),

        Item("Cloak", '(', 90, 90, 140,
             ItemType::Equipment, 2, 18,
             "A dark traveling cloak. Offers a little cover.",
             EquipSlot::Shoulder_L, UseEffect::None, 0, 0, 1),

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

        // Materials (gathering, building, crafting)
        Item("Wood", '=', 140, 100, 50,
             ItemType::Material, 2, 1,
             "A rough length of timber. Chopped from trees."),

        Item("Wood Plank", '=', 170, 130, 70,
             ItemType::Material, 1, 3,
             "A smoothly cut plank. Used for building and crafting."),

        Item("Stone", 'o', 140, 140, 140,
             ItemType::Material, 3, 1,
             "A chunk of rough-hewn rock. Mined from mountains."),

        Item("Stone Block", 'o', 170, 170, 170,
             ItemType::Material, 2, 3,
             "A squared block of stone. Used for building."),

        Item("Iron Ore", 'o', 120, 100, 90,
             ItemType::Material, 4, 8,
             "Rock veined with raw iron. Smelt into rods."),

        Item("Iron Rod", '/', 180, 180, 190,
             ItemType::Material, 2, 15,
             "A bar of worked iron. Used for crafting weapons and tools."),
    };
    return db;
}

const Item* find_item(const std::string& name) {
    for (const auto& item : item_db()) {
        if (item.name() == name) return &item;
    }
    return nullptr;
}
