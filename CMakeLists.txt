cmake_minimum_required(VERSION 3.16)
project(x68000_game_platform C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Shared, platform-independent core: game logic + software virtual hardware.
add_library(core STATIC
    src/game/game.c
    src/vhw/vhw.c
)
target_include_directories(core PUBLIC src/hal src/vhw src/game)

# --- headless backend: always built, no external deps (CI / parity tests) ---
add_executable(x68game_headless
    src/plat_headless/main_headless.c
    src/plat_common/audio_stub.c
)
target_link_libraries(x68game_headless PRIVATE core)

# --- SDL backend: built only when SDL2 is available (your Mac) ---
find_package(SDL2 QUIET)
if(SDL2_FOUND)
    add_executable(x68game_sdl
        src/plat_sdl/main_sdl.c
        src/plat_common/audio_stub.c
    )
    target_link_libraries(x68game_sdl PRIVATE core SDL2::SDL2 SDL2::SDL2main)
    message(STATUS "SDL2 found: building x68game_sdl")
else()
    message(STATUS "SDL2 not found: skipping x68game_sdl (brew install sdl2)")
endif()

enable_testing()
add_test(NAME parity_smoke COMMAND x68game_headless 180)
