# PLAN.md

Development roadmap for ASCII Game. Each milestone is a self-contained, shippable chunk. Don't start the next until the current one works.

---

## Milestone 1: World Awareness ✅

**Goal:** Player can't see the whole map — only what's nearby and previously explored.

### Tasks
- [x] Generic Entity class (position, glyph, name, color, optional flags)
- [x] Field of view (FOV) — raycast from player, reveal tiles within line of sight
- [x] Fog of war — unseen tiles are black, explored-but-not-visible are dimmed
- [x] Item entities (`$`) spawned on map in passable tiles
- [x] Pickup interaction (press key to grab item underfoot)
- [x] Simple inventory display (press `i` to see what you're carrying)

---

## Milestone 2: Living World ✅

**Goal:** NPCs exist, move around, and the player can talk to them.

### Tasks
- [x] NPC entity class (position, name, sprite)
- [x] NPC schedules (idle, wander states — move only when player moves)
- [x] Bump-to-interact (walk into adjacent NPC + Enter/Space to talk)
- [x] Dialogue box overlay with node-based dialogue trees
- [x] 5 unique NPCs (Merchant, Villager, Sage, Child, Wanderer)
- [x] Gift-giving system (Love/Like/Neutral/Dislike/Hate reactions per NPC personality)
- [x] Trading system (buy/sell with gold, merchant shop inventory)

---

## Milestone 3: Stats & Combat ✅

**Goal:** Player has health, can fight, can die.

### Tasks
- [x] Player stats (HP, attack, defense, XP, level)
- [x] D&D attributes (STR/DEX/CON/INT/WIS/CHA)
- [x] Enemy entities with stats (Rat, Goblin, Ogre, Spider, Bat)
- [x] Turn-based combat (bump to attack)
- [x] Damage variance system (per-weapon and per-enemy variance ranges)
- [x] Player counter-attack when enemies hit
- [x] Enemy AI (BFS chase when adjacent, wander when idle)
- [x] Enemy loot drops (spawn on ground, picked up with G)
- [x] HP display in HUD (shows ATK range, DEF, Gold, Weight, Level, XP)
- [x] Death/restart (R to restart)
- [x] Wait/skip turn (period key)

---

## Milestone 4: Open World Generation

**Goal:** Infinite chunk-based overworld with biomes, lighting, structures, and building.

Split into 4 sub-milestones for incremental delivery.

---

### Sub-milestone 4A: Chunk-Based World ✅

**Goal:** Infinite overworld with seamless chunk loading, dirty-chunk persistence, and unbounded camera.

#### New Files
- [x] `src/game/chunk.h/.cpp` — Chunk class (64×64 tile grid, biome ID, dirty flag, sparse modification map)
- [x] `src/game/world.h/.cpp` — World class (chunk map, load/unload rings, coordinate conversion, entity queries)
- [x] `src/game/noise.h/.cpp` — Simplex noise (seed-based, deterministic, 1D/2D)

#### Modified Files
- [x] `tile.h` — Add `uint8_t light_level` to Tile. Add new TileTypes: Grass, Dirt, Sand, Snow, Stone, Tree, TallGrass, Mountain, DeepWater
- [x] `map.h/.cpp` — Remove or gut (replaced by World)
- [x] `player.h/.cpp` — `move()`/`can_move()` take `World&` instead of `const Map&`
- [x] `enemy.h/.cpp` — `update()`/`chase_player()`/`can_move_to()` take `World&`
- [x] `npc.h/.cpp` — `update()`/`can_move_to()` take `World&`
- [x] `fov.h/.cpp` — `compute()` takes `World&`, raycasts across chunk boundaries
- [x] `save.h/.cpp` — Save/load chunk state (tile deltas + entity lists per chunk)
- [x] `main.cpp` — Replace `Map map` with `World world`, unbounded camera, entity spawning via World
- [x] `CMakeLists.txt` — Add new .cpp files

#### Chunk Loading
- 5×5 loaded chunks (320×320 tiles in memory)
- Chunks generate deterministically from seed (instant, no loading)
- Unloaded chunks lose in-memory state; dirty deltas + entity lists persist to save

#### Chunk Persistence
- **Dirty tiles:** Player places/removes tiles → sparse delta saved per chunk
- **Entity state:** Enemy/NPC/item positions saved per chunk on unload (never regenerated from seed for visited chunks)
- **Clean chunks:** Regenerated from seed, entities spawned from biome tables
- **Cleared chunks:** Stay clear unless enemies wander in from neighbors

#### Enemies
- Spawn from biome tables on first visit
- Saved with chunk on unload (exact HP, position, drops)
- Wander between chunks (removed from source, added to destination)
- Cleared chunks stay clear (only repopulated by wandering)

---

### Sub-milestone 4B: Lighting & Time ✅

**Goal:** Dynamic light levels, day/night cycle, perception-based FOV, torch mechanics.

#### New Files
- [x] `src/game/light.h/.cpp` — BFS flood-fill light propagation from sources
- [x] `src/game/time_system.h/.cpp` — Day/night cycle, tick counter, ambient light

#### Modified Files
- [x] `stats.h` — Add `int perception() const { return wis; }`
- [x] `player.h/.cpp` — Add `has_light_source()`, `torch_radius()`, `is_torch_lit()`
- [x] `fov.h/.cpp` — Tiles with `effective_light == 0` invisible, `< 4` dim, `>= 4` visible with color scaling
- [x] `item.h/.cpp` — New items: Torch (Hand, radius 5, burns out), Lantern (Hand, radius 7, permanent), Campfire Kit (placeable, radius 4)
- [x] `main.cpp` — Light-level color scaling in render, HUD time/torch display, night overlay
- [x] `enemy.h/.cpp` — Some enemies ignore darkness, some stronger at night

#### Light Sources
- Player torch: always centered on player, not stored in chunk
- Wall torches: placed in structures (pre-stamped)
- Campfires: player-placed (stored in chunk's placed_objects)
- Ambient: time-of-day dependent (dawn/day/dusk/night)

#### Perception (WIS) Effect
- Base FOV radius: 8
- WIS bonus: `(wis - 10) / 2` (range -5 to +5)
- In darkness (light < 3): WIS grants 1-2 tile glimmer regardless

#### Time of Day
- Dawn (10%): ambient 4→15
- Day (50%): ambient 15
- Dusk (10%): ambient 15→4
- Night (30%): ambient 2
- Full cycle: 600 turns

---

### Sub-milestone 4C: Biomes & Structures ✅

**Goal:** 6+ biomes with transitions, structure stamps, embedded dungeons.

#### New Files
- [x] `src/game/biome.h/.cpp` — Biome enum, per-biome config (terrain, enemies, structures, colors)
- [x] `src/game/structure.h/.cpp` — Structure templates, stamping into chunks

#### Modified Files
- [x] `chunk.h/.cpp` — Store biome_id, structure_id
- [x] `world.h/.cpp` — Biome assignment (noise-based), structure placement, per-chunk entity spawning
- [x] `tile.h` — New TileTypes: Floor, Wall_Dungeon, Door_Stone, Trap, Stairs_Down, Stairs_Up
- [x] `item.h/.cpp` — New materials: Wood Plank, Stone Block, Iron Rod
- [x] `main.cpp` — HUD biome indicator, structure discovery messages

#### Biomes
- Grassland (grass, tall grass, scattered trees, ponds)
- Forest (dense trees, streams, spiders, wolves)
- Desert (sand, oases, scorpions, ancient temples)
- Swamp (dirt/water, dead trees, frogs, witch huts)
- Mountain (stone/snow, goblins, trolls, mines/caves)
- Tundra (snow/ice, wolves, yetis, abandoned forts)

#### Transitions
- Noise-based biome selection (continuous, not grid)
- 2-tile blend zone at boundaries

#### Structures
- Village (Grassland): houses, merchant, villagers, torches, loot
- Cave (Mountain/Forest): dark rooms, torches, enemies, ore
- Ruins (Forest/Grassland): crumbling walls, loot
- Dungeon (embedded): multi-room, corridors, boss, doors

---

### Sub-milestone 4D: Building System ✅

**Goal:** Freeform tile placement + zone designation, full persistence.

#### New Files
- [x] `src/game/building.h/.cpp` — Build mode state, zone definitions, placement logic

#### Modified Files
- [x] `main.cpp` — BuildingMode/ZoneMode in GameMode enum, overlay UIs
- [x] `item.h/.cpp` — New placeables: Wood Wall, Stone Floor, Wooden Door, Torch Post, Campfire
- [x] `world.h/.cpp` — `place_tile()`, zone storage

#### Freeform Building
- Press B → build mode, cursor moves, Enter places, Tab cycles tiles ✅
- Consumes materials from inventory ✅
- Placed tiles added to chunk dirty delta ✅

#### Zone Designation
- Press Z → zone mode, rectangular selection, zone type picker ✅
- Zone types: Farm, Storage, Barracks, Bed ✅
- Zones saved in save file (world-space rects) ✅

#### Adjustable Render/Simulation Distance
- [x] `src/game/settings.h/.cpp` — Settings management, JSON serialization
- [x] Pause menu → Settings option
- Three sliders: Chunk Load Radius (2-4), Render Distance (1-4), Simulation Distance (1-3) ✅
- Constraints: load >= render + 1, render >= simulation (enforced by World setters) ✅
- Settings saved to `%APPDATA%/AsciiGame/settings.json` ✅

---

### Sub-milestone 4E: Correctness & Structure ✅

**Goal:** Make the existing systems behave as documented, and make them
testable, before building more on top of them.

#### New Files
- [x] `src/game/rng.h` — seedable xorshift64*, replacing `std::rand` so a seed
      reproduces a whole run and the state can be saved
- [x] `src/game/turn.h/.cpp` — turn resolution, lifted out of `main.cpp`
- [x] `src/game/session.h/.cpp` — one playthrough; new game, restore, capture
- [x] `src/platform/paths.h/.cpp` — user data directory, timestamps, font lookup
- [x] `src/ui/` — `key`, `input`, `ui_state`, `draw`, `world_view`, `menus`,
      `overlays`, `trade_view`
- [x] `tests/` — self-registering harness and 11 suites
- [x] `.github/workflows/ci.yml` — GCC and Clang, sanitizers, SDL2 build

#### Correctness
- [x] Player tile edits persist across chunk unload (`Chunk::modify` records a
      delta, `set_generated` does not; `World` archives what generation cannot
      reproduce)
- [x] Save v7 rebuilds entities from archetypes, fixing a constructor argument
      swap that corrupted enemy stats on load
- [x] Save output is written through a writer that owns comma placement, so it
      is always valid JSON
- [x] Player HP restores without passing through armour reduction
- [x] Turn resolution re-fetches entities by index instead of walking a
      snapshot of pointers it then invalidates
- [x] Trade and inventory copy the `Item` before mutating its container
- [x] Radius settings cannot be driven into an invalid combination
- [x] `APPDATA` and `localtime_s` no longer assumed to exist
- [x] Window resize events are handled
- [x] `Window` and `Renderer` suppress copies rather than double-freeing
- [x] Structure entity kinds are an enum, not compared string literals

#### Documented behaviour that was not implemented
- [x] Per-tile light levels shade the glyph when it is drawn (4B)
- [x] FOV radius uses the documented formula and scales with ambient light (4B)
- [x] Affinity thresholds gate talk, gift and trade (DESIGN.md)
- [x] Simulation distance actually bounds which chunks are stepped (4D)

#### Structure
- [x] `main.cpp` reduced from 2000 lines to ~350: setup, keycode mapping,
      frame loop, render dispatch
- [x] Split along the SDL boundary, so `game/` and the input half of `ui/`
      link into a library the tests use without a window
- [x] Font path resolved at runtime instead of hard-coded to Windows

---

## Milestone 5: Settlement (Revised)

**Goal:** Survival systems + NPC settlers who work designated zones.

Groundwork in place: zones are world-space rectangles on `World` and already
save; `resolve_turn` is the single place per-turn effects belong, so meters
tick there; `session_start` owns the starting kit and stat defaults.

### Tasks
- [ ] Hunger/thirst meters (deplete every ~50 moves)
- [ ] Food items on map, water tiles drinkable
- [ ] Fatigue system (stats degrade without sleep)
- [ ] Bed/campfire tiles (interact to sleep)
- [ ] Status indicators in UI (hungry, thirsty, tired)
- [ ] NPC settlers who arrive periodically
- [ ] Settlers auto-assigned to zones
- [x] Resource gathering (chop trees, mine stone) — done in 4D pass (bump trees/mountains; tools double yield)
- [x] Simple construction from materials — done in 4D pass (build mode + crafting)

---

## Milestone 6: Romance & Lineage

**Goal:** Relationships, marriage, children, generational play.

Groundwork in place: affinity with its five bands, gift reactions per NPC
personality, and `make_npc` as the single definition of an NPC archetype.
`Session` owns the `Player` rather than the reverse, so a run can outlive a
character.

### Tasks
- [ ] Relationship system (affinity meter with NPCs)
- [ ] Gift-giving (give items to increase affinity) — already partially done
- [ ] Romance dialogue options
- [ ] Marriage event (ceremony, spouse moves in)
- [ ] Children born after marriage (named, inherits some stats)
- [ ] Parent death → play as child (optional toggle)
- [ ] Family tree display

---

## Milestone 7: Character Creator & Scenarios

**Goal:** Player creates a character with a background that shapes stats, starting gear, and opening scenario.

Groundwork in place: `session_start` already decides stats and starting
inventory, so a background is a table it consults. The creator screen is
another `GameMode` and an `ui/overlays` entry.

### Tasks
- [ ] Background selection screen (Warrior, Scholar, Rogue, Noble, Outlander, etc.)
- [ ] Background-specific stat spreads
- [ ] Background-specific starting inventory
- [ ] Background-specific NPC dialogue variations
- [ ] Scenario intro text (unique opening narrative per background)
- [ ] Custom name input
- [ ] Optional: stat point buy after background
- [ ] Summary screen before game start

---

## Milestone 8: Save/Load System ✅

**Goal:** Full game state serialization with multiple save slots.

### Tasks
- [x] JSON serialization format
- [x] Save player state (position, stats, inventory, equipment, gold, HP, XP, level)
- [x] Save map state (tile grid, explored flags as hex bitfield, room seed)
- [x] Save NPC state (position, affinity, glyph, colors, is_merchant, shop inventory)
- [x] Save enemy state (position, glyph, colors, HP, attack, defense, damage_variance, xp_value, drops)
- [x] Save world items (position, item name, glyph, colors)
- [x] Save slot selection screen (8 slots)
- [x] Load slot selection screen
- [x] Save file versioning (version field)
- [x] Full state restoration from save (map seed restores identical terrain)

### Design Notes
- JSON format for human-readability and easy debugging
- Save files stored in `%APPDATA%/AsciiGame/saves/`
- Each save is a single `.json` file named `slot_N.json`
- Save metadata (name, timestamp, play time) stored in save index
- Version field in save file allows migration when data format changes
- Explored tiles serialized as hex bitfield (compact)
- Map seed saved so terrain is identical on reload
- NPC dialogue trees reconstructed by name lookup on load
- v7: entities are rebuilt from their archetype factory (`make_npc`,
  `make_enemy`) and only mutable state is stored, and the action RNG's state
  round-trips so a reloaded game continues the same sequence

### Save File Structure
```json
{
  "version": 7,
  "character": {
    "name": "Player",
    "stats": { "str": 10, "dex": 10, ... },
    "position": { "x": 42, "y": 23 },
    "hp": 20, "xp": 0, "level": 1,
    "gold": 50,
    "inventory": [...],
    "equipment": [...]
  },
  "world": {
    "seed": 12345,
    "chunk_states": {
      "5,3": {
        "tile_mods": [{"x": 12, "y": 8, "type": "Wall"}],
        "placed_objects": [...],
        "enemies": [...],
        "npcs": [...],
        "world_items": [...],
        "explored_hex": "0F3A..."
      }
    }
  },
  "meta": {
    "play_time_seconds": 3600
  }
}
```

---

## Milestone 9: Polish & Content (Future)

### Tasks
- [ ] Sound effects and ambient audio
- [ ] More enemy types and boss fights
- [ ] More NPC dialogue and quest lines
- [ ] Tile-based animations (doors, traps, etc.)
- [ ] Minimap
- [ ] Tutorial/onboarding flow
