/// @file inspect.cpp
/// @brief Implementation of the brain inspection tools.

#include "train/inspect.h"

#include <algorithm>
#include <utility>

#include "agent/brain.h"
#include "agent/senses.h"
#include "core/world.h"
#include "train/episode.h"
#include "train/parallel.h"

namespace ant {

AverageResult averageResult(const Network& brain, std::uint32_t firstSeed, int seedCount, float seconds) {
    std::vector<EpisodeResult> results(static_cast<std::size_t>(seedCount));
    parallelFor(results.size(), 0, [&](std::size_t i) {
        results[i] = runEpisode(firstSeed + static_cast<std::uint32_t>(i), seconds, &brain);
    });

    AverageResult average;
    for (const EpisodeResult& result : results) {
        average.delivered += static_cast<float>(result.delivered);
        average.winRate += result.won ? 1.0f : 0.0f;
        average.pickups += static_cast<float>(result.pickups);
        average.deaths += static_cast<float>(result.deaths);
    }
    const float games = static_cast<float>(std::max<std::size_t>(1, results.size()));
    average.delivered /= games;
    average.winRate /= games;
    average.pickups /= games;
    average.deaths /= games;
    return average;
}

std::vector<std::vector<float>> sampleObservations(const Network& brain, std::uint32_t firstSeed, int seedCount,
                                                   int stepInterval) {
    std::vector<std::vector<float>> samples;
    for (int game = 0; game < seedCount; ++game) {
        World world(makePicnicLevel(firstSeed + static_cast<std::uint32_t>(game)));
        std::vector<AntAction> actions(world.ants().size());
        for (int step = 0; world.status() == GameStatus::Playing; ++step) {
            for (std::size_t i = 0; i < actions.size(); ++i) {
                std::vector<float> observation = observe(world, i);
                actions[i] = toAction(brain.forward(observation));
                if (step % stepInterval == 0 && world.ants()[i].alive) {
                    samples.push_back(std::move(observation));
                }
            }
            world.step(actions, kEpisodeStep);
        }
    }
    return samples;
}

Activations averageActivations(const Network& brain, const std::vector<std::vector<float>>& observations) {
    Activations totals;
    for (const std::vector<float>& observation : observations) {
        const Activations activations = brain.run(observation);
        if (totals.empty()) {
            totals = activations;
            continue;
        }
        for (std::size_t layer = 0; layer < activations.size(); ++layer) {
            for (std::size_t i = 0; i < activations[layer].size(); ++i) {
                totals[layer][i] += activations[layer][i];
            }
        }
    }
    for (std::vector<float>& layer : totals) {
        for (float& value : layer) {
            value /= static_cast<float>(std::max<std::size_t>(1, observations.size()));
        }
    }
    return totals;
}

Network silenced(const Network& brain, int layer, int neuron, float average) {
    // The neuron feeds the next layer through one column of that layer's
    // weights. Fold "its usual value times each weight" into the biases, then
    // zero the column: the next layer now sees a constant instead of the neuron.
    Network result = brain;
    Layer& next = result.layers()[static_cast<std::size_t>(layer)];
    for (int row = 0; row < next.weights.rows(); ++row) {
        next.biases[static_cast<std::size_t>(row)] += next.weights.at(row, neuron) * average;
        next.weights.at(row, neuron) = 0.0f;
    }
    return result;
}

std::vector<std::vector<float>> averageJacobian(const Network& brain,
                                                const std::vector<std::vector<float>>& observations) {
    const auto outputs = static_cast<std::size_t>(brain.outputCount());
    const auto inputs = static_cast<std::size_t>(brain.inputCount());
    std::vector<std::vector<float>> total(outputs, std::vector<float>(inputs, 0.0f));

    for (const std::vector<float>& observation : observations) {
        const Activations activations = brain.run(observation);
        for (std::size_t output = 0; output < outputs; ++output) {
            // Start with "this output rises by 1" and carry that sensitivity
            // backwards through the layers, exactly as backpropagation carries
            // an error: scale by the slope of tanh, then by the weights.
            std::vector<float> sensitivity(outputs, 0.0f);
            sensitivity[output] = 1.0f;
            for (std::size_t layer = brain.layers().size(); layer-- > 0;) {
                const std::vector<float>& values = activations[layer + 1];
                for (std::size_t i = 0; i < sensitivity.size(); ++i) {
                    sensitivity[i] *= 1.0f - values[i] * values[i];
                }
                sensitivity = multiplyTransposed(brain.layers()[layer].weights, sensitivity);
            }
            for (std::size_t input = 0; input < inputs; ++input) {
                total[output][input] += sensitivity[input];
            }
        }
    }
    for (std::vector<float>& row : total) {
        for (float& value : row) {
            value /= static_cast<float>(std::max<std::size_t>(1, observations.size()));
        }
    }
    return total;
}

Network withGraftedNeuron(const Network& brain, const std::vector<float>& inputWeights, float bias,
                          const std::vector<float>& outputWeights) {
    const Layer& hidden = brain.layers()[0];
    const Layer& output = brain.layers()[1];
    const int oldCount = hidden.weights.rows();

    // The hidden layer gains one row (the new neuron's input weights)...
    Layer newHidden;
    newHidden.weights = Matrix(oldCount + 1, hidden.weights.columns());
    newHidden.biases = hidden.biases;
    newHidden.biases.push_back(bias);
    for (int column = 0; column < hidden.weights.columns(); ++column) {
        for (int row = 0; row < oldCount; ++row) {
            newHidden.weights.at(row, column) = hidden.weights.at(row, column);
        }
        newHidden.weights.at(oldCount, column) = inputWeights[static_cast<std::size_t>(column)];
    }

    // ...and the output layer gains one column (how the new neuron pushes each output).
    Layer newOutput;
    newOutput.weights = Matrix(output.weights.rows(), oldCount + 1);
    newOutput.biases = output.biases;
    for (int row = 0; row < output.weights.rows(); ++row) {
        for (int column = 0; column < oldCount; ++column) {
            newOutput.weights.at(row, column) = output.weights.at(row, column);
        }
        newOutput.weights.at(row, oldCount) = outputWeights[static_cast<std::size_t>(row)];
    }

    Network result({newHidden, newOutput});
    result.name = brain.name;
    result.senses = brain.senses;
    return result;
}

}  // namespace ant
