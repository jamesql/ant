/// @file episode.h
/// @brief Playing one complete game with no window, and scoring it.
///
/// An **episode** is one game played from start to finish. Training plays
/// thousands of them, so they run on the simulation alone: no graphics, no
/// waiting for real time.

#pragma once

#include <cstdint>

#include "nn/network.h"

namespace ant {

/// The simulation step used for all headless play; the same as the game's.
inline constexpr float kEpisodeStep = 1.0f / 60.0f;

/// What happened during one episode, counted over the whole colony.
struct EpisodeResult {
    int delivered = 0;           ///< Pieces of food brought home.
    int foodTotal = 0;           ///< Pieces of food that were on the map.
    int pickups = 0;             ///< Times an ant picked food up.
    int deaths = 0;              ///< Times an ant died of poison.
    float carryProgress = 0.0f;  ///< World units by which carried food got closer to the nest.
    bool won = false;            ///< Every piece was delivered before time ran out.
    float secondsLeft = 0.0f;    ///< Time remaining on the game clock at the end.
};

/// Plays one episode on the picnic level.
///
/// @param seed    Chooses the food layout.
/// @param seconds The longest the episode may run, in game time. It ends
///                sooner if the game is won or its own clock runs out.
/// @param brain   The network that drives **every** ant in the colony, or
///                null to let every ant follow the built-in instinct.
EpisodeResult runEpisode(std::uint32_t seed, float seconds, const Network* brain);

/// How much each kind of event is worth when turning an episode into a score.
/// These are the knobs for telling evolution what behaviour you want.
struct FitnessWeights {
    float delivered = 100.0f;       ///< The actual goal.
    float pickedUp = 25.0f;         ///< A stepping stone: finding food at all.
    float carryProgress = 2.0f;     ///< A stepping stone: bringing food toward home, per world unit.
    float death = -40.0f;           ///< Discourages walking into poison.
    float secondLeftOnWin = 1.0f;   ///< Rewards finishing early, once the colony can finish at all.
};

/// One number for how well an episode went; higher is better.
float fitness(const EpisodeResult& result, const FitnessWeights& weights = {});

}  // namespace ant
