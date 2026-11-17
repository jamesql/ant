/// @file test_world.cpp
/// @brief Tests for the game rules. Each test builds a tiny hand-made level
///        so exactly one rule is exercised at a time.

#include "core/world.h"
#include "test_framework.h"

using namespace ant;

namespace {

constexpr float kTolerance = 1e-4f;
constexpr float kStep = 1.0f / 60.0f;
constexpr AntAction kWalkAhead{1.0f, 0.0f};
constexpr AntAction kStandStill{0.0f, 0.0f};

/// A bare table: ant in the middle facing +x, nest far away in a corner.
Level emptyLevel() {
    Level level;
    level.nest = {{-18.0f, -10.0f}, 1.5f};
    return level;
}

/// A piece of food at `position`.
Food foodAt(Vec2 position) {
    Food food;
    food.position = position;
    return food;
}

/// Runs the world for `seconds` of game time with a constant action.
void run(World& world, AntAction action, float seconds) {
    const int steps = static_cast<int>(seconds / kStep + 0.5f);
    for (int i = 0; i < steps; ++i) {
        world.step(action, kStep);
    }
}

}  // namespace

TEST(world_ant_walks_along_its_heading) {
    World world(emptyLevel());
    run(world, kWalkAhead, 1.0f);
    CHECK_NEAR(world.ant().position.x, world.tuning().moveSpeed, 1e-2f);
    CHECK_NEAR(world.ant().position.y, 0.0f, kTolerance);
}

TEST(world_positive_turn_increases_heading) {
    World world(emptyLevel());
    world.step({0.0f, 1.0f}, 0.1f);
    CHECK_NEAR(world.ant().heading, world.tuning().turnSpeed * 0.1f, kTolerance);
}

TEST(world_action_is_clamped_to_unit_range) {
    World normal(emptyLevel());
    World excessive(emptyLevel());
    normal.step({1.0f, 1.0f}, kStep);
    excessive.step({50.0f, 50.0f}, kStep);
    CHECK_NEAR(excessive.ant().position.x, normal.ant().position.x, kTolerance);
    CHECK_NEAR(excessive.ant().heading, normal.ant().heading, kTolerance);
}

TEST(world_ant_cannot_leave_the_table) {
    World world(emptyLevel());
    run(world, kWalkAhead, 20.0f);
    CHECK_NEAR(world.ant().position.x, world.level().tableHalfSize.x - world.tuning().radius, kTolerance);
}

TEST(world_ant_is_pushed_out_of_obstacles) {
    Level level = emptyLevel();
    level.obstacles.push_back({ObstacleKind::Cup, {3.0f, 0.0f}, 1.0f});
    World world(level);
    run(world, kWalkAhead, 3.0f);
    const float gap = distance(world.ant().position, {3.0f, 0.0f});
    CHECK(gap >= 1.0f + world.tuning().radius - kTolerance);
    CHECK(world.ant().position.x < 3.0f);  // it did not pass through
}

TEST(world_ant_picks_up_food_it_touches) {
    Level level = emptyLevel();
    level.foods.push_back(foodAt({2.0f, 0.0f}));
    World world(level);
    run(world, kWalkAhead, 1.0f);
    CHECK(world.ant().carriedFood.has_value());
    CHECK(world.foods()[0].state == FoodState::Carried);
}

TEST(world_ant_carries_only_one_food) {
    Level level = emptyLevel();
    level.foods.push_back(foodAt({2.0f, 0.0f}));
    level.foods.push_back(foodAt({4.0f, 0.0f}));
    World world(level);
    run(world, kWalkAhead, 2.0f);
    CHECK(world.foods()[0].state == FoodState::Carried);
    CHECK(world.foods()[1].state == FoodState::OnTable);
}

TEST(world_carrying_food_slows_the_ant) {
    Level level = emptyLevel();
    level.foods.push_back(foodAt({0.0f, 0.0f}));  // picked up on the first step
    World world(level);
    world.step(kStandStill, kStep);
    const float before = world.ant().position.x;
    world.step(kWalkAhead, 1.0f);
    CHECK_NEAR(world.ant().position.x - before, world.tuning().moveSpeed * world.tuning().carrySpeedFactor, kTolerance);
}

TEST(world_food_is_delivered_at_the_nest) {
    Level level = emptyLevel();
    level.nest = {{4.0f, 0.0f}, 1.0f};
    level.foods.push_back(foodAt({1.0f, 0.0f}));
    level.foods.push_back(foodAt({-10.0f, 5.0f}));
    World world(level);
    run(world, kWalkAhead, 2.0f);
    CHECK(world.foods()[0].state == FoodState::Delivered);
    CHECK(!world.ant().carriedFood.has_value());
    CHECK(world.deliveredCount() == 1);
    CHECK(world.status() == GameStatus::Playing);  // one piece still out there
}

TEST(world_game_is_won_when_all_food_is_delivered) {
    Level level = emptyLevel();
    level.nest = {{4.0f, 0.0f}, 1.0f};
    level.foods.push_back(foodAt({1.0f, 0.0f}));
    World world(level);
    run(world, kWalkAhead, 2.0f);
    CHECK(world.status() == GameStatus::Won);
}

TEST(world_game_is_lost_when_time_runs_out) {
    Level level = emptyLevel();
    level.timeLimit = 1.0f;
    level.foods.push_back(foodAt({10.0f, 5.0f}));
    World world(level);
    run(world, kStandStill, 1.5f);
    CHECK(world.status() == GameStatus::Lost);
    CHECK(world.timeRemaining() == 0.0f);
}

TEST(world_nothing_moves_after_the_game_ends) {
    Level level = emptyLevel();
    level.timeLimit = 0.5f;
    level.foods.push_back(foodAt({10.0f, 5.0f}));
    World world(level);
    run(world, kStandStill, 1.0f);
    const Vec2 frozen = world.ant().position;
    run(world, kWalkAhead, 1.0f);
    CHECK(world.ant().position.x == frozen.x);
}
