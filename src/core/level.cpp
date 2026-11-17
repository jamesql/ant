/// @file level.cpp
/// @brief Construction of the picnic table level.

#include "core/level.h"

#include <random>

namespace ant {
namespace {

/// Food is never placed closer than this to the table edge.
constexpr float kEdgeMargin = 1.0f;
/// Free space kept between food and any obstacle, so food is always reachable.
constexpr float kObstacleClearance = 0.8f;
/// Food is kept this far outside the nest so it is never delivered for free.
constexpr float kNestClearance = 3.0f;
/// Minimum spacing between two pieces of food.
constexpr float kFoodSpacing = 1.5f;
/// Random spots tried per piece of food before giving up on it.
constexpr int kMaxPlacementAttempts = 1000;

/// Returns a random float in [low, high], built directly from the generator's
/// raw output so the result is identical on every compiler and platform.
float uniform(std::mt19937& rng, float low, float high) {
    const double unit = static_cast<double>(rng()) / 4294967295.0;
    return low + (high - low) * static_cast<float>(unit);
}

/// True if `spot` is a fair place to put a new piece of food.
bool isFreeSpot(const Level& level, Vec2 spot) {
    if (distance(spot, level.nest.position) < level.nest.radius + kNestClearance) {
        return false;
    }
    for (const Obstacle& obstacle : level.obstacles) {
        if (distance(spot, obstacle.position) < obstacle.radius + kObstacleClearance) {
            return false;
        }
    }
    for (const Food& food : level.foods) {
        if (distance(spot, food.position) < kFoodSpacing) {
            return false;
        }
    }
    return true;
}

}  // namespace

Level makePicnicLevel(std::uint32_t seed, int foodCount) {
    Level level;
    level.nest = {{-17.0f, -9.0f}, 1.5f};
    level.antStart = {-14.0f, -7.0f};
    level.antStartHeading = 0.3f;
    level.obstacles = {
        {ObstacleKind::Plate, {-6.0f, -3.0f}, 3.0f},
        {ObstacleKind::Plate, {7.0f, 4.0f}, 3.0f},
        {ObstacleKind::Plate, {12.0f, -6.0f}, 2.5f},
        {ObstacleKind::Cup, {-10.0f, 6.0f}, 1.2f},
        {ObstacleKind::Cup, {1.0f, -7.0f}, 1.2f},
        {ObstacleKind::Cup, {15.0f, 2.0f}, 1.2f},
        {ObstacleKind::Bottle, {0.0f, 2.0f}, 0.9f},
        {ObstacleKind::Bottle, {-3.0f, 8.0f}, 0.9f},
    };

    std::mt19937 rng(seed);
    const Vec2 reach = level.tableHalfSize - Vec2{kEdgeMargin, kEdgeMargin};
    for (int i = 0; i < foodCount; ++i) {
        for (int attempt = 0; attempt < kMaxPlacementAttempts; ++attempt) {
            const Vec2 spot{uniform(rng, -reach.x, reach.x), uniform(rng, -reach.y, reach.y)};
            if (isFreeSpot(level, spot)) {
                Food food;
                food.kind = static_cast<FoodKind>(i % 3);
                food.position = spot;
                level.foods.push_back(food);
                break;
            }
        }
    }
    return level;
}

}  // namespace ant
