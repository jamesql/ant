/// @file network_view.h
/// @brief A live picture of a neural network "thinking", drawn over the game.
///
/// How to read it:
///   - Each **circle** is a neuron. The left column is the inputs (what the
///     ant senses), the right column is the outputs (what it decides), and
///     the columns between are hidden layers.
///   - A circle's **colour** is its current value: orange for positive, blue
///     for negative, dark for near zero.
///   - Each **line** is a connection. Its colour and brightness show the
///     signal travelling along it right now (the weight times the value of
///     the neuron it comes from): orange pushes the next neuron up, blue
///     pushes it down, and an invisible line is carrying nothing.

#pragma once

#include <vector>

#include "nn/network.h"

namespace ant {

/// Draws `network` in a panel on the right of the screen.
///
/// @param activations  Every neuron's current value, from `Network::run`.
/// @param inputLabels  One name per input, drawn beside the left column.
/// @param outputLabels One name per output, drawn beside the right column.
/// @param status       One line of text under the title (for example who is driving the ant).
void drawNetworkView(const Network& network, const Activations& activations,
                     const std::vector<const char*>& inputLabels, const std::vector<const char*>& outputLabels,
                     const char* status);

}  // namespace ant
