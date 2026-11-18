/// @file network.cpp
/// @brief Implementation of the forward pass and of brain files.

#include "nn/network.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <random>
#include <utility>

namespace ant {
namespace {

/// The first words of every brain file, and the format version we read and write.
constexpr const char* kFileTag = "ant-brain";
constexpr int kFileVersion = 1;

/// Returns a random float in [low, high], built directly from the generator's
/// raw output so the result is identical on every compiler and platform.
float uniform(std::mt19937& rng, float low, float high) {
    const double unit = static_cast<double>(rng()) / 4294967295.0;
    return low + (high - low) * static_cast<float>(unit);
}

}  // namespace

Network::Network(const std::vector<int>& layerSizes, std::uint32_t seed) {
    std::mt19937 rng(seed);
    for (std::size_t i = 1; i < layerSizes.size(); ++i) {
        const int inputs = layerSizes[i - 1];
        const int neurons = layerSizes[i];
        const float range = std::sqrt(6.0f / static_cast<float>(inputs + neurons));

        Layer layer;
        layer.weights = Matrix(neurons, inputs);
        layer.biases.assign(static_cast<std::size_t>(neurons), 0.0f);
        for (int row = 0; row < neurons; ++row) {
            for (int column = 0; column < inputs; ++column) {
                layer.weights.at(row, column) = uniform(rng, -range, range);
            }
        }
        layers_.push_back(std::move(layer));
    }
}

Network::Network(std::vector<Layer> layers) : layers_(std::move(layers)) {}

Activations Network::run(const std::vector<float>& input) const {
    Activations activations;
    activations.push_back(input);
    for (const Layer& layer : layers_) {
        // z = W * a + b, then a = tanh(z), one neuron per row.
        std::vector<float> values = multiply(layer.weights, activations.back());
        for (std::size_t i = 0; i < values.size(); ++i) {
            values[i] = std::tanh(values[i] + layer.biases[i]);
        }
        activations.push_back(std::move(values));
    }
    return activations;
}

std::vector<float> Network::forward(const std::vector<float>& input) const { return run(input).back(); }

int Network::parameterCount() const {
    int count = 0;
    for (const Layer& layer : layers_) {
        count += layer.weights.rows() * layer.weights.columns() + static_cast<int>(layer.biases.size());
    }
    return count;
}

bool saveNetwork(const Network& network, const std::string& path) {
    std::ofstream file(path);
    if (!file) {
        return false;
    }
    // Nine significant digits are enough to write a float and read back the exact same value.
    file << std::setprecision(9);
    file << kFileTag << ' ' << kFileVersion << '\n';
    file << "name " << network.name << '\n';
    file << "senses " << network.senses << '\n';
    for (const Layer& layer : network.layers()) {
        file << "layer " << layer.weights.columns() << ' ' << layer.weights.rows() << " tanh\n";
        for (int row = 0; row < layer.weights.rows(); ++row) {
            for (int column = 0; column < layer.weights.columns(); ++column) {
                file << ' ' << layer.weights.at(row, column);
            }
            file << '\n';
        }
        for (const float bias : layer.biases) {
            file << ' ' << bias;
        }
        file << '\n';
    }
    return static_cast<bool>(file);
}

std::optional<Network> loadNetwork(const std::string& path) {
    std::ifstream file(path);
    std::string tag;
    int version = 0;
    if (!(file >> tag >> version) || tag != kFileTag || version != kFileVersion) {
        return std::nullopt;
    }

    std::string name = "unnamed";
    std::string senses = "none";
    std::vector<Layer> layers;
    std::string keyword;
    while (file >> keyword) {
        if (keyword == "name") {
            file >> name;
        } else if (keyword == "senses") {
            file >> senses;
        } else if (keyword == "layer") {
            int inputs = 0;
            int neurons = 0;
            std::string activation;
            file >> inputs >> neurons >> activation;
            // Each layer must accept exactly what the previous one produces.
            const bool fitsPrevious = layers.empty() || layers.back().weights.rows() == inputs;
            if (!file || inputs <= 0 || neurons <= 0 || activation != "tanh" || !fitsPrevious) {
                return std::nullopt;
            }
            Layer layer;
            layer.weights = Matrix(neurons, inputs);
            layer.biases.assign(static_cast<std::size_t>(neurons), 0.0f);
            for (int row = 0; row < neurons; ++row) {
                for (int column = 0; column < inputs; ++column) {
                    file >> layer.weights.at(row, column);
                }
            }
            for (float& bias : layer.biases) {
                file >> bias;
            }
            layers.push_back(std::move(layer));
        } else {
            return std::nullopt;  // an unknown word: not a brain file we understand
        }
        if (!file) {
            return std::nullopt;  // the file ended in the middle of something
        }
    }
    if (layers.empty()) {
        return std::nullopt;
    }

    Network network(std::move(layers));
    network.name = name;
    network.senses = senses;
    return network;
}

}  // namespace ant
