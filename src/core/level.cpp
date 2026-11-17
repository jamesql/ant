/// @file level.cpp
/// @brief Construction of the picnic level.

#include "core/level.h"

#include <random>

namespace ant {
namespace {

/// Scattered food is never placed closer than this to the blanket's edge.
constexpr float kEdgeMargin = 1.0f;
/// Free space kept between food and anything solid or poisonous, so food is always reachable.
constexpr float kClearance = 0.8f;
/// Food is kept this far outside the nest so it is never delivered for free.
constexpr float kNestClearance = 3.0f;
/// Minimum spacing between two pieces of scattered food.
constexpr float kFoodSpacing = 1.5f;
/// Minimum spacing between two pieces of food packed in the basket.
constexpr float kBasketFoodSpacing = 1.0f;
/// Random spots tried per piece of food before giving up on it.
constexpr int kMaxPlacementAttempts = 1000;

/// Returns a random float in [low, high], built directly from the generator's
/// raw output so the result is identical on every compiler and platform.
float uniform(std::mt19937& rng, float low, float high) {
    const double unit = static_cast<double>(rng()) / 4294967295.0;
    return low + (high - low) * static_cast<float>(unit);
}

/// True if `spot` is at least `spacing` away from every piece of food placed so far.
bool isClearOfFood(const Level& level, Vec2 spot, float spacing) {
    for (const Food& food : level.foods) {
        if (distance(spot, food.position) < spacing) {
            return false;
        }
    }
    return true;
}

/// True if `spot` is a fair place for a piece of food lying in the open.
bool isFreeOpenSpot(const Level& level, Vec2 spot) {
    if (distance(spot, level.nest.position) < level.nest.radius + kNestClearance) {
        return false;
    }
    for (const Obstacle& obstacle : level.obstacles) {
        if (distance(spot, obstacle.position) < obstacle.radius + kClearance) {
            return false;
        }
    }
    for (const Poison& poison : level.poisons) {
        if (distance(spot, poison.position) < poison.radius + kClearance) {
            return false;
        }
    }
    for (const Basket& basket : level.baskets) {
        const Vec2 offset = spot - basket.center;
        const bool nearBasket = std::fabs(offset.x) < basket.halfSize.x + kClearance &&
                                std::fabs(offset.y) < basket.halfSize.y + kClearance;
        if (nearBasket) {
            return false;
        }
    }
    return isClearOfFood(level, spot, kFoodSpacing);
}

/// Tries random spots from `randomSpot` until `accept` approves one, then
/// places a piece of food there. Gives up quietly after many failures.
template <typename RandomSpot, typename Accept>
void placeFood(Level& level, FoodKind kind, RandomSpot randomSpot, Accept accept) {
    for (int attempt = 0; attempt < kMaxPlacementAttempts; ++attempt) {
        const Vec2 spot = randomSpot();
        if (accept(spot)) {
            Food food;
            food.kind = kind;
            food.position = spot;
            level.foods.push_back(food);
            return;
        }
    }
}

/// Adds a basket: four walls with a doorway at the north end of the west side.
void addBasket(Level& level, Vec2 center, Vec2 halfSize, float doorWidth) {
    const float west = center.x - halfSize.x;
    const float east = center.x + halfSize.x;
    const float north = center.y - halfSize.y;
    const float south = center.y + halfSize.y;
    level.walls.push_back({{west, north}, {east, north}});
    level.walls.push_back({{west, south}, {east, south}});
    level.walls.push_back({{east, north}, {east, south}});
    level.walls.push_back({{west, north + doorWidth}, {west, south}});
    level.baskets.push_back({center, halfSize, doorWidth});
}

}  // namespace

Level makePicnicLevel(std::uint32_t seed, int scatteredFood, int basketFood) {
    Level level;
    level.nest = {{-23.0f, -14.0f}, 1.6f};
    level.antStart = {-21.5f, -12.5f};
    level.antStartHeading = 0.6f;
    level.antCount = 10;
    level.timeLimit = 240.0f;
    level.obstacles = {
        {ObstacleKind::Plate, {-8.0f, -4.0f}, 3.0f},
        {ObstacleKind::Plate, {2.0f, -7.0f}, 2.5f},
        {ObstacleKind::Cup, {-3.0f, -9.5f}, 1.2f},
        {ObstacleKind::Cup, {14.0f, -6.0f}, 1.2f},
        {ObstacleKind::Glass, {-12.0f, 5.0f}, 1.0f},
        {ObstacleKind::Glass, {-2.0f, 1.0f}, 1.0f},
        {ObstacleKind::Glass, {3.0f, 8.0f}, 1.0f},
        {ObstacleKind::Bottle, {-15.0f, -2.0f}, 0.9f},
        {ObstacleKind::Bottle, {8.0f, -9.0f}, 0.9f},
    };
    level.poisons = {
        {{-6.0f, 3.5f}, 1.3f},
        {{4.0f, 0.0f}, 1.2f},
        {{10.0f, -4.0f}, 1.2f},
    };
    addBasket(level, {12.0f, 3.0f}, {5.0f, 3.5f}, 1.8f);

    std::mt19937 rng(seed);

    // Food lying in the open, anywhere on the blanket.
    const Vec2 reach = level.blanketHalfSize - Vec2{kEdgeMargin, kEdgeMargin};
    for (int i = 0; i < scatteredFood; ++i) {
        placeFood(
            level, static_cast<FoodKind>(i % 3),
            [&] { return Vec2{uniform(rng, -reach.x, reach.x), uniform(rng, -reach.y, reach.y)}; },
            [&](Vec2 spot) { return isFreeOpenSpot(level, spot); });
    }

    // Food packed inside the basket, kept clear of its walls.
    const Basket& basket = level.baskets.front();
    const Vec2 inside = basket.halfSize - Vec2{kClearance, kClearance};
    for (int i = 0; i < basketFood; ++i) {
        placeFood(
            level, i % 2 == 0 ? FoodKind::Berry : FoodKind::Sugar,
            [&] {
                return basket.center + Vec2{uniform(rng, -inside.x, inside.x), uniform(rng, -inside.y, inside.y)};
            },
            [&](Vec2 spot) { return isClearOfFood(level, spot, kBasketFoodSpacing); });
    }
    return level;
}

}  // namespace ant
