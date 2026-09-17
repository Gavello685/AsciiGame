# AGENTS.md

## Project

C++17 ASCII fantasy game — single-character RPG with settlement building, romance, and lineage. Windowed (SDL2), not terminal-based.

`DESIGN.md` is the gameplay spec, `PLAN.md` the milestone roadmap, and `IMPLEMENTATION_NOTES.md` records why the tricky parts are the way they are. Read those before changing behaviour.

## Build

The build is plain CMake and works on Linux, macOS and Windows. SDL2 is only
needed for the `ascii_game` executable; the test suite links `ascii_game_core`,
which touches no SDL, so tests build and run on a machine with no display.

```bash
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure   # or ./build/ascii_game_tests
./build/ascii_game                           # needs SDL2 + SDL2_ttf
```

CMake options:

| Option | Default | Effect |
| --- | --- | --- |
| `ASCII_GAME_BUILD_TESTS` | `ON` | Build the `ascii_game_tests` target |
| `ASCII_GAME_WERROR` | `OFF` | Promote warnings to errors (CI turns this on) |

If `pkg-config` cannot find SDL2/SDL2_ttf, CMake prints a message and skips the
executable target rather than failing — check for that message before assuming
the game built.

### Windows / MSYS2

```bash
# From MSYS2 MinGW64 bash (not PowerShell/cmd)
export PATH=/mingw64/bin:$PATH
cmake -B build -S . -G "MinGW Makefiles"
cmake --build build
./build/ascii_game.exe
```

- Use the **MSYS2 cmake** (`C:\msys64\mingw64\bin\cmake.exe`), NOT the one in `C:\Program Files\CMake` — the stock one fails the compiler test against MinGW GCC.
- Required packages: `mingw-w64-x86_64-gcc`, `mingw-w64-x86_64-cmake`, `mingw-w64-x86_64-make`, `mingw-w64-x86_64-SDL2`, `mingw-w64-x86_64-SDL2_ttf`.
- Runtime DLLs (SDL2, SDL2_ttf, freetype/harfbuzz and friends) are copied next to the .exe by a CMake POST_BUILD step. Set `MSYS2_DLL_DIR` if MSYS2 is not at `C:\msys64`.
- Launch the .exe via `Start-Process` from PowerShell rather than inline in bash — MSYS2 bash subprocesses can hide the SDL window.

### Debian/Ubuntu

```bash
sudo apt-get install -y libsdl2-dev libsdl2-ttf-dev
```

## Structure

Three layers, and the dependency direction only ever points down:
`main.cpp` → `ui/` → `game/`, with `engine/` and `platform/` as leaves.
`game/` never includes SDL or anything from `ui/`.

```
src/
  main.cpp              — thin shell: SDL setup, event pump, key translation, frame loop (~350 lines)
  platform/
    paths.h/.cpp        — user data dir, local timestamp, monospace font discovery
  engine/
    window.h/.cpp       — SDL2 window wrapper (RAII, non-copyable)
    renderer.h/.cpp     — TrueType font grid renderer, glyph cache
  game/                 — simulation. No SDL, no UI includes.
    session.h/.cpp      — Session (world + player + clock + RNG); new game, restore, capture
    turn.h/.cpp         — turn resolution: NPC/enemy AI, clock advance, simulation radius
    rng.h               — seeded deterministic RNG (never use std::rand)
    tile.h              — TileType enum, Tile struct (incl. player-built tiles)
    chunk.h/.cpp        — 64x64 tile grid, biome ID, player-edit persistence across unload
    world.h/.cpp        — chunk manager, coordinate conversion, per-chunk entities, zones
    noise.h/.cpp        — Simplex noise (seed-based, deterministic)
    fov.h/.cpp          — FOV raycasting (perception-based radius)
    light.h/.cpp        — BFS flood-fill light propagation from sources
    time_system.h/.cpp  — day/night cycle, day counter, ambient light, time display
    player.h/.cpp       — position, stats, inventory, equipment, combat, light source
    entity.h/.cpp       — base entity class (world items)
    npc.h/.cpp          — Npc class, AI, affinity, dialogue, shop, gifts, make_npc archetypes
    enemy.h/.cpp        — Enemy class, BFS chase AI, night predators, make_enemy archetypes
    item.h/.cpp         — Item class, types, equip slots, item database
    stats.h             — PlayerStats struct (D&D attributes + perception)
    dialogue.h/.cpp     — DialogueNode tree, NPC dialogue builders, name lookup
    trade.h/.cpp        — trade state machine and pricing (SDL-free; view lives in ui/)
    save.h/.cpp         — JSON save/load (v7), strict validation, archetype rebuild
    biome.h/.cpp        — biomes, noise-based selection, per-biome tile generation
    structure.h/.cpp    — structure templates (Village, Cave, Ruins, Dungeon), stamping
    building.h/.cpp     — buildables (walls/floors/doors/lights), zone types & colours
    crafting.h/.cpp     — recipes, ingredient checking, crafting logic
    settings.h/.cpp     — load/render/sim radius settings, JSON persistence
  ui/
    key.h               — ui::Key enum: SDL-free key names
    ui_state.h/.cpp     — GameMode, message log, viewport, cursors, menu tables (SDL-free)
    input.h/.cpp        — all key handling: InputContext in, InputResult out (SDL-free)
    draw.h/.cpp         — drawing primitives (panels, glyphs, item rows)
    world_view.h/.cpp   — world/HUD rendering
    menus.h/.cpp        — main menu, pause, settings, save/load, death screens
    overlays.h/.cpp     — inventory, craft, build, zone, character, map, dialogue
    trade_view.h/.cpp   — trade overlay rendering
tests/                  — test_framework.h + one file per area; links ascii_game_core
assets/fonts/           — drop a .ttf here and it is picked up automatically
```

## Conventions

- C++17, no external deps beyond SDL2 + SDL2_ttf.
- **Keep simulation testable.** New gameplay logic goes in `game/` (or `ui/input.cpp`, `ui/ui_state.cpp` for input/UI state) and gets added to `ascii_game_core` in CMakeLists so tests can reach it. Only code that calls SDL belongs in the executable's source list.
- **Never `std::rand`.** Draw from `Session::rng` so a seed reproduces a run and a save resumes the same sequence.
- **No hard-coded paths.** Use `platform::user_data_dir()`, `platform::find_monospace_font()`.
- **No `const char*` identity comparisons.** Use enums for kinds/types; string literals are not guaranteed unique across translation units.
- Use RAII for SDL resources; `Window` and `Renderer` are non-copyable and non-movable by design.
- Watch for iterator/pointer invalidation: entity vectors live per chunk and mutate during turns. Copy an `Item` before mutating the container it came from, and re-fetch entity pointers by index rather than holding them across a mutation.
- Keep platform-specific code behind `#ifdef` guards.
- Add a test with every behaviour change. `ctest` must be green, and CI also runs the suite under ASan/UBSan.

## Gotchas

- **Path spaces**: on the original Windows checkout the project lives under "OpenCode Projects" — quote paths passed to bash.
- **Font**: resolution order is `ASCII_GAME_FONT` env var → `assets/fonts/` → system font dirs. If no font is found the game exits with a message instead of crashing.
- **Save location**: `platform::user_data_dir()` (`%APPDATA%` / `$XDG_DATA_HOME` / `~/Library/Application Support`), overridable with `ASCII_GAME_DATA_DIR` — tests use that to stay out of the real profile.
- **Save format is v7.** Terrain is regenerated from the seed; the file stores only what generation cannot reproduce, and entities are rebuilt through `make_npc`/`make_enemy` archetypes. Bump the version and handle the old one when you change the schema.
- **Radius invariants**: load ≥ render ≥ sim. `World`'s setters reconcile violations silently, so read values back after setting them (`session_apply_settings` does).
- `SDL_Delay(16)` is the frame cap (~60fps). Don't remove it or the loop spins at 100% CPU.

## Milestones Completed

- Milestone 1: World Awareness ✅
- Milestone 2: Living World ✅
- Milestone 3: Stats & Combat ✅
- Milestone 4A: Chunk-Based World ✅
- Milestone 4B: Lighting & Time ✅
- Milestone 4C: Biomes & Structures ✅
- Milestone 4D: Building System ✅ (+ resource gathering, crafting, main menu)
- Milestone 8: Save/Load System ✅ (v7: archetype rebuild, strict validation)
- Milestone 0 (retrofit): correctness pass, test suite, CI, `main.cpp` decomposition ✅

## Next Up

- Milestone 5: Settlement (survival meters, NPC settlers — gathering/construction already done)
- Milestone 6: Romance & Lineage
- Milestone 7: Character Creator
