/// @file test_instinct.cpp
/// @brief Tests for the worker ant's foraging rules, alone and as a colony.

#include <vector>

#include "core/instinct.h"
#include "core/world.h"
#include "test_framework.h"

using namespace ant;

namespace {

constexpr float kStep = 1.0f / 60.0f;

/// Bare ground with the nest in the middle and a single ant on it.
Level bareLevel() {
    Level level;
    level.nest = {{0.0f, 0.0f}, 1.5f};
    return level;
}

/// Runs the world for `seconds` with every ant, including ant 0, on instinct.
void runOnInstinct(World& world, float seconds) {
    std::vector<AntAction> actions(world.ants().size());
    const int steps = static_cast<int>(seconds / kStep + 0.5f);
    for (int i = 0; i < steps; ++i) {
        for (std::size_t ant = 0; ant < actions.size(); ++ant) {
            actions[ant] = instinctAction(world, ant);
        }
        world.step(actions, kStep);
    }
}

}  // namespace

TEST(instinct_turns_toward_food_in_sight) {
    Level level = bareLevel();
    Food food;
    food.position = {0.0f, 2.0f};  // directly to the ant's right
    level.foods.push_back(food);
    const World world(level);
    const AntAction action = instinctAction(world, 0);
    CHECK(action.turn > 0.5f);
    CHECK(action.forward > 0.0f);
}

TEST(instinct_steers_away_from_poison_ahead) {
    Level level = bareLevel();
    level.poisons.push_back({{1.2f, -0.6f}, 0.6f});  // ahead and to the left
    const World world(level);
    CHECK(instinctAction(world, 0).turn > 0.0f);  // so it turns right
}

TEST(instinct_fetches_nearby_food_and_brings_it_home) {
    Level level = bareLevel();
    Food food;
    food.position = {4.0f, 1.0f};
    level.foods.push_back(food);
    World world(level);
    runOnInstinct(world, 10.0f);
    CHECK(world.status() == GameStatus::Won);
}

TEST(instinct_colony_forages_the_picnic) {
    // The whole cycle with no player: search, find, carry home, follow trails
    // back, including getting in and out of the basket.
    World world(makePicnicLevel(1));
    runOnInstinct(world, world.level().timeLimit);
    CHECK(world.deliveredCount() >= 6);
}
