/// @file level.h
/// @brief Plain data describing a map: the table, obstacles, food and the nest.
///
/// A `Level` is only a description. `World` (see world.h) takes a level and
/// brings it to life by applying the game rules to it.

#pragma once

#include <cstdint>
#include <vector>

#include "core/vec2.h"

namespace ant {

/// The kinds of picnic clutter the ant has to walk around.
enum class ObstacleKind { Plate, Cup, Bottle };

/// A solid circular object standing on the table.
struct Obstacle {
    ObstacleKind kind = ObstacleKind::Plate;
    Vec2 position;
    float radius = 1.0f;
};

/// The kinds of food. They differ only in appearance.
enum class FoodKind { Crumb, Sugar, Berry };

/// Where a piece of food currently is in its life cycle.
enum class FoodState {
    OnTable,    ///< Lying on the table, waiting to be picked up.
    Carried,    ///< In the ant's jaws.
    Delivered,  ///< Dropped off at the nest; counts toward the score.
};

/// A piece of food the ant must bring home.
struct Food {
    FoodKind kind = FoodKind::Crumb;
    Vec2 position;
    float radius = 0.3f;
    FoodState state = FoodState::OnTable;
};

/// The ant's home: food carried into this circle is delivered.
struct Nest {
    Vec2 position;
    float radius = 1.5f;
};

/// A complete map plus the ant's starting pose and the time allowed.
struct Level {
    /// Half the table's size. The walkable area spans
    /// [-tableHalfSize.x, +tableHalfSize.x] by [-tableHalfSize.y, +tableHalfSize.y].
    Vec2 tableHalfSize{20.0f, 12.0f};
    Nest nest;
    Vec2 antStart;
    float antStartHeading = 0.0f;  ///< Radians; see `fromAngle`.
    std::vector<Obstacle> obstacles;
    std::vector<Food> foods;
    float timeLimit = 180.0f;  ///< Seconds allowed to deliver every piece of food.
};

/// Builds the picnic table level.
///
/// The table, nest and obstacles are always the same. Food is scattered at
/// random, but the same `seed` always produces the same scatter, which keeps
/// games repeatable (important later for training and for tests).
///
/// @param seed      Chooses the food layout.
/// @param foodCount How many pieces of food to place. On a very crowded table
///                  fewer may be placed if no free spot can be found.
Level makePicnicLevel(std::uint32_t seed, int foodCount = 8);

}  // namespace ant
