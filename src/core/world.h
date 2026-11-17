/// @file world.h
/// @brief The simulation: the complete game state and the rules that change it.
///
/// `World` knows nothing about windows, keyboards or graphics. It is advanced
/// by calling `step()` with one `AntAction` per ant, which makes it equally
/// easy to drive from a player, a unit test, or a neural network that plays
/// thousands of games per second with no window at all.

#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "core/action.h"
#include "core/level.h"
#include "core/pheromones.h"
#include "core/vec2.h"

namespace ant {

/// Numbers that define how an ant's body works.
struct AntTuning {
    float radius = 0.3f;             ///< Body size used for collisions and pickup.
    float moveSpeed = 5.0f;          ///< World units per second at full effort.
    float turnSpeed = 3.0f;          ///< Radians per second at full effort.
    float carrySpeedFactor = 0.75f;  ///< Speed multiplier while carrying food.
    float poisonKillTime = 2.5f;     ///< Seconds of standing in poison that kill a healthy ant.
    float healTime = 20.0f;          ///< Seconds to recover full health away from poison.
    float respawnDelay = 4.0f;       ///< Seconds before the colony replaces a dead ant.
    float scentRate = 2.0f;          ///< Scent laid per second by an ant with a full scent reserve.
    float scentFadeTime = 30.0f;     ///< Seconds for an ant's scent reserve to drop to about 37%.
};

/// One ant: its pose, what it holds and how it is doing.
struct Ant {
    Vec2 position;
    float heading = 0.0f;                    ///< Radians; see `fromAngle`.
    std::optional<std::size_t> carriedFood;  ///< Index into `World::foods()`, if carrying.
    float health = 1.0f;                     ///< 1 = healthy, 0 = dead.
    bool alive = true;
    float respawnTimer = 0.0f;               ///< While dead: seconds until a replacement hatches.
    /// How strongly the ant marks the ground, from 1 (just left the nest, or
    /// just picked up food) fading toward 0. Because it fades, a trail is
    /// strongest at the end worth walking toward.
    float scentReserve = 1.0f;
    float distanceWalked = 0.0f;  ///< Total ground covered (drives the walking animation).
    int deliveries = 0;           ///< Pieces of food this ant has brought home.
};

/// Whether the game is still running, and how it ended if not.
enum class GameStatus {
    Playing,
    Won,   ///< Every piece of food reached the nest in time.
    Lost,  ///< Time ran out.
};

/// One running game.
class World {
public:
    /// Starts a new game on `level`. Ant 0 is the player's ant and starts at
    /// `level.antStart`; the rest of the colony starts at the nest.
    explicit World(Level level, AntTuning tuning = {});

    /// Advances the game by `dt` seconds. `actions[i]` controls ant `i`; ants
    /// without an action stand still.
    ///
    /// For each living ant, in order: turn, walk, resolve collisions, suffer
    /// poison, pick up food, deliver food, lay scent. Then scent spreads and
    /// fades and the clock runs. Does nothing once the game is won or lost.
    void step(std::span<const AntAction> actions, float dt);

    /// Convenience for the normal game: the player's ant performs
    /// `playerAction` and every other ant follows its instinct (see instinct.h).
    void step(const AntAction& playerAction, float dt);

    const std::vector<Ant>& ants() const { return ants_; }
    const Ant& player() const { return ants_.front(); }
    const Level& level() const { return level_; }
    const std::vector<Food>& foods() const { return level_.foods; }
    const PheromoneField& pheromones() const { return pheromones_; }
    const AntTuning& tuning() const { return tuning_; }
    GameStatus status() const { return status_; }

    /// Seconds left on the clock (never negative).
    float timeRemaining() const { return timeRemaining_; }

    /// Seconds of game time played so far.
    float elapsedTime() const { return elapsedTime_; }

    /// How many pieces of food the whole colony has brought home.
    int deliveredCount() const { return deliveredCount_; }

    /// True if an ant cannot stand at `point`: it is off the map, inside an
    /// obstacle or inside a wall.
    bool isBlocked(Vec2 point) const;

    /// True if `point` is in a poison puddle.
    bool isPoisoned(Vec2 point) const;

    /// True if nothing solid lies on the straight line between two points.
    bool isPathClear(Vec2 from, Vec2 to) const;

private:
    /// Tells the scent grid which cells are covered by obstacles and walls,
    /// so that scent cannot spread through solid things.
    void markSolidScentCells();
    /// Places ant `index` at the nest, healthy and facing outward.
    void spawnAtNest(std::size_t index);
    /// Turns and walks one ant.
    void move(Ant& ant, const AntAction& action, float dt);
    /// Keeps an ant on the map and out of obstacles and walls.
    void resolveCollisions(Ant& ant) const;
    /// Drains or restores health; kills the ant if health runs out.
    void applyPoison(Ant& ant, float dt);
    /// Picks up the first piece of food the ant touches, if its jaws are free.
    void pickUpFood(Ant& ant);
    /// Drops carried food when inside the nest, and detects the win.
    void deliverFood(Ant& ant);
    /// Marks the ground under the ant with home or food scent.
    void layScent(Ant& ant, float dt);

    Level level_;  ///< The map. `level_.foods` holds the live food states.
    AntTuning tuning_;
    std::vector<Ant> ants_;
    PheromoneField pheromones_;
    std::vector<AntAction> actionBuffer_;  ///< Reused each step to avoid reallocating.
    GameStatus status_ = GameStatus::Playing;
    float timeRemaining_ = 0.0f;
    float elapsedTime_ = 0.0f;
    int deliveredCount_ = 0;
};

}  // namespace ant
