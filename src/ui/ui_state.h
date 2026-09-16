#pragma once

#include "game/item.h"
#include "game/world.h"
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ui {

// Which screen currently owns input and what is drawn on top of the world.
enum class GameMode : uint8_t {
    MainMenu,
    Normal,
    Inventory,
    InventoryAction,
    InventoryExamine,
    GiftSelect,
    Dialogue,
    Dead,
    PauseMenu,
    SaveGame,
    LoadGame,
    Settings,
    Craft,
    Build,
    Zone,
    Character,
    Map,
};

// A status line with a countdown, in frames.
struct Message {
    std::string text;
    int timer = 0;
};

// Rolling status log shown above the HUD, newest last.
class MessageLog {
public:
    static constexpr size_t MAX_ENTRIES = 4;

    void push(const std::string& text, int timer = 90) {
        entries_.push_back({text, timer});
        while (entries_.size() > MAX_ENTRIES) entries_.erase(entries_.begin());
    }

    // Age every entry by a frame and drop the ones that have expired.
    void tick() {
        for (auto& entry : entries_) entry.timer--;
        while (!entries_.empty() && entries_.front().timer <= 0) {
            entries_.erase(entries_.begin());
        }
    }

    void clear() { entries_.clear(); }
    bool empty() const { return entries_.empty(); }
    const std::vector<Message>& entries() const { return entries_; }
    const Message& newest() const { return entries_.back(); }

private:
    std::vector<Message> entries_;
};

// The visible grid and where it sits in the world.
struct Viewport {
    int cols = 0;
    int rows = 0;
    int cam_x = 0;
    int cam_y = 0;

    // Rows available for the world; the bottom two are the HUD.
    static constexpr int HUD_ROWS = 2;
    int map_rows() const { return rows - HUD_ROWS; }

    bool contains(int screen_x, int screen_y) const {
        return screen_x >= 0 && screen_x < cols && screen_y >= 0 && screen_y < map_rows();
    }
};

// Every piece of state that exists only to drive the interface. Kept apart
// from Player/World/TimeSystem so the draw functions can take it by const
// reference instead of the twenty-odd locals this used to be in main().
struct UiState {
    GameMode mode = GameMode::MainMenu;
    MessageLog messages;

    // Menus
    int menu_cursor = 0;
    bool load_from_menu = false;
    int pause_cursor = 0;
    int settings_cursor = 0;

    // Inventory
    int inv_cursor = 0;          // negative selects the equipment panel
    int inv_action_cursor = 0;
    int equip_slot_cursor = 0;
    Item examine_item;

    // Gifting
    int gift_cursor = 0;
    GameMode gift_return = GameMode::Normal;

    // Dialogue. The NPC is addressed by position because it can move or be
    // unloaded while the overlay is open.
    int dialogue_npc_x = -1;
    int dialogue_npc_y = -1;
    int dialogue_node = 0;
    int dialogue_option = 0;

    // Crafting
    int craft_cursor = 0;

    // Build mode
    int build_cx = 0, build_cy = 0;
    int build_sel = 0;

    // Zone mode
    int zone_cx = 0, zone_cy = 0;
    int zone_ax = 0, zone_ay = 0;
    bool zone_anchored = false;
    bool zone_pick_type = false;
    int zone_type_sel = 0;

    // World map. The per-chunk summary is cached because it is derived from
    // noise and only changes when the player enters a different chunk.
    int map_cursor_cx = 0, map_cursor_cy = 0;
    int map_cache_cx = 0, map_cache_cy = 0;
    std::vector<World::ChunkMapInfo> map_cache;

    // Chunks whose structure has already been announced.
    std::set<std::pair<int, int>> discovered_structures;

    // Clear everything a new or loaded game must not inherit.
    void reset_for_new_run() {
        messages.clear();
        discovered_structures.clear();
        map_cache.clear();
        inv_cursor = 0;
        inv_action_cursor = 0;
        equip_slot_cursor = 0;
        gift_cursor = 0;
        craft_cursor = 0;
        build_sel = 0;
        zone_anchored = false;
        zone_pick_type = false;
        zone_type_sel = 0;
        dialogue_npc_x = -1;
        dialogue_npc_y = -1;
        mode = GameMode::Normal;
    }
};

// Paper-doll slots, in the order the equipment panel lists them.
extern const EquipSlot EQUIP_SLOTS[];
extern const char* const EQUIP_SLOT_NAMES[];
extern const int NUM_EQUIP_SLOTS;

// Menu option labels. Shared between the renderer that lists them and the
// input handler that bounds the cursor against them, so the two cannot drift
// apart and leave the cursor selecting a row that is not drawn.
extern const char* const MAIN_MENU_OPTIONS[];
extern const int MAIN_MENU_OPTION_COUNT;
extern const char* const PAUSE_MENU_OPTIONS[];
extern const int PAUSE_MENU_OPTION_COUNT;
extern const char* const SETTINGS_LABELS[];
extern const int SETTINGS_OPTION_COUNT;

// Main menu rows, in the order MAIN_MENU_OPTIONS lists them.
enum class MainMenuItem { NewGame, LoadGame, Quit };

// Pause menu rows, in the order PAUSE_MENU_OPTIONS lists them.
enum class PauseMenuItem { Save, Load, Settings, MainMenu, Quit };

// Chunks shown in each direction from the player on the world map.
inline constexpr int MAP_RADIUS = 10;

// Furthest the build cursor may stray from the player.
inline constexpr int BUILD_RANGE = 6;

// Inventory actions, in the order the action bar lists them. Use and Equip
// are only offered for items that support them.
enum class ItemAction { Use, Equip, Drop, Examine, Gift };

// The actions available for one item, in display order.
std::vector<ItemAction> item_actions(const Item& item);

const char* item_action_name(ItemAction action);

}
