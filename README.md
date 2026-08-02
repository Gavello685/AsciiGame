# ASCII Game

A single-character ASCII roguelike RPG built in C++17 with SDL2. Features an infinite procedural overworld with biomes, structures, day/night cycle, turn-based combat, NPC dialogue and trading, settlement building, and lineage systems.

## Gameplay

- **Explore** a seamless infinite world of 64×64 tile chunks, generated deterministically from a seed
- **Fight** enemies with turn-based bump combat — counter-attack when hit, damage variance per weapon/enemy
- **Talk** to NPCs with node-based dialogue trees, give gifts, trade at shops
- **Build** structures and designate zones (Farm, Storage, Barracks, Bed)
- **Survive** a day/night cycle with dynamic lighting, torches, and perception-based field of view
- **Settle** — NPCs arrive periodically, auto-assign to zones, gather resources
- **Marry** and have children who inherit stats — optionally continue as your child on death

## Controls

| Key | Action |
|-----|--------|
| Arrow keys / WASD | Move |
| Period (`.`) | Wait / skip turn |
| `G` | Pick up item underfoot |
| `I` | Inventory |
| `E` | Equipment |
| `X` | Examine item in inventory |
| `F` | Gift item to adjacent NPC |
| Enter | Interact / confirm |
| `B` | Build mode |
| `Z` | Zone designation |
| `M` | World map view (biomes + structures) |
| `R` | Restart (when dead) |
| Escape | Back / close overlay |

## Building

### Requirements

- C++17 compiler (MinGW GCC recommended on Windows)
- CMake 3.20+
- SDL2
- SDL2_ttf

### MSYS2 (Windows)

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf mingw-w64-x86_64-make
```

```bash
export PATH=/mingw64/bin:$PATH
cmake -B build -S . -G "MinGW Makefiles"
cmake --build build
./build/ascii_game.exe
```

### Linux

```bash
sudo apt install libsdl2-dev libsdl2-ttf-dev cmake g++
cmake -B build -S .
cmake --build build
./build/ascii_game
```

## Project Structure

```
src/
  main.cpp                  Entry point, game loop, all overlay UIs
  engine/
    window.h/.cpp           SDL2 window wrapper
    renderer.h/.cpp         TrueType font grid renderer with glyph caching
  game/
    tile.h                  TileType enum (20 types), Tile struct, passability/light helpers
    chunk.h/.cpp            64×64 tile grid, biome ID, dirty tracking, placed objects, modification persistence
    world.h/.cpp            Chunk manager, coordinate conversion, per-chunk entity storage, biome/structure queries
    noise.h/.cpp            Simplex noise (seed-based, deterministic, 1D/2D/fractal)
    biome.h/.cpp            6 biomes (Grassland, Forest, Desert, Swamp, Mountain, Tundra), noise-based terrain generation
    structure.h/.cpp        4 structure types (Village, Cave, Ruins, Dungeon), tile templates, entity spawning
    fov.h/.cpp              FOV raycasting with perception-based radius, ambient light scaling
    light.h/.cpp            BFS flood-fill light propagation from torches/campfires
    time_system.h/.cpp      Day/night cycle (600-turn days), ambient light levels, time display
    player.h/.cpp           Player position, stats, inventory, equipment, combat, light source
    entity.h/.cpp           Base entity class for world items
    npc.h/.cpp              NPC class, AI, affinity, dialogue, shop
    enemy.h/.cpp            Enemy class, BFS chase AI, combat stats, damage variance
    item.h/.cpp             Item class, 16-item database, equip/use/stack
    stats.h                 PlayerStats struct (D&D attributes + perception)
    dialogue.h/.cpp         DialogueNode tree, 5 NPC dialogue builders
    trade.h/.cpp            Trade overlay UI
    save.h/.cpp             JSON save/load, full state restoration, per-chunk entities
assets/fonts/               Place .ttf monospace fonts here
```

## Architecture

### World Generation
- World is divided into 64×64 tile chunks, loaded in a 5×5 grid around the player
- Chunks are deterministically generated from a seed using simplex noise
- Each tile gets its own biome via temperature/moisture noise (allows biome transitions)
- Structures (Villages, Caves, Ruins, Dungeons) are stamped into chunks with associated NPCs/enemies

### Entity System
- Entities are stored per-chunk in a hash map keyed by chunk coordinate
- When entities move across chunk boundaries, `rekey_entities()` fixes their storage
- On chunk unload, entities are cleared from memory; dirty chunks persist to save file

### Lighting & Time
- 600-turn day/night cycle: Dawn → Day → Dusk → Night
- Ambient light scales from 2 (night) to 15 (day)
- FOV radius scales with ambient light — full radius during day, torch range at night
- Light sources (torches, campfires) propagate via BFS flood-fill

### Save System
- JSON format, human-readable
- Per-chunk storage: tile modifications, explored bits, entities, placed objects
- Save slots (8 slots), stored in `%APPDATA%/AsciiGame/saves/`
- Format versioned for forward compatibility

## Milestones

| Milestone | Status |
|-----------|--------|
| 1: World Awareness (FOV, items, inventory) | Done |
| 2: Living World (NPCs, dialogue, trading) | Done |
| 3: Stats & Combat (attributes, enemies, death) | Done |
| 4A: Chunk-Based World (infinite overworld, persistence) | Done |
| 4B: Lighting & Time (day/night, torches, perception) | Done |
| 4C: Biomes & Structures (6 biomes, structure stamps) | Done |
| 4D: Building System | Planned |
| 5: Settlement (survival, settlers, resources) | Planned |
| 6: Romance & Lineage (marriage, children, generational play) | Planned |
| 7: Character Creator & Scenarios | Planned |
| 8: Save/Load System | Done |
| 9: Polish & Content | Planned |

## Design Documents

- [PLAN.md](PLAN.md) — Full development roadmap with detailed task breakdowns
- [DESIGN.md](DESIGN.md) — Comprehensive design reference for all systems
- [AGENTS.md](AGENTS.md) — Coding agents guide with build instructions and conventions
