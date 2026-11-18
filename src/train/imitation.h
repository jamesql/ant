/// @file imitation.h
/// @brief Imitation learning: train a network to copy a teacher's actions.
///
/// See docs/TRAINING.md section 2.1. This is ordinary supervised learning
/// with backpropagation: the teacher's action is the target for each example.

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "nn/network.h"
#include "train/dataset.h"

namespace ant {

/// The settings (hyperparameters) of an imitation run.
struct ImitationSettings {
    int epochs = 30;                 ///< Passes through the whole training set.
    int batchSize = 32;              ///< Examples whose gradients are averaged for one update.
    float learningRate = 0.05f;      ///< Step size of gradient descent.
    float validationShare = 0.2f;    ///< Share of the examples held back to check for memorising.
    std::uint32_t shuffleSeed = 1;   ///< Makes the shuffling, and so the whole run, repeatable.
};

/// The losses after one epoch. Lower is better; both are averages per example.
struct EpochReport {
    int epoch = 0;
    float trainingLoss = 0.0f;    ///< On the examples the network learns from.
    float validationLoss = 0.0f;  ///< On examples it never trains on: the honest number.
};

/// The average loss of `network` over `examples`.
float averageLoss(const Network& network, const Dataset& examples);

/// Trains `network` on `dataset`.
///
/// The dataset is shuffled and split into a training set and a validation
/// set. Each epoch shuffles the training set, updates the network once per
/// mini-batch, then measures both losses and reports them through `onEpoch`
/// (if given). When training finishes, `network` holds the numbers from the
/// epoch with the lowest validation loss, which guards against memorising.
///
/// @return One report per epoch.
std::vector<EpochReport> trainByImitation(Network& network, Dataset dataset, const ImitationSettings& settings = {},
                                          const std::function<void(const EpochReport&)>& onEpoch = nullptr);

}  // namespace ant
