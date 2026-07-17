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
    src/game/map.cpp
    src/game/player.cpp
    src/game/entity.cpp
    src/game/fov.cpp
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
