/// @file main.cpp
/// @brief Program entry point: opens the window and runs the game loop.
///
/// Each frame the loop does three things:
///   1. Input  - turn the keyboard state into an `AntAction`.
///   2. Update - advance the `World` in fixed-size time steps.
///   3. Draw   - hand the `World` to the `Renderer`.
///
/// Usage: `ant_game [--screenshot <file.png>]`
/// With `--screenshot` the game draws one second of frames, saves a picture
/// of the window to the given file and exits (handy for automated checks).

#include <algorithm>
#include <cstdint>
#include <cstring>

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

/// Frames drawn before the picture is taken in `--screenshot` mode.
constexpr int kScreenshotFrame = 60;

/// Reads the keyboard and converts it into the ant's control signal.
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

/// Saves the frame currently being drawn to an image file.
void saveScreenshot(const char* path) {
    rlDrawRenderBatchActive();  // make sure everything queued so far is really drawn
    const Image image = LoadImageFromScreen();
    ExportImage(image, path);
    UnloadImage(image);
}

}  // namespace

int main(int argc, char** argv) {
    const char* screenshotPath = nullptr;
    if (argc == 3 && std::strcmp(argv[1], "--screenshot") == 0) {
        screenshotPath = argv[2];
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Ant Picnic");

    {
        std::uint32_t seed = 1;
        ant::World world(ant::makePicnicLevel(seed));
        const ant::Renderer renderer(world.level());

        float unsimulatedTime = 0.0f;
        int frame = 0;
        bool running = true;

        while (running && !WindowShouldClose()) {
            if (IsKeyPressed(KEY_R)) {
                world = ant::World(ant::makePicnicLevel(++seed));  // new food layout each restart
            }

            // Run as many fixed steps as real time has passed since last frame.
            const ant::AntAction action = readPlayerAction();
            unsimulatedTime += std::min(GetFrameTime(), kMaxFrameTime);
            while (unsimulatedTime >= kFixedStep) {
                world.step(action, kFixedStep);
                unsimulatedTime -= kFixedStep;
            }

            BeginDrawing();
            renderer.draw(world);
            if (screenshotPath != nullptr && ++frame == kScreenshotFrame) {
                saveScreenshot(screenshotPath);
                running = false;
            }
            EndDrawing();
        }
    }  // the renderer must release its textures before the window closes

    CloseWindow();
    return 0;
}
