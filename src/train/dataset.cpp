/// @file dataset.cpp
/// @brief Implementation of recording, saving and loading examples.

#include "train/dataset.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <random>

#include "agent/senses.h"
#include "core/instinct.h"
#include "core/world.h"
#include "train/episode.h"

namespace ant {
namespace {

constexpr const char* kFileTag = "ant-examples";
constexpr int kFileVersion = 1;

}  // namespace

bool isStraightAhead(const std::vector<float>& action) { return action[0] > 0.9f && std::fabs(action[1]) < 0.15f; }

Dataset recordInstinct(std::uint32_t firstSeed, std::uint32_t lastSeed, const RecordSettings& settings) {
    Dataset dataset;
    std::mt19937 rng(firstSeed);  // decides which straight-ahead moments to keep; repeatable
    const std::uint32_t keepBelow = static_cast<std::uint32_t>(settings.keepStraightShare * 4294967295.0);

    for (std::uint32_t seed = firstSeed; seed <= lastSeed; ++seed) {
        World world(makePicnicLevel(seed));
        std::vector<AntAction> actions(world.ants().size());
        const int steps = static_cast<int>(settings.seconds / kEpisodeStep);

        for (int step = 0; step < steps && world.status() == GameStatus::Playing; ++step) {
            for (std::size_t i = 0; i < actions.size(); ++i) {
                actions[i] = instinctAction(world, i);
                if (step % settings.stepInterval != 0 || !world.ants()[i].alive) {
                    continue;
                }
                Example example{{}, {actions[i].forward, actions[i].turn}};
                if (isStraightAhead(example.action) && rng() > keepBelow) {
                    continue;
                }
                example.observation = observe(world, i);
                dataset.push_back(std::move(example));
            }
            world.step(actions, kEpisodeStep);
        }
    }
    return dataset;
}

bool saveDataset(const Dataset& dataset, const std::string& path) {
    std::ofstream file(path);
    if (!file || dataset.empty()) {
        return false;
    }
    file << std::setprecision(6);
    file << kFileTag << ' ' << kFileVersion << ' ' << dataset.front().observation.size() << ' '
         << dataset.front().action.size() << '\n';
    for (const Example& example : dataset) {
        for (const float value : example.observation) {
            file << value << ' ';
        }
        for (const float value : example.action) {
            file << value << ' ';
        }
        file << '\n';
    }
    return static_cast<bool>(file);
}

std::optional<Dataset> loadDataset(const std::string& path) {
    std::ifstream file(path);
    std::string tag;
    int version = 0;
    std::size_t observationSize = 0;
    std::size_t actionSize = 0;
    if (!(file >> tag >> version >> observationSize >> actionSize) || tag != kFileTag || version != kFileVersion ||
        observationSize == 0 || actionSize == 0) {
        return std::nullopt;
    }

    Dataset dataset;
    while (true) {
        Example example{std::vector<float>(observationSize), std::vector<float>(actionSize)};
        for (float& value : example.observation) {
            file >> value;
        }
        for (float& value : example.action) {
            file >> value;
        }
        if (!file) {
            break;  // the end of the file (a cut-off last line is dropped)
        }
        dataset.push_back(std::move(example));
    }
    if (dataset.empty()) {
        return std::nullopt;
    }
    return dataset;
}

}  // namespace ant
