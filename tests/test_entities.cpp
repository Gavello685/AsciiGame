#include "test_framework.h"

#include "game/enemy.h"
#include "game/item.h"
#include "game/npc.h"
#include "game/player.h"
#include "game/world.h"

// make_enemy and make_npc are the single source of truth for archetype stats.
// Reconstructing saved entities through them is what removed the positional
// constructor whose arguments were swapped on load.

TEST(make_enemy_stats_match_design) {
    Enemy rat = make_enemy("Rat", 3, 4);
    CHECK_EQ(rat.name(), std::string("Rat"));
    CHECK_EQ(rat.x(), 3);
    CHECK_EQ(rat.y(), 4);
    CHECK_EQ(rat.max_hp(), 5);
    CHECK_EQ(rat.attack(), 2);
    CHECK_EQ(rat.defense(), 0);
    CHECK_EQ(rat.xp_value(), 3);
    CHECK_EQ(rat.damage_variance(), 0);

    Enemy ogre = make_enemy("Ogre", 0, 0);
    CHECK_EQ(ogre.max_hp(), 25);
    CHECK_EQ(ogre.attack(), 7);
    CHECK_EQ(ogre.defense(), 3);
    CHECK_EQ(ogre.xp_value(), 20);
    CHECK_EQ(ogre.damage_variance(), 2);
}

TEST(enemy_xp_and_variance_are_not_swapped) {
    // The old save path passed these two in the wrong order, which turned a
    // 20 XP ogre into a 2 XP ogre that hit for wildly random damage.
    for (const char* name : {"Rat", "Goblin", "Ogre", "Troll", "Bandit"}) {
        Enemy enemy = make_enemy(name, 0, 0);
        CHECK(enemy.xp_value() >= enemy.damage_variance());
        CHECK(enemy.damage_variance() <= 2);
    }
}

TEST(enemy_set_hp_clamps) {
    Enemy goblin = make_enemy("Goblin", 0, 0);
    goblin.set_hp(5);
    CHECK_EQ(goblin.hp(), 5);
    goblin.set_hp(-3);
    CHECK_EQ(goblin.hp(), 0);
    CHECK_EQ(goblin.is_alive(), false);
    goblin.set_hp(9999);
    CHECK_EQ(goblin.hp(), goblin.max_hp());
}

TEST(enemy_drops_come_from_archetype) {
    Enemy rat = make_enemy("Rat", 0, 0);
    CHECK_EQ(rat.drops().size(), size_t{1});
    if (!rat.drops().empty()) CHECK_EQ(rat.drops()[0].first.name(), std::string("Bread"));

    Enemy bat = make_enemy("Bat", 0, 0);
    CHECK_EQ(bat.drops().size(), size_t{0});
}

TEST(night_predators_are_name_derived) {
    CHECK_EQ(make_enemy("Wolf", 0, 0).night_predator(), true);
    CHECK_EQ(make_enemy("Zombie", 0, 0).night_predator(), true);
    CHECK_EQ(make_enemy("Rat", 0, 0).night_predator(), false);
}

TEST(make_npc_archetypes) {
    Npc merchant = make_npc("Merchant", 2, 2);
    CHECK_EQ(merchant.name(), std::string("Merchant"));
    CHECK_EQ(merchant.is_merchant(), true);
    CHECK_EQ(merchant.affinity(), 60);
    CHECK(!merchant.shop_inventory().empty());
    CHECK(!merchant.dialogue_tree().empty());

    Npc farmer = make_npc("Farmer", 0, 0);
    CHECK_EQ(farmer.is_merchant(), false);
    CHECK_EQ(farmer.shop_inventory().size(), size_t{0});
    CHECK(!farmer.dialogue_tree().empty());
}

TEST(make_npc_falls_back_to_villager) {
    Npc unknown = make_npc("Nobody In Particular", 0, 0);
    CHECK_EQ(unknown.name(), std::string("Villager"));
    CHECK(!unknown.dialogue_tree().empty());
}

TEST(every_village_npc_has_an_archetype) {
    for (const auto& name : village_npc_names()) {
        Npc npc = make_npc(name, 0, 0);
        CHECK_EQ(npc.name(), name);
        CHECK(!npc.dialogue_tree().empty());
    }
}

TEST(affinity_thresholds_follow_design) {
    Npc villager = make_npc("Villager", 0, 0);

    villager.set_affinity(10);
    CHECK_EQ(villager.can_talk(), false);
    CHECK_EQ(villager.can_gift(), false);

    villager.set_affinity(30);
    CHECK_EQ(villager.can_talk(), true);
    CHECK_EQ(villager.can_gift(), false);

    villager.set_affinity(50);
    CHECK_EQ(villager.can_gift(), true);
    // Non-merchants never trade, however well liked.
    CHECK_EQ(villager.can_trade(), false);

    Npc merchant = make_npc("Merchant", 0, 0);
    merchant.set_affinity(50);
    CHECK_EQ(merchant.can_trade(), false);
    merchant.set_affinity(70);
    CHECK_EQ(merchant.can_trade(), true);
}

TEST(affinity_clamps_to_range) {
    Npc npc = make_npc("Villager", 0, 0);
    npc.adjust_affinity(-1000);
    CHECK_EQ(npc.affinity(), 0);
    npc.adjust_affinity(1000);
    CHECK_EQ(npc.affinity(), 100);
    CHECK_EQ(std::string(npc.affinity_label()), std::string("Devoted"));
}

TEST(structure_entities_resolve_to_real_archetypes) {
    // These used to be dispatched by comparing const char* with ==, which is
    // a pointer comparison and not guaranteed to match across translation
    // units. A mismatch silently spawned nothing.
    for (int i = 1; i < static_cast<int>(StructureType::StructureCount); ++i) {
        const StructureDef& def = structure_def(static_cast<StructureType>(i));
        for (const auto& entity : def.entities) {
            if (entity.kind == StructureEntityKind::Npc) {
                Npc npc = make_npc(entity.name, 0, 0);
                CHECK_EQ(npc.name(), std::string(entity.name));
            } else {
                Enemy enemy = make_enemy(entity.name, 0, 0);
                CHECK_EQ(enemy.name(), std::string(entity.name));
                CHECK(enemy.max_hp() > 0);
            }
        }
    }
}

TEST(village_spawns_its_settlement_npcs) {
    World world;
    world.init(777);
    world.update_loaded_chunks(0, 0);

    // The origin chunk always holds a village.
    int found = 0;
    for (const auto& key : world.entity_chunk_keys()) {
        for (size_t i = 0; i < world.npc_count_in_chunk(key); ++i) {
            if (world.npc_in_chunk(key, i)) found++;
        }
    }
    CHECK(found > 0);
}

TEST(player_equip_accepts_the_partner_slot) {
    Player player;
    const Item* sword = find_item("Iron Sword");
    CHECK(sword != nullptr);
    if (!sword) return;
    player.add_item(*sword);

    CHECK(player.equip(0, EquipSlot::Hand_R));
    CHECK(player.equipped_at(EquipSlot::Hand_R) != nullptr);
    CHECK(player.equipped_at(EquipSlot::Hand_L) == nullptr);
    CHECK(!player.has_item("Iron Sword"));
}

TEST(player_equip_rejects_an_unrelated_slot) {
    Player player;
    const Item* sword = find_item("Iron Sword");
    CHECK(sword != nullptr);
    if (!sword) return;
    player.add_item(*sword);

    CHECK(!player.equip(0, EquipSlot::Head));
    CHECK(player.has_item("Iron Sword"));
    CHECK(player.equipped_at(EquipSlot::Head) == nullptr);
}

TEST(player_equip_copies_before_displacing_the_occupant) {
    // unequip() can reallocate the inventory vector. The item being equipped
    // used to be held by reference into that vector.
    Player player;
    const Item* sword = find_item("Iron Sword");
    const Item* torch = find_item("Torch");
    CHECK(sword != nullptr && torch != nullptr);
    if (!sword || !torch) return;

    player.add_item(*sword);
    CHECK(player.equip(0, EquipSlot::Hand_L));
    player.add_item(*torch);
    CHECK(player.equip(0, EquipSlot::Hand_L));

    CHECK_EQ(player.equipped_at(EquipSlot::Hand_L)->name(), std::string("Torch"));
    CHECK(player.has_item("Iron Sword"));
}
