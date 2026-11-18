/// @file test_training.cpp
/// @brief Tests for learning: backpropagation, gradient descent, episodes,
///        datasets, imitation and evolution.

#include <cstdio>
#include <random>
#include <string>

#include "agent/brain.h"
#include "agent/senses.h"
#include "test_framework.h"
#include "train/dataset.h"
#include "train/episode.h"
#include "train/evolution.h"
#include "train/imitation.h"

using namespace ant;

namespace {

/// A file name that will not collide with anything real.
const std::string kTempFile = "ant_test_examples.tmp";

/// A short recording of the instinct, shared by the tests that need examples.
Dataset shortRecording() {
    RecordSettings settings;
    settings.seconds = 20.0f;
    return recordInstinct(1, 1, settings);
}

}  // namespace

TEST(matrix_transposed_multiply_sums_down_the_columns) {
    // transpose of | 1 2 |  is | 1 3 |, and | 1 3 | * | 10 | = | 1*10 + 3*20 | = | 70  |
    //              | 3 4 |     | 2 4 |      | 2 4 |   | 20 |   | 2*10 + 4*20 |   | 100 |
    Matrix matrix(2, 2);
    matrix.at(0, 0) = 1.0f;
    matrix.at(0, 1) = 2.0f;
    matrix.at(1, 0) = 3.0f;
    matrix.at(1, 1) = 4.0f;
    const std::vector<float> result = multiplyTransposed(matrix, {10.0f, 20.0f});
    CHECK_NEAR(result[0], 70.0f, 1e-4f);
    CHECK_NEAR(result[1], 100.0f, 1e-4f);
}

TEST(learning_one_step_matches_the_worked_example) {
    // The full example from docs/NEURAL_NETWORK.md section 3.5: one tanh
    // neuron, inputs (1.0, 0.5), target 1.0, learning rate 0.1.
    Layer layer;
    layer.weights = Matrix(1, 2);
    layer.weights.at(0, 0) = 0.4f;
    layer.weights.at(0, 1) = -0.2f;
    layer.biases = {0.1f};
    Network network({layer});

    Gradients gradients = network.zeroGradients();
    const float loss = network.addGradients({1.0f, 0.5f}, {1.0f}, gradients);
    CHECK_NEAR(loss, 0.1922f, 1e-4f);
    CHECK_NEAR(gradients[0].weights.at(0, 0), -0.5305f, 1e-4f);
    CHECK_NEAR(gradients[0].weights.at(0, 1), -0.2653f, 1e-4f);
    CHECK_NEAR(gradients[0].biases[0], -0.5305f, 1e-4f);

    network.descend(gradients, 0.1f);
    CHECK_NEAR(network.layers()[0].weights.at(0, 0), 0.4531f, 1e-4f);
    CHECK_NEAR(network.layers()[0].weights.at(0, 1), -0.1735f, 1e-4f);
    CHECK_NEAR(network.layers()[0].biases[0], 0.1531f, 1e-4f);
    CHECK_NEAR(network.forward({1.0f, 0.5f})[0], 0.477f, 1e-3f);  // closer to the target than 0.380 was
}

TEST(learning_backpropagation_agrees_with_nudging_each_weight) {
    // Gradient checking (docs/NEURAL_NETWORK.md 3.8): the slow, foolproof
    // way to get a gradient is to nudge one weight up and down and see how
    // the loss changes. Backpropagation must give the same answer.
    Network network({3, 4, 2}, 2);
    const std::vector<float> input{0.5f, -0.3f, 0.8f};
    const std::vector<float> target{0.2f, -0.6f};
    const float nudge = 1e-2f;

    Gradients gradients = network.zeroGradients();
    network.addGradients(input, target, gradients);

    float worstDifference = 0.0f;
    for (std::size_t layer = 0; layer < network.layers().size(); ++layer) {
        Matrix& weights = network.layers()[layer].weights;
        for (int row = 0; row < weights.rows(); ++row) {
            for (int column = 0; column < weights.columns(); ++column) {
                const float original = weights.at(row, column);
                weights.at(row, column) = original + nudge;
                const float lossUp = squaredError(network.forward(input), target);
                weights.at(row, column) = original - nudge;
                const float lossDown = squaredError(network.forward(input), target);
                weights.at(row, column) = original;

                const float measured = (lossUp - lossDown) / (2.0f * nudge);
                worstDifference = std::max(worstDifference, std::fabs(measured - gradients[layer].weights.at(row, column)));
            }
        }
    }
    CHECK(worstDifference < 1e-3f);
}

TEST(learning_solves_xor) {
    // XOR cannot be learned by a single neuron, so this exercises the hidden layer.
    const std::vector<std::vector<float>> inputs{{0, 0}, {0, 1}, {1, 0}, {1, 1}};
    const std::vector<std::vector<float>> targets{{0}, {1}, {1}, {0}};
    Network network({2, 4, 1}, 1);
    for (int epoch = 0; epoch < 2000; ++epoch) {
        Gradients gradients = network.zeroGradients();
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            network.addGradients(inputs[i], targets[i], gradients);
        }
        network.descend(gradients, 0.5f / 4.0f);
    }
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        CHECK_NEAR(network.forward(inputs[i])[0], targets[i][0], 0.1f);
    }
}

TEST(episode_fitness_adds_up_the_weighted_events) {
    EpisodeResult result;
    result.delivered = 3;
    result.pickups = 4;
    result.carryProgress = 10.0f;
    result.deaths = 1;
    result.secondsLeft = 50.0f;
    CHECK_NEAR(fitness(result), 3 * 100.0f + 4 * 25.0f + 10.0f * 2.0f - 40.0f, 1e-3f);  // not won: no time bonus
    result.won = true;
    CHECK_NEAR(fitness(result), 3 * 100.0f + 4 * 25.0f + 10.0f * 2.0f - 40.0f + 50.0f, 1e-3f);
}

TEST(episode_is_repeatable_and_the_instinct_forages) {
    const EpisodeResult first = runEpisode(1, 60.0f, nullptr);
    const EpisodeResult second = runEpisode(1, 60.0f, nullptr);
    CHECK(first.delivered == second.delivered);
    CHECK(first.carryProgress == second.carryProgress);
    CHECK(first.delivered >= 1);
    CHECK(first.pickups >= first.delivered);
    CHECK(first.foodTotal == 14);
}

TEST(episode_can_be_driven_by_a_network) {
    const Network brain = makeRandomBrain(3);
    const EpisodeResult result = runEpisode(1, 5.0f, &brain);
    CHECK(result.foodTotal == 14);
    CHECK(!result.won);
}

TEST(dataset_records_what_the_instinct_sensed_and_did) {
    const Dataset dataset = shortRecording();
    CHECK(dataset.size() > 100);
    CHECK(dataset.front().observation.size() == static_cast<std::size_t>(kSenseCount));
    CHECK(dataset.front().action.size() == 2);

    // Straight-ahead moments are thinned out, so they must not dominate.
    std::size_t straight = 0;
    for (const Example& example : dataset) {
        straight += isStraightAhead(example.action) ? 1 : 0;
    }
    CHECK(straight * 2 < dataset.size());
}

TEST(dataset_survives_being_saved_and_loaded) {
    const Dataset original = shortRecording();
    CHECK(saveDataset(original, kTempFile));
    const std::optional<Dataset> loaded = loadDataset(kTempFile);
    std::remove(kTempFile.c_str());
    CHECK(loaded.has_value());
    if (loaded) {
        CHECK(loaded->size() == original.size());
        CHECK_NEAR(loaded->back().observation[18], original.back().observation[18], 1e-4f);
        CHECK_NEAR(loaded->back().action[1], original.back().action[1], 1e-4f);
    }
    CHECK(!loadDataset("no_such_file.examples").has_value());
}

TEST(imitation_brings_the_network_closer_to_the_teacher) {
    const Dataset dataset = shortRecording();
    Network brain = makeRandomBrain(1);
    const float before = averageLoss(brain, dataset);

    ImitationSettings settings;
    settings.epochs = 5;
    const std::vector<EpochReport> reports = trainByImitation(brain, dataset, settings);
    CHECK(reports.size() == 5);
    CHECK(reports.back().trainingLoss < reports.front().trainingLoss);
    CHECK(averageLoss(brain, dataset) < before * 0.8f);
}

TEST(evolution_mutation_is_small_and_repeatable) {
    const Network original = makeRandomBrain(1);
    Network a = original;
    Network b = original;
    std::mt19937 rngA(7);
    std::mt19937 rngB(7);
    mutate(a, 0.1f, rngA);
    mutate(b, 0.1f, rngB);

    const float before = original.layers()[0].weights.at(0, 0);
    const float after = a.layers()[0].weights.at(0, 0);
    CHECK(after != before);
    CHECK(std::fabs(after - before) < 1.0f);                    // a nudge, not a new number
    CHECK(after == b.layers()[0].weights.at(0, 0));             // same seed, same mutation

    Network untouched = original;
    std::mt19937 rng(7);
    mutate(untouched, 0.0f, rng);
    CHECK(untouched.layers()[0].weights.at(3, 3) == original.layers()[0].weights.at(3, 3));
}

TEST(evolution_runs_and_gives_the_same_result_every_time) {
    EvolutionSettings settings;
    settings.population = 6;
    settings.generations = 2;
    settings.seedsPerGeneration = 1;
    settings.episodeSeconds = 5.0f;
    settings.finalExamSeeds = 2;

    std::vector<float> bestScores;
    const auto remember = [&](const GenerationReport& report, const Network&) {
        bestScores.push_back(report.bestFitness);
        CHECK(report.bestFitness >= report.averageFitness);
    };
    const Network first = evolve(makeRandomBrain(1), settings, remember);
    const Network second = evolve(makeRandomBrain(1), settings, remember);

    CHECK(isAntBrain(first));
    CHECK(bestScores.size() == 4);
    CHECK(bestScores[0] == bestScores[2] && bestScores[1] == bestScores[3]);  // threads do not change the outcome
    CHECK(first.layers()[0].weights.at(0, 0) == second.layers()[0].weights.at(0, 0));
}
