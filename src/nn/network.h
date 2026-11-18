/// @file network.h
/// @brief A fully connected neural network: the forward pass, and brain files.
///
/// This is the "function with adjustable numbers" from docs/NEURAL_NETWORK.md.
/// It can be created with random numbers, run, saved and loaded, and it can
/// learn from examples by backpropagation and gradient descent.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "nn/matrix.h"

namespace ant {

/// One layer of neurons. Every neuron reads all of the layer's inputs.
struct Layer {
    Matrix weights;             ///< `weights.at(i, j)`: how much neuron `i` cares about input `j`.
    std::vector<float> biases;  ///< One per neuron: its starting level before any input arrives.
};

/// The values of every neuron during one forward pass.
/// Entry 0 is the network's input; the last entry is its output.
using Activations = std::vector<std::vector<float>>;

/// For every weight and bias of a network, how much the loss changes when
/// that number changes. Stored in the same shape as the network's layers.
using Gradients = std::vector<Layer>;

/// The loss for one example: `1/2 * sum of (output - target)^2`.
/// Zero means the output matched the target exactly.
float squaredError(const std::vector<float>& output, const std::vector<float>& target);

/// A stack of layers. Every neuron uses the `tanh` activation function.
class Network {
public:
    /// Creates a network with random weights and zero biases.
    ///
    /// @param layerSizes Neurons per layer, starting with the number of
    ///                   inputs. `{22, 16, 2}` means 22 inputs, one hidden
    ///                   layer of 16 neurons, and 2 outputs.
    /// @param seed       Chooses the random weights; the same seed always
    ///                   gives the same network.
    ///
    /// Weights use "Xavier initialisation": each is picked evenly from
    /// `-r..+r` with `r = sqrt(6 / (inputs + neurons))` for its layer, which
    /// keeps signals from growing or shrinking as they pass through.
    Network(const std::vector<int>& layerSizes, std::uint32_t seed);

    /// Creates a network from layers whose numbers are already known.
    explicit Network(std::vector<Layer> layers);

    /// Runs the forward pass and returns every neuron's value along the way:
    /// for each layer, `a = tanh(W * previous + b)`.
    /// `input` must have exactly `inputCount()` entries.
    Activations run(const std::vector<float>& input) const;

    /// Runs the forward pass and returns only the output layer's values.
    std::vector<float> forward(const std::vector<float>& input) const;

    /// A set of gradients with the same shape as this network, all zero.
    Gradients zeroGradients() const;

    /// Backpropagation for one training example.
    ///
    /// Runs the forward pass on `input`, compares the output with `target`,
    /// and works out, for every weight and bias, how the loss would change
    /// if that number were nudged up (its gradient). The gradients are
    /// *added* to `sum`, so calling this for several examples in a row
    /// accumulates them for a mini-batch.
    ///
    /// The steps are those of docs/NEURAL_NETWORK.md section 3.6:
    ///   delta[last] = (a[last] - y) .* (1 - a[last]^2)
    ///   delta[l]    = (transpose(W[l+1]) * delta[l+1]) .* (1 - a[l]^2)
    ///   dL/dW[l]    = delta[l] * transpose(a[l-1])
    ///   dL/db[l]    = delta[l]
    /// where `1 - a^2` is the slope of `tanh`.
    ///
    /// @return This example's loss (see `squaredError`).
    float addGradients(const std::vector<float>& input, const std::vector<float>& target, Gradients& sum) const;

    /// Gradient descent: moves every weight and bias a small step against
    /// its gradient, `w <- w - stepSize * gradient`. For a mini-batch whose
    /// gradients were accumulated, pass `learningRate / batchSize` to use
    /// their average.
    void descend(const Gradients& gradients, float stepSize);

    const std::vector<Layer>& layers() const { return layers_; }
    /// Direct access to the numbers, for code that changes them (mutation).
    std::vector<Layer>& layers() { return layers_; }
    int inputCount() const { return layers_.front().weights.columns(); }
    int outputCount() const { return layers_.back().weights.rows(); }

    /// How many adjustable numbers (weights and biases) the network contains.
    int parameterCount() const;

    std::string name = "unnamed";  ///< A label for people; one word.
    std::string senses = "none";   ///< Which list of inputs this network expects; one word.

private:
    std::vector<Layer> layers_;
};

/// Writes `network` to a plain-text brain file. Returns false if the file
/// could not be written.
///
/// The format is one header line, the two labels, then for each layer its
/// size followed by its weight rows and its biases:
/// @code
///   ant-brain 1
///   name forager
///   senses standard-22
///   layer 22 16 tanh
///   <16 rows of 22 weights>
///   <16 biases>
///   layer 16 2 tanh
///   <2 rows of 16 weights>
///   <2 biases>
/// @endcode
bool saveNetwork(const Network& network, const std::string& path);

/// Reads a brain file written by `saveNetwork`. Returns nothing if the file
/// is missing or is not a valid brain file.
std::optional<Network> loadNetwork(const std::string& path);

}  // namespace ant
