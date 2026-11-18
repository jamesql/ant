/// @file episode.cpp
/// @brief Implementation of headless play and scoring.

#include "train/episode.h"

#include <vector>

#include "agent/brain.h"
#include "core/instinct.h"
#include "core/world.h"

namespace ant {

EpisodeResult runEpisode(std::uint32_t seed, float seconds, const Network* brain) {
    World world(makePicnicLevel(seed));
    const Vec2 nest = world.level().nest.position;
    const std::size_t antCount = world.ants().size();

    EpisodeResult result;
    result.foodTotal = static_cast<int>(world.foods().size());

    std::vector<AntAction> actions(antCount);
    // What each ant was doing before the step, so changes can be noticed after it.
    std::vector<bool> wasAlive(antCount, true);
    std::vector<bool> wasCarrying(antCount, false);
    std::vector<float> nestDistance(antCount, 0.0f);

    const int steps = static_cast<int>(seconds / kEpisodeStep);
    for (int step = 0; step < steps && world.status() == GameStatus::Playing; ++step) {
        for (std::size_t i = 0; i < antCount; ++i) {
            const Ant& ant = world.ants()[i];
            wasAlive[i] = ant.alive;
            wasCarrying[i] = ant.carriedFood.has_value();
            nestDistance[i] = distance(ant.position, nest);
            actions[i] = brain != nullptr ? brainAction(*brain, world, i) : instinctAction(world, i);
        }

        world.step(actions, kEpisodeStep);

        for (std::size_t i = 0; i < antCount; ++i) {
            const Ant& ant = world.ants()[i];
            const bool carrying = ant.carriedFood.has_value();
            result.deaths += (wasAlive[i] && !ant.alive) ? 1 : 0;
            result.pickups += (!wasCarrying[i] && carrying) ? 1 : 0;
            if (wasCarrying[i] && carrying) {
                // Counted with its sign, so pacing back and forth earns nothing.
                result.carryProgress += nestDistance[i] - distance(ant.position, nest);
            }
        }
    }

    result.delivered = world.deliveredCount();
    result.won = world.status() == GameStatus::Won;
    result.secondsLeft = world.timeRemaining();
    return result;
}

float fitness(const EpisodeResult& result, const FitnessWeights& weights) {
    float score = weights.delivered * static_cast<float>(result.delivered) +
                  weights.pickedUp * static_cast<float>(result.pickups) +
                  weights.carryProgress * result.carryProgress + weights.death * static_cast<float>(result.deaths);
    if (result.won) {
        score += weights.secondLeftOnWin * result.secondsLeft;
    }
    return score;
}

}  // namespace ant
