/// @file senses.cpp
/// @brief Implementation of the ant's senses.

#include "agent/senses.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace ant {
namespace {

/// Directions of the five feelers, in radians from straight ahead (negative = left).
constexpr std::array<float, 5> kFeelerAngles{-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
constexpr float kFeelerReach = 3.0f;  ///< How far a feeler can sense something solid.
constexpr float kFeelerStep = 0.1f;   ///< Spacing of the points tested along a feeler.

/// Left, ahead, right: the three directions used for smelling.
constexpr std::array<float, 3> kSniffAngles{-0.6f, 0.0f, 0.6f};
constexpr float kPoisonReach = 0.9f;  ///< How far ahead poison can be smelled.
constexpr float kScentReach = 1.4f;   ///< How far ahead pheromone is sampled.
constexpr float kSightRange = 3.0f;   ///< How close food must be to be seen.

/// How close the first solid thing along a ray is: 1 = touching, 0 = nothing within reach.
float feel(const World& world, const Ant& ant, float angle) {
    const Vec2 direction = fromAngle(ant.heading + angle);
    for (float reach = kFeelerStep; reach <= kFeelerReach; reach += kFeelerStep) {
        if (world.isBlocked(ant.position + direction * reach)) {
            return 1.0f - reach / kFeelerReach;
        }
    }
    return 0.0f;
}

/// Appends the sin and cos of the angle from the ant's heading to `target`.
void appendDirection(std::vector<float>& senses, const Ant& ant, Vec2 target) {
    const float angle = toAngle(target - ant.position) - ant.heading;
    senses.push_back(std::sin(angle));
    senses.push_back(std::cos(angle));
}

}  // namespace

std::vector<float> observe(const World& world, std::size_t antIndex) {
    const Ant& ant = world.ants()[antIndex];
    const Level& level = world.level();
    std::vector<float> senses;
    senses.reserve(kSenseCount);

    // 0-4: feelers.
    for (const float angle : kFeelerAngles) {
        senses.push_back(feel(world, ant, angle));
    }

    // 5-7: poison just ahead.
    for (const float angle : kSniffAngles) {
        senses.push_back(world.isPoisoned(ant.position + fromAngle(ant.heading + angle) * kPoisonReach) ? 1.0f : 0.0f);
    }

    // 8-13: the two pheromones. Scent cannot be smelled through something
    // solid. Most trails are faint, so the square root is taken to spread
    // small values out (0.01 becomes 0.1) where the network can tell them apart.
    for (const Scent scent : {Scent::Food, Scent::Home}) {
        for (const float angle : kSniffAngles) {
            const Vec2 point = ant.position + fromAngle(ant.heading + angle) * kScentReach;
            const float strength = world.isPathClear(ant.position, point) ? world.pheromones().sample(scent, point) : 0.0f;
            senses.push_back(std::sqrt(strength));
        }
    }

    // 14-16: the nearest food in sight, if any.
    const Food* nearest = nullptr;
    float nearestDistance = kSightRange;
    for (const Food& food : world.foods()) {
        const float dist = distance(ant.position, food.position);
        if (food.state == FoodState::OnGround && dist < nearestDistance &&
            world.isPathClear(ant.position, food.position)) {
            nearest = &food;
            nearestDistance = dist;
        }
    }
    if (nearest != nullptr) {
        appendDirection(senses, ant, nearest->position);
        senses.push_back(1.0f - nearestDistance / kSightRange);
    } else {
        senses.insert(senses.end(), {0.0f, 0.0f, 0.0f});
    }

    // 17-19: the nest. Closeness is measured against the longest distance on the map.
    const float mapDiagonal = 2.0f * length(level.areaHalfSize);
    appendDirection(senses, ant, level.nest.position);
    senses.push_back(std::clamp(1.0f - distance(ant.position, level.nest.position) / mapDiagonal, 0.0f, 1.0f));

    // 20-21: the ant's own state.
    senses.push_back(ant.carriedFood ? 1.0f : 0.0f);
    senses.push_back(ant.health);
    return senses;
}

const char* senseName(int index) {
    static constexpr std::array<const char*, kSenseCount> names{
        "feel far left",   "feel left",       "feel ahead",      "feel right",     "feel far right",
        "poison left",     "poison ahead",    "poison right",    "food scent left", "food scent ahead",
        "food scent right", "home scent left", "home scent ahead", "home scent right", "food seen: side",
        "food seen: ahead", "food seen: near", "nest: side",      "nest: ahead",    "nest: near",
        "carrying",        "health",
    };
    return index >= 0 && index < kSenseCount ? names[static_cast<std::size_t>(index)] : "?";
}

}  // namespace ant
