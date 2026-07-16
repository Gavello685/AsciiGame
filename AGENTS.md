# AGENTS.md

## Project

C++17 ASCII fantasy game — single-character RPG with settlement building, romance, and lineage. Windowed, not terminal-based.

## Build

```bash
# From MSYS2 MinGW64 bash (not PowerShell/cmd)
export PATH=/mingw64/bin:$PATH
cmake -B build -S . -G "MinGW Makefiles"
cmake --build build
./build/ascii_game.exe
```

One-liner to paste into MSYS2:
```bash
export PATH=/mingw64/bin:$PATH; cd "/c/Users/Hayden/Documents/OpenCode Projects/ASCII Game"; cmake -B build -S . -G "MinGW Makefiles" && cmake --build build && ./build/ascii_game.exe
```

MSYS2 install path: `C:\msys64`
Installed packages: `mingw-w64-x86_64-gcc`, `mingw-w64-x86_64-cmake`, `mingw-w64-x86_64-SDL2`, `mingw-w64-x86_64-SDL2_ttf`, `mingw-w64-x86_64-make`

## Structure

```
src/
  main.cpp          — entry point, game loop
  engine/
    window.h/.cpp   — SDL2 window wrapper
    renderer.h/.cpp — TrueType font grid renderer
  game/             — game logic (empty — next up)
assets/fonts/       — place .ttf monospace fonts here
```

## Conventions

- C++17, no external deps beyond SDL2 + SDL2_ttf.
- Engine code in `engine/`, game logic in `game/`.
- Keep platform-specific code behind `#ifdef` guards — target Windows first.
- Use RAII for all SDL resources (no raw new/delete for SDL objects).
- Build with MinGW GCC via MSYS2, NOT MSVC.

## Gotchas

- **Path spaces**: The project lives in a path with spaces ("OpenCode Projects"). When passing paths to bash, always quote them.
- **DLLs**: Executables built with MSYS2 need MinGW runtime DLLs. Either run from the MSYS2 build dir, or copy `SDL2.dll`, `SDL2_ttf.dll`, `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll` alongside the .exe.
- **Font**: Renderer loads `C:/Windows/Fonts/cour.ttf` by default. Put a .ttf in `assets/fonts/` and update the path in `main.cpp` if you want a different font.
- `SDL_Delay(16)` is the frame cap (~60fps). Don't remove it or the loop spins at 100% CPU.
- Run the exe via `Start-Process` from PowerShell (not inline in bash) to get a visible window — MSYS2 bash subprocesses can hide the SDL window.
