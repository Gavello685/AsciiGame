#include "test_framework.h"

#include "game/building.h"
#include "game/crafting.h"
#include "game/player.h"
#include "game/world.h"

namespace {

int count_in_inventory(const Player& player, const std::string& name) {
    for (const auto& [item, qty] : player.inventory()) {
        if (item.name() == name) return qty;
    }
    return 0;
}

void give(Player& player, const char* name, int qty) {
    if (const Item* item = find_item(name)) player.add_item(*item, qty);
}

}

TEST(every_recipe_references_real_items) {
    for (const auto& recipe : recipe_db()) {
        CHECK(find_item(recipe.result) != nullptr);
        CHECK(recipe.result_qty > 0);
        CHECK(!recipe.ingredients.empty());
        for (const auto& [name, qty] : recipe.ingredients) {
            CHECK(find_item(name) != nullptr);
            CHECK(qty > 0);
        }
    }
}

TEST(every_buildable_references_real_materials) {
    for (int i = 0; i < static_cast<int>(BuildTile::Count); ++i) {
        const BuildDef& def = build_def(static_cast<BuildTile>(i));
        CHECK(def.name != nullptr);
        CHECK(!def.materials.empty());
        for (const auto& [name, qty] : def.materials) {
            CHECK(find_item(name) != nullptr);
            CHECK(qty > 0);
        }
    }
}

TEST(crafting_consumes_exactly_the_recipe_cost) {
    const Recipe* plank_recipe = nullptr;
    for (const auto& recipe : recipe_db()) {
        if (std::string(recipe.result) == "Wood Plank") { plank_recipe = &recipe; break; }
    }
    CHECK(plank_recipe != nullptr);
    if (!plank_recipe) return;

    Player player;
    give(player, "Wood", 5);
    CHECK_EQ(craft_can_make(*plank_recipe, player), true);

    craft_make(*plank_recipe, player);

    int wood_cost = 0;
    for (const auto& [name, qty] : plank_recipe->ingredients) {
        if (std::string(name) == "Wood") wood_cost = qty;
    }
    CHECK_EQ(count_in_inventory(player, "Wood"), 5 - wood_cost);
    CHECK_EQ(count_in_inventory(player, "Wood Plank"), plank_recipe->result_qty);
}

TEST(crafting_without_materials_is_refused) {
    Player empty_handed;
    for (const auto& recipe : recipe_db()) {
        if (recipe.ingredients.empty()) continue;
        CHECK_EQ(craft_can_make(recipe, empty_handed), false);
    }
}

TEST(building_consumes_materials_and_places_a_tile) {
    World world;
    world.init(555);
    world.update_loaded_chunks(0, 0);

    Player player;
    player.spawn(5, 5);

    const BuildDef& wall = build_def(BuildTile::WoodWall);
    CHECK_EQ(build_can_afford(BuildTile::WoodWall, player), false);

    for (const auto& [name, qty] : wall.materials) give(player, name, qty + 3);
    CHECK_EQ(build_can_afford(BuildTile::WoodWall, player), true);

    // Find a buildable spot next to the player.
    int target_x = -1, target_y = -1;
    std::string reason;
    for (int dy = -3; dy <= 3 && target_x < 0; ++dy) {
        for (int dx = -3; dx <= 3 && target_x < 0; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int wx = player.x() + dx, wy = player.y() + dy;
            if (build_can_place(BuildTile::WoodWall, world, wx, wy, reason)) {
                target_x = wx;
                target_y = wy;
            }
        }
    }
    CHECK(target_x >= 0);
    if (target_x < 0) return;

    std::vector<std::pair<std::string, int>> before;
    for (const auto& [name, qty] : wall.materials) {
        before.emplace_back(name, count_in_inventory(player, name));
        (void)qty;
    }

    std::string result = build_place(world, player, BuildTile::WoodWall, target_x, target_y);
    CHECK_EQ(result.rfind("Built", 0), size_t{0});
    CHECK(world.get_tile(target_x, target_y).type == wall.tile);

    for (size_t i = 0; i < wall.materials.size(); ++i) {
        int cost = wall.materials[i].second;
        CHECK_EQ(count_in_inventory(player, before[i].first), before[i].second - cost);
    }
}

TEST(building_a_tile_records_it_as_a_persistent_delta) {
    World world;
    world.init(556);
    world.update_loaded_chunks(0, 0);

    Player player;
    player.spawn(5, 5);
    for (const auto& [name, qty] : build_def(BuildTile::WoodFloor).materials) {
        give(player, name, qty + 5);
    }

    std::string reason;
    int target_x = -1, target_y = -1;
    for (int dy = -3; dy <= 3 && target_x < 0; ++dy) {
        for (int dx = -3; dx <= 3 && target_x < 0; ++dx) {
            int wx = player.x() + dx, wy = player.y() + dy;
            if (build_can_place(BuildTile::WoodFloor, world, wx, wy, reason)) {
                target_x = wx;
                target_y = wy;
            }
        }
    }
    if (target_x < 0) return;

    build_place(world, player, BuildTile::WoodFloor, target_x, target_y);

    bool recorded = false;
    for (const auto& chunk : world.gather_save_data()) {
        for (const auto& mod : chunk.modifications) {
            int wx = chunk.cx * CHUNK_SIZE + mod.local_x;
            int wy = chunk.cy * CHUNK_SIZE + mod.local_y;
            if (wx == target_x && wy == target_y) recorded = true;
        }
    }
    CHECK_EQ(recorded, true);
}

TEST(building_is_refused_without_materials) {
    World world;
    world.init(557);
    world.update_loaded_chunks(0, 0);

    Player player;
    player.spawn(5, 5);

    std::string reason;
    for (int dy = -3; dy <= 3; ++dy) {
        for (int dx = -3; dx <= 3; ++dx) {
            int wx = player.x() + dx, wy = player.y() + dy;
            if (!build_can_place(BuildTile::StoneWall, world, wx, wy, reason)) continue;
            std::string result = build_place(world, player, BuildTile::StoneWall, wx, wy);
            CHECK_NE(result.rfind("Built", 0), size_t{0});
            CHECK(world.get_tile(wx, wy).type != TileType::Wall);
            return;
        }
    }
}

TEST(building_is_refused_on_impassable_terrain) {
    World world;
    world.init(558);
    world.update_loaded_chunks(0, 0);

    Player player;
    player.spawn(0, 0);

    // Find any blocked tile in range and confirm it is rejected with a reason.
    std::string reason;
    for (int dy = -6; dy <= 6; ++dy) {
        for (int dx = -6; dx <= 6; ++dx) {
            Tile tile = world.get_tile(dx, dy);
            if (tile.type != TileType::Tree && tile.type != TileType::Mountain &&
                tile.type != TileType::Water && tile.type != TileType::DeepWater) {
                continue;
            }
            reason.clear();
            CHECK_EQ(build_can_place(BuildTile::WoodFloor, world, dx, dy, reason), false);
            CHECK(!reason.empty());
            return;
        }
    }
}

TEST(inventory_weight_tracks_items) {
    Player player;
    int empty_weight = player.current_weight();

    const Item* stone = find_item("Stone");
    CHECK(stone != nullptr);
    if (!stone) return;

    player.add_item(*stone, 4);
    CHECK_EQ(player.current_weight(), empty_weight + stone->weight() * 4);

    player.remove_item(0, 4);
    CHECK_EQ(player.current_weight(), empty_weight);
}
