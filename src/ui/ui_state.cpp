#include "ui/ui_state.h"

namespace ui {

const EquipSlot EQUIP_SLOTS[] = {
    EquipSlot::Head, EquipSlot::Shoulder_L, EquipSlot::Shoulder_R,
    EquipSlot::Torso, EquipSlot::Arm_L, EquipSlot::Arm_R,
    EquipSlot::Hand_L, EquipSlot::Hand_R, EquipSlot::Leg_L,
    EquipSlot::Leg_R, EquipSlot::Foot_L, EquipSlot::Foot_R,
};

const char* const EQUIP_SLOT_NAMES[] = {
    "Head", "L Shoulder", "R Shoulder", "Torso",
    "L Arm", "R Arm", "L Hand", "R Hand",
    "L Leg", "R Leg", "L Foot", "R Foot",
};

const int NUM_EQUIP_SLOTS =
    static_cast<int>(sizeof(EQUIP_SLOTS) / sizeof(EQUIP_SLOTS[0]));

const char* equip_slot_name(EquipSlot slot) {
    for (int i = 0; i < NUM_EQUIP_SLOTS; ++i) {
        if (EQUIP_SLOTS[i] == slot) return EQUIP_SLOT_NAMES[i];
    }
    return "-";
}

const char* const MAIN_MENU_OPTIONS[] = {"New Game", "Load Game", "Quit"};
const int MAIN_MENU_OPTION_COUNT =
    static_cast<int>(sizeof(MAIN_MENU_OPTIONS) / sizeof(MAIN_MENU_OPTIONS[0]));

const char* const PAUSE_MENU_OPTIONS[] = {
    "Save Game", "Load Game", "Settings", "Main Menu", "Quit",
};
const int PAUSE_MENU_OPTION_COUNT =
    static_cast<int>(sizeof(PAUSE_MENU_OPTIONS) / sizeof(PAUSE_MENU_OPTIONS[0]));

const char* const SETTINGS_LABELS[] = {
    "Chunk Load Radius", "Render Distance", "Simulation Distance",
};
const int SETTINGS_OPTION_COUNT =
    static_cast<int>(sizeof(SETTINGS_LABELS) / sizeof(SETTINGS_LABELS[0]));

std::vector<ItemAction> item_actions(const Item& item) {
    std::vector<ItemAction> actions;
    if (item.is_usable()) actions.push_back(ItemAction::Use);
    if (item.is_equippable()) actions.push_back(ItemAction::Equip);
    actions.push_back(ItemAction::Drop);
    actions.push_back(ItemAction::Examine);
    actions.push_back(ItemAction::Gift);
    return actions;
}

const char* item_action_name(ItemAction action) {
    switch (action) {
        case ItemAction::Use:     return "Use";
        case ItemAction::Equip:   return "Equip";
        case ItemAction::Drop:    return "Drop";
        case ItemAction::Examine: return "Examine";
        case ItemAction::Gift:    return "Gift";
    }
    return "";
}

}
