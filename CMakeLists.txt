# Ant Picnic - build configuration.
#
# Targets:
#   ant_core  - game rules and simulation. Pure C++, no graphics, fully testable.
#   ant_tests - unit tests for ant_core (run with `ctest`).
#   ant_game  - the playable first-person game (ant_core + raylib rendering).

cmake_minimum_required(VERSION 3.24)
project(AntPicnic VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ANT_BUILD_GAME "Build the playable game (downloads raylib)" ON)
option(ANT_BUILD_TESTS "Build the unit tests" ON)

# Turns on a strict warning level for our own targets (not for third-party code).
function(ant_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# --- Core simulation library -------------------------------------------------
add_library(ant_core STATIC
    src/core/level.cpp
    src/core/world.cpp
)
target_include_directories(ant_core PUBLIC src)
ant_enable_warnings(ant_core)

# --- Unit tests --------------------------------------------------------------
if(ANT_BUILD_TESTS)
    enable_testing()
    add_executable(ant_tests
        tests/test_main.cpp
        tests/test_vec2.cpp
        tests/test_level.cpp
        tests/test_world.cpp
    )
    target_link_libraries(ant_tests PRIVATE ant_core)
    ant_enable_warnings(ant_tests)
    add_test(NAME ant_tests COMMAND ant_tests)
endif()

# --- Playable game -----------------------------------------------------------
if(ANT_BUILD_GAME)
    include(FetchContent)

    # raylib 5.5 declares an old minimum CMake version; this lets CMake 4+ accept it.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(raylib)

    add_executable(ant_game
        src/app/main.cpp
        src/render/renderer.cpp
        src/render/sprites.cpp
    )
    target_link_libraries(ant_game PRIVATE ant_core raylib)
    ant_enable_warnings(ant_game)
endif()
