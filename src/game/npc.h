#pragma once

#include "game/item.h"
#include "game/dialogue.h"
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

class World;
class Npc;
class Rng;

// Create an NPC of a known archetype with its glyph, colours, merchant flag,
// starting affinity, dialogue tree and shop stock. Single source of truth for
// NPC definitions, mirroring make_enemy(). Unknown names produce a Villager.
Npc make_npc(const std::string& name, int x, int y);

// Archetype names that a village populates with.
const std::vector<std::string>& village_npc_names();

enum class NpcState : uint8_t {
    Idle,
    Wandering,
};

enum class GiftReaction : uint8_t {
    Love,
    Like,
    Neutral,
    Dislike,
    Hate,
};

class Npc {
public:
    Npc() = default;
    Npc(int x, int y, uint32_t glyph, const std::string& name,
        uint8_t fg_r, uint8_t fg_g, uint8_t fg_b,
        bool is_merchant, int starting_affinity,
        const std::vector<DialogueNode>& dialogue_tree,
        const std::vector<std::pair<Item, int>>& shop_inventory = {});

    int x() const { return x_; }
    int y() const { return y_; }

    uint32_t glyph() const { return glyph_; }
    const std::string& name() const { return name_; }

    uint8_t fg_r() const { return fg_r_; }
    uint8_t fg_g() const { return fg_g_; }
    uint8_t fg_b() const { return fg_b_; }

    NpcState state() const { return state_; }

    // Affinity (0-100). Thresholds follow DESIGN.md: 0-20 hostile,
    // 21-40 wary, 41-60 friendly, 61-80 trusting, 81-100 devoted.
    int affinity() const { return affinity_; }
    void set_affinity(int v);
    void adjust_affinity(int delta);

    bool can_talk() const { return affinity_ > HOSTILE_MAX; }
    bool can_gift() const { return affinity_ > WARY_MAX; }
    bool can_trade() const { return is_merchant_ && affinity_ > FRIENDLY_MAX; }

    // Short relationship label for the dialogue header.
    const char* affinity_label() const;

    static constexpr int HOSTILE_MAX = 20;
    static constexpr int WARY_MAX = 40;
    static constexpr int FRIENDLY_MAX = 60;
    static constexpr int TRUSTING_MAX = 80;

    // Dialogue
    const std::vector<DialogueNode>& dialogue_tree() const { return dialogue_tree_; }

    // Shop
    bool is_merchant() const { return is_merchant_; }
    const std::vector<std::pair<Item, int>>& shop_inventory() const { return shop_inventory_; }
    void set_shop_inventory(const std::vector<std::pair<Item, int>>& stock) {
        shop_inventory_ = stock;
    }

    // Returns a copy: buy_from_shop may erase the stack, so callers must not
    // hold a reference into the vector across the call.
    Item shop_item_at(int index) const;

    bool buy_from_shop(int index, int count = 1);
    bool sell_to_shop(const Item& item, int count = 1);

    // Gift reaction
    GiftReaction react_to_gift(const Item& item) const;
    std::string gift_reaction_text(GiftReaction reaction) const;

    // AI
    void update(World& world, int player_x, int player_y, Rng& rng);

    bool can_move_to(int x, int y, const World& world) const;

private:
    void wander(World& world, Rng& rng);
    void pick_wander_direction(Rng& rng);

    int x_ = 0;
    int y_ = 0;
    uint32_t glyph_ = ' ';
    std::string name_;
    uint8_t fg_r_ = 255, fg_g_ = 255, fg_b_ = 255;

    bool is_merchant_ = false;
    int affinity_ = 0;
    std::vector<DialogueNode> dialogue_tree_;
    std::vector<std::pair<Item, int>> shop_inventory_;

    NpcState state_ = NpcState::Idle;
    int wander_timer_ = 0;
    int wander_dx_ = 0;
    int wander_dy_ = 0;
    int idle_timer_ = 0;
};
