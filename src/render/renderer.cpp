/// @file renderer.cpp
/// @brief First-person rendering and HUD.
///
/// Coordinate mapping: a simulation point (x, y) on the table becomes the 3D
/// point (x, height, y). raylib's Y axis points up, so the table top is the
/// plane height = 0.

#include "render/renderer.h"

#include <algorithm>
#include <vector>

#include "render/sprites.h"

namespace ant {
namespace {

constexpr float kEyeHeight = 0.3f;    ///< How far the ant's eyes are above the table.
constexpr float kFieldOfView = 70.0f; ///< Vertical field of view, in degrees.
constexpr float kFoodSpriteSize = 0.7f;
constexpr float kAnthillSpriteSize = 2.4f;
constexpr float kGroundDepth = 20.0f; ///< How far the grass is below the table.

constexpr Color kSky{135, 200, 240, 255};
constexpr Color kGrass{80, 150, 70, 255};
constexpr Color kNestDirt{120, 80, 45, 255};

/// How an obstacle kind looks: its height and colours.
struct ObstacleStyle {
    float height;
    Color fill;
    Color outline;
};

ObstacleStyle styleOf(ObstacleKind kind) {
    switch (kind) {
        case ObstacleKind::Plate:  return {0.35f, Color{240, 240, 245, 255}, Color{170, 175, 190, 255}};
        case ObstacleKind::Cup:    return {4.0f, Color{60, 120, 210, 255}, Color{30, 70, 150, 255}};
        case ObstacleKind::Bottle: return {7.0f, Color{60, 150, 90, 255}, Color{30, 95, 55, 255}};
    }
    return {1.0f, GRAY, DARKGRAY};
}

/// Lifts a table-top point into 3D at the given height.
Vector3 toWorld(Vec2 point, float height) { return {point.x, height, point.y}; }

/// A sprite waiting to be drawn, with its distance from the camera.
struct Billboard {
    const Texture2D* texture;
    Vector3 position;
    float size;
    float distanceToCamera;
};

}  // namespace

Renderer::Renderer(const Level& level)
    : crumb_(makeCrumbSprite()),
      sugar_(makeSugarSprite()),
      berry_(makeBerrySprite()),
      anthill_(makeAnthillSprite()) {
    const float width = 2.0f * level.tableHalfSize.x;
    const float length = 2.0f * level.tableHalfSize.y;
    tablecloth_ = LoadModelFromMesh(GenMeshPlane(width, length, 1, 1));
    tablecloth_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = makeTableclothTexture(width, length);
}

Renderer::~Renderer() {
    UnloadModel(tablecloth_);  // also releases the tablecloth texture
    UnloadTexture(anthill_);
    UnloadTexture(berry_);
    UnloadTexture(sugar_);
    UnloadTexture(crumb_);
}

void Renderer::draw(const World& world) const {
    ClearBackground(kSky);
    drawScene(world);
    drawHud(world);
}

const Texture2D& Renderer::foodSprite(FoodKind kind) const {
    switch (kind) {
        case FoodKind::Crumb: return crumb_;
        case FoodKind::Sugar: return sugar_;
        case FoodKind::Berry: return berry_;
    }
    return crumb_;
}

void Renderer::drawScene(const World& world) const {
    const Ant& ant = world.ant();
    const Level& level = world.level();

    // The camera sits at the ant's eyes and looks along its heading.
    Camera3D camera{};
    camera.position = toWorld(ant.position, kEyeHeight);
    camera.target = toWorld(ant.position + fromAngle(ant.heading), kEyeHeight);
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = kFieldOfView;
    camera.projection = CAMERA_PERSPECTIVE;

    BeginMode3D(camera);

    DrawPlane({0.0f, -kGroundDepth, 0.0f}, {1500.0f, 1500.0f}, kGrass);
    DrawModel(tablecloth_, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);

    for (const Obstacle& obstacle : level.obstacles) {
        const ObstacleStyle style = styleOf(obstacle.kind);
        const Vector3 base = toWorld(obstacle.position, 0.0f);
        DrawCylinder(base, obstacle.radius, obstacle.radius, style.height, 24, style.fill);
        DrawCylinderWires(base, obstacle.radius, obstacle.radius, style.height, 24, style.outline);
    }

    // The nest: a flat patch of dirt showing the drop-off area.
    DrawCylinder(toWorld(level.nest.position, 0.0f), level.nest.radius, level.nest.radius, 0.02f, 32, kNestDirt);

    // Sprites always face the camera ("billboards"). Their transparent pixels
    // would hide sprites behind them, so we draw the farthest ones first.
    std::vector<Billboard> billboards;
    const auto addBillboard = [&](const Texture2D& texture, Vec2 position, float size) {
        billboards.push_back({&texture, toWorld(position, size / 2.0f), size, distance(position, ant.position)});
    };
    addBillboard(anthill_, level.nest.position, kAnthillSpriteSize);
    for (const Food& food : world.foods()) {
        if (food.state == FoodState::OnTable) {
            addBillboard(foodSprite(food.kind), food.position, kFoodSpriteSize);
        }
    }
    std::sort(billboards.begin(), billboards.end(),
              [](const Billboard& a, const Billboard& b) { return a.distanceToCamera > b.distanceToCamera; });
    for (const Billboard& billboard : billboards) {
        DrawBillboard(camera, *billboard.texture, billboard.position, billboard.size, WHITE);
    }

    EndMode3D();
}

void Renderer::drawHud(const World& world) const {
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const int foodTotal = static_cast<int>(world.foods().size());

    DrawRectangle(10, 10, 300, 74, Fade(BLACK, 0.5f));
    DrawText(TextFormat("Food delivered: %d / %d", world.deliveredCount(), foodTotal), 20, 18, 24, RAYWHITE);
    const int seconds = static_cast<int>(world.timeRemaining());
    DrawText(TextFormat("Time: %d:%02d", seconds / 60, seconds % 60), 20, 50, 24,
             seconds < 20 ? ORANGE : RAYWHITE);

    // Show what the ant is carrying, like an item held in front of the camera.
    if (world.ant().carriedFood) {
        const Texture2D& sprite = foodSprite(world.foods()[*world.ant().carriedFood].kind);
        const float scale = 3.0f;
        const Vector2 corner{screenWidth / 2.0f - sprite.width * scale / 2.0f,
                             static_cast<float>(screenHeight) - sprite.height * scale * 0.8f};
        DrawTextureEx(sprite, corner, 0.0f, scale, WHITE);
        const char* hint = "Carrying food - take it to the anthill!";
        DrawText(hint, (screenWidth - MeasureText(hint, 20)) / 2, 20, 20, YELLOW);
    }

    drawMiniMap(world);

    if (world.status() != GameStatus::Playing) {
        const bool won = world.status() == GameStatus::Won;
        const char* title = won ? "ALL FOOD DELIVERED!" : "OUT OF TIME";
        const char* prompt = "Press R to play again";
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.6f));
        DrawText(title, (screenWidth - MeasureText(title, 60)) / 2, screenHeight / 2 - 60, 60, won ? GREEN : RED);
        DrawText(prompt, (screenWidth - MeasureText(prompt, 28)) / 2, screenHeight / 2 + 20, 28, RAYWHITE);
    } else {
        const char* controls = "W/S walk   A/D turn   R restart   Esc quit";
        DrawRectangle(10, screenHeight - 38, MeasureText(controls, 18) + 20, 28, Fade(BLACK, 0.5f));
        DrawText(controls, 20, screenHeight - 33, 18, RAYWHITE);
    }
}

void Renderer::drawMiniMap(const World& world) const {
    const Level& level = world.level();
    const float scale = 5.0f;  // screen pixels per table unit
    const float width = 2.0f * level.tableHalfSize.x * scale;
    const float height = 2.0f * level.tableHalfSize.y * scale;
    const Vector2 origin{static_cast<float>(GetScreenWidth()) - width - 14.0f, 14.0f};

    // Converts a table point to a mini-map pixel.
    const auto toMap = [&](Vec2 point) {
        return Vector2{origin.x + (point.x + level.tableHalfSize.x) * scale,
                       origin.y + (point.y + level.tableHalfSize.y) * scale};
    };

    DrawRectangleV(origin, {width, height}, Fade(BLACK, 0.55f));
    DrawRectangleLinesEx({origin.x, origin.y, width, height}, 2.0f, RAYWHITE);

    DrawCircleV(toMap(level.nest.position), level.nest.radius * scale, kNestDirt);
    for (const Obstacle& obstacle : level.obstacles) {
        DrawCircleV(toMap(obstacle.position), obstacle.radius * scale, styleOf(obstacle.kind).fill);
    }
    for (const Food& food : world.foods()) {
        if (food.state == FoodState::OnTable) {
            DrawCircleV(toMap(food.position), 3.0f, YELLOW);
        }
    }

    // The ant: a dot with a short line showing which way it faces.
    const Ant& ant = world.ant();
    const Vector2 antDot = toMap(ant.position);
    DrawLineEx(antDot, toMap(ant.position + fromAngle(ant.heading) * 2.0f), 2.0f, ORANGE);
    DrawCircleV(antDot, 4.0f, ORANGE);
}

}  // namespace ant
