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

std::vector<DialogueNode> build_guard_dialogue() {
    return {
        // Node 0: Root
        { "Halt! State your business. ...Ah, a traveler. Keep your weapon sheathed inside the village.",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 1: Talk
        { "We've had bandits on the roads and wolves in the woods. At night, worse things crawl out of the ruins.",
          {
            {"Talk", DialogueAction::Talk, 2},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 2: Talk deeper
        { "If you're heading into the wilds, go armed and carry a torch. The dark hides more than shadows.",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
    };
}

std::vector<DialogueNode> build_blacksmith_dialogue() {
    return {
        // Node 0: Root
        { "Welcome to the forge. Blades, armor, tools — all hammered by my own hand.",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Trade", DialogueAction::Trade, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 1: Talk
        { "Bring me iron rods and good planks and I can forge almost anything. Raw ore works too, if you find a smelter.",
          {
            {"Talk", DialogueAction::Talk, 2},
            {"Trade", DialogueAction::Trade, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 2: Talk deeper
        { "A steel blade will serve you far better than that old iron. And don't neglect a good shield — dead heroes swing no swords.",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Trade", DialogueAction::Trade, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
    };
}

std::vector<DialogueNode> build_farmer_dialogue() {
    return {
        // Node 0: Root
        { "Fine day for it, isn't it? The soil's rich this season.",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 1: Talk
        { "Between the rats in the grain and the wolves in the pasture, it's a wonder we harvest anything at all.",
          {
            {"Talk", DialogueAction::Talk, 2},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 2: Talk deeper
        { "If you ever settle down and designate a farm plot, I'd be happy to work it. A strong back is all I need.",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Gift", DialogueAction::Gift, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
    };
}

std::vector<DialogueNode> build_herbalist_dialogue() {
    return {
        // Node 0: Root
        { "Shh... the mandrake is sleeping. What remedy do you seek, dear?",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Trade", DialogueAction::Trade, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 1: Talk
        { "Red petals for mending flesh, blue moss for clarity of mind. The swamp provides, if you know where to look.",
          {
            {"Talk", DialogueAction::Talk, 2},
            {"Trade", DialogueAction::Trade, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
        // Node 2: Talk deeper
        { "Two small draughts, distilled together, make a far stronger elixir. Even an adventurer could manage it.",
          {
            {"Talk", DialogueAction::Talk, 1},
            {"Trade", DialogueAction::Trade, -1},
            {"Goodbye", DialogueAction::Exit, -1},
          }},
    };
}

std::vector<DialogueNode> dialogue_for_name(const std::string& name) {
    if (name == "Merchant") return build_merchant_dialogue();
    if (name == "Villager") return build_villager_dialogue();
    if (name == "Old Sage") return build_sage_dialogue();
    if (name == "Child") return build_child_dialogue();
    if (name == "Wanderer") return build_wanderer_dialogue();
    if (name == "Guard") return build_guard_dialogue();
    if (name == "Blacksmith") return build_blacksmith_dialogue();
    if (name == "Farmer") return build_farmer_dialogue();
    if (name == "Herbalist") return build_herbalist_dialogue();
    return {};
}
