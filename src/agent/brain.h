/// @file brain.h
/// @brief Lets a neural network drive an ant: senses in, action out.
///
/// This is the network's equivalent of `instinctAction` (core/instinct.h).
/// Either can produce the `AntAction` for any ant.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/action.h"
#include "core/world.h"
#include "nn/network.h"

namespace ant {

/// A fresh ant brain with random numbers: 22 senses, 16 hidden neurons,
/// 2 outputs. It has learned nothing, so it behaves randomly.
Network makeRandomBrain(std::uint32_t seed);

/// True if `network` has the right shape to drive an ant: it reads this
/// project's senses and produces the two action numbers.
bool isAntBrain(const Network& network);

/// Reads a network's two outputs as an action: output 0 is the walking
/// effort and output 1 the turning effort. Both are already within -1..1
/// because the output neurons use `tanh`.
AntAction toAction(const std::vector<float>& outputs);

/// Decides what ant `antIndex` should do by showing `network` what the ant
/// senses and reading off its answer.
AntAction brainAction(const Network& network, const World& world, std::size_t antIndex);

}  // namespace ant
