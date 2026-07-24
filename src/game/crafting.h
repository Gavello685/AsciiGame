#pragma once

#include <string>
#include <vector>
#include <utility>

class Player;

struct Recipe {
    const char* result;
    int result_qty;
    std::vector<std::pair<const char*, int>> ingredients; // item name, count
};

// All known recipes, in display order
const std::vector<Recipe>& recipe_db();

// Count of an item the player carries (by name)
int craft_count_of(const Player& player, const char* name);

// Does the player have all ingredients for this recipe?
bool craft_can_make(const Recipe& r, const Player& player);

// Consume ingredients and add the result. Returns a status message.
// Caller should check craft_can_make first.
std::string craft_make(const Recipe& r, Player& player);
