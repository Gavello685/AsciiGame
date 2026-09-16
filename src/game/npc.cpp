#include "game/npc.h"
#include "game/rng.h"
#include "game/world.h"
#include <algorithm>
#include <utility>

Npc::Npc(int x, int y, uint32_t glyph, const std::string& name,
         uint8_t fg_r, uint8_t fg_g, uint8_t fg_b,
         bool is_merchant, int starting_affinity,
         const std::vector<DialogueNode>& dialogue_tree,
         const std::vector<std::pair<Item, int>>& shop_inventory)
    : x_(x), y_(y), glyph_(glyph), name_(name),
      fg_r_(fg_r), fg_g_(fg_g), fg_b_(fg_b),
      is_merchant_(is_merchant), affinity_(starting_affinity),
      dialogue_tree_(dialogue_tree), shop_inventory_(shop_inventory) {
}

void Npc::set_affinity(int v) {
    affinity_ = std::max(0, std::min(100, v));
}

void Npc::adjust_affinity(int delta) {
    set_affinity(affinity_ + delta);
}

const char* Npc::affinity_label() const {
    if (affinity_ <= HOSTILE_MAX) return "Hostile";
    if (affinity_ <= WARY_MAX) return "Wary";
    if (affinity_ <= FRIENDLY_MAX) return "Friendly";
    if (affinity_ <= TRUSTING_MAX) return "Trusting";
    return "Devoted";
}

// ── NPC archetype database ────────────────────────────────────────

namespace {

struct NpcArchetype {
    const char* name;
    uint32_t glyph;
    uint8_t r, g, b;
    bool is_merchant;
    int affinity;
    std::vector<DialogueNode> (*dialogue)();
    std::vector<std::pair<const char*, int>> stock;
};

const std::vector<NpcArchetype>& npc_archetypes() {
    static const std::vector<NpcArchetype> archetypes = {
        { "Merchant", 'm', 220, 180,  60, true,  60, build_merchant_dialogue,
          {{"Bread", 10}, {"Health Potion", 5}, {"Water Skin", 5}, {"Iron Sword", 3},
           {"Leather Armor", 2}, {"Wooden Shield", 3}, {"Hood", 3},
           {"Leather Boots", 3}, {"Torch", 4}} },

        { "Blacksmith", 'b', 220, 120,  60, true,  50, build_blacksmith_dialogue,
          {{"Iron Sword", 3}, {"Steel Sword", 1}, {"Spear", 3}, {"Battle Axe", 1},
           {"Iron Shield", 2}, {"Steel Shield", 1}, {"Chain Mail", 1},
           {"Steel Helm", 1}, {"Gauntlets", 2}, {"Woodcutter's Axe", 2},
           {"Pickaxe", 2}, {"Iron Rod", 5}} },

        { "Herbalist", 'h', 180, 120, 200, true,  50, build_herbalist_dialogue,
          {{"Health Potion", 8}, {"Greater Health Potion", 2}, {"Water Skin", 6},
           {"Bread", 6}, {"Apple", 8}, {"Cheese", 4}, {"Cooked Meat", 3}} },

        { "Villager",  'v', 140, 200, 140, false, 30, build_villager_dialogue, {} },
        { "Guard",     'g', 100, 140, 220, false, 40, build_guard_dialogue,    {} },
        { "Farmer",    'f', 100, 180,  80, false, 40, build_farmer_dialogue,   {} },
        { "Old Sage",  'a', 180, 160, 220, false, 40, build_sage_dialogue,     {} },
        { "Wanderer",  'a', 100, 160, 200, false, 20, build_wanderer_dialogue, {} },
        { "Child",     'v', 200, 200, 120, false, 50, build_child_dialogue,    {} },
    };
    return archetypes;
}

const NpcArchetype& archetype_for(const std::string& name) {
    const auto& all = npc_archetypes();
    for (const auto& a : all) {
        if (name == a.name) return a;
    }
    // Villager is the fallback for unknown names.
    for (const auto& a : all) {
        if (std::string("Villager") == a.name) return a;
    }
    return all.front();
}

}

Npc make_npc(const std::string& name, int x, int y) {
    const NpcArchetype& a = archetype_for(name);

    std::vector<std::pair<Item, int>> shop;
    shop.reserve(a.stock.size());
    for (const auto& [item_name, qty] : a.stock) {
        if (const Item* item = find_item(item_name)) shop.emplace_back(*item, qty);
    }

    return Npc(x, y, a.glyph, a.name, a.r, a.g, a.b,
               a.is_merchant, a.affinity, a.dialogue(), shop);
}

const std::vector<std::string>& village_npc_names() {
    // DESIGN.md: villages spawn all six settlement NPCs.
    static const std::vector<std::string> names = {
        "Villager", "Merchant", "Guard", "Blacksmith", "Farmer", "Herbalist"
    };
    return names;
}

Item Npc::shop_item_at(int index) const {
    if (index < 0 || index >= static_cast<int>(shop_inventory_.size())) return Item();
    return shop_inventory_[index].first;
}

bool Npc::buy_from_shop(int index, int count) {
    if (index < 0 || index >= static_cast<int>(shop_inventory_.size())) return false;
    auto& [item, stock] = shop_inventory_[index];
    if (stock < count) return false;
    stock -= count;
    if (stock <= 0) {
        shop_inventory_.erase(shop_inventory_.begin() + index);
    }
    return true;
}

bool Npc::sell_to_shop(const Item& item, int count) {
    // Check if merchant already stocks this item
    for (auto& [existing, stock] : shop_inventory_) {
        if (existing.stacks_with(item)) {
            stock += count;
            return true;
        }
    }
    // New item in stock
    shop_inventory_.emplace_back(item, count);
    return true;
}

GiftReaction Npc::react_to_gift(const Item& item) const {
    int value = item.value();

    // Base reaction from value
    GiftReaction base;
    if (value >= 80) base = GiftReaction::Love;
    else if (value >= 40) base = GiftReaction::Like;
    else if (value >= 10) base = GiftReaction::Neutral;
    else if (value >= 3) base = GiftReaction::Dislike;
    else base = GiftReaction::Hate;

    // Personality modifiers — name-specific tastes take priority
    // over the generic merchant behavior below.
    if (name_ == "Child") {
        // Children love shiny things (gold, rings) and food
        if (item.glyph() == '=' || item.glyph() == '$') return GiftReaction::Love;
        if (item.type() == ItemType::Consumable) return GiftReaction::Like;
        if (item.type() == ItemType::Equipment && item.weight() > 5) return GiftReaction::Dislike;
        return base;
    }

    if (name_ == "Old Sage") {
        // Sages love scrolls and knowledge items
        if (item.name().find("Scroll") != std::string::npos) return GiftReaction::Love;
        if (item.type() == ItemType::Key) return GiftReaction::Like;
        if (item.type() == ItemType::Equipment && item.name().find("Sword") != std::string::npos)
            return GiftReaction::Dislike;
        return base;
    }

    if (name_ == "Wanderer") {
        // Wanderers love supplies and tools, hate luxury
        if (item.type() == ItemType::Consumable) return GiftReaction::Like;
        if (value >= 80) return GiftReaction::Dislike; // too fancy
        return base;
    }

    if (name_ == "Guard") {
        // Guards appreciate weapons, shields, and practical gear
        if (item.type() == ItemType::Equipment && item.attack() > 0) return GiftReaction::Like;
        if (item.type() == ItemType::Equipment && item.defense() > 0) return GiftReaction::Like;
        if (item.name() == "Torch" || item.name() == "Lantern") return GiftReaction::Like;
        return base;
    }

    if (name_ == "Blacksmith") {
        // Blacksmiths love raw materials and fine craftsmanship
        if (item.type() == ItemType::Material) return GiftReaction::Love;
        if (item.type() == ItemType::Equipment && value >= 60) return GiftReaction::Like;
        return base;
    }

    if (name_ == "Farmer") {
        // Farmers love food and humble, useful things
        if (item.type() == ItemType::Consumable) return GiftReaction::Love;
        if (item.name() == "Wood" || item.name() == "Stone") return GiftReaction::Like;
        if (value >= 80) return GiftReaction::Dislike; // too fancy for the farm
        return base;
    }

    if (name_ == "Herbalist") {
        // Herbalists love potions and ingredients
        if (item.type() == ItemType::Consumable && item.use_effect() == UseEffect::Heal)
            return GiftReaction::Love;
        if (item.type() == ItemType::Consumable) return GiftReaction::Like;
        if (item.type() == ItemType::Equipment && item.attack() > 0) return GiftReaction::Dislike;
        return base;
    }

    // Generic merchants love high-value, hate junk
    if (is_merchant_) {
        if (value >= 50) return GiftReaction::Love;
        if (value >= 20) return GiftReaction::Like;
        if (value >= 5) return GiftReaction::Neutral;
        return GiftReaction::Dislike;
    }

    // Villagers: love food, neutral on most things
    if (item.type() == ItemType::Consumable) return GiftReaction::Like;
    if (item.type() == ItemType::Equipment && item.name().find("Sword") != std::string::npos)
        return GiftReaction::Dislike;

    return base;
}

std::string Npc::gift_reaction_text(GiftReaction reaction) const {
    switch (reaction) {
        case GiftReaction::Love:    return "Oh, this is wonderful! Thank you so much!";
        case GiftReaction::Like:    return "Thank you, that's very kind of you.";
        case GiftReaction::Neutral: return "Oh. Thank you, I suppose.";
        case GiftReaction::Dislike: return "I... don't really need this, but thanks.";
        case GiftReaction::Hate:    return "What is this supposed to mean?";
    }
    return "Thanks.";
}

bool Npc::can_move_to(int x, int y, const World& world) const {
    if (!world.is_passable(x, y)) return false;
    if (world.has_npc_at(x, y)) return false;
    return true;
}

void Npc::update(World& world, int player_x, int player_y, Rng& rng) {
    (void)player_x;
    (void)player_y;

    switch (state_) {
        case NpcState::Idle:
            idle_timer_++;
            if (idle_timer_ > 30 + rng.below(40)) {
                state_ = NpcState::Wandering;
                wander_timer_ = 10 + rng.below(20);
                pick_wander_direction(rng);
            }
            break;

        case NpcState::Wandering:
            wander(world, rng);
            break;
    }
}

void Npc::pick_wander_direction(Rng& rng) {
    const int dirs[][2] = {{0,1},{0,-1},{1,0},{-1,0}};
    int d = rng.below(4);
    wander_dx_ = dirs[d][0];
    wander_dy_ = dirs[d][1];
}

void Npc::wander(World& world, Rng& rng) {
    wander_timer_--;
    if (wander_timer_ <= 0) {
        state_ = NpcState::Idle;
        idle_timer_ = 0;
        return;
    }

    int nx = x_ + wander_dx_;
    int ny = y_ + wander_dy_;

    if (can_move_to(nx, ny, world)) {
        x_ = nx;
        y_ = ny;
    } else {
        pick_wander_direction(rng);
    }
}
