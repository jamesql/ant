/// @file brain.cpp
/// @brief Implementation of the network-to-ant hookup.

#include "agent/brain.h"

#include <string>

#include "agent/senses.h"

namespace ant {
namespace {

constexpr int kHiddenNeurons = 16;
constexpr int kActionOutputs = 2;  ///< Walking effort and turning effort.

}  // namespace

Network makeRandomBrain(std::uint32_t seed) {
    Network network({kSenseCount, kHiddenNeurons, kActionOutputs}, seed);
    network.name = "random-" + std::to_string(seed);
    network.senses = kSensesName;
    return network;
}

bool isAntBrain(const Network& network) {
    return network.senses == kSensesName && network.inputCount() == kSenseCount &&
           network.outputCount() == kActionOutputs;
}

AntAction toAction(const std::vector<float>& outputs) { return {outputs[0], outputs[1]}; }

AntAction brainAction(const Network& network, const World& world, std::size_t antIndex) {
    return toAction(network.forward(observe(world, antIndex)));
}

}  // namespace ant
