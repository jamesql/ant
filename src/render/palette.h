/// @file palette.h
/// @brief Colours shared by the 3D scene and the HUD, so both always agree.

#pragma once

#include <cstddef>

#include "core/level.h"
#include "raylib.h"

namespace ant {

inline constexpr Color kSkyTop{64, 132, 214, 255};      ///< Sky colour straight up.
inline constexpr Color kHorizon{196, 226, 244, 255};    ///< Sky colour at the horizon; also the fog colour.
inline constexpr Color kClothCream{246, 240, 230, 255}; ///< Blanket: light squares.
inline constexpr Color kClothRed{206, 52, 58, 255};     ///< Blanket: dark squares.
inline constexpr Color kNestDirt{124, 84, 50, 255};     ///< The dirt patch around the anthill.
inline constexpr Color kInk{40, 28, 24, 255};           ///< Dark outline colour (sprites, ant body parts).

/// The main colour of an obstacle. `index` is its position in the level's
/// obstacle list and gives neighbouring cups different colours.
inline Color obstacleColor(ObstacleKind kind, std::size_t index) {
    constexpr Color cupColors[3] = {{222, 60, 64, 255}, {58, 124, 214, 255}, {244, 186, 48, 255}};
    switch (kind) {
        case ObstacleKind::Plate:  return {244, 244, 248, 255};
        case ObstacleKind::Cup:    return cupColors[index % 3];
        case ObstacleKind::Bottle: return {44, 128, 78, 255};
    }
    return GRAY;
}

/// A flat colour representing a kind of food (used for mini-map dots).
inline Color foodColor(FoodKind kind) {
    switch (kind) {
        case FoodKind::Crumb: return {226, 176, 104, 255};
        case FoodKind::Sugar: return {250, 250, 255, 255};
        case FoodKind::Berry: return {214, 40, 60, 255};
    }
    return WHITE;
}

}  // namespace ant
