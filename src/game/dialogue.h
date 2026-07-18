#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class DialogueAction : uint8_t {
    None,
    Talk,
    Trade,
    Gift,
    Exit,
};

struct DialogueOption {
    std::string text;
    DialogueAction action = DialogueAction::None;
    int next_node = -1;
};

struct DialogueNode {
    std::string speaker_text;
    std::vector<DialogueOption> options;
};

// Builds a merchant dialogue tree
std::vector<DialogueNode> build_merchant_dialogue();

// Builds a villager dialogue tree
std::vector<DialogueNode> build_villager_dialogue();

// Builds a sage dialogue tree
std::vector<DialogueNode> build_sage_dialogue();

// Builds a child dialogue tree
std::vector<DialogueNode> build_child_dialogue();

// Builds a wanderer dialogue tree
std::vector<DialogueNode> build_wanderer_dialogue();
