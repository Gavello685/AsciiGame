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

## Session 4 (Next): 4C Biomes & Structures
