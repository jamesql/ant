/// @file network.h
/// @brief A fully connected neural network: the forward pass, and brain files.
///
/// This is the "function with adjustable numbers" from docs/NEURAL_NETWORK.md.
/// It can be created with random numbers, run, saved and loaded. It cannot
/// learn yet; training comes later and will only change the numbers stored
/// here.

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

    const std::vector<Layer>& layers() const { return layers_; }
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
