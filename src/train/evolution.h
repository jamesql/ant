/// @file evolution.h
/// @brief Neuroevolution: improve a brain by selection and mutation.
///
/// See docs/TRAINING.md section 2.2. No teacher and no derivatives: make
/// many brains, let each play, keep the best, and refill the population with
/// slightly changed copies of them.

#pragma once

#include <cstdint>
#include <functional>
#include <random>
#include <vector>

#include "nn/network.h"
#include "train/episode.h"

namespace ant {

/// The settings (hyperparameters) of an evolution run.
struct EvolutionSettings {
    int population = 64;             ///< Brains compared each generation.
    int generations = 40;            ///< Rounds of play, score, select, mutate.
    float eliteShare = 0.2f;         ///< Share of the population kept unchanged as parents.
    float mutationStrength = 0.08f;  ///< Size of the random change made to each number ("sigma").
    int seedsPerGeneration = 2;      ///< Food layouts each brain plays; its fitness is the average.
    float episodeSeconds = 90.0f;    ///< Game time per episode.
    std::uint32_t firstSeed = 1;     ///< Layouts used are firstSeed, firstSeed + 1, ... moving on every generation.
    std::uint32_t mutationSeed = 1;  ///< Makes the mutations, and so the whole run, repeatable.
    int threads = 0;                 ///< Episodes played at once; 0 = one per processor core.
    FitnessWeights weights;          ///< What counts as doing well.
};

/// How one generation went.
struct GenerationReport {
    int generation = 0;
    float bestFitness = 0.0f;
    float averageFitness = 0.0f;
};

/// Adds a small random amount to every weight and bias of `network`:
/// `w <- w + strength * n`, where `n` is drawn from a bell curve centred on
/// zero (mostly small, occasionally large).
void mutate(Network& network, float strength, std::mt19937& rng);

/// The average fitness of `brain` over `seedCount` episodes on the food
/// layouts `firstSeed`, `firstSeed + 1`, ...
float averageFitness(const Network& brain, std::uint32_t firstSeed, int seedCount, float episodeSeconds,
                     const FitnessWeights& weights);

/// Evolves a brain, starting from `start`.
///
/// The first population is `start` plus mutated copies of it. Every
/// generation, each brain plays the same few food layouts and is scored; the
/// best share (the elite) survive unchanged and the rest are replaced by
/// mutated copies of elite brains. The layouts change every generation, so a
/// brain cannot succeed by memorising one.
///
/// `onGeneration` (if given) is called after each generation with its report
/// and that generation's best brain.
///
/// @return The best brain of the final generation.
Network evolve(const Network& start, const EvolutionSettings& settings,
               const std::function<void(const GenerationReport&, const Network&)>& onGeneration = nullptr);

}  // namespace ant
