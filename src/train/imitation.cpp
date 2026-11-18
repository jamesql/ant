/// @file imitation.cpp
/// @brief Implementation of imitation learning.

#include "train/imitation.h"

#include <algorithm>
#include <random>

namespace ant {

float averageLoss(const Network& network, const Dataset& examples) {
    if (examples.empty()) {
        return 0.0f;
    }
    double total = 0.0;
    for (const Example& example : examples) {
        total += squaredError(network.forward(example.observation), example.action);
    }
    return static_cast<float>(total / static_cast<double>(examples.size()));
}

std::vector<EpochReport> trainByImitation(Network& network, Dataset dataset, const ImitationSettings& settings,
                                          const std::function<void(const EpochReport&)>& onEpoch) {
    std::mt19937 rng(settings.shuffleSeed);
    std::shuffle(dataset.begin(), dataset.end(), rng);

    // Hold back the last part of the shuffled examples for validation.
    const auto validationCount = static_cast<std::size_t>(static_cast<float>(dataset.size()) * settings.validationShare);
    const Dataset validation(dataset.end() - static_cast<std::ptrdiff_t>(validationCount), dataset.end());
    dataset.resize(dataset.size() - validationCount);

    std::vector<EpochReport> reports;
    Network best = network;
    float bestValidationLoss = averageLoss(network, validation);
    const auto batchSize = static_cast<std::size_t>(settings.batchSize);

    for (int epoch = 1; epoch <= settings.epochs; ++epoch) {
        std::shuffle(dataset.begin(), dataset.end(), rng);

        double trainingTotal = 0.0;
        for (std::size_t start = 0; start < dataset.size(); start += batchSize) {
            const std::size_t end = std::min(start + batchSize, dataset.size());
            Gradients gradients = network.zeroGradients();
            for (std::size_t i = start; i < end; ++i) {
                trainingTotal += network.addGradients(dataset[i].observation, dataset[i].action, gradients);
            }
            // Dividing by the batch size turns the summed gradients into their average.
            network.descend(gradients, settings.learningRate / static_cast<float>(end - start));
        }

        EpochReport report;
        report.epoch = epoch;
        report.trainingLoss = static_cast<float>(trainingTotal / static_cast<double>(std::max<std::size_t>(1, dataset.size())));
        report.validationLoss = averageLoss(network, validation);
        reports.push_back(report);
        if (onEpoch) {
            onEpoch(report);
        }

        // With no validation set there is nothing to compare, so the latest numbers are kept.
        if (validation.empty() || report.validationLoss < bestValidationLoss) {
            bestValidationLoss = report.validationLoss;
            best = network;
        }
    }

    network = best;
    return reports;
}

}  // namespace ant
