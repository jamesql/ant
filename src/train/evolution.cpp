/// @file evolution.cpp
/// @brief Implementation of neuroevolution.

#include "train/evolution.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "train/parallel.h"

namespace ant {
namespace {

/// A random float in (0, 1], built from the generator's raw output so results
/// are identical on every compiler and platform.
float unitRandom(std::mt19937& rng) {
    return (static_cast<float>(rng()) + 1.0f) / 4294967296.0f;
}

/// A random number from the standard bell curve (mean 0, spread 1), made
/// from two evenly spread random numbers by the Box-Muller method.
float bellCurveRandom(std::mt19937& rng) {
    const float radius = std::sqrt(-2.0f * std::log(unitRandom(rng)));
    return radius * std::cos(2.0f * 3.14159265f * unitRandom(rng));
}

}  // namespace

void mutate(Network& network, float strength, std::mt19937& rng) {
    for (Layer& layer : network.layers()) {
        for (int row = 0; row < layer.weights.rows(); ++row) {
            for (int column = 0; column < layer.weights.columns(); ++column) {
                layer.weights.at(row, column) += strength * bellCurveRandom(rng);
            }
        }
        for (float& bias : layer.biases) {
            bias += strength * bellCurveRandom(rng);
        }
    }
}

float averageFitness(const Network& brain, std::uint32_t firstSeed, int seedCount, float episodeSeconds,
                     const FitnessWeights& weights) {
    float total = 0.0f;
    for (int i = 0; i < seedCount; ++i) {
        total += fitness(runEpisode(firstSeed + static_cast<std::uint32_t>(i), episodeSeconds, &brain), weights);
    }
    return total / static_cast<float>(std::max(1, seedCount));
}

Network evolve(const Network& start, const EvolutionSettings& settings,
               const std::function<void(const GenerationReport&, const Network&)>& onGeneration) {
    std::mt19937 rng(settings.mutationSeed);
    const auto populationSize = static_cast<std::size_t>(std::max(2, settings.population));
    const std::size_t eliteCount =
        std::clamp<std::size_t>(static_cast<std::size_t>(static_cast<float>(populationSize) * settings.eliteShare), 1,
                                populationSize - 1);

    // The first population: the starting brain and mutated copies of it.
    std::vector<Network> population(populationSize, start);
    for (std::size_t i = 1; i < populationSize; ++i) {
        mutate(population[i], settings.mutationStrength, rng);
    }

    std::vector<float> scores(populationSize, 0.0f);
    std::vector<std::size_t> ranking(populationSize);

    for (int generation = 1; generation <= settings.generations; ++generation) {
        // Every brain plays the same layouts this generation, so the comparison is fair.
        const std::uint32_t seed =
            settings.firstSeed + static_cast<std::uint32_t>((generation - 1) * settings.seedsPerGeneration);
        parallelFor(populationSize, settings.threads, [&](std::size_t i) {
            scores[i] = averageFitness(population[i], seed, settings.seedsPerGeneration, settings.episodeSeconds,
                                       settings.weights);
        });

        // Rank the brains, best first.
        std::iota(ranking.begin(), ranking.end(), std::size_t{0});
        std::sort(ranking.begin(), ranking.end(), [&](std::size_t a, std::size_t b) { return scores[a] > scores[b]; });

        if (onGeneration) {
            GenerationReport report;
            report.generation = generation;
            report.bestFitness = scores[ranking.front()];
            report.averageFitness = std::accumulate(scores.begin(), scores.end(), 0.0f) / static_cast<float>(populationSize);
            onGeneration(report, population[ranking.front()]);
        }

        // The next population: the elite unchanged, then mutated copies of random elite members.
        std::vector<Network> next;
        next.reserve(populationSize);
        for (std::size_t i = 0; i < eliteCount; ++i) {
            next.push_back(population[ranking[i]]);
        }
        if (generation == settings.generations) {
            return next.front();
        }
        while (next.size() < populationSize) {
            Network child = next[rng() % eliteCount];
            mutate(child, settings.mutationStrength, rng);
            next.push_back(std::move(child));
        }
        population = std::move(next);
    }
    return start;  // only reached when zero generations were asked for
}

}  // namespace ant
