cmake_minimum_required(VERSION 3.20)
project(AsciiGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

add_executable(ascii_game
    src/main.cpp
    src/engine/window.cpp
    src/engine/renderer.cpp
    src/game/player.cpp
    src/game/entity.cpp
    src/game/fov.cpp
    src/game/npc.cpp
    src/game/item.cpp
    src/game/dialogue.cpp
    src/game/trade.cpp
    src/game/enemy.cpp
    src/game/save.cpp
    src/game/noise.cpp
    src/game/chunk.cpp
    src/game/world.cpp
    src/game/time_system.cpp
    src/game/light.cpp
    src/game/biome.cpp
    src/game/structure.cpp
    src/game/building.cpp
    src/game/crafting.cpp
    src/game/settings.cpp
)

target_include_directories(ascii_game PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
)

target_link_directories(ascii_game PRIVATE
    ${SDL2_LIBRARY_DIRS}
    ${SDL2_TTF_LIBRARY_DIRS}
)

target_link_libraries(ascii_game PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
)

# Copy required runtime DLLs next to the executable so it runs outside MSYS2.
# This is the FULL dependency closure (SDL2_ttf pulls in freetype/harfbuzz/glib/etc).
# Adjust MSYS2_DLL_DIR if your MSYS2 lives elsewhere.
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
