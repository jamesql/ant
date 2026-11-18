/// @file test_network.cpp
/// @brief Tests for the matrix type, the forward pass and brain files.

#include <cstdio>
#include <fstream>
#include <string>

#include "nn/network.h"
#include "test_framework.h"

using namespace ant;

namespace {

constexpr float kTolerance = 1e-5f;

/// A one-layer network holding a single neuron with the given numbers.
Network singleNeuron(float w1, float w2, float bias) {
    Layer layer;
    layer.weights = Matrix(1, 2);
    layer.weights.at(0, 0) = w1;
    layer.weights.at(0, 1) = w2;
    layer.biases = {bias};
    return Network({layer});
}

/// A file name that will not collide with anything real.
const std::string kTempFile = "ant_test_brain.tmp";

}  // namespace

TEST(matrix_starts_at_zero_and_remembers_values) {
    Matrix matrix(2, 3);
    CHECK(matrix.rows() == 2 && matrix.columns() == 3);
    CHECK(matrix.at(1, 2) == 0.0f);
    matrix.at(1, 2) = 5.0f;
    CHECK(matrix.at(1, 2) == 5.0f);
    CHECK(matrix.at(0, 2) == 0.0f);
}

TEST(matrix_times_vector_is_a_weighted_sum_per_row) {
    // | 1 2 |   | 10 |   | 1*10 + 2*20 |   |  50 |
    // | 3 4 | * | 20 | = | 3*10 + 4*20 | = | 110 |
    Matrix matrix(2, 2);
    matrix.at(0, 0) = 1.0f;
    matrix.at(0, 1) = 2.0f;
    matrix.at(1, 0) = 3.0f;
    matrix.at(1, 1) = 4.0f;
    const std::vector<float> result = multiply(matrix, {10.0f, 20.0f});
    CHECK_NEAR(result[0], 50.0f, kTolerance);
    CHECK_NEAR(result[1], 110.0f, kTolerance);
}

TEST(network_single_neuron_matches_the_worked_example) {
    // The example from docs/NEURAL_NETWORK.md section 3.5:
    // z = 0.4*1.0 + (-0.2)*0.5 + 0.1 = 0.4, and tanh(0.4) = 0.379949.
    const Network network = singleNeuron(0.4f, -0.2f, 0.1f);
    CHECK_NEAR(network.forward({1.0f, 0.5f})[0], 0.379949f, kTolerance);
}

TEST(network_feeds_each_layer_into_the_next) {
    // Two neurons in a chain: h = tanh(2 * x), output = tanh(-1 * h + 0.5).
    Layer first;
    first.weights = Matrix(1, 1);
    first.weights.at(0, 0) = 2.0f;
    first.biases = {0.0f};
    Layer second;
    second.weights = Matrix(1, 1);
    second.weights.at(0, 0) = -1.0f;
    second.biases = {0.5f};
    const Network network({first, second});

    const Activations activations = network.run({0.3f});
    CHECK(activations.size() == 3);  // input, hidden, output
    CHECK_NEAR(activations[0][0], 0.3f, kTolerance);
    CHECK_NEAR(activations[1][0], std::tanh(0.6f), kTolerance);
    CHECK_NEAR(activations[2][0], std::tanh(-std::tanh(0.6f) + 0.5f), kTolerance);
}

TEST(network_random_weights_are_small_and_repeatable) {
    const Network a({22, 16, 2}, 7);
    const Network b({22, 16, 2}, 7);
    const Network other({22, 16, 2}, 8);
    const float range = std::sqrt(6.0f / (22.0f + 16.0f));  // the Xavier limit for the first layer
    bool allWithinRange = true;
    bool identical = true;
    for (int row = 0; row < 16; ++row) {
        for (int column = 0; column < 22; ++column) {
            const float weight = a.layers()[0].weights.at(row, column);
            allWithinRange = allWithinRange && std::fabs(weight) <= range;
            identical = identical && weight == b.layers()[0].weights.at(row, column);
        }
    }
    CHECK(allWithinRange);
    CHECK(identical);
    CHECK(a.layers()[0].weights.at(0, 0) != other.layers()[0].weights.at(0, 0));
}

TEST(network_counts_its_parameters) {
    // 22*16 weights + 16 biases + 16*2 weights + 2 biases = 402.
    const Network network({22, 16, 2}, 1);
    CHECK(network.parameterCount() == 402);
    CHECK(network.inputCount() == 22);
    CHECK(network.outputCount() == 2);
}

TEST(network_outputs_stay_between_minus_one_and_one) {
    const Network network({4, 8, 2}, 3);
    for (const float output : network.forward({100.0f, -100.0f, 50.0f, -50.0f})) {
        CHECK(output >= -1.0f && output <= 1.0f);
    }
}

TEST(network_survives_being_saved_and_loaded) {
    Network original({5, 4, 2}, 11);
    original.name = "tester";
    original.senses = "five-inputs";
    CHECK(saveNetwork(original, kTempFile));

    const std::optional<Network> loaded = loadNetwork(kTempFile);
    std::remove(kTempFile.c_str());
    CHECK(loaded.has_value());
    if (loaded) {
        const std::vector<float> input{0.1f, -0.7f, 0.3f, 0.9f, -0.2f};
        CHECK(loaded->name == "tester");
        CHECK(loaded->senses == "five-inputs");
        CHECK(loaded->forward(input)[0] == original.forward(input)[0]);  // exactly the same numbers
        CHECK(loaded->forward(input)[1] == original.forward(input)[1]);
    }
}

TEST(network_refuses_files_that_are_not_brains) {
    CHECK(!loadNetwork("no_such_file.brain").has_value());

    std::ofstream(kTempFile) << "ant-brain 1\nlayer 3 2 tanh\n 1 2 3\n";  // cut off part-way
    CHECK(!loadNetwork(kTempFile).has_value());

    std::ofstream(kTempFile) << "hello world\n";
    CHECK(!loadNetwork(kTempFile).has_value());
    std::remove(kTempFile.c_str());
}
