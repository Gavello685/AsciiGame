#include "game/crafting.h"
#include "game/player.h"
#include "game/item.h"

static const std::vector<Recipe> recipes = {
    // Materials processing
    { "Wood Plank", 2, {{"Wood", 1}} },
    { "Stone Block", 2, {{"Stone", 1}} },
    { "Iron Rod", 1, {{"Iron Ore", 2}} },

    // Light & camp
    { "Torch", 1, {{"Wood Plank", 1}, {"Iron Rod", 1}} },
    { "Campfire Kit", 1, {{"Wood", 2}, {"Stone", 1}} },

    // Tools
    { "Woodcutter's Axe", 1, {{"Wood Plank", 1}, {"Iron Rod", 1}} },
    { "Pickaxe", 1, {{"Wood Plank", 1}, {"Iron Rod", 1}} },

    // Weapons
    { "Dagger", 1, {{"Wood Plank", 1}, {"Iron Rod", 1}} },
    { "Iron Sword", 1, {{"Wood Plank", 1}, {"Iron Rod", 2}} },
    { "Spear", 1, {{"Wood Plank", 2}, {"Iron Rod", 1}} },
    { "Steel Sword", 1, {{"Wood Plank", 1}, {"Iron Rod", 3}} },
    { "Battle Axe", 1, {{"Wood Plank", 2}, {"Iron Rod", 2}} },
    { "War Hammer", 1, {{"Wood Plank", 2}, {"Iron Rod", 3}} },

    // Armor
    { "Chain Mail", 1, {{"Iron Rod", 4}} },
    { "Steel Helm", 1, {{"Iron Rod", 2}, {"Stone Block", 1}} },

    // Alchemy
    { "Greater Health Potion", 1, {{"Health Potion", 2}} },
};

const std::vector<Recipe>& recipe_db() {
    return recipes;
}

int craft_count_of(const Player& player, const char* name) {
    int total = 0;
    for (const auto& [item, qty] : player.inventory()) {
        if (item.name() == name) total += qty;
    }
    return total;
}

bool craft_can_make(const Recipe& r, const Player& player) {
    for (const auto& [name, needed] : r.ingredients) {
        if (craft_count_of(player, name) < needed) return false;
    }
    return true;
}

std::string craft_make(const Recipe& r, Player& player) {
    if (!craft_can_make(r, player)) {
        return "Missing materials.";
    }

    const Item* result_item = find_item(r.result);
    if (!result_item) return "Unknown item.";

    if (!player.can_carry(result_item->weight() * r.result_qty)) {
        return "Too heavy to carry.";
    }

    // Consume ingredients
    for (const auto& [name, needed] : r.ingredients) {
        int remaining = needed;
        for (int i = 0; i < static_cast<int>(player.inventory().size()) && remaining > 0; ++i) {
            if (player.inventory_item(i).name() == name) {
                int take = player.inventory_count(i);
                if (take > remaining) take = remaining;
                player.remove_item(i, take);
                remaining -= take;
                --i; // stack may have been erased; recheck this index
            }
        }
    }

    player.add_item(*result_item, r.result_qty);

    std::string msg = "Crafted " + std::string(r.result);
    if (r.result_qty > 1) msg += " x" + std::to_string(r.result_qty);
    return msg;
}
