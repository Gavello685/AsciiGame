# Implementation Notes

Cross-session notes for tracking progress, decisions, and context.

---

## Session 1: 4A Core Infrastructure ✅

### What Was Done
- Created `noise.h/.cpp` — simplex noise, seed-based, deterministic, 1D/2D/fractal
- Created `chunk.h/.cpp` — 64×64 tile grid, biome ID, dirty flag, sparse modification tracking, placed objects
- Created `world.h/.cpp` — chunk manager, 5×5 load radius, coordinate conversion, tile queries across chunks, adjustable radii
- Expanded `tile.h` — added Grass, Dirt, Sand, Snow, Stone, Tree, TallGrass, Mountain, DeepWater, Floor, Wall_Dungeon, Door_Stone, Trap, Stairs_Down, Stairs_Up. Added `light_level` field. Added `tile_blocks_movement()` and `tile_blocks_light()` helpers.
- Refactored `player.h/.cpp` — `move()`/`can_move()` now take `const World&`
- Refactored `enemy.h/.cpp` — `update()`/`chase_player()`/`can_move_to()` now take `const World&`. BFS pathfinding uses world-space coordinates with hash map instead of fixed-size vector.
- Refactored `npc.h/.cpp` — `update()`/`can_move_to()` now take `const World&`
- Refactored `fov.h/.cpp` — `compute()` takes `World&`, clears visibility across nearby chunks, raycasts across chunk boundaries
- Updated `save.h/.cpp` — `capture_state()` takes `const World&` instead of `const Map&`. Removed explored bitfield serialization (chunk-based persistence TBD). Save format version bumped to 3.
- Updated `main.cpp` — replaced `Map map` with `World world`. Entity spawning near player. Unbounded camera. Chunk loading updates after player movement.
- Updated `CMakeLists.txt` — added new source files

### What Still Needs Doing (4A)
- **Chunk-based entity persistence**: Entities (enemies, NPCs, world items) are currently global flat lists. Need to move to per-chunk storage for proper persistence. Currently if you save/load, entities are restored globally but don't respect chunk boundaries.
- **Chunk-based explored state**: Explored tiles are not persisted per-chunk yet. The old hex bitfield approach was removed but not replaced.
- **Remove old Map class**: `map.h/.cpp` still exists in the codebase but nothing uses it. Can be removed from CMakeLists.txt.
- **Dirty chunk save/load**: The infrastructure is in Chunk (mods_, placed_) but the save system doesn't serialize/deserialize per-chunk data yet.
- **Testing**: Need to verify walking across chunk boundaries works smoothly, terrain generates correctly, and save/load works.

### Key Architecture Decisions
- Chunks are 64×64 tiles, deterministically generated from seed
- World keeps 5×5 chunks loaded (320×320 tiles), unloads beyond radius 5
- All entity positions are world-space (not chunk-relative)
- FOV clears visibility across all loaded chunks, not just one map
- Enemy BFS pathfinding uses `unordered_map<int64_t, ...>` for visited tracking (no fixed-size vector)
- Camera is unbounded — no map edge clamping
- `tile_blocks_movement()` and `tile_blocks_light()` helpers centralize passability logic

### Files Created This Session
- `src/game/noise.h` (26 lines)
- `src/game/noise.cpp` (103 lines)
- `src/game/chunk.h` (82 lines)
- `src/game/chunk.cpp` (161 lines)
- `src/game/world.h` (91 lines)
- `src/game/world.cpp` (178 lines)

### Files Modified This Session
- `src/game/tile.h` — expanded from 37 to 131 lines
- `src/game/player.h` — forward declare World, update method signatures
- `src/game/player.cpp` — include world.h, update implementations
- `src/game/enemy.h` — forward declare World, update method signatures
- `src/game/enemy.cpp` — complete rewrite for world-space BFS
- `src/game/npc.h` — forward declare World, update method signatures
- `src/game/npc.cpp` — include world.h, update implementations
- `src/game/fov.h` — World& instead of Map&
- `src/game/fov.cpp` — complete rewrite for chunk-based clearing + raycasting
- `src/game/save.h` — World& in capture_state, removed map_width/height/explored from GameState
- `src/game/save.cpp` — World& in capture_state, simplified save format
- `src/main.cpp` — World instead of Map, entity spawning near player, unbounded camera
- `CMakeLists.txt` — added noise.cpp, chunk.cpp, world.cpp

---

## Session 2: 4A Persistence Wiring ✅

### What Was Done
- Added per-chunk entity fields (npcs, enemies, world_items) to `GameState::ChunkSave` in `save.h`
- Bumped `SAVE_VERSION` to 5
- Modified `capture_state()` in `save.cpp` to store entities in matching `ChunkSave` entries instead of flat lists
- Modified `save_game()` to serialize per-chunk NPCs, enemies, and world items inside each chunk object
- Modified `load_game()` to parse per-chunk entities from chunk objects
- Modified `apply_chunk_data()` to restore entities (NPCs, enemies, world items) per-chunk via `World::spawn_*`
- Added `World::clear_entities_in_chunk(int cx, int cy)` to `world.h/.cpp`
- Modified `World::update_loaded_chunks()` to clear entities when chunks unload
- Modified `main.cpp` load path: single `apply_chunk_data()` call now handles terrain + entities, removed manual flat-list entity spawning
- Removed dead code: `map.h` and `map.cpp` (superseded by World, never referenced in CMakeLists)

### Key Architecture Decisions
- Entities are now fully per-chunk: saved with their chunk, cleared on chunk unload, restored on chunk load
- Save format v5 stores entities inside each chunk object (npcs, enemies, world_items arrays)
- `apply_chunk_data()` is now the single entry point for restoring a saved world (terrain + entities)
- Flat entity lists (`state.npcs`, `state.enemies`, `state.world_items`) remain in GameState for backward compat but are no longer populated on save

### Files Modified This Session
- `src/game/save.h` — added entity fields to ChunkSave, bumped version
- `src/game/save.cpp` — per-chunk entity serialization/deserialization, entity restoration in apply_chunk_data
- `src/game/world.h` — added `clear_entities_in_chunk()` declaration
- `src/game/world.cpp` — implemented `clear_entities_in_chunk()`, used in chunk unload
- `src/main.cpp` — simplified load path, removed manual entity spawning

### Files Removed This Session
- `src/game/map.h` — dead code
- `src/game/map.cpp` — dead code

---

## Session 3: 4B Lighting & Time ✅

### What Was Done
- Created `time_system.h/.cpp` — day/night cycle (600-turn cycle: Dawn/Day/Dusk/Night), ambient light levels (2-15), time string formatting
- Created `light.h/.cpp` — BFS flood-fill light propagation from player torch and placed light sources, 8-directional spread, opacity-aware, diagonal cost
- Added 3 light source items to item database: Torch (Hand, radius 5), Lantern (Hand, radius 7), Campfire Kit (Misc, placeable)
- Added `perception()` method to `PlayerStats` (returns WIS)
- Added `has_light_source()` and `torch_radius()` to Player (checks Hand_L/Hand_R for Torch/Lantern)
- Added `set_light_level()` to World (world-space light level setter)
- Modified FOV: radius now computed from perception (base 8 + (WIS-10)/2, min 3)
- Modified render loop: tile and entity colors scaled by `light_level / 15.0f`
- Added time display to HUD (time string + period + torch indicator)
- Added time system persistence to save/load (turn_of_day field, version stays at 5)
- Updated `capture_state()` signature to accept `const TimeSystem&`

### Architecture Notes
- Light levels (0-15) stored per-tile in `Tile::light_level`
- Ambient light from time-of-day is the baseline; torches/placed objects add local light on top
- BFS from each source: step = 15/radius, diagonal costs extra, opaque tiles block but get lit
- FOV raycasting still determines line-of-sight visibility; light levels determine brightness
- Rendering: visible tiles drawn with `color * (light_level / 15.0f)`, explored tiles stay dim gray

### Files Created This Session
- `src/game/time_system.h` (33 lines)
- `src/game/time_system.cpp` (53 lines)
- `src/game/light.h` (14 lines)
- `src/game/light.cpp` (143 lines)

### Files Modified This Session
- `src/game/stats.h` — added `perception()` method
- `src/game/player.h` — added `has_light_source()`, `torch_radius()`
- `src/game/player.cpp` — implemented torch methods
- `src/game/item.cpp` — added Torch, Lantern, Campfire Kit items
- `src/game/world.h` — added `set_light_level()` declaration
- `src/game/world.cpp` — implemented `set_light_level()`
- `src/game/save.h` — added `turn_of_day` to GameState, `TimeSystem` include, updated `capture_state` signature
- `src/game/save.cpp` — time system serialization/deserialization, updated `capture_state` implementation
- `src/main.cpp` — integrated time/light systems, perception-based FOV, light-scaled rendering, HUD time display
- `CMakeLists.txt` — added time_system.cpp, light.cpp

---

## Session 4: 4C Biomes & Structures ✅
## Session 5: 4D Building, Zones, Crafting, Settings, Main Menu ✅

Both delivered as described in PLAN.md. See DESIGN.md for the resulting
systems; the notes below pick up from the audit that followed.

---

## Session 6: 4E Correctness & Structure ✅

An audit of the codebase found data-loss and memory-safety bugs, documented
behaviour that was never wired up, and a 2000-line `main.cpp` with nothing in
it reachable from a test. This session fixed the bugs, implemented the
documented behaviour, and split the code so the simulation can be tested
without a window.

### Bugs fixed

**Player edits vanished when a chunk unloaded.** `Chunk::set()` was used both
by world generation and by player building, and generation's writes were
recorded as deltas alongside the player's. Split into `set_generated()` (no
delta) and `modify()` (records a delta, marks the chunk dirty). `World` gained
a `ChunkArchive` holding what generation cannot reproduce — explored bits,
tile deltas, player-placed objects and live entities — archived on unload and
replayed on load. Repeated edits to one tile now overwrite rather than
accumulate.

**Enemy stats were corrupted by loading.** The v6 load path passed
`damage_variance` and `xp_value` to the `Enemy` constructor in swapped
positions. Rather than fix the call, save v7 rebuilds entities from
`make_enemy(name)` / `make_npc(name)` and stores only mutable state. There is
now one definition of what a Rat is and the save cannot disagree with it.

**Saves could be invalid JSON.** Serialisation emitted a trailing comma after
the equipment array. Replaced the ad-hoc string building with a `JsonWriter`
that owns comma placement, and added `tests/json_validator.h` plus a suite
that parses what the game writes.

**Use-after-free in the enemy turn loop.** The loop walked a `vector<Enemy*>`
snapshot while removing dead enemies and spawning loot, invalidating the
pointers behind it. `World` now exposes indexed per-chunk access
(`enemy_count_in_chunk`, `enemy_in_chunk`, `erase_enemy_in_chunk`) and
`resolve_turn` re-fetches each pointer per step. The bulk `get_all_enemies()`
accessors are gone, so the unsafe pattern is no longer reachable.

**Use-after-free in trade and inventory.** Both held `const Item&` into the
inventory, then removed the stack, then read the item's name for a status
message. All such paths now copy the `Item` first. `Npc::shop_item_at()`
returns by value for the same reason.

**HP came back wrong from a save.** Loading called
`take_damage(max_hp - saved_hp)`, which applied armour reduction, so an
armoured character loaded with more HP than they saved. Added
`Player::set_hp()`, which clamps to `[0, max_hp]` directly. Stats and
equipment are applied before it so max HP is final.

**Settings could be driven invalid.** The radius setters clamped in cascade,
so the UI could reach `render_radius` 0 or `sim_radius` -1. Rewrote them
around a `reconcile_radii()` that enforces `2 <= load <= 4`,
`1 <= render <= load - 1`, `1 <= sim <= render`. The shipped defaults
(load 2, render 2) violated the invariants and silently shrank render
distance at startup; now 3/2/1.

**Portability.** `std::getenv("APPDATA")` was dereferenced unchecked, and
`localtime_s` is Windows-only. Both moved behind `platform/paths`, which also
gained `find_monospace_font()` so the renderer no longer hard-codes
`C:/Windows/Fonts/cour.ttf`.

**Renderer demanded acceleration.** `SDL_CreateRenderer` was called with
`SDL_RENDERER_ACCELERATED` only, so the game exited at startup on any machine
with no usable GPU driver. A grid of glyphs costs nothing to draw, so it now
falls back to `SDL_RENDERER_SOFTWARE`, which is what makes the `--demo` run
work headless.

**Undefined behaviour.** `StructureEntity::type` was a `const char*` compared
with `==` against string literals. Now a `StructureEntityKind` enum.

**Rule of three.** `Window` and `Renderer` own raw SDL pointers and had
implicit copy operations. Copies and moves are deleted.

**Resize was ignored.** The window is `SDL_WINDOW_RESIZABLE` but
`SDL_WINDOWEVENT_SIZE_CHANGED` was unhandled, so the layout kept using the
startup size. Added `Window::on_resized()`.

### Documented behaviour that was missing

- **Light levels were computed and discarded.** `light::compute` filled
  `Tile::light_level` and rendering never read it. Visible tiles are now
  shaded by it (`light_shade()`), so a lit room reads differently from one lit
  only by the player's torch. Actors keep full colour so they stay readable.
- **FOV ignored its formula.** PLAN.md 4B specifies base 8 plus `(WIS-10)/2`
  scaled by ambient light. Implemented, with a floor of torch radius + 2 so
  the player is never blind.
- **Affinity gated nothing.** DESIGN.md's five bands existed only as prose.
  `Npc::can_talk/can_gift/can_trade` implement them, refusals are reported in
  the log, and the dialogue header shows the band name.
- **Simulation distance did nothing.** All loaded chunks were stepped every
  turn. `resolve_turn` now skips chunks outside the simulation radius, so the
  setting bounds per-turn cost as documented.

### Determinism

`std::rand()` drove combat rolls, AI wander and spawn jitter, which
contradicted the seed-reproducible world PLAN.md describes. Added `Rng`
(xorshift64*), seeded from the map seed, with its state saved and restored.
Same seed, same run.

### Structure

`main.cpp` went from 2000 lines to ~350. Split along the SDL boundary:

| Module | Contents |
|---|---|
| `game/session` | one playthrough; new game, restore from save, capture for save |
| `game/turn` | advance the world one player turn |
| `ui/key` | the keys the game reacts to, named without SDL |
| `ui/input` | every key press, dispatched per screen |
| `ui/ui_state` | interface-only state, menu tables, item actions |
| `ui/draw` | drawing primitives over Renderer |
| `ui/world_view` | terrain, actors, cursors, HUD, message log |
| `ui/menus` | title, pause, settings, save/load and death screens |
| `ui/overlays` | inventory, craft, build, zone, character, map, dialogue |
| `ui/trade_view` | trade panels, split out of `game/trade` |

`game/`, `platform/` and the SDL-free half of `ui/` build into
`ascii_game_core`; only `main.cpp`, `engine/` and the drawing code need SDL2.
That is what lets the test suite configure and run with no SDL2 installed and
no display.

`ui::handle_key` returns an `InputResult` (`quit`, `took_turn`, `fov_dirty`)
instead of writing to shared flags, so the frame loop reads as a sequence of
steps rather than a scan for mutated locals.

### Key architecture decisions

- Dependencies are one-way: `ui/` and `engine/` may use `game/`; `game/` uses
  neither. Enforced by the fact that `game/` compiles into a library with no
  SDL include path.
- Entity definitions live in exactly one place (`make_npc`, `make_enemy`), and
  the save format defers to them.
- Terrain is never serialised, only deviations from what the seed produces.
- Input is expressed in the game's own key vocabulary, so the whole input
  layer is testable and SDL stays at the edge.

### Tests and CI

136 tests across 11 suites, from 0. The two largest are `test_input`, which
drives menus, movement, combat, gathering, inventory, building, zoning,
crafting, the affinity gates, trade and the overlay cursors through
`handle_key`, and `test_save_roundtrip`, which asserts every field survives a
save and reload. CI builds under GCC and Clang, runs the suite again under
ASan and UBSan, and builds the game against SDL2. Warnings are
`-Wall -Wextra -Wshadow -Wnon-virtual-dtor`, promoted to errors in CI.

### What still needs doing

- `Torch` is documented as burning out; it does not. Duration is not specified.

---

## Session 7: EquipSelect, build labels

`DESIGN.md` described an EquipSelect step for items that fit more than one
slot (Gold Ring on either hand). Equip always used `Item::equip_slot()`, so
a sword and a torch — both named `Hand_L` — could not be held at once, and
boots only ever filled the left foot. Choosing Equip now opens a two-row
picker when `partner_slot` is set; unique slots (head, torso) still equip
immediately. `Player::equip` takes the target slot, copies the item before
unequipping the occupant (the previous reference dangled if `add_item`
reallocated), and refuses a slot the item cannot occupy.

The build panel's cost column now uses the same `name have/need` form as
crafting, so a campfire reads `Wood 0/2, Stone 0/1` rather than `0/2, 0/1`.

`Chunk::dirty()` stays as a "has player edits" flag. The archive keys off
visited chunks on purpose: explored tiles have to survive even when nothing
was built.

### What still needs doing

- `Torch` is documented as burning out; it does not. Duration is not specified.

---

## Session 8 (Next): Milestone 5 Settlement
