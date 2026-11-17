/// @file world.h
/// @brief The simulation: the complete game state and the rules that change it.
///
/// `World` knows nothing about windows, keyboards or graphics. It is advanced
/// by calling `step()` with an `AntAction`, which makes it equally easy to
/// drive from a player, a unit test, or (later) a neural network that plays
/// thousands of games per second with no window at all.

#pragma once

#include <cstddef>
#include <optional>

#include "core/action.h"
#include "core/level.h"
#include "core/vec2.h"

namespace ant {

/// Numbers that define how the ant handles.
struct AntTuning {
    float radius = 0.3f;            ///< Body size used for collisions and pickup.
    float moveSpeed = 5.0f;         ///< Table units per second at full effort.
    float turnSpeed = 2.5f;         ///< Radians per second at full effort.
    float carrySpeedFactor = 0.75f; ///< Speed multiplier while carrying food.
};

/// The ant's current pose and what it holds.
struct Ant {
    Vec2 position;
    float heading = 0.0f;                    ///< Radians; see `fromAngle`.
    std::optional<std::size_t> carriedFood;  ///< Index into `World::foods()`, if carrying.
};

/// Whether the game is still running, and how it ended if not.
enum class GameStatus {
    Playing,
    Won,   ///< Every piece of food was delivered in time.
    Lost,  ///< Time ran out.
};

/// One running game.
class World {
public:
    /// Starts a new game on `level`.
    explicit World(Level level, AntTuning tuning = {});

    /// Advances the game by `dt` seconds while the ant performs `action`.
    ///
    /// In order: turn, walk, resolve collisions, pick up food, deliver food,
    /// run the clock. Does nothing once the game is won or lost.
    void step(const AntAction& action, float dt);

    const Ant& ant() const { return ant_; }
    const Level& level() const { return level_; }
    const std::vector<Food>& foods() const { return level_.foods; }
    const AntTuning& tuning() const { return tuning_; }
    GameStatus status() const { return status_; }

    /// Seconds left on the clock (never negative).
    float timeRemaining() const { return timeRemaining_; }

    /// How many pieces of food have reached the nest.
    int deliveredCount() const { return deliveredCount_; }

private:
    /// Keeps the ant on the table and out of obstacles.
    void resolveCollisions();
    /// Picks up the first piece of food the ant is touching, if its jaws are free.
    void pickUpFood();
    /// Drops carried food when inside the nest, and detects the win.
    void deliverFood();

    Level level_;  ///< The map. `level_.foods` holds the live food states.
    AntTuning tuning_;
    Ant ant_;
    GameStatus status_ = GameStatus::Playing;
    float timeRemaining_ = 0.0f;
    int deliveredCount_ = 0;
};

}  // namespace ant
