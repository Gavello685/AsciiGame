# DESIGN.md

Comprehensive design reference for ASCII Game. Updated as systems are implemented.

---

## Item System ✅

### ItemType enum
- `Consumable` — food, potions, scrolls (can be Used)
- `Equipment` — weapons, armor, accessories (can be Equipped)
- `Material` — wood, stone, ore (for crafting/construction, Milestone 5)
- `Key` — quest items, special-purpose
- `Misc` — everything else (can be Dropped, Gifted, Examined)

### EquipSlot enum
11 slots forming a paper doll:
- `Head` — helmets, hoods
- `Shoulder_L`, `Shoulder_R` — capes, pauldrons
- `Torso` — chest armor, shirts
- `Arm_L`, `Arm_R` — bracers, gauntlets (distinct from hands)
- `Hand_L`, `Hand_R` — weapons, shields, rings
- `Leg_L`, `Leg_R` — greaves, leggings
- `Foot_L`, `Foot_R` — boots, sandals
- `None` — not equippable

### UseEffect enum
- `None` — can't be used
- `Heal` — restores HP (amount from use_amount)
- `Feed` — reduces hunger (amount from use_amount)
- `Drink` — reduces thirst (amount from use_amount)

### Item class fields
- `name` (string) — display name, used for stacking
- `glyph` (uint32_t) — ASCII character
- `fg_r, fg_g, fg_b` (uint8_t) — color
- `type` (ItemType)
- `weight` (int) — in lbs
- `value` (int) — gold value for trading
- `description` (string) — examine text
- `equip_slot` (EquipSlot)
- `use_effect` (UseEffect)
- `use_amount` (int) — heal/feed/drink amount
- `attack` (int) — damage bonus when equipped
- `defense` (int) — armor bonus when equipped
- `damage_variance` (int) — adds randomness to damage (±variance around attack stat)

### Item Stacking
Items stack if they share the same `name` AND `type`.
Inventory is stored as `vector<pair<Item, int>>` where int is stack count.

### Item Database (current, 35 items)
| Item | Type | Slot | Weight | Value | Atk | Def | Variance | Effect |
|---|---|---|---|---|---|---|---|---|
| Bread | Consumable | - | 1 | 2 | - | - | - | Feed +10 |
| Health Potion | Consumable | - | 1 | 25 | - | - | - | Heal +20 |
| Water Skin | Consumable | - | 1 | 5 | - | - | - | Drink +15 |
| Apple | Consumable | - | 1 | 1 | - | - | - | Feed +5 |
| Cooked Meat | Consumable | - | 1 | 6 | - | - | - | Feed +25 |
| Cheese | Consumable | - | 1 | 4 | - | - | - | Feed +15 |
| Greater Health Potion | Consumable | - | 1 | 60 | - | - | - | Heal +50 |
| Gold Coin | Misc | - | 0 | 1 | - | - | - | Adds to gold on pickup |
| Old Scroll | Misc | - | 0 | 10 | - | - | - | - |
| Rusty Key | Key | - | 0 | 0 | - | - | - | - |
| Iron Sword | Equipment | Hand_L | 4 | 30 | 3 | 0 | 1 | - |
| Steel Sword | Equipment | Hand_L | 5 | 75 | 5 | 0 | 1 | - |
| Dagger | Equipment | Hand_L | 1 | 12 | 2 | 0 | 1 | - |
| Spear | Equipment | Hand_L | 4 | 25 | 4 | 0 | 1 | - |
| Battle Axe | Equipment | Hand_L | 7 | 65 | 6 | 0 | 2 | - |
| War Hammer | Equipment | Hand_L | 10 | 80 | 7 | 0 | 2 | - |
| Woodcutter's Axe | Equipment | Hand_L | 5 | 20 | 3 | 0 | 1 | 2x wood yield |
| Pickaxe | Equipment | Hand_L | 6 | 25 | 2 | 0 | 1 | 2x stone yield |
| Iron Shield | Equipment | Hand_R | 8 | 50 | 0 | 3 | 0 | - |
| Steel Shield | Equipment | Hand_R | 10 | 110 | 0 | 5 | 0 | - |
| Leather Armor | Equipment | Torso | 8 | 40 | 0 | 2 | 0 | - |
| Chain Mail | Equipment | Torso | 15 | 90 | 0 | 4 | 0 | - |
| Steel Armor | Equipment | Torso | 25 | 180 | 0 | 6 | 0 | - |
| Wooden Shield | Equipment | Hand_R | 5 | 20 | 0 | 1 | 0 | - |
| Hood | Equipment | Head | 1 | 10 | 0 | 1 | 0 | - |
| Steel Helm | Equipment | Head | 4 | 60 | 0 | 2 | 0 | - |
| Leather Boots | Equipment | Foot_L | 2 | 15 | 0 | 1 | 0 | - |
| Steel Boots | Equipment | Foot_L | 5 | 70 | 0 | 2 | 0 | - |
| Gauntlets | Equipment | Arm_L | 3 | 35 | 0 | 1 | 0 | - |
| Cloak | Equipment | Shoulder_L | 2 | 18 | 0 | 1 | 0 | - |
| Gold Ring | Equipment | Hand_R | 0 | 100 | 0 | 0 | 0 | - |
| Torch | Equipment | Hand_L | 2 | 8 | 1 | 0 | - | Light (radius 5) |
| Lantern | Equipment | Hand_L | 3 | 30 | 0 | 0 | - | Light (radius 7) |
| Campfire Kit | Misc | - | 4 | 15 | - | - | - | Placeable light source |
| Wood | Material | - | 2 | 1 | - | - | - | Chopped from trees |
| Wood Plank | Material | - | 1 | 3 | - | - | - | Building/crafting |
| Stone | Material | - | 3 | 1 | - | - | - | Mined from mountains |
| Stone Block | Material | - | 2 | 3 | - | - | - | Building |
| Iron Ore | Material | - | 4 | 8 | - | - | - | Smelt into rods |
| Iron Rod | Material | - | 2 | 15 | - | - | - | Craft weapons/tools |

---

## Player Stats ✅

### D&D Attributes (all 10 for playtesting)
- **STR** — carry capacity (STR × 15 lbs), melee attack
- **DEX** — initiative, defense bonus
- **CON** — max HP (10 + CON)
- **INT** — spell power, skill learning (future)
- **WIS** — perception, FOV radius bonus (future)
- **CHA** — NPC starting affinity, gift effectiveness

### Derived Stats
- `carry_capacity` = STR × 15 (150 lbs default)
- `max_hp` = 10 + CON (20 default)
- `melee_attack` = STR / 2 + 1 (6 default)
- `defense_bonus` = DEX / 2 + 1 (6 default)

### Combat Stats (sum from equipment)
- `total_attack` = melee_attack + sum of all equipped items' attack
- `total_defense` = defense_bonus + sum of all equipped items' defense
- `total_damage_variance` = sum of all equipped items' damage_variance

### Damage Formula
- Player attacking: `total_attack ± total_damage_variance - enemy.defense`
- Enemy attacking: `enemy.attack ± enemy.damage_variance - player.total_defense / 2`
- Minimum damage is always 1
- Status messages show the actual post-defense damage dealt

### Leveling
- XP to next level = level × 25
- On level up: +1 to all attributes, full HP restore

---

## Damage Variance System ✅

### How it works
- Each weapon and each enemy has a `damage_variance` stat
- Damage roll = `base_stat + random(-variance, +variance)` before defense subtraction
- Player's total variance = sum of all equipped items' variance values
- Enemy variance is per-enemy-type

### Per-weapon variance
- Iron Sword: ±1 (damage ranges 2-4 before defense)

### Per-enemy variance
- Rat, Bat: 0 (consistent, predictable damage)
- Goblin, Spider: 1 (slight variation)
- Ogre: 2 (unpredictable heavy hits)

---

## Combat System ✅

### Player → Enemy (bump combat)
1. Player moves into enemy tile
2. Roll damage: `total_attack ± total_variance - enemy.defense`
3. Apply damage, show status message with actual damage dealt
4. If enemy dies: gain XP, drop items on ground, remove enemy

### Enemy → Player (enemy turn)
1. Enemy moves, then attacks if adjacent (Manhattan distance ≤ 1)
2. Roll damage: `enemy.attack ± enemy.variance`
3. Apply through player defense: `raw_damage - total_defense / 2`
4. Show status message with actual damage dealt
5. Player counter-attacks automatically (same formula as player attack)
6. If enemy dies from counter: gain XP, drop items on ground

### Enemy AI
- **Idle**: waits for player to get close, then switches to Chase
- **Chase**: BFS pathfinding to tile adjacent to player (stops 1 tile away, doesn't move onto player)
- **Wander**: random movement when not chasing
- Turn-based: enemies only move when player moves

### Enemy Types (13)
Created via `make_enemy(name, x, y)` — single source of truth for stats/drops.

| Enemy | HP | Atk | Def | XP | Var | Drops | Notes |
|---|---|---|---|---|---|---|---|
| Rat | 5 | 2 | 0 | 3 | 0 | Bread | |
| Bat | 4 | 1 | 0 | 2 | 0 | - | Night predator |
| Spider | 8 | 3 | 0 | 5 | 1 | Health Potion | Night predator |
| Goblin | 12 | 4 | 1 | 8 | 1 | Gold Coin | |
| Scorpion | 6 | 3 | 0 | 4 | 1 | - | Desert |
| Frog | 4 | 1 | 0 | 2 | 0 | - | Swamp |
| Snake | 6 | 3 | 0 | 4 | 1 | - | Grassland/Desert/Swamp |
| Wolf | 10 | 4 | 1 | 7 | 1 | Cooked Meat | Night predator |
| Bandit | 14 | 5 | 1 | 10 | 1 | Gold Coin x2 | Roads/open country |
| Zombie | 16 | 4 | 0 | 8 | 0 | Old Scroll | Night predator, ruins |
| Troll | 30 | 8 | 2 | 25 | 2 | Stone x2 | Mountains |
| Yeti | 28 | 7 | 1 | 22 | 2 | Cooked Meat | Tundra |
| Ogre | 25 | 7 | 3 | 20 | 2 | Gold Coin x3 | Dungeons only |

### Night Predators
- Wolf, Bat, Spider, Zombie (name-derived, no save data needed)
- Chase range 12 ignoring darkness (normal enemies need visible tile within 8)
- +2 attack at night (TimeOfDay::Night)

---

## Inventory UI (Overlay) ✅

### Layout
Two-panel overlay rendered on top of the game view:
```
┌─ INVENTORY ───── Weight: 45/150 ─ Gold: 25 ─┐
│ [Items]              │ [Equipment]            │
│ > Bread (3)    1 lb  │ Head: Hood             │
│   Health Potion 1 lb │ Shoulders: -           │
│   Iron Sword    4 lb │ Torso: Leather Armor   │
│   Leather Armor 8 lb │ Arms: -                │
│                       │ Hands: Iron Sword      │
│                       │ Legs: -                │
│                       │ Feet: Leather Boots    │
│                       │                        │
│ [Actions]                                    │
│ > Use  Equip  Drop  Examine  Gift  Cancel    │
└──────────────────────────────────────────────┘
```

### Navigation
- Left/Right: switch between item list and paper doll panels
- Up/Down: scroll through list or slots
- Enter: select item → show action menu, or select slot → show equipped item info
- TAB: switch between item list and equipment panel
- Esc: close inventory

---

## Equipment System ✅

### Equip Logic
1. Select item in inventory → choose "Equip"
2. If item has exactly one equip_slot → equip directly (unequip current if occupied)
3. If item has multiple valid slots (e.g., Gold Ring → Hand_L or Hand_R) → show EquipSelect
4. Equipped item removes from inventory, adds to equipment map
5. Unequipped item removes from equipment map, adds back to inventory

### Equipment Bonuses
- `total_attack` = sum of all equipped items' attack values + base melee attack
- `total_defense` = sum of all equipped items' defense values + base defense bonus
- `total_damage_variance` = sum of all equipped items' damage_variance values

---

## NPC System ✅

### NPC Types (9 unique NPCs)
- **Merchant** — is_merchant, trades with player, general goods shop
- **Villager** — generic settlement NPC
- **Sage** — knowledge-focused, values scrolls/books
- **Child** — easily impressed, likes shiny things
- **Wanderer** — wary, values practical items
- **Guard** — village protector, likes weapons/armor/light sources
- **Blacksmith** — merchant (weapons, armor, tools, iron rods), loves material gifts
- **Farmer** — farm worker, loves food gifts, hints at settlement zones
- **Herbalist** — merchant (potions, food), loves healing-item gifts

Villages spawn all six settlement NPCs; Sage/Wanderer/Child spawn near the
player at game start.

### NPC Fields
- Position, glyph, name, colors
- `affinity` (int, 0-100) — relationship level
- `is_merchant` (bool) — enables trade option
- `shop_inventory` (vector<pair<Item, int>>) — items for sale
- `dialogue_tree` (vector<DialogueNode>) — dialogue nodes
- `idle_timer`, `wander_timer` — AI timing

### NPC Movement
- Turn-based only (NPCs move when player moves)
- Idle state: waits, periodically switches to wander
- Wander state: picks random direction, moves if passable

---

## NPC Dialogue System ✅

### DialogueNode struct
```cpp
struct DialogueOption {
    string text;           // "Talk", "Trade", "Gift", "Goodbye"
    DialogueAction action; // Talk, Trade, Gift, Exit
    int next_node;         // index into dialogue tree, -1 for end
};

struct DialogueNode {
    string speaker_text;
    vector<DialogueOption> options;
};
```

### DialogueAction enum
- `None` — no action
- `Talk` — advance to next_node, show NPC lore
- `Trade` — open trade UI (merchants only)
- `Gift` — open gift item select
- `Exit` — close dialogue

### Dialogue Trees
Each NPC has a `vector<DialogueNode>` tree. Node 0 is the root.
Trees are reconstructed on load by name lookup.

---

## NPC Affinity System ✅

### Starting Affinity (per NPC type)
- Merchant: 60
- Villager: 30
- Sage: 40
- Child: 50
- Wanderer: 20

### Affinity Thresholds
- 0-20: Hostile (won't talk)
- 21-40: Wary (talks but no trade/gift)
- 41-60: Friendly (talks, accepts gifts)
- 61-80: Trusting (talks, accepts gifts, trades if merchant)
- 81-100: Devoted (special dialogue, future romance option)

### Affinity Changes
- Gift (Love): +15
- Gift (Like): +8
- Gift (Neutral): +2
- Gift (Dislike): -5
- Gift (Hate): -12

---

## Gift Reaction System ✅

### Reaction Types
- **Love** — NPC is thrilled, big affinity boost
- **Like** — NPC appreciates it, moderate boost
- **Neutral** — NPC accepts it, small boost
- **Dislike** — NPC is disappointed, small penalty
- **Hate** — NPC is offended, large penalty

### Reaction Determination
Base: item.value / 10 → positive reaction, modified by NPC personality.
Name-specific personalities are checked before the generic merchant rule.

| NPC Type | Loves | Hates |
|---|---|---|
| Merchant | High-value items | Cheap junk |
| Villager | Food, practical items | Weapons, scary things |
| Sage | Scrolls, books, knowledge items | Crude weapons |
| Child | Shiny things, food | Heavy armor, boring things |
| Wanderer | Maps, supplies, tools | Luxury items (show-off) |
| Guard | Weapons, armor, torches | - |
| Blacksmith | Materials (wood/stone/iron), fine gear | - |
| Farmer | Food, raw wood/stone | Luxury items |
| Herbalist | Healing items, consumables | Weapons |

---

## Trading System ✅

### Trade UI (Overlay)
Two-panel overlay:
```
┌─ TRADE ──── Gold: 25 ─────────────────────┐
│ [Your Inventory]  │ [Merchant Inventory]   │
│ > Bread (3)       │ > Health Potion (5)    │
│   Iron Sword      │   Iron Sword (3)       │
│   Leather Armor   │   Leather Armor (2)    │
│                    │   Wooden Shield (4)    │
│ [Buy: 30g] [Sell: 15g]                     │
│ [Confirm] [Cancel]                          │
└────────────────────────────────────────────┘
```

### Price Formula
- Buy price: `item.value × 1.5` (rounded up)
- Sell price: `item.value × 0.5` (rounded down)

---

## Save/Load System ✅

### What gets saved
- Player: position, HP, XP, level, gold, inventory, equipment, stats
- Map: tile grid, explored tiles (hex bitfield), map seed
- NPCs: position, glyph, colors, name, affinity, is_merchant, shop inventory, dialogue tree name
- Enemies: position, glyph, colors, name, HP, max HP, attack, defense, damage_variance, xp_value
- World items: position, item name, glyph, colors
- Meta: play time

### Save file format
- JSON, one file per slot: `%APPDATA%/AsciiGame/saves/slot_N.json`
- 8 save slots
- Version field for forward compatibility
- Explored tiles encoded as hex bitfield (compact)
- Map seed saved so terrain is identical on reload
- NPC dialogue trees reconstructed by name lookup on load

### Key implementation details
- Equipment JSON uses `[` ... `]` array brackets (not `{` `}`)
- Gold Coin pickup adds to gold currency, not inventory
- Enemy drops spawn as ground items only (not in player inventory)
- Enemies stop adjacent to player (BFS targets tile adjacent, not player tile)
- Player counter-attacks when enemies hit

---

## Resource Gathering ✅

Bump into terrain to harvest (costs a turn, updates FOV/light):
- **Tree** → 1 Wood, tree becomes Grass. Woodcutter's Axe equipped: 2 Wood.
- **Mountain** → 1 Stone (+15% Iron Ore), rock face becomes Stone floor.
  Pickaxe equipped: 2 Stone (+30% Iron Ore).

## Building System ✅

Press **B** → build mode. Cursor moves within 6 tiles of player (WASD),
Tab/[ ] cycles buildable, Enter places (costs a turn), Esc exits.
Buildable preview shows green (can place) / red (blocked or unaffordable).

| Buildable | Result | Materials |
|---|---|---|
| Wood Wall | WoodWall tile (#, blocks move+light) | 1 Wood Plank |
| Wood Floor | WoodFloor tile (.) | 1 Wood Plank |
| Wood Door | WoodDoor tile (+, walkable) | 2 Wood Plank |
| Stone Wall | Wall tile (#, blocks move+light) | 1 Stone Block |
| Stone Floor | StoneFloor tile (.) | 1 Stone Block |
| Campfire | PlacedObject light, radius 4 | 2 Wood + 1 Stone |
| Torch Post | PlacedObject light, radius 3 | 1 Wood |

- Placed tiles become chunk dirty deltas (persist via save)
- Placed light objects are rendered and feed the light BFS
- Can't build on water/trees/mountains, or wall in NPCs/enemies/yourself

## Zone System ✅

Press **Z** → zone mode. Enter marks corner 1, Enter marks corner 2,
then pick a type (Tab/arrows cycle, Enter confirms). X deletes the zone
under the cursor. Zones are world-space rectangles stored on World,
tinted on the map at all times (stronger when visible, dimmer when explored).

- **Farm** (green), **Storage** (brown), **Barracks** (red), **Bed** (blue)
- Saved in save file v6+ (world-space, spans chunks)

## Crafting System ✅

Press **C** → crafting overlay. 16 recipes, Enter crafts (costs a turn).
Ingredient counts shown as have/need; uncraftable rows dimmed.

- Processing: Wood→2 Plank, Stone→2 Block, 2 Iron Ore→1 Iron Rod
- Light/camp: Torch, Campfire Kit
- Tools: Woodcutter's Axe, Pickaxe
- Weapons: Dagger, Iron Sword, Spear, Steel Sword, Battle Axe, War Hammer
- Armor: Chain Mail, Steel Helm
- Alchemy: 2 Health Potion→1 Greater Health Potion

## Settings ✅

Pause menu → Settings. Three sliders (left/right adjust):
- Chunk Load Radius (2-4), Render Distance (1-4), Simulation Distance (1-3)
- Constraints load >= render+1, render >= sim enforced by World setters
- Persisted to `%APPDATA%/AsciiGame/settings.json`, applied at startup

## Main Menu & UI Polish ✅

- Title screen on startup: New Game / Load Game / Quit
- Pause menu: Save, Load, Settings, Main Menu, Quit
- Two-row HUD: combat stats on top, Day/time/biome/structure below
- 4-entry message log above HUD (newest brightest)
- Character sheet on **X**: attributes with derived stat notes, combat totals
- Structure discovery toast: "You discover a Village!" (once per structure)
- Placed objects (torches, campfires) rendered on the map
- Origin village is deterministic — regenerated identically on save/load

## File Structure (current)

```
src/
  main.cpp              — entry point, main menu, game loop, all overlay UIs (~1700 lines)
  engine/
    window.h/.cpp       — SDL2 window wrapper
    renderer.h/.cpp     — TrueType font grid renderer
  game/
    tile.h              — TileType enum, Tile struct (incl. player-built tiles)
    chunk.h/.cpp        — 64x64 tile grid, biome ID, dirty tracking, modification persistence
    world.h/.cpp        — Chunk manager, coordinate conversion, per-chunk entity storage, zones
    noise.h/.cpp        — Simplex noise (seed-based, deterministic)
    fov.h/.cpp          — FOV raycasting (perception-based radius)
    light.h/.cpp        — BFS flood-fill light propagation from sources
    time_system.h/.cpp  — Day/night cycle, day counter, ambient light, time display
    player.h/.cpp       — Player position, stats, inventory, equipment, combat, light source
    entity.h/.cpp       — Base entity class (world items)
    npc.h/.cpp          — NPC class, AI, affinity, dialogue, shop, gift personalities
    item.h/.cpp         — Item class, types, equip slots, 35-item database
    stats.h             — PlayerStats struct (D&D attributes + perception)
    dialogue.h/.cpp     — DialogueNode tree, 9 NPC dialogue builders, name lookup
    trade.h/.cpp        — Trade overlay UI
    enemy.h/.cpp        — Enemy class, BFS chase AI, night predators, make_enemy type DB
    save.h/.cpp         — JSON save/load (v6), full state restoration, per-chunk entities, zones
    biome.h/.cpp        — 6 biomes, noise-based selection, per-biome tile generation
    structure.h/.cpp    — 4 structure templates (Village, Cave, Ruins, Dungeon), stamping
    building.h/.cpp     — Buildables (walls/floors/doors/lights), zone types & colors
    crafting.h/.cpp     — 16 recipes, ingredient checking, crafting logic
    settings.h/.cpp     — Load/render/sim radius settings, JSON persistence
  assets/fonts/         — .ttf monospace fonts
  saves/                — save files (created at runtime)
```

---

## Future Considerations

### Milestone 4C: Biomes & Structures
- 6+ biomes with noise-based transitions
- Structure stamps (villages, caves, ruins, dungeons)
- Biome-dependent terrain generation and enemy spawning

### Milestone 4D: Building System
- Freeform tile placement mode
- Zone designation (farm, storage, barracks, bed)
- Adjustable render/simulation distance settings

### Milestone 5: Survival Systems
- Zone designation mode (farm, storage, building, bed)
- NPC settlers who arrive periodically
- Resource gathering (chop trees, mine stone)
- Simple construction from materials

### Milestone 6: Romance & Lineage
- Romance dialogue options (affinity > 80)
- Marriage event, spouse moves in
- Children born after marriage (inherit stats)
- Parent death → play as child (optional toggle)
- Family tree display

### Milestone 7: Character Creator
- Background selection (Warrior, Scholar, Rogue, Noble, Outlander)
- Background-specific stat spreads (primary stat determinant)
- Background-specific starting inventory
- Scenario intro text per background
- Custom name input

### Enchantments (post-Milestone 3)
- Items can have modifiers (+1, +2, etc.)
- Different name → separate stack
- Procedurally generated or found as loot

### Crafting (Milestone 5)
- Materials (wood, stone, ore) used in construction
- Recipe system for combining items
- Crafting stations
