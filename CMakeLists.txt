cmake_minimum_required(VERSION 3.20)
project(AsciiGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ASCII_GAME_BUILD_TESTS "Build the game-logic test suite" ON)
option(ASCII_GAME_WERROR "Treat compiler warnings as errors" OFF)

# ── Game logic ────────────────────────────────────────────────────────
#
# Everything that does not touch SDL lives here so the test suite can link it
# without a window or a font.
add_library(ascii_game_core STATIC
    src/platform/paths.cpp
    src/game/session.cpp
    src/game/trade.cpp
    src/game/player.cpp
    src/game/entity.cpp
    src/game/fov.cpp
    src/game/npc.cpp
    src/game/item.cpp
    src/game/dialogue.cpp
    src/game/enemy.cpp
    src/game/save.cpp
    src/game/noise.cpp
    src/game/chunk.cpp
    src/game/world.cpp
    src/game/turn.cpp
    src/game/time_system.cpp
    src/game/light.cpp
    src/game/biome.cpp
    src/game/structure.cpp
    src/game/building.cpp
    src/game/crafting.cpp
    src/game/settings.cpp
    src/ui/ui_state.cpp
    src/ui/input.cpp
)

target_include_directories(ascii_game_core PUBLIC ${CMAKE_SOURCE_DIR}/src)

if(MSVC)
    set(ASCII_GAME_WARNINGS /W4)
else()
    set(ASCII_GAME_WARNINGS -Wall -Wextra -Wshadow -Wnon-virtual-dtor)
endif()
if(ASCII_GAME_WERROR)
    if(MSVC)
        list(APPEND ASCII_GAME_WARNINGS /WX)
    else()
        list(APPEND ASCII_GAME_WARNINGS -Werror)
    endif()
endif()

target_compile_options(ascii_game_core PRIVATE ${ASCII_GAME_WARNINGS})

# ── Game executable ───────────────────────────────────────────────────
#
# Only the executable needs SDL2: the drawing code talks to Renderer, which
# wraps it. SDL2 is looked up without REQUIRED so the test suite still
# configures and runs on a machine with no SDL2 and no display.

find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 sdl2)
    pkg_check_modules(SDL2_TTF SDL2_ttf)
endif()

if(SDL2_FOUND AND SDL2_TTF_FOUND)
    set(ASCII_GAME_HAVE_SDL2 TRUE)
else()
    set(ASCII_GAME_HAVE_SDL2 FALSE)
    message(STATUS
        "SDL2/SDL2_ttf not found: skipping the 'ascii_game' target. "
        "The 'ascii_game_tests' target is unaffected.")
endif()

if(ASCII_GAME_HAVE_SDL2)
    add_executable(ascii_game
        src/main.cpp
        src/engine/window.cpp
        src/engine/renderer.cpp
        src/ui/draw.cpp
        src/ui/world_view.cpp
        src/ui/menus.cpp
        src/ui/overlays.cpp
        src/ui/trade_view.cpp
    )

    target_compile_options(ascii_game PRIVATE ${ASCII_GAME_WARNINGS})

    target_include_directories(ascii_game PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${SDL2_TTF_INCLUDE_DIRS}
    )

    target_link_directories(ascii_game PRIVATE
        ${SDL2_LIBRARY_DIRS}
        ${SDL2_TTF_LIBRARY_DIRS}
    )

    target_link_libraries(ascii_game PRIVATE
        ascii_game_core
        ${SDL2_LIBRARIES}
        ${SDL2_TTF_LIBRARIES}
    )
endif()

# ── Tests ─────────────────────────────────────────────────────────────

if(ASCII_GAME_BUILD_TESTS)
    enable_testing()
    add_executable(ascii_game_tests
        tests/main.cpp
        tests/test_chunk_coords.cpp
        tests/test_tile_persistence.cpp
        tests/test_save_roundtrip.cpp
        tests/test_json_format.cpp
        tests/test_entities.cpp
        tests/test_crafting_building.cpp
        tests/test_world_settings.cpp
        tests/test_turn.cpp
        tests/test_rng.cpp
    )
    target_compile_options(ascii_game_tests PRIVATE ${ASCII_GAME_WARNINGS})
    target_include_directories(ascii_game_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    target_link_libraries(ascii_game_tests PRIVATE ascii_game_core)
    add_test(NAME ascii_game_tests COMMAND ascii_game_tests)
endif()

# ── Windows runtime DLLs ──────────────────────────────────────────────
#
# Copy the full dependency closure next to the executable so it runs outside
# MSYS2 (SDL2_ttf pulls in freetype/harfbuzz/glib and friends).
# Adjust MSYS2_DLL_DIR if your MSYS2 lives elsewhere.
if(WIN32 AND ASCII_GAME_HAVE_SDL2)
    set(MSYS2_DLL_DIR "C:/msys64/mingw64/bin" CACHE PATH "Directory containing SDL2/MinGW runtime DLLs")
    set(GAME_DLLS
        SDL2.dll
        SDL2_ttf.dll
        libgcc_s_seh-1.dll
        libstdc++-6.dll
        libwinpthread-1.dll
        libfreetype-6.dll
        libharfbuzz-0.dll
        libbrotlidec.dll
        libbrotlicommon.dll
        libbz2-1.dll
        libpng16-16.dll
        zlib1.dll
        libglib-2.0-0.dll
        libgraphite2.dll
        libiconv-2.dll
        libintl-8.dll
        libpcre2-8-0.dll
    )
    foreach(dll ${GAME_DLLS})
        add_custom_command(TARGET ascii_game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MSYS2_DLL_DIR}/${dll}" "$<TARGET_FILE_DIR:ascii_game>/${dll}")
    endforeach()
endif()
