/// @file main.cpp
/// @brief Program entry point: opens the window and runs the game loop.
///
/// Each frame the loop does three things:
///   1. Input  - turn the keyboard state into an `AntAction`.
///   2. Update - advance the `World` in fixed-size time steps.
///   3. Draw   - hand the `World` to the `Renderer`.
///
/// Usage: `ant_game [--autopilot] [--screenshot <file.png> [seconds]]`
///   --autopilot   The player's ant follows its instinct too, so the whole
///                 colony can simply be watched.
///   --screenshot  Skip ahead `seconds` of game time (default 1), save a
///                 picture of the window to the file and exit. Handy for
///                 automated checks.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "core/instinct.h"
#include "core/world.h"
#include "raylib.h"
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
    const char* screenshotPath = nullptr;
    float screenshotSeconds = 1.0f;
};

/// Reads the command line. Unknown arguments are ignored.
Options parseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--autopilot") == 0) {
            options.autopilot = true;
        } else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            options.screenshotPath = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                options.screenshotSeconds = static_cast<float>(std::atof(argv[++i]));
            }
        }
    }
    return options;
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

/// Advances the game by one fixed step.
void stepWorld(ant::World& world, const Options& options) {
    const ant::AntAction action = options.autopilot ? ant::instinctAction(world, 0) : readPlayerAction();
    world.step(action, kFixedStep);
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

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Ant Picnic");
    SetWindowMinSize(960, 600);

    {
        std::uint32_t seed = 1;
        ant::World world(ant::makePicnicLevel(seed));
        ant::Renderer renderer(world);

        if (options.screenshotPath != nullptr) {
            for (float t = 0.0f; t < options.screenshotSeconds; t += kFixedStep) {
                stepWorld(world, options);
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

            // Run as many fixed steps as real time has passed since last frame.
            unsimulatedTime += std::min(GetFrameTime(), kMaxFrameTime);
            while (unsimulatedTime >= kFixedStep) {
                stepWorld(world, options);
                unsimulatedTime -= kFixedStep;
            }

            BeginDrawing();
            renderer.draw(world);
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
