/// @file train_main.cpp
/// @brief `ant_train`: the command-line tool that trains and scores ant brains.
///
/// Usage: `ant_train <command> [options]`. Run it with no arguments for the
/// full list. The commands are:
///   xor      Proves the learning math works on the classic XOR problem.
///   imitate  Trains a brain to copy a teacher (the instinct, or a recording of you).
///   evolve   Improves a brain by selection and mutation.
///   score    Plays unseen maps with no window and reports how a brain did.
///   inspect  Finds out what each neuron and sense of a brain is for.
///   graft    Adds a hand-made neuron to a brain and measures the effect.
///   silence  Fixes one neuron or sense at a constant and measures the effect.
///   behaviour Describes what a colony driven by a brain is like.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "agent/brain.h"
#include "agent/senses.h"
#include "train/dataset.h"
#include "train/episode.h"
#include "train/evolution.h"
#include "train/imitation.h"
#include "train/inspect.h"
#include "train/parallel.h"

namespace {

/// The options given after the command, as `--name value` pairs.
class Arguments {
public:
    Arguments(int argc, char** argv) : arguments_(argv + 2, argv + argc) {}

    /// The value following `--name`, or `fallback` if the option was not given.
    std::string text(const char* name, const std::string& fallback) const {
        for (std::size_t i = 0; i + 1 < arguments_.size(); ++i) {
            if (arguments_[i] == name) {
                return arguments_[i + 1];
            }
        }
        return fallback;
    }
    int integer(const char* name, int fallback) const { return std::atoi(text(name, std::to_string(fallback)).c_str()); }
    float number(const char* name, float fallback) const {
        return static_cast<float>(std::atof(text(name, std::to_string(fallback)).c_str()));
    }

private:
    std::vector<std::string> arguments_;
};

/// A range of food layouts, written `first-last` (or a single number).
struct SeedRange {
    std::uint32_t first = 1;
    std::uint32_t last = 1;
    int count() const { return static_cast<int>(last - first) + 1; }
};

SeedRange parseSeeds(const std::string& text) {
    SeedRange range;
    const std::size_t dash = text.find('-');
    range.first = static_cast<std::uint32_t>(std::atoi(text.substr(0, dash).c_str()));
    range.last = dash == std::string::npos ? range.first
                                           : static_cast<std::uint32_t>(std::atoi(text.substr(dash + 1).c_str()));
    if (range.last < range.first) {
        range.last = range.first;
    }
    return range;
}

/// Loads the brain named by `source`: a brain file, or "random" for a new
/// untrained one. Prints the reason and returns nothing if it cannot be used.
std::optional<ant::Network> openBrain(const std::string& source, std::uint32_t randomSeed) {
    if (source == "random") {
        return ant::makeRandomBrain(randomSeed);
    }
    std::optional<ant::Network> brain = ant::loadNetwork(source);
    if (!brain || !ant::isAntBrain(*brain)) {
        std::printf("'%s' is not a usable ant brain file.\n", source.c_str());
        return std::nullopt;
    }
    return brain;
}

/// Saves `brain` to `path` (creating its folder if needed), named after the file.
bool storeBrain(ant::Network brain, const std::string& path) {
    const std::filesystem::path file(path);
    if (file.has_parent_path()) {
        std::error_code ignored;
        std::filesystem::create_directories(file.parent_path(), ignored);
    }
    brain.name = file.stem().string();
    if (!ant::saveNetwork(brain, path)) {
        std::printf("Could not write '%s'.\n", path.c_str());
        return false;
    }
    return true;
}

/// Plays every layout in `seeds` (several at once) and prints the totals.
/// `brain` may be null to score the built-in instinct. If `perGamePath` is not
/// empty, one line per game is also written there (for charts and analysis).
void printScore(const char* label, const ant::Network* brain, SeedRange seeds, float seconds,
                const std::string& perGamePath = "") {
    std::vector<ant::EpisodeResult> results(static_cast<std::size_t>(seeds.count()));
    ant::parallelFor(results.size(), 0, [&](std::size_t i) {
        results[i] = ant::runEpisode(seeds.first + static_cast<std::uint32_t>(i), seconds, brain);
    });

    int delivered = 0;
    int total = 0;
    int deaths = 0;
    int wins = 0;
    float fitness = 0.0f;
    for (const ant::EpisodeResult& result : results) {
        delivered += result.delivered;
        total += result.foodTotal;
        deaths += result.deaths;
        wins += result.won ? 1 : 0;
        fitness += ant::fitness(result);
    }
    const float games = static_cast<float>(results.size());
    std::printf("%-12s food delivered %5.1f of %.0f per game | games won %d of %d | deaths %.1f per game | fitness %.0f\n",
                label, static_cast<float>(delivered) / games, static_cast<float>(total) / games, wins,
                seeds.count(), static_cast<float>(deaths) / games, fitness / games);

    if (!perGamePath.empty()) {
        std::ofstream file(perGamePath);
        file << "seed delivered food_total won deaths seconds_left\n";
        for (std::size_t i = 0; i < results.size(); ++i) {
            file << seeds.first + i << ' ' << results[i].delivered << ' ' << results[i].foodTotal << ' '
                 << (results[i].won ? 1 : 0) << ' ' << results[i].deaths << ' ' << results[i].secondsLeft << '\n';
        }
    }
}

/// `xor`: trains a tiny network on "output 1 if exactly one input is 1".
/// No single neuron can do this, so success shows that the forward pass,
/// backpropagation and gradient descent all work together.
int runXor() {
    const std::vector<std::vector<float>> inputs{{0, 0}, {0, 1}, {1, 0}, {1, 1}};
    const std::vector<std::vector<float>> targets{{0}, {1}, {1}, {0}};
    ant::Network network({2, 4, 1}, 1);

    std::printf("Learning XOR with a 2-4-1 network (%d numbers).\n\n epoch    loss\n", network.parameterCount());
    for (int epoch = 0; epoch <= 2000; ++epoch) {
        ant::Gradients gradients = network.zeroGradients();
        float loss = 0.0f;
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            loss += network.addGradients(inputs[i], targets[i], gradients);
        }
        if (epoch % 250 == 0) {
            std::printf("%6d  %.5f\n", epoch, loss / 4.0f);
        }
        network.descend(gradients, 0.5f / 4.0f);
    }
    std::printf("\n inputs   target   network says\n");
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        std::printf("  %.0f  %.0f      %.0f        %.3f\n", inputs[i][0], inputs[i][1], targets[i][0],
                    network.forward(inputs[i])[0]);
    }
    return 0;
}

/// `imitate`: records a teacher (or loads a recording) and trains a brain to copy it.
int runImitate(const Arguments& arguments) {
    const std::string dataPath = arguments.text("--data", "");
    const std::string outPath = arguments.text("--out", "brains/copycat.brain");
    std::optional<ant::Network> brain =
        openBrain(arguments.text("--start", "random"), static_cast<std::uint32_t>(arguments.integer("--brain-seed", 1)));
    if (!brain) {
        return 1;
    }

    ant::Dataset dataset;
    if (!dataPath.empty()) {
        std::optional<ant::Dataset> loaded = ant::loadDataset(dataPath);
        if (!loaded || loaded->front().observation.size() != static_cast<std::size_t>(brain->inputCount())) {
            std::printf("'%s' is not a usable recording.\n", dataPath.c_str());
            return 1;
        }
        dataset = std::move(*loaded);
        std::printf("Teacher: the recording '%s'.\n", dataPath.c_str());
    } else {
        const SeedRange seeds = parseSeeds(arguments.text("--seeds", "1-10"));
        std::printf("Teacher: the built-in instinct, recorded on maps %u to %u...\n", seeds.first, seeds.last);
        dataset = ant::recordInstinct(seeds.first, seeds.last);
    }

    int straight = 0;
    for (const ant::Example& example : dataset) {
        straight += ant::isStraightAhead(example.action) ? 1 : 0;
    }
    std::printf("%zu examples (%d%% of them plain \"walk straight ahead\").\n\n epoch  training loss  validation loss\n",
                dataset.size(), static_cast<int>(100.0 * straight / static_cast<double>(dataset.size())));

    ant::ImitationSettings settings;
    settings.epochs = arguments.integer("--epochs", settings.epochs);
    settings.batchSize = arguments.integer("--batch", settings.batchSize);
    settings.learningRate = arguments.number("--rate", settings.learningRate);
    ant::trainByImitation(*brain, std::move(dataset), settings, [](const ant::EpochReport& report) {
        std::printf("%6d  %13.5f  %15.5f\n", report.epoch, report.trainingLoss, report.validationLoss);
    });

    if (!storeBrain(*brain, outPath)) {
        return 1;
    }
    std::printf("\nSaved %s. How it does on maps it has never seen:\n", outPath.c_str());
    const SeedRange unseen{1000, 1005};
    printScore("this brain", &*brain, unseen, 240.0f);
    printScore("instinct", nullptr, unseen, 240.0f);
    return 0;
}

/// `evolve`: improves a brain over many generations and logs the learning curve.
int runEvolve(const Arguments& arguments) {
    const std::string outPath = arguments.text("--out", "brains/forager.brain");
    const std::optional<ant::Network> start =
        openBrain(arguments.text("--start", "random"), static_cast<std::uint32_t>(arguments.integer("--brain-seed", 1)));
    if (!start) {
        return 1;
    }

    ant::EvolutionSettings settings;
    settings.population = arguments.integer("--population", settings.population);
    settings.generations = arguments.integer("--generations", settings.generations);
    settings.mutationStrength = arguments.number("--sigma", settings.mutationStrength);
    settings.seedsPerGeneration = arguments.integer("--seeds-per-generation", settings.seedsPerGeneration);
    settings.episodeSeconds = arguments.number("--seconds", settings.episodeSeconds);
    settings.firstSeed = static_cast<std::uint32_t>(arguments.integer("--first-seed", 1));
    settings.threads = arguments.integer("--threads", 0);
    settings.finalExamSeeds = arguments.integer("--final-exam", settings.finalExamSeeds);
    const int checkpointEvery = arguments.integer("--checkpoint-every", 10);

    // Open the log before the long run, so a bad path is reported straight away.
    if (!storeBrain(*start, outPath)) {
        return 1;
    }
    std::ofstream log(outPath + ".log");
    log << "generation best_fitness average_fitness\n";

    std::printf("Evolving %d brains for %d generations (%d maps of %.0f s each per brain).\n\n"
                " generation  best fitness  average fitness\n",
                settings.population, settings.generations, settings.seedsPerGeneration, settings.episodeSeconds);

    const ant::Network best =
        ant::evolve(*start, settings, [&](const ant::GenerationReport& report, const ant::Network& leader) {
            std::printf("%11d  %12.1f  %15.1f\n", report.generation, report.bestFitness, report.averageFitness);
            std::fflush(stdout);
            log << report.generation << ' ' << report.bestFitness << ' ' << report.averageFitness << std::endl;
            // Save the leader every generation, so the run can be stopped at any time.
            storeBrain(leader, outPath);
            if (checkpointEvery > 0 && report.generation % checkpointEvery == 0) {
                const std::filesystem::path file(outPath);
                char suffix[32];
                std::snprintf(suffix, sizeof(suffix), "-gen%03d", report.generation);
                storeBrain(leader, (file.parent_path() / (file.stem().string() + suffix + file.extension().string())).string());
            }
        });

    storeBrain(best, outPath);
    std::printf("\nSaved %s (learning curve in %s.log). How it does on maps it has never seen:\n", outPath.c_str(),
                outPath.c_str());
    const SeedRange unseen{1000, 1005};
    printScore("this brain", &best, unseen, 240.0f);
    printScore("start brain", &*start, unseen, 240.0f);
    printScore("instinct", nullptr, unseen, 240.0f);
    return 0;
}

/// `score`: plays layouts with no window and reports the results.
int runScore(const Arguments& arguments) {
    const std::string source = arguments.text("--brain", "instinct");
    const SeedRange seeds = parseSeeds(arguments.text("--seeds", "1000-1009"));
    const float seconds = arguments.number("--seconds", 240.0f);
    const std::string perGamePath = arguments.text("--per-game", "");

    std::printf("Playing maps %u to %u, %.0f s of game time each, every ant driven by the same brain.\n", seeds.first,
                seeds.last, seconds);
    if (source == "instinct") {
        printScore("instinct", nullptr, seeds, seconds, perGamePath);
        return 0;
    }
    const std::optional<ant::Network> brain = openBrain(source, static_cast<std::uint32_t>(arguments.integer("--brain-seed", 1)));
    if (!brain) {
        return 1;
    }
    printScore(brain->name.c_str(), &*brain, seeds, seconds, perGamePath);
    return 0;
}

/// `inspect`: finds out what each neuron and each sense of a brain is for.
int runInspect(const Arguments& arguments) {
    const std::optional<ant::Network> loaded = openBrain(arguments.text("--brain", "brains/forager.brain"), 1);
    if (!loaded) {
        return 1;
    }
    const ant::Network& brain = *loaded;
    const SeedRange seeds = parseSeeds(arguments.text("--seeds", "40000-40039"));
    const int hiddenCount = brain.layers()[0].weights.rows();

    std::printf("Inspecting '%s' on maps %u to %u.\n\n", brain.name.c_str(), seeds.first, seeds.last);
    const std::vector<std::vector<float>> observations = ant::sampleObservations(brain, seeds.first, 6);
    const ant::Activations usual = ant::averageActivations(brain, observations);
    const ant::AverageResult baseline = ant::averageResult(brain, seeds.first, seeds.count());
    std::printf("Whole brain: %.1f food per game, wins %.0f%%, %.1f pickups, %.2f deaths. (%zu sampled moments)\n\n",
                baseline.delivered, 100.0f * baseline.winRate, baseline.pickups, baseline.deaths, observations.size());

    // How much each sense varies in practice; a big weight on a sense that never changes means little.
    std::vector<float> spread(static_cast<std::size_t>(ant::kSenseCount), 0.0f);
    for (const std::vector<float>& observation : observations) {
        for (std::size_t i = 0; i < spread.size(); ++i) {
            const float offset = observation[i] - usual[0][i];
            spread[i] += offset * offset;
        }
    }
    for (float& value : spread) {
        value = std::sqrt(value / static_cast<float>(observations.size()));
    }

    // Prints what the colony loses when one part is silenced.
    const auto printLoss = [&](const ant::Network& damaged) {
        const ant::AverageResult result = ant::averageResult(damaged, seeds.first, seeds.count());
        std::printf(" %+6.1f food %+5.0f%% wins %+6.1f pickups %+6.2f deaths", result.delivered - baseline.delivered,
                    100.0f * (result.winRate - baseline.winRate), result.pickups - baseline.pickups,
                    result.deaths - baseline.deaths);
    };

    std::printf("HIDDEN NEURONS - what the colony loses with each one silenced, and what it listens to\n");
    for (int neuron = 0; neuron < hiddenCount; ++neuron) {
        std::printf("h%-2d", neuron + 1);
        printLoss(ant::silenced(brain, 1, neuron, usual[1][static_cast<std::size_t>(neuron)]));
        std::printf(" | to walk %+5.2f, to turn %+5.2f | listens to:", brain.layers()[1].weights.at(0, neuron),
                    brain.layers()[1].weights.at(1, neuron));

        // The three senses with the most influence on this neuron: weight times how much the sense varies.
        std::vector<int> order(static_cast<std::size_t>(ant::kSenseCount));
        for (std::size_t i = 0; i < order.size(); ++i) {
            order[i] = static_cast<int>(i);
        }
        const auto influence = [&](int sense) {
            return std::fabs(brain.layers()[0].weights.at(neuron, sense)) * spread[static_cast<std::size_t>(sense)];
        };
        std::sort(order.begin(), order.end(), [&](int a, int b) { return influence(a) > influence(b); });
        for (int rank = 0; rank < 3; ++rank) {
            std::printf(" %s (%+.1f)", ant::senseName(order[static_cast<std::size_t>(rank)]),
                        brain.layers()[0].weights.at(neuron, order[static_cast<std::size_t>(rank)]));
        }
        std::printf("\n");
    }

    std::printf("\nSENSES - what the colony loses with each one switched off\n");
    for (int sense = 0; sense < ant::kSenseCount; ++sense) {
        std::printf("%-17s", ant::senseName(sense));
        printLoss(ant::silenced(brain, 0, sense, usual[0][static_cast<std::size_t>(sense)]));
        std::printf("\n");
    }

    // The Jacobian, separately for ants that are searching and ants that are carrying food.
    const int carryingSense = 20;
    for (const bool carrying : {false, true}) {
        std::vector<std::vector<float>> subset;
        for (const std::vector<float>& observation : observations) {
            if ((observation[carryingSense] > 0.5f) == carrying) {
                subset.push_back(observation);
            }
        }
        const std::vector<std::vector<float>> jacobian = ant::averageJacobian(brain, subset);
        std::printf("\nJACOBIAN while %s (%zu moments): how much each action changes per unit of each sense\n",
                    carrying ? "CARRYING food" : "SEARCHING", subset.size());
        std::printf("%-17s %8s %8s\n", "sense", "walk", "turn");
        for (int sense = 0; sense < ant::kSenseCount; ++sense) {
            std::printf("%-17s %+8.2f %+8.2f\n", ant::senseName(sense), jacobian[0][static_cast<std::size_t>(sense)],
                        jacobian[1][static_cast<std::size_t>(sense)]);
        }
    }
    return 0;
}

/// `graft`: adds one hand-made hidden neuron to a brain and reports the effect.
int runGraft(const Arguments& arguments) {
    const std::optional<ant::Network> loaded = openBrain(arguments.text("--brain", "brains/forager.brain"), 1);
    if (!loaded) {
        return 1;
    }
    // The new neuron's input weights, written as "sense=weight,sense=weight" with senses numbered from 0.
    std::vector<float> inputWeights(static_cast<std::size_t>(ant::kSenseCount), 0.0f);
    std::string list = arguments.text("--inputs", "");
    while (!list.empty()) {
        const std::size_t comma = list.find(',');
        const std::string item = list.substr(0, comma);
        const std::size_t equals = item.find('=');
        const int sense = std::atoi(item.substr(0, equals).c_str());
        if (equals == std::string::npos || sense < 0 || sense >= ant::kSenseCount) {
            std::printf("Could not understand '%s'. Write inputs as sense=weight, for example 10=4,8=-4.\n", item.c_str());
            return 1;
        }
        inputWeights[static_cast<std::size_t>(sense)] = static_cast<float>(std::atof(item.substr(equals + 1).c_str()));
        list = comma == std::string::npos ? "" : list.substr(comma + 1);
    }
    const ant::Network grafted =
        ant::withGraftedNeuron(*loaded, inputWeights, arguments.number("--bias", 0.0f),
                               {arguments.number("--walk", 0.0f), arguments.number("--turn", 0.0f)});

    std::printf("New neuron listens to:");
    for (int sense = 0; sense < ant::kSenseCount; ++sense) {
        if (inputWeights[static_cast<std::size_t>(sense)] != 0.0f) {
            std::printf(" %s (%+.1f)", ant::senseName(sense), inputWeights[static_cast<std::size_t>(sense)]);
        }
    }
    std::printf("\n");

    if (arguments.integer("--score", 1) != 0) {
        const SeedRange seeds = parseSeeds(arguments.text("--seeds", "40000-40199"));
        const ant::AverageResult before = ant::averageResult(*loaded, seeds.first, seeds.count());
        const ant::AverageResult after = ant::averageResult(grafted, seeds.first, seeds.count());
        std::printf("            food/game   wins   pickups   deaths     (maps %u to %u)\n", seeds.first, seeds.last);
        std::printf("before      %9.2f  %4.0f%%  %8.1f  %7.2f\n", before.delivered, 100.0f * before.winRate,
                    before.pickups, before.deaths);
        std::printf("with neuron %9.2f  %4.0f%%  %8.1f  %7.2f\n", after.delivered, 100.0f * after.winRate, after.pickups,
                    after.deaths);
    }

    const std::string outPath = arguments.text("--out", "");
    if (!outPath.empty() && storeBrain(grafted, outPath)) {
        std::printf("Saved %s.\n", outPath.c_str());
    }
    return 0;
}

/// `silence`: replaces one hidden neuron or one sense with a constant and reports the effect.
int runSilence(const Arguments& arguments) {
    const std::optional<ant::Network> loaded = openBrain(arguments.text("--brain", "brains/forager.brain"), 1);
    if (!loaded) {
        return 1;
    }
    // Exactly one of these chooses the part: a hidden neuron (numbered from 1,
    // as `inspect` prints them) or a sense (numbered from 0).
    const int neuron = arguments.integer("--neuron", 0);
    const int sense = arguments.integer("--sense", -1);
    const bool isSense = sense >= 0;
    if (isSense ? sense >= ant::kSenseCount : (neuron < 1 || neuron > loaded->layers()[0].weights.rows())) {
        std::printf("Give --neuron 1..%d or --sense 0..%d.\n", loaded->layers()[0].weights.rows(), ant::kSenseCount - 1);
        return 1;
    }
    const int layer = isSense ? 0 : 1;
    const int index = isSense ? sense : neuron - 1;

    // The constant that replaces the part: a value you choose, or else its usual (average) value.
    const SeedRange seeds = parseSeeds(arguments.text("--seeds", "40000-40199"));
    const std::string chosen = arguments.text("--value", "");
    float value = static_cast<float>(std::atof(chosen.c_str()));
    if (chosen.empty()) {
        const ant::Activations usual = ant::averageActivations(*loaded, ant::sampleObservations(*loaded, seeds.first, 6));
        value = usual[static_cast<std::size_t>(layer)][static_cast<std::size_t>(index)];
    }
    const ant::Network damaged = ant::silenced(*loaded, layer, index, value);
    const std::string label = isSense ? std::string("sense '") + ant::senseName(sense) + "'" : "neuron h" + std::to_string(neuron);
    std::printf("%s is now fixed at %+.2f.\n", label.c_str(), value);

    if (arguments.integer("--score", 1) != 0) {
        const ant::AverageResult before = ant::averageResult(*loaded, seeds.first, seeds.count());
        const ant::AverageResult after = ant::averageResult(damaged, seeds.first, seeds.count());
        std::printf("          food/game   wins   pickups   deaths     (maps %u to %u)\n", seeds.first, seeds.last);
        std::printf("before    %9.2f  %4.0f%%  %8.1f  %7.2f\n", before.delivered, 100.0f * before.winRate, before.pickups,
                    before.deaths);
        std::printf("after     %9.2f  %4.0f%%  %8.1f  %7.2f\n", after.delivered, 100.0f * after.winRate, after.pickups,
                    after.deaths);
    }

    const std::string outPath = arguments.text("--out", "");
    if (!outPath.empty() && storeBrain(damaged, outPath)) {
        std::printf("Saved %s.\n", outPath.c_str());
    }
    return 0;
}

/// `behaviour`: describes what a colony driven by a brain is like, in numbers.
int runBehaviour(const Arguments& arguments) {
    const std::optional<ant::Network> loaded = openBrain(arguments.text("--brain", "brains/forager.brain"), 1);
    if (!loaded) {
        return 1;
    }
    const SeedRange seeds = parseSeeds(arguments.text("--seeds", "40000-40049"));
    const ant::Behaviour b = ant::measureBehaviour(*loaded, seeds.first, seeds.count());
    if (arguments.integer("--header", 1) != 0) {
        std::printf("%-16s %5s %7s %6s %8s %9s %9s %6s %9s %9s\n", "brain", "food", "pickups", "deaths", "poison s",
                    "poisoned", "from nest", "edge", "turns/min", "backwards");
        std::printf("%-16s %5s %7s %6s %8s %9s %9s %6s %9s %9s\n", "", "", "", "", "", "homecoming", "", "", "", "");
    }
    std::printf("%-16s %5.1f %7.1f %6.2f %8.1f %9.1f %9.1f %5.0f%% %+9.1f %8.0f%%\n", loaded->name.c_str(), b.delivered,
                b.pickups, b.deaths, b.poisonSeconds, b.poisonedHomecomings, b.nestDistance, 100.0f * b.edgeShare, b.spin,
                100.0f * b.backwardShare);
    return 0;
}

void printUsage() {
    std::printf(
        "ant_train - trains and scores ant brains\n\n"
        "  ant_train xor\n"
        "      Prove the learning math works on the classic XOR problem.\n\n"
        "  ant_train imitate [--seeds 1-10 | --data FILE] [--start FILE|random] [--epochs 30]\n"
        "                    [--rate 0.05] [--batch 32] [--out brains/copycat.brain]\n"
        "      Train a brain to copy a teacher: the built-in instinct played on the given\n"
        "      food layouts, or a recording of your own play (ant_game --record FILE).\n\n"
        "  ant_train evolve [--start FILE|random] [--generations 40] [--population 64]\n"
        "                   [--sigma 0.08] [--seeds-per-generation 4] [--seconds 240]\n"
        "                   [--first-seed 1] [--final-exam 16] [--threads 0] [--checkpoint-every 10]\n"
        "                   [--out brains/forager.brain]\n"
        "      Improve a brain by selection and mutation. Every generation plays new maps.\n\n"
        "  ant_train score [--brain FILE|instinct|random] [--seeds 1000-1009] [--seconds 240]\n"
        "      Play maps with no window and report how the brain did.\n\n"
        "Watch a trained brain:  ant_game --colony-brain FILE --autopilot\n");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 1;
    }
    const Arguments arguments(argc, argv);
    const std::string command = argv[1];
    if (command == "xor") {
        return runXor();
    }
    if (command == "imitate") {
        return runImitate(arguments);
    }
    if (command == "evolve") {
        return runEvolve(arguments);
    }
    if (command == "inspect") {
        return runInspect(arguments);
    }
    if (command == "behaviour") {
        return runBehaviour(arguments);
    }
    if (command == "silence") {
        return runSilence(arguments);
    }
    if (command == "graft") {
        return runGraft(arguments);
    }
    if (command == "score") {
        return runScore(arguments);
    }
    printUsage();
    return 1;
}
