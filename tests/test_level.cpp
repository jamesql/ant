/// @file test_level.cpp
/// @brief Tests for the picnic level generator.

#include "core/level.h"
#include "test_framework.h"

using namespace ant;

namespace {

/// True if `point` lies inside the basket's rectangle.
bool isInside(const Basket& basket, Vec2 point) {
    const Vec2 offset = point - basket.center;
    return std::fabs(offset.x) < basket.halfSize.x && std::fabs(offset.y) < basket.halfSize.y;
}

}  // namespace

TEST(level_places_requested_food_counts) {
    const Level level = makePicnicLevel(1, 6, 8);
    int inBasket = 0;
    for (const Food& food : level.foods) {
        inBasket += isInside(level.baskets.front(), food.position) ? 1 : 0;
    }
    CHECK(level.foods.size() == 14);
    CHECK(inBasket == 8);
}

TEST(level_same_seed_gives_same_layout) {
    const Level a = makePicnicLevel(42);
    const Level b = makePicnicLevel(42);
    for (std::size_t i = 0; i < a.foods.size(); ++i) {
        CHECK(a.foods[i].position.x == b.foods[i].position.x);
        CHECK(a.foods[i].position.y == b.foods[i].position.y);
    }
}

TEST(level_different_seeds_give_different_layouts) {
    const Level a = makePicnicLevel(1);
    const Level b = makePicnicLevel(2);
    CHECK(a.foods[0].position.x != b.foods[0].position.x);
}

TEST(level_food_is_reachable_and_safe) {
    // Checked across many seeds: food is on the map, outside every obstacle
    // and poison puddle, and not already sitting in the nest.
    for (std::uint32_t seed = 0; seed < 50; ++seed) {
        const Level level = makePicnicLevel(seed);
        for (const Food& food : level.foods) {
            CHECK(std::fabs(food.position.x) < level.areaHalfSize.x);
            CHECK(std::fabs(food.position.y) < level.areaHalfSize.y);
            CHECK(distance(food.position, level.nest.position) > level.nest.radius);
            for (const Obstacle& obstacle : level.obstacles) {
                CHECK(distance(food.position, obstacle.position) > obstacle.radius);
            }
            for (const Poison& poison : level.poisons) {
                CHECK(distance(food.position, poison.position) > poison.radius);
            }
        }
    }
}

TEST(level_basket_has_a_doorway) {
    // The west wall stops short of the north wall, leaving a gap wide enough for an ant.
    const Level level = makePicnicLevel(1);
    const Basket& basket = level.baskets.front();
    const Wall& north = level.walls[0];
    const Wall& west = level.walls[3];
    CHECK_NEAR(west.start.y - north.start.y, basket.doorWidth, 1e-5f);
    CHECK(basket.doorWidth > 2.0f * (west.thickness + 0.3f));
}
