/// @file main.cpp
/// @brief Program entry point: opens the window and runs the game loop.
///
/// Each frame the loop does three things:
///   1. Input  - decide the player ant's `AntAction` (keyboard, instinct or network).
///   2. Update - advance the `World` in fixed-size time steps.
///   3. Draw   - hand the `World` to the `Renderer`, then draw the network view.
///
/// Usage: `ant_game [options]`
///   --autopilot             The player's ant follows its instinct, so the
///                           whole colony can simply be watched.
///   --brain <file|random>   A neural network drives the player's ant: one
///                           loaded from a brain file, or a random one.
///   --colony-brain <file>   A neural network drives the other nine ants in place of
///                           their instinct.
///   --save-brain <file>     Write the current network to a brain file at start-up.
///   --record <file>         Record your own play (what your ant sensed and what you
///                           pressed) to a file that `ant_train imitate --data` can
///                           learn from.
///   --show-network          Start with the network view open (key N).
///   --seed <number>         Start on that map. Without it, each launch picks a
///                           different random map.
///   --screenshot <file.png> [seconds]
///                           Skip ahead `seconds` of game time (default 1),
///                           save a picture of the window and exit. Handy
///                           for automated checks.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <random>
#include <vector>

#include "agent/brain.h"
#include "agent/senses.h"
#include "core/instinct.h"
#include "core/world.h"
#include "raylib.h"
#include "render/network_view.h"
#include "render/renderer.h"
#include "train/dataset.h"
#include "rlgl.h"

namespace {

/// The simulation always advances in steps of exactly this many seconds.
/// A fixed step makes the game behave identically on fast and slow computers.
constexpr float kFixedStep = 1.0f / 60.0f;

/// The longest frame time we will simulate, so a long pause (for example
/// dragging the window) cannot cause a huge burst of catch-up steps.
constexpr float kMaxFrameTime = 0.25f;

/// Frames drawn before the picture is taken in `--screenshot` mode, giving
/// the window time to appear.
constexpr int kScreenshotFrame = 10;

/// What the command line asked for.
struct Options {
    bool autopilot = false;
    bool showNetwork = false;
    std::optional<std::uint32_t> seed;  ///< The first map; empty = pick one at random.
    const char* brainPath = nullptr;  ///< A file name, or "random".
    const char* colonyBrainPath = nullptr;
    const char* saveBrainPath = nullptr;
    const char* recordPath = nullptr;
    const char* screenshotPath = nullptr;
    float screenshotSeconds = 1.0f;
};

/// Who is deciding what the player's ant does.
enum class Driver { Keyboard, Instinct, Network };

/// Reads the command line. Unknown arguments are ignored.
Options parseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const bool hasValue = i + 1 < argc;
        if (std::strcmp(argv[i], "--autopilot") == 0) {
            options.autopilot = true;
        } else if (std::strcmp(argv[i], "--seed") == 0 && hasValue) {
            options.seed = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--show-network") == 0) {
            options.showNetwork = true;
        } else if (std::strcmp(argv[i], "--brain") == 0 && hasValue) {
            options.brainPath = argv[++i];
        } else if (std::strcmp(argv[i], "--colony-brain") == 0 && hasValue) {
            options.colonyBrainPath = argv[++i];
        } else if (std::strcmp(argv[i], "--record") == 0 && hasValue) {
            options.recordPath = argv[++i];
        } else if (std::strcmp(argv[i], "--save-brain") == 0 && hasValue) {
            options.saveBrainPath = argv[++i];
        } else if (std::strcmp(argv[i], "--screenshot") == 0 && hasValue) {
            options.screenshotPath = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                options.screenshotSeconds = static_cast<float>(std::atof(argv[++i]));
            }
        }
    }
    return options;
}

/// Loads the network named on the command line, or makes a random one.
/// A file that is missing or has the wrong shape is reported and replaced
/// by a random network, so the game always has one to show.
ant::Network chooseBrain(const Options& options, std::uint32_t seed) {
    if (options.brainPath != nullptr && std::strcmp(options.brainPath, "random") != 0) {
        const std::optional<ant::Network> loaded = ant::loadNetwork(options.brainPath);
        if (loaded && ant::isAntBrain(*loaded)) {
            return *loaded;
        }
        std::fprintf(stderr, "Could not use '%s' as an ant brain; using a random one instead.\n", options.brainPath);
    }
    return ant::makeRandomBrain(seed);
}

/// Reads the keyboard and converts it into the player ant's control signal.
ant::AntAction readPlayerAction() {
    const bool ahead = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
    const bool back = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
    const bool right = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
    const bool left = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);

    ant::AntAction action;
    action.forward = static_cast<float>(ahead) - static_cast<float>(back);
    action.turn = static_cast<float>(right) - static_cast<float>(left);
    return action;
}

/// Everything that decides how the ants are driven during one step.
struct Controllers {
    Driver driver = Driver::Keyboard;          ///< Who decides for the player's ant.
    const ant::Network* brain = nullptr;       ///< The network used when `driver` is `Network`.
    const ant::Network* colonyBrain = nullptr; ///< Drives the other ants; null = their instinct.
    ant::Dataset* recording = nullptr;         ///< Where to record the player's own play; null = don't.
};

/// Advances the game by one fixed step.
void stepWorld(ant::World& world, const Controllers& controllers) {
    std::vector<ant::AntAction> actions(world.ants().size());
    switch (controllers.driver) {
        case Driver::Keyboard: actions[0] = readPlayerAction(); break;
        case Driver::Instinct: actions[0] = ant::instinctAction(world, 0); break;
        case Driver::Network:  actions[0] = ant::brainAction(*controllers.brain, world, 0); break;
    }
    for (std::size_t i = 1; i < actions.size(); ++i) {
        actions[i] = controllers.colonyBrain != nullptr ? ant::brainAction(*controllers.colonyBrain, world, i)
                                                        : ant::instinctAction(world, i);
    }

    // Record what the player sensed and pressed, as an example to learn from.
    // Moments with no key held are skipped; they would only teach "stand still".
    const bool pressing = actions[0].forward != 0.0f || actions[0].turn != 0.0f;
    if (controllers.recording != nullptr && controllers.driver == Driver::Keyboard && pressing &&
        world.player().alive && world.status() == ant::GameStatus::Playing) {
        controllers.recording->push_back({ant::observe(world, 0), {actions[0].forward, actions[0].turn}});
    }

    world.step(actions, kFixedStep);
}

/// The line of text under the network view's title.
const char* networkStatus(Driver driver) {
    switch (driver) {
        case Driver::Keyboard: return "You are driving. This is what the network WOULD do.   B: let it drive   G: new random brain";
        case Driver::Instinct: return "Instinct is driving. This is what the network WOULD do.   B: let it drive   G: new random brain";
        case Driver::Network:  return "The network is driving your ant.   B: take back control   G: new random brain";
    }
    return "";
}

/// Saves the frame currently being drawn to an image file.
void saveScreenshot(const char* path) {
    rlDrawRenderBatchActive();  // make sure everything queued so far is really drawn
    Image image = LoadImageFromScreen();
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8);  // drop transparency: a screenshot is opaque
    ExportImage(image, path);
    UnloadImage(image);
}

}  // namespace

int main(int argc, char** argv) {
    const Options options = parseOptions(argc, argv);

    std::uint32_t brainSeed = 1;
    ant::Network brain = chooseBrain(options, brainSeed);
    if (options.saveBrainPath != nullptr && !ant::saveNetwork(brain, options.saveBrainPath)) {
        std::fprintf(stderr, "Could not write the brain file '%s'.\n", options.saveBrainPath);
    }

    // An optional second network that replaces the instinct of the other nine ants.
    std::optional<ant::Network> colonyBrain;
    if (options.colonyBrainPath != nullptr) {
        colonyBrain = ant::loadNetwork(options.colonyBrainPath);
        if (!colonyBrain || !ant::isAntBrain(*colonyBrain)) {
            std::fprintf(stderr, "Could not use '%s' as an ant brain; the colony keeps its instinct.\n",
                         options.colonyBrainPath);
            colonyBrain.reset();
        }
    }
    ant::Dataset recording;  // filled only when --record was given

    // The names drawn beside the network's inputs and outputs.
    std::vector<const char*> inputLabels;
    for (int i = 0; i < ant::kSenseCount; ++i) {
        inputLabels.push_back(ant::senseName(i));
    }
    const std::vector<const char*> outputLabels{"walk  (+ ahead)", "turn  (+ right)"};

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Ant Picnic");
    SetWindowMinSize(960, 600);

    {
        // Every map is random. A fresh launch starts on a different one each time,
        // unless a seed was given (screenshots default to map 1 so they are repeatable).
        std::uint32_t seed = options.seed ? *options.seed
                                          : (options.screenshotPath != nullptr ? 1u : std::random_device{}() % 100000u);
        std::printf("Map seed: %u (replay it with --seed %u)\n", seed, seed);
        ant::World world(ant::makePicnicLevel(seed));
        ant::Renderer renderer(world);

        bool networkDrives = options.brainPath != nullptr;
        bool showNetwork = options.showNetwork || networkDrives;
        // The network takes priority, then the instinct, then the keyboard.
        const auto currentDriver = [&] {
            return networkDrives ? Driver::Network : (options.autopilot ? Driver::Instinct : Driver::Keyboard);
        };
        const auto controllers = [&] {
            return Controllers{currentDriver(), &brain, colonyBrain ? &*colonyBrain : nullptr,
                               options.recordPath != nullptr ? &recording : nullptr};
        };

        if (options.screenshotPath != nullptr) {
            for (float t = 0.0f; t < options.screenshotSeconds; t += kFixedStep) {
                stepWorld(world, controllers());
            }
        }

        float unsimulatedTime = 0.0f;
        int frame = 0;
        bool running = true;

        while (running && !WindowShouldClose()) {
            if (IsKeyPressed(KEY_R)) {
                world = ant::World(ant::makePicnicLevel(++seed));  // a whole new map each restart
            }
            if (IsKeyPressed(KEY_P)) {
                renderer.toggleScentView();
            }
            if (IsKeyPressed(KEY_N)) {
                showNetwork = !showNetwork;
            }
            if (IsKeyPressed(KEY_B)) {
                networkDrives = !networkDrives;
            }
            if (IsKeyPressed(KEY_G)) {
                brain = ant::makeRandomBrain(++brainSeed);  // a different set of random numbers
            }

            // Run as many fixed steps as real time has passed since last frame.
            unsimulatedTime += std::min(GetFrameTime(), kMaxFrameTime);
            while (unsimulatedTime >= kFixedStep) {
                stepWorld(world, controllers());
                unsimulatedTime -= kFixedStep;
            }

            BeginDrawing();
            renderer.draw(world);
            if (showNetwork) {
                // Show the network what the player's ant senses right now and draw its reaction.
                ant::drawNetworkView(brain, brain.run(ant::observe(world, 0)), inputLabels, outputLabels,
                                     networkStatus(currentDriver()));
            }
            if (options.screenshotPath != nullptr && ++frame == kScreenshotFrame) {
                saveScreenshot(options.screenshotPath);
                running = false;
            }
            EndDrawing();
        }
    }  // the renderer must release its graphics resources before the window closes

    if (options.recordPath != nullptr) {
        if (ant::saveDataset(recording, options.recordPath)) {
            std::printf("Recorded %zu examples of your play to '%s'.\n", recording.size(), options.recordPath);
        } else {
            std::fprintf(stderr, "Nothing was recorded to '%s'.\n", options.recordPath);
        }
    }

    CloseWindow();
    return 0;
}
