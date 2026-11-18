/// @file test_senses.cpp
/// @brief Tests for the ant's senses and for letting a network drive an ant.
///        Each test puts one thing near a single ant and checks the one
///        sense it should affect. The ant stands at the origin facing +x,
///        so +y is to its right.

#include <string>

#include "agent/brain.h"
#include "agent/senses.h"
#include "test_framework.h"

using namespace ant;

namespace {

constexpr float kTolerance = 1e-4f;

// Positions of each sense in the observation (see senses.h).
constexpr int kFeelAhead = 2;
constexpr int kFeelFarLeft = 0;
constexpr int kPoisonAhead = 6;
constexpr int kFoodScentAhead = 9;
constexpr int kHomeScentAhead = 12;
constexpr int kFoodSide = 14;
constexpr int kFoodAhead = 15;
constexpr int kFoodNear = 16;
constexpr int kNestSide = 17;
constexpr int kNestAhead = 18;
constexpr int kNestNear = 19;
constexpr int kCarrying = 20;
constexpr int kHealth = 21;

/// Bare ground with the nest directly behind the ant.
Level bareLevel() {
    Level level;
    level.nest = {{-10.0f, 0.0f}, 1.0f};
    return level;
}

/// A piece of food at `position`.
Food foodAt(Vec2 position) {
    Food food;
    food.position = position;
    return food;
}

}  // namespace

TEST(senses_have_the_expected_count_and_names) {
    const World world(bareLevel());
    CHECK(observe(world, 0).size() == static_cast<std::size_t>(kSenseCount));
    CHECK(std::string(senseName(kCarrying)) == "carrying");
    CHECK(std::string(senseName(kHealth)) == "health");
}

TEST(senses_feelers_report_how_close_a_wall_is) {
    Level level = bareLevel();
    level.walls.push_back({{1.7f, -5.0f}, {1.7f, 5.0f}, 0.2f});  // its near face is 1.5 ahead
    const std::vector<float> senses = observe(World(level), 0);
    CHECK_NEAR(senses[kFeelAhead], 0.5f, 0.05f);   // half of the 3-unit reach
    CHECK(senses[kFeelFarLeft] > 0.0f);            // the angled feeler touches it too, further along
    CHECK(senses[kFeelFarLeft] < senses[kFeelAhead]);
}

TEST(senses_feelers_are_quiet_in_open_space) {
    const std::vector<float> senses = observe(World(bareLevel()), 0);
    CHECK(senses[kFeelAhead] == 0.0f);
}

TEST(senses_smell_poison_ahead) {
    Level level = bareLevel();
    level.poisons.push_back({{0.9f, 0.0f}, 0.3f});
    const std::vector<float> senses = observe(World(level), 0);
    CHECK(senses[kPoisonAhead] == 1.0f);
    CHECK(senses[kPoisonAhead - 1] == 0.0f);  // not to the left
}

TEST(senses_see_food_and_where_it_is) {
    Level level = bareLevel();
    level.foods.push_back(foodAt({0.0f, 1.5f}));  // directly to the right, half the sight range away
    const std::vector<float> senses = observe(World(level), 0);
    CHECK_NEAR(senses[kFoodSide], 1.0f, kTolerance);   // sin: fully to the right
    CHECK_NEAR(senses[kFoodAhead], 0.0f, kTolerance);  // cos: neither ahead nor behind
    CHECK_NEAR(senses[kFoodNear], 0.5f, kTolerance);
}

TEST(senses_do_not_see_food_that_is_far_or_behind_a_wall) {
    Level far = bareLevel();
    far.foods.push_back(foodAt({8.0f, 0.0f}));
    CHECK(observe(World(far), 0)[kFoodNear] == 0.0f);

    Level hidden = bareLevel();
    hidden.foods.push_back(foodAt({2.0f, 0.0f}));
    hidden.walls.push_back({{1.0f, -5.0f}, {1.0f, 5.0f}, 0.2f});
    const std::vector<float> senses = observe(World(hidden), 0);
    CHECK(senses[kFoodSide] == 0.0f && senses[kFoodAhead] == 0.0f && senses[kFoodNear] == 0.0f);
}

TEST(senses_know_the_way_home) {
    const std::vector<float> senses = observe(World(bareLevel()), 0);
    CHECK_NEAR(senses[kNestSide], 0.0f, kTolerance);
    CHECK_NEAR(senses[kNestAhead], -1.0f, kTolerance);  // cos: directly behind
    CHECK(senses[kNestNear] > 0.0f && senses[kNestNear] < 1.0f);
}

TEST(senses_report_carrying_and_health) {
    Level level = bareLevel();
    level.foods.push_back(foodAt({0.0f, 0.0f}));     // picked up on the first step
    level.poisons.push_back({{0.0f, 0.0f}, 1.0f});   // and the ant is standing in poison
    World world(level);
    CHECK(observe(world, 0)[kCarrying] == 0.0f);
    CHECK(observe(world, 0)[kHealth] == 1.0f);

    world.step(std::vector<AntAction>{{0.0f, 0.0f}}, 0.5f);
    CHECK(observe(world, 0)[kCarrying] == 1.0f);
    CHECK(observe(world, 0)[kHealth] < 1.0f);
}

TEST(senses_smell_pheromone_ahead) {
    // The ant stands still marking its own spot with home scent, which
    // slowly spreads to the point ahead of it where scent is sampled.
    Level level = bareLevel();
    World world(level);
    CHECK(observe(world, 0)[kHomeScentAhead] == 0.0f);

    // Scent is sampled 1.4 units ahead; let the ground there soak up some.
    const Vec2 ahead{1.4f, 0.0f};
    for (int i = 0; i < 600; ++i) {
        world.step(std::vector<AntAction>{}, 1.0f / 60.0f);  // no action given: the ant stands still
    }
    CHECK(world.pheromones().sample(Scent::Home, ahead) > 0.0f);
    CHECK(observe(world, 0)[kHomeScentAhead] > 0.0f);
    CHECK(observe(world, 0)[kFoodScentAhead] == 0.0f);
}

TEST(brain_random_brain_has_the_ant_shape) {
    const Network brain = makeRandomBrain(1);
    CHECK(isAntBrain(brain));
    CHECK(brain.parameterCount() == 402);
    CHECK(!isAntBrain(Network({3, 2}, 1)));
}

TEST(brain_turns_network_outputs_into_an_action) {
    const AntAction action = toAction({0.25f, -0.5f});
    CHECK(action.forward == 0.25f);
    CHECK(action.turn == -0.5f);
}

TEST(brain_drives_an_ant_within_the_action_limits) {
    const World world(bareLevel());
    const AntAction action = brainAction(makeRandomBrain(5), world, 0);
    CHECK(action.forward >= -1.0f && action.forward <= 1.0f);
    CHECK(action.turn >= -1.0f && action.turn <= 1.0f);
}
