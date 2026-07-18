#include "game/dialogue.h"

std::vector<DialogueNode> build_merchant_dialogue() {
    return {
        // Node 0: Root
        { "Welcome, traveler! Take a look at my wares.",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Trade", DialogueAction::Trade, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 1: Talk
        { "I've been running this shop for years. Business has been slow lately, what with all the creatures in the halls.",
          {
            {"Talk", DialogueAction::Talk, 2},
            {"Trade", DialogueAction::Trade, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 2: Talk deeper
        { "If you bring me something rare, I might give you a good price. Or just browse — no pressure.",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Trade", DialogueAction::Trade, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
    };
}

std::vector<DialogueNode> build_villager_dialogue() {
    return {
        // Node 0: Root
        { "Welcome to our settlement! Not many strangers come through here.",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 1: Talk
        { "The merchant has whatever you need. Just watch out for the rats in the lower halls — they've been bold lately.",
          {
            {"Talk", DialogueAction::Talk, 2},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 2: Talk deeper
        { "I heard the Old Sage knows something about the crystals. He's been muttering about them for days.",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
    };
}

std::vector<DialogueNode> build_sage_dialogue() {
    return {
        // Node 0: Root
        { "Ah, a newcomer. The crystals hold great power, you know.",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 1: Talk
        { "I've spent thirty years studying them. They react to proximity... and to certain items.",
          {
            {"Talk", DialogueAction::Talk, 2},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 2: Talk deeper
        { "Be wary of the shadows in the deep places. Not everything down there is friendly.",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
    };
}

std::vector<DialogueNode> build_child_dialogue() {
    return {
        // Node 0: Root
        { "Are you an adventurer? You look so strong!",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 1: Talk
        { "I found a shiny rock yesterday! Mother says I shouldn't wander alone, but it was so exciting!",
          {
            {"Talk", DialogueAction::Talk, 2},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 2: Talk deeper
        { "Do you have any more shiny things? I love anything that glimmers!",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
    };
}

std::vector<DialogueNode> build_wanderer_dialogue() {
    return {
        // Node 0: Root
        { "I came from the east. The roads are dangerous these days.",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 1: Talk
        { "Have you seen the crystals glimmer at night? Something stirs in the ruins beyond the mountains.",
          {
            {"Talk", DialogueAction::Talk, 2},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 2: Talk deeper
        { "I seek the ruins. If you find anything useful, I'd be grateful for supplies.",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
    };
}
