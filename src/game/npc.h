#pragma once

#include "game/item.h"
#include "game/dialogue.h"
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

class World;

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

    // Affinity
    int affinity() const { return affinity_; }
    void adjust_affinity(int delta);
    bool can_trade() const { return is_merchant_; }
    bool can_gift() const { return affinity_ >= 40; }

    // Dialogue
    const std::vector<DialogueNode>& dialogue_tree() const { return dialogue_tree_; }

    // Shop
    bool is_merchant() const { return is_merchant_; }
    const std::vector<std::pair<Item, int>>& shop_inventory() const { return shop_inventory_; }
    bool buy_from_shop(int index, int count = 1);
    bool sell_to_shop(const Item& item, int count = 1);

    // Gift reaction
    GiftReaction react_to_gift(const Item& item) const;
    std::string gift_reaction_text(GiftReaction reaction) const;

    // AI
    void update(World& world, int player_x, int player_y);

    bool can_move_to(int x, int y, const World& world) const;

private:
    void wander(World& world);

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
