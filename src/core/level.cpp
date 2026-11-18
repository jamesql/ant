/// @file level.cpp
/// @brief Construction of the picnic level.

#include "core/level.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace ant {
namespace {

/// Nothing is placed closer than this to the edge of the blanket.
constexpr float kEdgeMargin = 1.0f;
/// Free space kept between food and anything solid or poisonous, so food is always reachable.
constexpr float kClearance = 0.8f;
/// Food is kept this far outside the nest so it is never delivered for free.
constexpr float kNestClearance = 3.0f;
/// Minimum spacing between two pieces of scattered food.
constexpr float kFoodSpacing = 1.5f;
/// Minimum spacing between two pieces of food packed in the basket.
constexpr float kBasketFoodSpacing = 1.0f;
/// Random spots tried per item before giving up on it.
constexpr int kMaxPlacementAttempts = 1000;
/// Free space left between any two solid or poisonous things, so an ant
/// (0.6 wide) can always walk between them.
constexpr float kPassage = 1.6f;
/// Free space kept around the doorway of the basket.
constexpr float kDoorClearance = 3.0f;
/// The basket is kept at least this far from the edge of the blanket...
constexpr float kBasketMargin = 2.0f;
/// ...and at least this far from the nest, so reaching it is a real journey.
constexpr float kBasketNestDistance = 20.0f;

/// Returns a random float in [low, high], built directly from the generator's
/// raw output so the result is identical on every compiler and platform.
float uniform(std::mt19937& rng, float low, float high) {
    const double unit = static_cast<double>(rng()) / 4294967295.0;
    return low + (high - low) * static_cast<float>(unit);
}

/// How far `point` is from the outside of a basket's rectangle (0 if inside it).
float distanceToBasket(const Basket& basket, Vec2 point) {
    const Vec2 offset = point - basket.center;
    const Vec2 outside{std::max(std::fabs(offset.x) - basket.halfSize.x, 0.0f),
                       std::max(std::fabs(offset.y) - basket.halfSize.y, 0.0f)};
    return length(outside);
}

/// True if a circle of `radius` at `spot` lies wholly on the blanket.
bool isOnBlanket(const Level& level, Vec2 spot, float radius) {
    return std::fabs(spot.x) <= level.blanketHalfSize.x - radius - kEdgeMargin &&
           std::fabs(spot.y) <= level.blanketHalfSize.y - radius - kEdgeMargin;
}

/// A random point on the blanket.
Vec2 randomBlanketSpot(const Level& level, std::mt19937& rng) {
    return {uniform(rng, -level.blanketHalfSize.x, level.blanketHalfSize.x),
            uniform(rng, -level.blanketHalfSize.y, level.blanketHalfSize.y)};
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
    if (!isOnBlanket(level, spot, 0.0f)) {
        return false;
    }
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
        if (distanceToBasket(basket, spot) < kClearance) {
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

/// Puts the nest somewhere on the strip of grass around the blanket, and the
/// player's ant just in front of it, facing the picnic.
void placeNest(Level& level, std::mt19937& rng) {
    // The middle of the grass strip, on each side of the blanket.
    const Vec2 strip = (level.blanketHalfSize + level.areaHalfSize) * 0.5f;
    Vec2 spot;
    switch (rng() % 4) {
        case 0:  spot = {-strip.x, uniform(rng, -strip.y, strip.y)}; break;
        case 1:  spot = {strip.x, uniform(rng, -strip.y, strip.y)}; break;
        case 2:  spot = {uniform(rng, -strip.x, strip.x), -strip.y}; break;
        default: spot = {uniform(rng, -strip.x, strip.x), strip.y}; break;
    }
    level.nest = {spot, 1.6f};

    const Vec2 towardPicnic = spot * (-1.0f / length(spot));
    level.antStart = spot + towardPicnic * 2.4f;
    level.antStartHeading = toAngle(towardPicnic);
}

/// Adds a basket: four walls with a doorway in the corner nearest the nest.
///
/// The doorway is a gap at one end of one wall. It is cut from whichever of
/// the two walls meeting at that corner faces the nest more squarely, so an
/// ant inside that simply pushes toward home slides along the walls and out.
void addBasket(Level& level, Vec2 center, Vec2 halfSize, float doorWidth) {
    const Vec2 nest = level.nest.position;
    // Which way the nest lies from the basket, along each axis (+1 or -1).
    const float sideX = nest.x >= center.x ? 1.0f : -1.0f;
    const float sideY = nest.y >= center.y ? 1.0f : -1.0f;

    // The four corner coordinates: "near" is on the nest's side, "far" the other.
    const float nearX = center.x + sideX * halfSize.x;
    const float farX = center.x - sideX * halfSize.x;
    const float nearY = center.y + sideY * halfSize.y;
    const float farY = center.y - sideY * halfSize.y;
    const bool doorInVerticalWall = std::fabs(nest.x - nearX) >= std::fabs(nest.y - nearY);

    Basket basket;
    basket.center = center;
    basket.halfSize = halfSize;
    basket.doorWidth = doorWidth;

    level.walls.push_back({{farX, farY}, {nearX, farY}});  // the two walls away from the nest
    level.walls.push_back({{farX, farY}, {farX, nearY}});
    if (doorInVerticalWall) {
        level.walls.push_back({{farX, nearY}, {nearX, nearY}});
        level.walls.push_back({{nearX, farY}, {nearX, nearY - sideY * doorWidth}});  // stops short of the corner
        basket.door = {nearX, nearY - sideY * doorWidth * 0.5f};
    } else {
        level.walls.push_back({{nearX, farY}, {nearX, nearY}});
        level.walls.push_back({{farX, nearY}, {nearX - sideX * doorWidth, nearY}});  // stops short of the corner
        basket.door = {nearX - sideX * doorWidth * 0.5f, nearY};
    }
    level.baskets.push_back(basket);
}

/// Puts a basket of random size somewhere on the blanket, well away from the nest.
void placeBasket(Level& level, std::mt19937& rng) {
    Vec2 halfSize{uniform(rng, 4.0f, 5.5f), uniform(rng, 3.0f, 4.0f)};
    if (rng() % 2 == 0) {
        halfSize = {halfSize.y, halfSize.x};  // turned a quarter-turn
    }
    // If no random spot is far enough from the nest, use the far side of the blanket.
    Vec2 center{level.nest.position.x > 0.0f ? -11.0f : 11.0f, level.nest.position.y > 0.0f ? -5.0f : 5.0f};
    for (int attempt = 0; attempt < kMaxPlacementAttempts; ++attempt) {
        const Vec2 spot = randomBlanketSpot(level, rng);
        const bool fits = std::fabs(spot.x) <= level.blanketHalfSize.x - halfSize.x - kBasketMargin &&
                          std::fabs(spot.y) <= level.blanketHalfSize.y - halfSize.y - kBasketMargin;
        if (fits && distance(spot, level.nest.position) >= kBasketNestDistance) {
            center = spot;
            break;
        }
    }
    addBasket(level, center, halfSize, 1.8f);
}

/// True if a circle of `radius` at `spot` leaves walking room around the
/// basket, its doorway and every obstacle and puddle placed so far.
bool hasRoomAround(const Level& level, Vec2 spot, float radius) {
    if (!isOnBlanket(level, spot, radius)) {
        return false;
    }
    for (const Basket& basket : level.baskets) {
        if (distanceToBasket(basket, spot) < radius + kPassage ||
            distance(spot, basket.door) < radius + kDoorClearance) {
            return false;
        }
    }
    for (const Obstacle& obstacle : level.obstacles) {
        if (distance(spot, obstacle.position) < radius + obstacle.radius + kPassage) {
            return false;
        }
    }
    for (const Poison& poison : level.poisons) {
        if (distance(spot, poison.position) < radius + poison.radius + kPassage) {
            return false;
        }
    }
    return true;
}

/// Scatters 7 to 10 plates, cups, glasses and bottles.
void placeObstacles(Level& level, std::mt19937& rng) {
    constexpr ObstacleKind kinds[] = {ObstacleKind::Plate, ObstacleKind::Cup,   ObstacleKind::Glass, ObstacleKind::Bottle,
                                      ObstacleKind::Glass, ObstacleKind::Plate, ObstacleKind::Cup,   ObstacleKind::Bottle,
                                      ObstacleKind::Glass, ObstacleKind::Cup};
    const int count = 7 + static_cast<int>(rng() % 4);
    for (int i = 0; i < count; ++i) {
        float radius = 1.0f;
        switch (kinds[i]) {
            case ObstacleKind::Plate:  radius = uniform(rng, 2.2f, 3.2f); break;
            case ObstacleKind::Cup:    radius = 1.2f; break;
            case ObstacleKind::Glass:  radius = 1.0f; break;
            case ObstacleKind::Bottle: radius = 0.9f; break;
        }
        for (int attempt = 0; attempt < kMaxPlacementAttempts; ++attempt) {
            const Vec2 spot = randomBlanketSpot(level, rng);
            if (hasRoomAround(level, spot, radius)) {
                level.obstacles.push_back({kinds[i], spot, radius});
                break;
            }
        }
    }
}

/// Spills 3 or 4 puddles of poison.
void placePoisons(Level& level, std::mt19937& rng) {
    const int count = 3 + static_cast<int>(rng() % 2);
    for (int i = 0; i < count; ++i) {
        const float radius = uniform(rng, 1.0f, 1.4f);
        for (int attempt = 0; attempt < kMaxPlacementAttempts; ++attempt) {
            const Vec2 spot = randomBlanketSpot(level, rng);
            if (hasRoomAround(level, spot, radius)) {
                level.poisons.push_back({spot, radius});
                break;
            }
        }
    }
}

}  // namespace

Level makePicnicLevel(std::uint32_t seed, int scatteredFood, int basketFood) {
    Level level;
    level.antCount = 10;
    level.timeLimit = 240.0f;

    // Everything below draws from this one generator, in a fixed order, so
    // the seed alone decides the whole map.
    std::mt19937 rng(seed);
    placeNest(level, rng);
    placeBasket(level, rng);
    placeObstacles(level, rng);
    placePoisons(level, rng);

    // Food lying in the open, anywhere on the blanket.
    for (int i = 0; i < scatteredFood; ++i) {
        placeFood(
            level, static_cast<FoodKind>(i % 3), [&] { return randomBlanketSpot(level, rng); },
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
