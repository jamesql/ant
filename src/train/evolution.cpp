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

    // One member of the population: a brain and its record so far.
    struct Member {
        Network brain;
        float totalScore = 0.0f;  ///< Sum of its fitness over every generation it has played.
        int generationsPlayed = 0;
        float average() const { return totalScore / static_cast<float>(std::max(1, generationsPlayed)); }
    };

    // The first population: the starting brain and mutated copies of it.
    std::vector<Member> population(populationSize, Member{start});
    for (std::size_t i = 1; i < populationSize; ++i) {
        mutate(population[i].brain, settings.mutationStrength, rng);
    }

    std::vector<float> scores(populationSize, 0.0f);
    std::uint32_t nextSeed = settings.firstSeed;

    for (int generation = 1; generation <= settings.generations; ++generation) {
        // Every brain plays the same maps this generation, so the comparison is fair.
        const std::uint32_t seed = nextSeed;
        nextSeed += static_cast<std::uint32_t>(settings.seedsPerGeneration);
        parallelFor(populationSize, settings.threads, [&](std::size_t i) {
            scores[i] = averageFitness(population[i].brain, seed, settings.seedsPerGeneration,
                                       settings.episodeSeconds, settings.weights);
        });
        for (std::size_t i = 0; i < populationSize; ++i) {
            population[i].totalScore += scores[i];
            ++population[i].generationsPlayed;
        }

        // Rank by lifetime average, best first. Some maps are easier than
        // others, so one good generation can be luck; a brain that has
        // survived several generations is judged on all of them.
        std::sort(population.begin(), population.end(),
                  [](const Member& a, const Member& b) { return a.average() > b.average(); });

        if (onGeneration) {
            GenerationReport report;
            report.generation = generation;
            report.bestFitness = population.front().average();
            report.averageFitness = std::accumulate(scores.begin(), scores.end(), 0.0f) / static_cast<float>(populationSize);
            onGeneration(report, population.front().brain);
        }
        if (generation == settings.generations) {
            break;
        }

        // The next population: the elite unchanged (keeping their records),
        // then mutated copies of random elite members (starting fresh).
        population.erase(population.begin() + static_cast<std::ptrdiff_t>(eliteCount), population.end());
        while (population.size() < populationSize) {
            Member child{population[rng() % eliteCount].brain};
            mutate(child.brain, settings.mutationStrength, rng);
            population.push_back(std::move(child));
        }
    }

    // The final exam: the elite all play a larger set of maps none of them
    // has seen, and the one that does best there is the result.
    const std::size_t finalists = std::min(eliteCount, population.size());
    if (settings.finalExamSeeds <= 0 || settings.generations <= 0) {
        return population.front().brain;
    }
    std::vector<float> examScores(finalists, 0.0f);
    parallelFor(finalists, settings.threads, [&](std::size_t i) {
        examScores[i] = averageFitness(population[i].brain, nextSeed, settings.finalExamSeeds,
                                       settings.episodeSeconds, settings.weights);
    });
    const std::size_t winner =
        static_cast<std::size_t>(std::max_element(examScores.begin(), examScores.end()) - examScores.begin());
    return population[winner].brain;
}

}  // namespace ant
