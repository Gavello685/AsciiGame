# PLAN.md

Development roadmap for ASCII Game. Each milestone is a self-contained, shippable chunk. Don't start the next until the current one works.

---

## Milestone 1: World Awareness

**Goal:** Player can't see the whole map — only what's nearby and previously explored.

### Tasks
- [ ] Field of view (FOV) — raycast from player, reveal tiles within line of sight
- [ ] Fog of war — unseen tiles are black, explored-but-not-visible are dimmed
- [ ] Item tiles (`$`) scattered on map in passable tiles
- [ ] Pickup interaction (press key to grab item underfoot)
- [ ] Simple inventory display (press `i` to see what you're carrying)

### Design Notes
- FOV radius: ~8 tiles (standard roguelike)
- Explored tiles stay visible but dimmed (gray foreground instead of color)
- Items are part of the map tile data, not separate entities yet
- Inventory is a simple list — no stacks, no equip, no weight (yet)

---

## Milestone 2: Living World

**Goal:** NPCs exist, move around, and the player can talk to them.

### Tasks
- [ ] NPC entity class (position, name, sprite)
- [ ] Simple pathfinding (walk toward target, avoid walls)
- [ ] NPC schedules (wander between rooms, stand in doorways)
- [ ] Bump-to-interact (walk into NPC to trigger dialogue)
- [ ] Dialogue box overlay (text only, no choices yet)
- [ ] 3-5 placeholder NPCs with unique dialogue lines

### Design Notes
- NPCs are rendered as distinct characters: `a` (adventurer), `v` (villager), `m` (merchant)
- No combat, no trading yet — just flavour
- Dialogue is a simple text box at bottom of screen

---

## Milestone 3: Stats & Combat

**Goal:** Player has health, can fight, can die.

### Tasks
- [ ] Player stats (HP, attack, defense)
- [ ] Enemy entities with stats
- [ ] Turn-based combat (bump to attack, damage = atk - def)
- [ ] HP display in UI
- [ ] Death screen (simple: "You died. Press R to restart.")
- [ ] Basic enemy AI (chase player when in sight)

### Design Notes
- Combat is one-tile bump, no animation
- Enemies: `r` (rat, weak), `g` (goblin, medium), `o` (ogre, tough)
- Stats are integers, no scaling formulas

---

## Milestone 4: Survival Systems

**Goal:** Player must manage hunger, thirst, and sleep to survive.

### Tasks
- [ ] Hunger/thirst meters (deplete over time, every N turns)
- [ ] Food items on map (restore hunger)
- [ ] Water tiles become drinkable (walk into to drink)
- [ ] Fatigue system (stats degrade if no sleep)
- [ ] Bed/campfire tiles (interact to sleep, restore fatigue)
- [ ] Status indicators in UI (hungry, thirsty, tired)

### Design Notes
- Hunger/thirst tick every ~50 moves
- No cooking yet — raw food only
- Sleep is instant (no time skip)

---

## Milestone 5: Settlement

**Goal:** Player can designate zones and NPCs work them.

### Tasks
- [ ] Zone designation mode (press key, select area with cursor)
- [ ] Zone types: farm, storage, building, bed
- [ ] NPC settlers who arrive periodically
- [ ] Settlers assigned to zones (auto-assign or manual)
- [ ] Resource gathering (chop trees, mine stone — tile changes)
- [ ] Simple construction (place walls/floors from materials)

### Design Notes
- Zone mode is a separate input mode (WASD moves cursor, Enter confirms)
- Settlers are like NPCs but with a job
- Resources are abstract (no individual logs/stones — just "wood: 10")

---

## Milestone 6: Romance & Lineage

**Goal:** Relationships, marriage, children, generational play.

### Tasks
- [ ] Relationship system (affinity meter with NPCs)
- [ ] Gift-giving (give items to increase affinity)
- [ ] Romance dialogue options
- [ ] Marriage event (ceremony, spouse moves in)
- [ ] Children born after marriage (named, inherits some stats)
- [ ] Parent death → play as child (optional toggle)
- [ ] Family tree display

### Design Notes
- Affinity increases with gifts and dialogue choices
- Marriage is a threshold event (affinity > 80)
- Children are small, weaker versions of parent
- "Play as child" is opt-in — game can end on parent death too
