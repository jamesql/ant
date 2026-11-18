/// @file inspect.h
/// @brief Tools for finding out what a trained brain has learned, and for
///        changing it by hand.
///
/// A brain's numbers do not explain themselves. Three experiments do:
///
///   - **Ablation** ("what breaks if I remove this?"): silence one neuron or
///     one sense, let the colony play, and see what it can no longer do.
///     This is the strongest evidence, because it tests cause, not appearance.
///   - **The Jacobian** ("what pushes what?"): for each sense and each action,
///     how much a small increase in the sense changes the action. It is the
///     network's input-to-output sensitivity, averaged over real situations.
///   - **Grafting** ("can I add a behaviour?"): attach a new hidden neuron with
///     hand-chosen weights and measure whether the colony's behaviour changes.

#pragma once

#include <cstdint>
#include <vector>

#include "nn/network.h"

namespace ant {

/// How a colony did, averaged over several games.
struct AverageResult {
    float delivered = 0.0f;  ///< Pieces of food brought home per game.
    float winRate = 0.0f;    ///< Share of games in which all food came home in time.
    float pickups = 0.0f;    ///< Times an ant picked food up, per game.
    float deaths = 0.0f;     ///< Ants lost to poison, per game.
};

/// Plays `seedCount` maps starting at `firstSeed` (several at once) with
/// `brain` driving every ant, and averages the results.
AverageResult averageResult(const Network& brain, std::uint32_t firstSeed, int seedCount, float seconds = 240.0f);

/// How a colony behaves, averaged over several games. Where `AverageResult`
/// says how *well* a brain does, this describes *what it is like*.
struct Behaviour {
    float delivered = 0.0f;           ///< Pieces of food brought home per game.
    float pickups = 0.0f;             ///< Times an ant picked food up, per game.
    float deaths = 0.0f;              ///< Ants lost to poison, per game.
    float poisonSeconds = 0.0f;       ///< Ant-seconds spent standing in poison, per game.
    float poisonedHomecomings = 0.0f; ///< Times an ant walked into the nest while poisoned, per game.
    float nestDistance = 0.0f;        ///< Average distance of an ant from the nest, in world units.
    float edgeShare = 0.0f;           ///< Share of the time an ant is pressed against the edge of the map.
    float spin = 0.0f;                ///< Average turning, in full turns per minute (+ = clockwise from above).
    float backwardShare = 0.0f;       ///< Share of the time an ant is walking backwards.
};

/// Plays `seedCount` maps starting at `firstSeed` (several at once) with
/// `brain` driving every ant, and describes how the colony behaved.
Behaviour measureBehaviour(const Network& brain, std::uint32_t firstSeed, int seedCount, float seconds = 240.0f);

/// Plays `seedCount` maps with `brain` and returns what the ants sensed on
/// every `stepInterval`-th step: a sample of the situations this brain
/// really gets into.
std::vector<std::vector<float>> sampleObservations(const Network& brain, std::uint32_t firstSeed, int seedCount,
                                                   int stepInterval = 20);

/// The average value of every neuron in every layer over `observations`.
/// Entry 0 is the inputs (senses), entry 1 the first hidden layer, and so on.
Activations averageActivations(const Network& brain, const std::vector<std::vector<float>>& observations);

/// A copy of `brain` with one neuron of layer `layer` silenced (layer 0 = the
/// inputs, 1 = the first hidden layer).
///
/// The neuron is replaced by a constant equal to `average`, its usual value.
/// Simply forcing it to zero would also remove its usual push on the next
/// layer and could break the brain for an uninteresting reason; this way
/// only the *information* the neuron carried is removed.
Network silenced(const Network& brain, int layer, int neuron, float average);

/// The Jacobian of `brain`, averaged over `observations`: entry
/// `[output][input]` is how much that output rises when that input rises a
/// little. Positive means "more of this sense, more of this action".
///
/// For a network with one hidden layer `h = tanh(W1 x + b1)` and outputs
/// `o = tanh(W2 h + b2)`, the chain rule gives, for one observation,
///   `J = diag(1 - o^2) * W2 * diag(1 - h^2) * W1`.
/// (Works for any number of layers: the same pattern repeats.)
std::vector<std::vector<float>> averageJacobian(const Network& brain,
                                                const std::vector<std::vector<float>>& observations);

/// A copy of `brain` with one extra neuron in its (single) hidden layer.
/// The new neuron computes `tanh(inputWeights . senses + bias)` and adds
/// `outputWeights[k]` times that to output `k`. Everything else is unchanged.
Network withGraftedNeuron(const Network& brain, const std::vector<float>& inputWeights, float bias,
                          const std::vector<float>& outputWeights);

}  // namespace ant
