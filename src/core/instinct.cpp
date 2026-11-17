/// @file instinct.cpp
/// @brief Implementation of the worker ant's foraging rules.

#include "core/instinct.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "core/world.h"

namespace ant {
namespace {

constexpr float kFoodSightRange = 3.0f;   ///< How close food must be for an ant to notice it directly.
constexpr float kNestSightRange = 5.0f;   ///< How close the nest must be for an ant to notice it directly.
constexpr float kFeelerLength = 0.9f;     ///< How far ahead an ant feels for trouble.
constexpr float kFeelerAngle = 0.7f;      ///< Angle of the side feelers from straight ahead (radians).
constexpr float kSnifferLength = 1.4f;    ///< How far ahead an ant samples scent.
constexpr float kSnifferAngle = 0.6f;     ///< Angle of the side scent samples from straight ahead (radians).
constexpr float kScentThreshold = 0.004f; ///< Scent weaker than this is ignored.
constexpr float kWanderStrength = 0.6f;   ///< How sharply an ant meanders when it has no lead.
constexpr float kHomeScentPull = 0.6f;    ///< How strongly home scent bends a homing ant's course.
/// A searching ant whose scent reserve has faded below this (about 70
/// seconds without finding anything) gives up and walks home to start afresh.
constexpr float kGiveUpReserve = 0.1f;

/// A turn effort that points the ant toward `target`.
float steerToward(const Ant& ant, Vec2 target) {
    const float error = wrapAngle(toAngle(target - ant.position) - ant.heading);
    return std::clamp(error * 3.0f, -1.0f, 1.0f);
}

/// A smooth, repeatable "random" number in [-1, 1] that is different for
/// every ant and drifts over time. Built from sine waves rather than a
/// random generator so the simulation stays exactly repeatable.
float wanderNoise(float time, std::size_t antIndex) {
    const float phase = static_cast<float>(antIndex);
    return 0.5f * std::sin(time * 1.7f + phase * 2.39f) + 0.5f * std::sin(time * 0.63f + phase * 4.1f);
}

/// The nearest piece of food the ant can see, if any.
std::optional<Vec2> visibleFood(const World& world, const Ant& ant) {
    std::optional<Vec2> nearest;
    float nearestDistance = kFoodSightRange;
    for (const Food& food : world.foods()) {
        const float dist = distance(ant.position, food.position);
        if (food.state == FoodState::OnGround && dist < nearestDistance &&
            world.isPathClear(ant.position, food.position)) {
            nearest = food.position;
            nearestDistance = dist;
        }
    }
    return nearest;
}

/// What the ant's three feelers or sniffers report: left, straight ahead, right.
template <typename T>
struct Fan {
    T left;
    T front;
    T right;
};

/// Evaluates `sense` at three points fanned out ahead of the ant.
template <typename Sense>
auto fanAhead(const Ant& ant, float sideAngle, float reach, Sense sense) {
    const auto at = [&](float angle) { return sense(ant.position + fromAngle(ant.heading + angle) * reach); };
    return Fan{at(-sideAngle), at(0.0f), at(sideAngle)};
}

/// If any feeler touches trouble, the action that steers away from it.
/// `preferRight` breaks the tie when trouble is on both sides.
std::optional<AntAction> avoid(const Fan<bool>& trouble, bool preferRight) {
    if (!trouble.left && !trouble.front && !trouble.right) {
        return std::nullopt;
    }
    float turn = preferRight ? 1.0f : -1.0f;
    if (trouble.left && !trouble.right) {
        turn = 1.0f;
    } else if (trouble.right && !trouble.left) {
        turn = -1.0f;
    }
    return AntAction{trouble.front ? 0.3f : 1.0f, turn};
}

}  // namespace

AntAction instinctAction(const World& world, std::size_t antIndex) {
    const Ant& ant = world.ants()[antIndex];
    const bool carrying = ant.carriedFood.has_value();
    const bool homing = carrying || ant.scentReserve < kGiveUpReserve;
    const Vec2 nest = world.level().nest.position;
    const float wander = wanderNoise(world.elapsedTime(), antIndex);

    // Scent at a point ahead, or nothing if something solid is in the way
    // (an ant cannot smell through a wall).
    const auto sniff = [&](Scent scent) {
        return fanAhead(ant, kSnifferAngle, kSnifferLength, [&](Vec2 point) {
            return world.isPathClear(ant.position, point) ? world.pheromones().sample(scent, point) : 0.0f;
        });
    };

    // Rule 1: the goal is in sight - go straight to it.
    if (carrying) {
        if (distance(ant.position, nest) < kNestSightRange && world.isPathClear(ant.position, nest)) {
            return {1.0f, steerToward(ant, nest)};
        }
    } else if (const std::optional<Vec2> food = visibleFood(world, ant)) {
        return {1.0f, steerToward(ant, *food)};
    }

    if (homing) {
        // Rule 2 (homing): never walk into poison. Solid things are not
        // avoided: pressing on toward home slides the ant around them.
        const Fan<bool> poison =
            fanAhead(ant, kFeelerAngle, kFeelerLength, [&](Vec2 point) { return world.isPoisoned(point); });
        if (const std::optional<AntAction> dodge = avoid(poison, wander >= 0.0f)) {
            return *dodge;
        }

        // Rule 3 (homing): head for the nest by sense of direction, letting
        // home scent bend the course toward wherever it is stronger.
        const Fan<float> home = sniff(Scent::Home);
        float turn = steerToward(ant, nest) + 0.3f * wander;
        if (std::max({home.left, home.front, home.right}) > kScentThreshold) {
            turn += kHomeScentPull * (home.right - home.left) / (home.right + home.left + kScentThreshold);
        }
        return {1.0f, std::clamp(turn, -1.0f, 1.0f)};
    }

    // Rule 2 (searching): steer away from anything solid or poisonous ahead.
    const Fan<bool> trouble = fanAhead(ant, kFeelerAngle, kFeelerLength, [&](Vec2 point) {
        return !world.isPathClear(ant.position, point) || world.isPoisoned(point);
    });
    if (const std::optional<AntAction> dodge = avoid(trouble, wander >= 0.0f)) {
        return *dodge;
    }

    // Rule 3 (searching): follow food scent toward wherever it is strongest.
    const Fan<float> food = sniff(Scent::Food);
    if (std::max({food.left, food.front, food.right}) > kScentThreshold) {
        if (food.front >= food.left && food.front >= food.right) {
            return {1.0f, 0.25f * wander};  // the trail runs straight ahead
        }
        return {1.0f, food.right > food.left ? 0.8f : -0.8f};
    }

    // Rule 4 (searching): no lead at all - wander.
    return {1.0f, kWanderStrength * wander};
}

}  // namespace ant
