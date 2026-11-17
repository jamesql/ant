/// @file level.h
/// @brief Plain data describing a map: the ground, the picnic and the nest.
///
/// A `Level` is only a description. `World` (see world.h) takes a level and
/// brings it to life by applying the game rules to it.

#pragma once

#include <cstdint>
#include <vector>

#include "core/vec2.h"

namespace ant {

/// The kinds of round picnic clutter the ants have to walk around.
enum class ObstacleKind { Plate, Cup, Glass, Bottle };

/// A solid circular object standing on the blanket.
struct Obstacle {
    ObstacleKind kind = ObstacleKind::Plate;
    Vec2 position;
    float radius = 1.0f;
};

/// A solid straight barrier, such as one side of the picnic basket.
/// It is the set of all points within `thickness` of the segment start-end.
struct Wall {
    Vec2 start;
    Vec2 end;
    float thickness = 0.2f;
};

/// A picnic basket: a walled rectangle with one doorway, at the north end of
/// its west side (the corner nearest the nest).
/// The solid parts are stored separately in `Level::walls`; this record
/// tells the renderer where the basket is.
struct Basket {
    Vec2 center;
    Vec2 halfSize;
    float doorWidth = 1.8f;
};

/// A puddle of poison. Ants standing in it lose health.
struct Poison {
    Vec2 position;
    float radius = 1.2f;
};

/// The kinds of food. They differ only in appearance (for now).
enum class FoodKind { Crumb, Sugar, Berry };

/// Where a piece of food currently is in its life cycle.
enum class FoodState {
    OnGround,   ///< Lying somewhere, waiting to be picked up.
    Carried,    ///< In an ant's jaws.
    Delivered,  ///< Dropped off at the nest; counts toward the colony's store.
};

/// A piece of food the colony wants to bring home.
struct Food {
    FoodKind kind = FoodKind::Crumb;
    Vec2 position;
    float radius = 0.3f;
    FoodState state = FoodState::OnGround;
};

/// The colony's home: food carried into this circle is delivered.
struct Nest {
    Vec2 position;
    float radius = 1.6f;
};

/// A complete map plus the colony's size and the time allowed.
struct Level {
    /// Half the size of the walkable ground. Ants can roam
    /// [-areaHalfSize.x, +areaHalfSize.x] by [-areaHalfSize.y, +areaHalfSize.y].
    Vec2 areaHalfSize{26.0f, 17.0f};
    /// Half the size of the picnic blanket, centred on the origin (visual only).
    Vec2 blanketHalfSize{20.0f, 12.0f};
    Nest nest;
    Vec2 antStart;                 ///< Where the player's ant begins.
    float antStartHeading = 0.0f;  ///< Radians; see `fromAngle`.
    int antCount = 1;              ///< Colony size, including the player's ant.
    std::vector<Obstacle> obstacles;
    std::vector<Wall> walls;
    std::vector<Basket> baskets;
    std::vector<Poison> poisons;
    std::vector<Food> foods;
    float timeLimit = 300.0f;  ///< Seconds allowed to bring home every piece of food.
};

/// Builds the picnic level: a blanket on the grass with plates, cups,
/// glasses, bottles, poison puddles and a basket full of food. The anthill
/// is on the grass just off one corner of the blanket.
///
/// The layout is always the same; only the food is scattered at random. The
/// same `seed` always produces the same scatter, which keeps games
/// repeatable (important for training and for tests).
///
/// @param seed          Chooses the food layout.
/// @param scatteredFood Pieces of food lying in the open on the blanket.
/// @param basketFood    Pieces of food inside the basket.
/// On a very crowded map fewer pieces may be placed if no free spot is found.
Level makePicnicLevel(std::uint32_t seed, int scatteredFood = 6, int basketFood = 8);

}  // namespace ant
