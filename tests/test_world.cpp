/// @file test_world.cpp
/// @brief Tests for the game rules. Each test builds a tiny hand-made level
///        so exactly one rule is exercised at a time.

#include <vector>

#include "core/world.h"
#include "test_framework.h"

using namespace ant;

namespace {

constexpr float kTolerance = 1e-4f;
constexpr float kStep = 1.0f / 60.0f;
constexpr AntAction kWalkAhead{1.0f, 0.0f};
constexpr AntAction kStandStill{0.0f, 0.0f};

/// Bare ground: one ant in the middle facing +x, nest far away in a corner.
Level emptyLevel() {
    Level level;
    level.nest = {{-23.0f, -14.0f}, 1.5f};
    return level;
}

/// A piece of food at `position`.
Food foodAt(Vec2 position) {
    Food food;
    food.position = position;
    return food;
}

/// Runs the world for `seconds` of game time, giving ant 0 a constant action.
void run(World& world, AntAction action, float seconds) {
    const std::vector<AntAction> actions{action};
    const int steps = static_cast<int>(seconds / kStep + 0.5f);
    for (int i = 0; i < steps; ++i) {
        world.step(actions, kStep);
    }
}

}  // namespace

TEST(world_ant_walks_along_its_heading) {
    World world(emptyLevel());
    run(world, kWalkAhead, 1.0f);
    CHECK_NEAR(world.player().position.x, world.tuning().moveSpeed, 1e-2f);
    CHECK_NEAR(world.player().position.y, 0.0f, kTolerance);
    CHECK_NEAR(world.player().distanceWalked, world.tuning().moveSpeed, 1e-2f);
}

TEST(world_positive_turn_increases_heading) {
    World world(emptyLevel());
    world.step(std::vector<AntAction>{{0.0f, 1.0f}}, 0.1f);
    CHECK_NEAR(world.player().heading, world.tuning().turnSpeed * 0.1f, kTolerance);
}

TEST(world_action_is_clamped_to_unit_range) {
    World normal(emptyLevel());
    World excessive(emptyLevel());
    normal.step(std::vector<AntAction>{{1.0f, 1.0f}}, kStep);
    excessive.step(std::vector<AntAction>{{50.0f, 50.0f}}, kStep);
    CHECK_NEAR(excessive.player().position.x, normal.player().position.x, kTolerance);
    CHECK_NEAR(excessive.player().heading, normal.player().heading, kTolerance);
}

TEST(world_ant_cannot_leave_the_map) {
    World world(emptyLevel());
    run(world, kWalkAhead, 20.0f);
    CHECK_NEAR(world.player().position.x, world.level().areaHalfSize.x - world.tuning().radius, kTolerance);
}

TEST(world_ant_is_pushed_out_of_obstacles) {
    Level level = emptyLevel();
    level.obstacles.push_back({ObstacleKind::Cup, {3.0f, 0.0f}, 1.0f});
    World world(level);
    run(world, kWalkAhead, 3.0f);
    const float gap = distance(world.player().position, {3.0f, 0.0f});
    CHECK(gap >= 1.0f + world.tuning().radius - kTolerance);
    CHECK(world.player().position.x < 3.0f);  // it did not pass through
}

TEST(world_ant_cannot_walk_through_walls) {
    Level level = emptyLevel();
    level.walls.push_back({{3.0f, -5.0f}, {3.0f, 5.0f}, 0.2f});
    World world(level);
    run(world, kWalkAhead, 3.0f);
    CHECK_NEAR(world.player().position.x, 3.0f - 0.2f - world.tuning().radius, kTolerance);
}

TEST(world_blocked_and_clear_paths_are_detected) {
    Level level = emptyLevel();
    level.walls.push_back({{3.0f, -5.0f}, {3.0f, 5.0f}, 0.2f});
    const World world(level);
    CHECK(world.isBlocked({3.0f, 0.0f}));
    CHECK(!world.isBlocked({0.0f, 0.0f}));
    CHECK(world.isBlocked({100.0f, 0.0f}));  // off the map
    CHECK(!world.isPathClear({0.0f, 0.0f}, {6.0f, 0.0f}));
    CHECK(world.isPathClear({0.0f, 0.0f}, {2.0f, 4.0f}));
}

TEST(world_ant_picks_up_food_it_touches) {
    Level level = emptyLevel();
    level.foods.push_back(foodAt({2.0f, 0.0f}));
    World world(level);
    run(world, kWalkAhead, 1.0f);
    CHECK(world.player().carriedFood.has_value());
    CHECK(world.foods()[0].state == FoodState::Carried);
}

TEST(world_ant_carries_only_one_food) {
    Level level = emptyLevel();
    level.foods.push_back(foodAt({2.0f, 0.0f}));
    level.foods.push_back(foodAt({4.0f, 0.0f}));
    World world(level);
    run(world, kWalkAhead, 2.0f);
    CHECK(world.foods()[0].state == FoodState::Carried);
    CHECK(world.foods()[1].state == FoodState::OnGround);
}

TEST(world_carrying_food_slows_the_ant) {
    Level level = emptyLevel();
    level.foods.push_back(foodAt({0.0f, 0.0f}));  // picked up on the first step
    World world(level);
    world.step(std::vector<AntAction>{kStandStill}, kStep);
    const float before = world.player().position.x;
    world.step(std::vector<AntAction>{kWalkAhead}, 1.0f);
    CHECK_NEAR(world.player().position.x - before, world.tuning().moveSpeed * world.tuning().carrySpeedFactor,
               kTolerance);
}

TEST(world_food_is_delivered_at_the_nest) {
    Level level = emptyLevel();
    level.nest = {{4.0f, 0.0f}, 1.0f};
    level.foods.push_back(foodAt({1.0f, 0.0f}));
    level.foods.push_back(foodAt({-10.0f, 5.0f}));
    World world(level);
    run(world, kWalkAhead, 2.0f);
    CHECK(world.foods()[0].state == FoodState::Delivered);
    CHECK(!world.player().carriedFood.has_value());
    CHECK(world.deliveredCount() == 1);
    CHECK(world.player().deliveries == 1);
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
    const Vec2 frozen = world.player().position;
    run(world, kWalkAhead, 1.0f);
    CHECK(world.player().position.x == frozen.x);
}

TEST(world_poison_drains_health_and_rest_restores_it) {
    Level level = emptyLevel();
    level.poisons.push_back({{0.0f, 0.0f}, 1.0f});
    World world(level);
    run(world, kStandStill, 1.0f);
    const float hurt = world.player().health;
    CHECK_NEAR(hurt, 1.0f - 1.0f / world.tuning().poisonKillTime, 1e-2f);

    run(world, kWalkAhead, 4.0f);  // walk out of the puddle and keep going
    CHECK(world.player().health > hurt);
}

TEST(world_poisoned_ant_dies_drops_food_and_is_replaced) {
    Level level = emptyLevel();
    level.poisons.push_back({{0.0f, 0.0f}, 1.0f});
    level.foods.push_back(foodAt({0.0f, 0.0f}));
    World world(level);
    run(world, kStandStill, world.tuning().poisonKillTime + 0.1f);
    CHECK(!world.player().alive);
    CHECK(world.foods()[0].state == FoodState::OnGround);  // dropped where it fell

    run(world, kStandStill, world.tuning().respawnDelay + 0.1f);
    CHECK(world.player().alive);
    CHECK(world.player().health == 1.0f);
    CHECK(distance(world.player().position, level.nest.position) <= level.nest.radius);
}

TEST(world_searching_ants_lay_home_scent_and_carriers_lay_food_scent) {
    Level level = emptyLevel();
    level.foods.push_back(foodAt({5.0f, 0.0f}));
    World world(level);
    run(world, kWalkAhead, 2.0f);  // reaches the food after about one second
    CHECK(world.pheromones().sample(Scent::Home, {2.0f, 0.0f}) > 0.0f);
    CHECK(world.pheromones().sample(Scent::Food, {2.0f, 0.0f}) < 1e-3f);  // at most a trace that drifted over
    CHECK(world.pheromones().sample(Scent::Food, {7.0f, 0.0f}) > 0.0f);
}

TEST(world_colony_starts_at_the_nest) {
    Level level = emptyLevel();
    level.antCount = 5;
    const World world(level);
    CHECK(world.ants().size() == 5);
    for (std::size_t i = 1; i < world.ants().size(); ++i) {
        CHECK(distance(world.ants()[i].position, level.nest.position) <= level.nest.radius);
    }
}

TEST(world_scent_does_not_leak_through_walls) {
    Level level = emptyLevel();
    level.walls.push_back({{3.0f, -17.0f}, {3.0f, 17.0f}, 0.2f});  // splits the map in two
    World world(level);
    run(world, kStandStill, 20.0f);  // the ant stands at the origin, soaking the ground in scent
    CHECK(world.pheromones().sample(Scent::Home, {2.0f, 0.0f}) > 0.0f);
    CHECK(world.pheromones().sample(Scent::Home, {4.0f, 0.0f}) == 0.0f);
}
