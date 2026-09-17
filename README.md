# ASCII Game

A single-character ASCII roguelike RPG built in C++17 with SDL2. An infinite
procedural overworld with biomes and structures, a day/night cycle with dynamic
lighting, turn-based combat, NPC dialogue and trading, freeform building and
zone designation, and crafting.

## Gameplay

- **Explore** a seamless infinite world of 64×64 tile chunks, generated
  deterministically from a seed
- **Fight** enemies with turn-based bump combat — counter-attack when hit,
  damage variance per weapon and per enemy
- **Talk** to NPCs with node-based dialogue trees, give gifts, trade at shops.
  Affinity gates what an NPC will do: below 21 they will not talk, below 41
  they refuse gifts, and a merchant will not trade below 61.
- **Build** walls, floors, doors and light sources, and designate zones
  (Farm, Storage, Barracks, Bed)
- **Craft** 16 recipes from gathered wood, stone and ore
- **Survive** a day/night cycle with dynamic lighting, torches, and
  perception-based field of view

Not yet implemented: survival meters and NPC settlers (Milestone 5), romance
and lineage (Milestone 6), and the character creator (Milestone 7). See
[PLAN.md](PLAN.md).

## Controls

Arrow keys and WASD are interchangeable throughout.

### Playing

| Key | Action |
|-----|--------|
| WASD / arrows | Move, attack by walking into an enemy, gather by walking into a tree or mountain |
| `.` | Wait / skip turn |
| `G` | Pick up everything underfoot |
| Enter / Space | Talk to an adjacent NPC |
| `I` | Inventory |
| `C` | Crafting |
| `B` | Build mode |
| `Z` | Zone designation |
| `X` | Character sheet |
| `M` | World map |
| Escape | Pause menu |
| `R` | Restart (when dead) |

### Overlays

| Overlay | Keys |
|---------|------|
| Inventory | Up/Down select, Enter open actions, Left/Right pick an action, Enter on Equip opens a left/right slot picker for paired items, Tab switch to the paper doll, Escape or `I` close |
| Crafting | Up/Down select, Enter craft, Escape or `C` close |
| Build | WASD move the cursor (within 6 tiles), Tab or `]`/`[` cycle buildables, Enter place, Escape or `B` exit |
| Zone | WASD move the cursor, Enter mark a corner then the opposite corner, Enter again to pick a type, `X` delete the zone under the cursor, Escape or `Z` back out one step |
| World map | WASD move the cursor, Escape or `M` close |
| Dialogue | Up/Down select a reply, Enter choose, Escape leave |
| Trade | Tab or Left/Right switch panel, Up/Down select, Enter offer, Enter/Escape on the confirmation, Escape close |
| Settings | Up/Down pick a slider, Left/Right adjust, Escape or Enter save and close |

## Building

### Requirements

- C++17 compiler (MinGW GCC on Windows, GCC or Clang elsewhere)
- CMake 3.20+
- SDL2 and SDL2_ttf — needed only for the game itself. The test suite links
  the game logic directly and builds with no SDL2 and no display.

### MSYS2 (Windows)

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf mingw-w64-x86_64-make
```

```bash
export PATH=/mingw64/bin:$PATH
cmake -B build -S . -G "MinGW Makefiles"
cmake --build build
./build/ascii_game.exe
```

Use the MSYS2 CMake (`C:\msys64\mingw64\bin\cmake.exe`), not the one from
`C:\Program Files\CMake` — the stock build fails its compiler test against
MinGW GCC. Runtime DLLs are copied next to the executable by a post-build step.

### Linux

```bash
sudo apt install libsdl2-dev libsdl2-ttf-dev cmake g++
cmake -B build -S . -DCMAKE_CXX_COMPILER=g++
cmake --build build
./build/ascii_game
```

### Fonts

The renderer needs a monospace TrueType font. It looks for, in order:

1. `ASCII_GAME_FONT`, if set
2. any font under `assets/fonts` (searched from the working directory and two
   levels up, so running from `build/` works)
3. a per-platform list of system monospace fonts

## Tests

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DASCII_GAME_WERROR=ON
cmake --build build --target ascii_game_tests
./build/ascii_game_tests
```

136 tests over chunk coordinates, tile persistence across chunk unload, the
save round-trip and its JSON validity, entity archetypes, crafting and
building, world radius invariants, turn resolution, the RNG, the session
lifecycle, and every input path. CI runs them under GCC and Clang and again
under AddressSanitizer and UndefinedBehaviorSanitizer, then builds the game
against SDL2.

Saves and settings go to a scratch directory during tests;
`ASCII_GAME_DATA_DIR` overrides the location.

## Project Structure

```
src/
  main.cpp                  Window and renderer setup, SDL keycode mapping, frame loop
  platform/
    paths.h/.cpp            User data directory, local timestamps, font lookup
  engine/
    window.h/.cpp           SDL2 window wrapper
    renderer.h/.cpp         TrueType font grid renderer, glyphs cached white and tinted per draw
  game/                     Simulation. No SDL anywhere in here.
    session.h/.cpp          One playthrough: new game, restore from save, capture for save
    turn.h/.cpp             Advance the world one player turn
    rng.h                   Seedable xorshift64* PRNG, state round-trips through saves
    tile.h                  TileType enum, Tile struct, passability and light helpers
    chunk.h/.cpp            64×64 tile grid, biome ID, player tile deltas, placed objects
    world.h/.cpp            Chunk manager, coordinates, per-chunk entities, zones, archive
    noise.h/.cpp            Simplex noise (seed-based, deterministic, 1D/2D/fractal)
    biome.h/.cpp            6 biomes, noise-based terrain generation
    structure.h/.cpp        4 structure types, tile templates, entity spawning
    fov.h/.cpp              FOV raycasting, perception-based radius
    light.h/.cpp            BFS flood-fill light propagation
    time_system.h/.cpp      Day/night cycle (600-turn days), ambient light
    player.h/.cpp           Position, stats, inventory, equipment, combat, light source
    entity.h/.cpp           Base entity class for world items
    npc.h/.cpp              NPC AI, affinity gates, shops, gift reactions, make_npc
    enemy.h/.cpp            Enemy BFS chase AI, night predators, make_enemy
    item.h/.cpp             Item class, 40-item database, equip/use/stack
    stats.h                 PlayerStats struct (D&D attributes + perception)
    dialogue.h/.cpp         DialogueNode trees, 9 NPC dialogue builders
    trade.h/.cpp            Trade state and transactions
    building.h/.cpp         Buildables, placement rules, zone types
    crafting.h/.cpp         16 recipes, ingredient checks
    settings.h/.cpp         Load/render/simulation radii, JSON persistence
    save.h/.cpp             JSON save/load (v7), per-chunk state
  ui/
    key.h                   The keys the game reacts to, named without SDL
    input.h/.cpp            Every key press, dispatched per screen
    ui_state.h/.cpp         Interface-only state, menu tables, item actions
    draw.h/.cpp             Drawing primitives over Renderer
    world_view.h/.cpp       Terrain, actors, cursors, HUD, message log
    menus.h/.cpp            Title, pause, settings, save/load and death screens
    overlays.h/.cpp         Inventory, craft, build, zone, character, map, dialogue
    trade_view.h/.cpp       Trade panels
tests/                      Self-registering test harness and suites
assets/fonts/               Place .ttf monospace fonts here
```

## Architecture

The dependency rule is one-way: `ui/` and `engine/` may use `game/`, and
`game/` uses neither. SDL only appears in `engine/`, in the `ui/` files that
draw, and in `main.cpp`, which is why `game/` and the SDL-free half of `ui/`
build into `ascii_game_core` and can be tested without a window.

### World Generation
- 64×64 tile chunks, loaded in a ring around the player whose radius is a setting
- Chunks generate deterministically from the world seed using simplex noise
- Biome comes from temperature/moisture noise, so boundaries blend rather than grid
- Structures (Villages, Caves, Ruins, Dungeons) are stamped in with their NPCs and enemies

### Determinism
- One 32-bit world seed drives terrain, and the action RNG's seed is derived
  from it, so a seed reproduces a whole run — combat rolls, AI wander and
  spawn jitter included
- The RNG state is saved, so a reloaded game continues the same sequence

### Persistence
- Terrain is reproducible from the seed, so only what generation cannot
  reproduce is retained: explored bits, player tile deltas, player-placed
  objects and live entity state
- That state is archived when a chunk unloads and replayed when it loads
  again, which is what keeps a wall you built five chunks ago standing when
  you walk home
- Saves are JSON, eight slots, under the platform's user data directory
  (`%APPDATA%/AsciiGame` on Windows, `$XDG_DATA_HOME` or `~/.local/share`
  elsewhere); `ASCII_GAME_DATA_DIR` overrides it

### Entities
- Stored per-chunk in a hash map keyed by chunk coordinate
- `rekey_entities()` moves them between chunks when they cross a boundary
- Turn resolution walks chunks and re-fetches each entity by index, because
  a dying enemy and its dropped loot both mutate the vector being walked
- Only chunks within the simulation radius are stepped, so that setting bounds
  per-turn cost

### Lighting & Time
- 600-turn day/night cycle: Dawn → Day → Dusk → Night
- Ambient light runs 2 (night) to 15 (day)
- FOV radius is base 8 plus the WIS bonus, scaled by ambient light, with a
  floor so a torch always reveals its own radius
- Torches and campfires propagate light by BFS flood-fill; the resulting
  per-tile level shades the glyph when it is drawn

## Milestones

| Milestone | Status |
|-----------|--------|
| 1: World Awareness (FOV, items, inventory) | Done |
| 2: Living World (NPCs, dialogue, trading) | Done |
| 3: Stats & Combat (attributes, enemies, death) | Done |
| 4A: Chunk-Based World (infinite overworld, persistence) | Done |
| 4B: Lighting & Time (day/night, torches, perception) | Done |
| 4C: Biomes & Structures (6 biomes, structure stamps) | Done |
| 4D: Building System (building, zones, crafting, settings) | Done |
| 5: Settlement (survival meters, settlers) | Planned |
| 6: Romance & Lineage (marriage, children, generational play) | Planned |
| 7: Character Creator & Scenarios | Planned |
| 8: Save/Load System | Done |
| 9: Polish & Content | Planned |

## Design Documents

- [PLAN.md](PLAN.md) — development roadmap with task breakdowns
- [DESIGN.md](DESIGN.md) — design reference for all systems
- [IMPLEMENTATION_NOTES.md](IMPLEMENTATION_NOTES.md) — session-by-session log
- [AGENTS.md](AGENTS.md) — build instructions and conventions for coding agents
