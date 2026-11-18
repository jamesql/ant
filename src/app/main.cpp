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
///   --save-brain <file>     Write the current network to a brain file at start-up.
///   --show-network          Start with the network view open (key N).
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
#include <vector>

#include "agent/brain.h"
#include "agent/senses.h"
#include "core/instinct.h"
#include "core/world.h"
#include "raylib.h"
#include "render/network_view.h"
#include "render/renderer.h"
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
    const char* brainPath = nullptr;  ///< A file name, or "random".
    const char* saveBrainPath = nullptr;
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
        } else if (std::strcmp(argv[i], "--show-network") == 0) {
            options.showNetwork = true;
        } else if (std::strcmp(argv[i], "--brain") == 0 && hasValue) {
            options.brainPath = argv[++i];
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

/// Advances the game by one fixed step, with `driver` deciding for the player's ant.
void stepWorld(ant::World& world, Driver driver, const ant::Network& brain) {
    ant::AntAction action;
    switch (driver) {
        case Driver::Keyboard: action = readPlayerAction(); break;
        case Driver::Instinct: action = ant::instinctAction(world, 0); break;
        case Driver::Network:  action = ant::brainAction(brain, world, 0); break;
    }
    world.step(action, kFixedStep);
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
        std::uint32_t seed = 1;
        ant::World world(ant::makePicnicLevel(seed));
        ant::Renderer renderer(world);

        bool networkDrives = options.brainPath != nullptr;
        bool showNetwork = options.showNetwork || networkDrives;
        // The network takes priority, then the instinct, then the keyboard.
        const auto currentDriver = [&] {
            return networkDrives ? Driver::Network : (options.autopilot ? Driver::Instinct : Driver::Keyboard);
        };

        if (options.screenshotPath != nullptr) {
            for (float t = 0.0f; t < options.screenshotSeconds; t += kFixedStep) {
                stepWorld(world, currentDriver(), brain);
            }
        }

        float unsimulatedTime = 0.0f;
        int frame = 0;
        bool running = true;

        while (running && !WindowShouldClose()) {
            if (IsKeyPressed(KEY_R)) {
                world = ant::World(ant::makePicnicLevel(++seed));  // new food layout each restart
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
                stepWorld(world, currentDriver(), brain);
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

    CloseWindow();
    return 0;
}
