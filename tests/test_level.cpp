/// @file test_level.cpp
/// @brief Tests for the picnic level generator.

#include "core/level.h"
#include "test_framework.h"

using namespace ant;

TEST(level_places_requested_food_count) {
    CHECK(makePicnicLevel(1, 8).foods.size() == 8);
    CHECK(makePicnicLevel(1, 3).foods.size() == 3);
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

TEST(level_food_is_reachable) {
    // Checked across many seeds: food is on the table, outside every
    // obstacle, and not already sitting in the nest.
    for (std::uint32_t seed = 0; seed < 50; ++seed) {
        const Level level = makePicnicLevel(seed);
        for (const Food& food : level.foods) {
            CHECK(std::fabs(food.position.x) < level.tableHalfSize.x);
            CHECK(std::fabs(food.position.y) < level.tableHalfSize.y);
            CHECK(distance(food.position, level.nest.position) > level.nest.radius);
            for (const Obstacle& obstacle : level.obstacles) {
                CHECK(distance(food.position, obstacle.position) > obstacle.radius);
            }
        }
    }
}

TEST(level_ant_starts_in_open_space) {
    const Level level = makePicnicLevel(1);
    for (const Obstacle& obstacle : level.obstacles) {
        CHECK(distance(level.antStart, obstacle.position) > obstacle.radius);
    }
}
