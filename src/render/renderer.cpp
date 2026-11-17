/// @file renderer.cpp
/// @brief First-person rendering of the world.
///
/// Coordinate mapping: a simulation point (x, y) on the ground becomes the 3D
/// point (x, height, y). raylib's Y axis points up, so the ground is the
/// plane height = 0.
///
/// Every shape is one of five simple meshes (square, box, cylinder, cone,
/// ball) stretched, turned and moved into place by a *transform matrix*.

#include "render/renderer.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"
#include "render/palette.h"
#include "rlgl.h"

namespace ant {
namespace {

constexpr float kEyeHeight = 0.3f;     ///< How far the player ant's eyes are above the ground.
constexpr float kFieldOfView = 70.0f;  ///< Vertical field of view, in degrees.
constexpr float kNearClip = 0.05f;     ///< Nearest distance the camera can see.
constexpr float kFarClip = 3000.0f;    ///< Farthest distance the camera can see.
constexpr float kHeadBob = 0.012f;     ///< How far the view bounces while walking.
constexpr float kStepsPerUnit = 9.0f;  ///< Walking-animation speed: radians of leg swing per unit walked.

constexpr float kGrassSize = 1200.0f;       ///< The lawn reaches far past where haze hides it.
constexpr float kGrassTile = 24.0f;         ///< World units covered by one copy of the grass texture.
constexpr float kBlanketHeight = 0.02f;     ///< The blanket lies just above the grass.
constexpr float kDecalHeight = 0.03f;       ///< Decals lie just above the blanket.
constexpr float kBasketHeight = 4.5f;
constexpr float kWickerTile = 1.6f;         ///< World units covered by one copy of the wicker texture.
constexpr float kFoodSize = 0.7f;
constexpr float kAnthillSize = 2.6f;

constexpr Color kShadowTint{255, 255, 255, 255};
constexpr Color kPoisonRim{66, 120, 30, 235};
constexpr Color kPoisonCore{150, 230, 60, 235};
constexpr Color kAntBody{58, 36, 28, 255};
constexpr Color kAntLimb{34, 22, 18, 255};
constexpr Color kHomeScent{80, 170, 255, 255};  ///< Scent trails leading home: blue.
constexpr Color kFoodScent{255, 170, 40, 255};  ///< Scent trails leading to food: orange.

/// Lifts a ground point into 3D at the given height.
Vector3 lift(Vec2 point, float height) { return {point.x, height, point.y}; }

/// Builds a transform from where a shape's centre goes (`origin`) and where
/// its three local axes end up. The lengths of the axes stretch the shape.
Matrix basis(Vector3 origin, Vector3 xAxis, Vector3 yAxis, Vector3 zAxis) {
    Matrix m{};
    m.m0 = xAxis.x;  m.m4 = yAxis.x;  m.m8 = zAxis.x;   m.m12 = origin.x;
    m.m1 = xAxis.y;  m.m5 = yAxis.y;  m.m9 = zAxis.y;   m.m13 = origin.y;
    m.m2 = xAxis.z;  m.m6 = yAxis.z;  m.m10 = zAxis.z;  m.m14 = origin.z;
    m.m15 = 1.0f;
    return m;
}

/// A transform that only stretches a shape along the world axes and moves it.
Matrix placed(Vector3 origin, Vector3 size) {
    return basis(origin, {size.x, 0.0f, 0.0f}, {0.0f, size.y, 0.0f}, {0.0f, 0.0f, size.z});
}

/// A transform that stretches the unit cylinder into a rod from `from` to `to`.
Matrix rod(Vector3 from, Vector3 to, float radius) {
    const Vector3 along = Vector3Subtract(to, from);
    // Any direction not parallel to the rod works as a helper for finding its sides.
    const Vector3 helper = std::fabs(Vector3Normalize(along).y) > 0.99f ? Vector3{1.0f, 0.0f, 0.0f}
                                                                         : Vector3{0.0f, 1.0f, 0.0f};
    const Vector3 side = Vector3Scale(Vector3Normalize(Vector3CrossProduct(along, helper)), radius);
    const Vector3 other = Vector3Scale(Vector3Normalize(Vector3CrossProduct(side, along)), radius);
    return basis(from, side, along, other);
}

/// The height and main colour of each obstacle kind.
float obstacleHeight(ObstacleKind kind) {
    switch (kind) {
        case ObstacleKind::Plate:  return 0.42f;
        case ObstacleKind::Cup:    return 4.0f;
        case ObstacleKind::Glass:  return 3.2f;
        case ObstacleKind::Bottle: return 6.9f;
    }
    return 1.0f;
}

/// Darkens (factor below 1) or lightens (above 1) a colour.
Color shade(Color color, float factor) {
    const auto channel = [factor](unsigned char value) {
        return static_cast<unsigned char>(std::clamp(static_cast<float>(value) * factor, 0.0f, 255.0f));
    };
    return {channel(color.r), channel(color.g), channel(color.b), color.a};
}

/// A repeatable pseudo-random number in [0, 1] for scenery item `index`.
float scatter(int index, float salt) {
    const float wave = std::sin(static_cast<float>(index) * 12.9898f + salt) * 43758.5453f;
    return wave - std::floor(wave);
}

}  // namespace

Renderer::Renderer(const World& world)
    : sprites_(loadSprites(2.0f * world.level().blanketHalfSize.x, 2.0f * world.level().blanketHalfSize.y)),
      lighting_(loadLightingShader()),
      cutout_(loadCutoutShader()),
      litMaterial_(LoadMaterialDefault()),
      flatMaterial_(LoadMaterialDefault()),
      white_(litMaterial_.maps[MATERIAL_MAP_DIFFUSE].texture),
      plane_(GenMeshPlane(1.0f, 1.0f, 1, 1)),
      cube_(GenMeshCube(1.0f, 1.0f, 1.0f)),
      cylinder_(GenMeshCylinder(1.0f, 1.0f, 40)),
      cone_(GenMeshCone(1.0f, 1.0f, 40)),
      sphere_(GenMeshSphere(1.0f, 12, 18)),
      hud_(sprites_) {
    litMaterial_.shader = lighting_.shader;

    // The scent texture has one pixel per pheromone cell and starts out clear.
    const PheromoneField& scent = world.pheromones();
    scentPixels_.assign(static_cast<std::size_t>(scent.columns()) * scent.rows(), BLANK);
    const Image blank = GenImageColor(scent.columns(), scent.rows(), BLANK);
    scentTexture_ = LoadTextureFromImage(blank);
    UnloadImage(blank);
    SetTextureFilter(scentTexture_, TEXTURE_FILTER_BILINEAR);
}

Renderer::~Renderer() {
    UnloadTexture(scentTexture_);
    UnloadMesh(sphere_);
    UnloadMesh(cone_);
    UnloadMesh(cylinder_);
    UnloadMesh(cube_);
    UnloadMesh(plane_);
    // The materials only borrow their shader and textures, so free just their own storage.
    MemFree(flatMaterial_.maps);
    MemFree(litMaterial_.maps);
    UnloadShader(cutout_);
    UnloadShader(lighting_.shader);
    unloadSprites(sprites_);
}

void Renderer::draw(const World& world) {
    const Ant& player = world.player();

    // The camera sits at the player ant's eyes and looks along its heading.
    // It bobs gently with each step, and sinks to the ground if the ant dies.
    const float bob = kHeadBob * std::sin(player.distanceWalked * kStepsPerUnit);
    const float eye = player.alive ? kEyeHeight + bob : kEyeHeight * 0.4f;
    camera_.position = lift(player.position, eye);
    camera_.target = lift(player.position + fromAngle(player.heading), eye);
    camera_.up = {0.0f, 1.0f, 0.0f};
    camera_.fovy = kFieldOfView;
    camera_.projection = CAMERA_PERSPECTIVE;

    updateScentTexture(world);
    ClearBackground(kHorizon);  // also resets the depth buffer left over from the last frame
    drawSky();

    rlSetClipPlanes(kNearClip, kFarClip);
    BeginMode3D(camera_);
    setCameraPosition(lighting_, camera_.position);

    const Level& level = world.level();
    drawGround(level);
    drawGroundDecals(world);
    drawNest(level.nest);
    for (std::size_t i = 0; i < level.obstacles.size(); ++i) {
        drawObstacle(level.obstacles[i], i);
    }
    drawBasket(level);
    for (std::size_t i = 1; i < world.ants().size(); ++i) {  // ant 0 is the camera
        if (world.ants()[i].alive) {
            drawAnt(world.ants()[i]);
        }
    }

    BeginShaderMode(cutout_);
    for (const Billboard& billboard : collectBillboards(world)) {
        DrawBillboard(camera_, *billboard.texture, billboard.center, billboard.height, WHITE);
    }
    EndShaderMode();

    drawGlassShells(level);
    EndMode3D();

    hud_.draw(world, showScent_ ? &scentTexture_ : nullptr);
}

void Renderer::updateScentTexture(const World& world) {
    const PheromoneField& scent = world.pheromones();
    for (int row = 0; row < scent.rows(); ++row) {
        for (int column = 0; column < scent.columns(); ++column) {
            const float home = scent.cell(Scent::Home, column, row);
            const float food = scent.cell(Scent::Food, column, row);
            const float total = home + food;
            Color& pixel = scentPixels_[static_cast<std::size_t>(row) * scent.columns() + column];
            if (total <= 0.0005f) {
                pixel = BLANK;
                continue;
            }
            // The colour is a mix of the two scents; faint scent is boosted
            // (square root) so that thin trails are still visible.
            const float foodShare = food / total;
            const auto blend = [foodShare](unsigned char a, unsigned char b) {
                return static_cast<unsigned char>(static_cast<float>(a) + (static_cast<float>(b) - a) * foodShare);
            };
            const float strength = std::sqrt(std::min(1.0f, std::max(home, food)));
            pixel = {blend(kHomeScent.r, kFoodScent.r), blend(kHomeScent.g, kFoodScent.g),
                     blend(kHomeScent.b, kFoodScent.b), static_cast<unsigned char>(strength * 150.0f)};
        }
    }
    UpdateTexture(scentTexture_, scentPixels_.data());
}

void Renderer::drawSky() const {
    // The camera never tilts, so the horizon is always halfway up the screen.
    const int width = GetScreenWidth();
    const int half = GetScreenHeight() / 2;
    DrawRectangleGradientV(0, 0, width, half, kSkyTop, kHorizon);
    DrawRectangle(0, half, width, GetScreenHeight() - half, kHorizon);
}

void Renderer::drawLit(const Mesh& mesh, const Texture2D* texture, Color color, const Matrix& transform,
                       Vector2 textureRepeats) {
    setTextureScale(lighting_, textureRepeats);
    litMaterial_.maps[MATERIAL_MAP_DIFFUSE].texture = texture != nullptr ? *texture : white_;
    litMaterial_.maps[MATERIAL_MAP_DIFFUSE].color = color;
    DrawMesh(mesh, litMaterial_, transform);
}

void Renderer::drawDecal(const Texture2D& texture, Color tint, Vec2 center, Vec2 lengthAxis, float length,
                         float width, float height) {
    const Vector3 along{lengthAxis.x * length, 0.0f, lengthAxis.y * length};
    const Vector3 across{-lengthAxis.y * width, 0.0f, lengthAxis.x * width};
    flatMaterial_.maps[MATERIAL_MAP_DIFFUSE].texture = texture;
    flatMaterial_.maps[MATERIAL_MAP_DIFFUSE].color = tint;
    DrawMesh(plane_, flatMaterial_, basis(lift(center, height), along, {0.0f, 1.0f, 0.0f}, across));
}

void Renderer::drawCylinder(Vec2 base, float bottom, float radius, float height, Color color) {
    drawLit(cylinder_, nullptr, color, placed(lift(base, bottom), {radius, height, radius}));
}

void Renderer::drawShadow(Vec2 base, float radius, float height) {
    // The shadow of an upright object is a blob stretched away from the sun.
    // The blob texture fades toward its rim, so it is drawn a little oversized.
    const float reach = height * kShadowLengthPerHeight;
    const float length = (2.0f * radius + reach) * 1.35f;
    const float width = 2.0f * radius * 1.35f;
    drawDecal(sprites_.shadow, kShadowTint, base + kShadowDirection * (reach / 2.0f), kShadowDirection, length,
              width, kDecalHeight + 0.01f);
}

void Renderer::drawGround(const Level& level) {
    const float repeats = kGrassSize / kGrassTile;
    drawLit(plane_, &sprites_.grass, WHITE, placed({0.0f, 0.0f, 0.0f}, {kGrassSize, 1.0f, kGrassSize}),
            {repeats, repeats});
    drawLit(plane_, &sprites_.blanket, WHITE,
            placed({0.0f, kBlanketHeight, 0.0f},
                   {2.0f * level.blanketHalfSize.x, 1.0f, 2.0f * level.blanketHalfSize.y}));
}

void Renderer::drawGroundDecals(const World& world) {
    const Level& level = world.level();
    const Vec2 east{1.0f, 0.0f};

    // Decals are see-through, so they must not block what is drawn after them.
    rlDisableDepthMask();

    for (const Poison& poison : level.poisons) {
        const float pulse = 1.0f + 0.04f * std::sin(static_cast<float>(GetTime()) * 2.0f + poison.position.x);
        const float size = 2.0f * poison.radius;
        drawDecal(sprites_.puddle, kPoisonRim, poison.position, east, size, size, kDecalHeight);
        drawDecal(sprites_.puddle, kPoisonCore, poison.position, east, size * 0.72f * pulse, size * 0.72f * pulse,
                  kDecalHeight + 0.002f);
    }

    if (showScent_) {
        drawDecal(scentTexture_, WHITE, {0.0f, 0.0f}, east, 2.0f * level.areaHalfSize.x,
                  2.0f * level.areaHalfSize.y, kDecalHeight + 0.005f);
    }

    for (const Obstacle& obstacle : level.obstacles) {
        drawShadow(obstacle.position, obstacle.radius, obstacleHeight(obstacle.kind));
    }
    for (const Wall& wall : level.walls) {
        // A wall's shadow: a blob as long as the wall, pushed away from the sun.
        const Vec2 along = wall.end - wall.start;
        const float reach = kBasketHeight * kShadowLengthPerHeight;
        const Vec2 middle = (wall.start + wall.end) * 0.5f + kShadowDirection * (reach / 2.0f);
        drawDecal(sprites_.shadow, kShadowTint, middle, along * (1.0f / length(along)), length(along) * 1.3f,
                  (reach + 2.0f * wall.thickness) * 1.3f, kDecalHeight + 0.01f);
    }
    drawShadow(level.nest.position, kAnthillSize * 0.4f, kAnthillSize * 0.5f);
    for (const Food& food : world.foods()) {
        if (food.state == FoodState::OnGround) {
            drawShadow(food.position, kFoodSize * 0.35f, kFoodSize * 0.3f);
        }
    }
    for (std::size_t i = 1; i < world.ants().size(); ++i) {
        if (world.ants()[i].alive) {
            drawShadow(world.ants()[i].position, 0.3f, 0.2f);
        }
    }

    rlEnableDepthMask();
}

void Renderer::drawNest(const Nest& nest) {
    drawCylinder(nest.position, 0.0f, nest.radius, 0.04f, kNestDirt);
    drawCylinder(nest.position, 0.0f, nest.radius * 0.7f, 0.07f, shade(kNestDirt, 1.15f));
}

void Renderer::drawObstacle(const Obstacle& obstacle, std::size_t index) {
    const Vec2 at = obstacle.position;
    const float r = obstacle.radius;
    const Color color = obstacleColor(obstacle.kind, index);

    switch (obstacle.kind) {
        case ObstacleKind::Plate:
            // A foot, a wide dish, and a painted blue band around the rim.
            drawCylinder(at, 0.0f, r * 0.6f, 0.12f, shade(color, 0.85f));
            drawCylinder(at, 0.12f, r, 0.30f, color);
            drawCylinder(at, 0.22f, r * 1.004f, 0.08f, Color{70, 120, 200, 255});
            break;
        case ObstacleKind::Cup:
            // A party cup: coloured body, a darker ridge, and a white rolled rim.
            drawCylinder(at, 0.0f, r, 3.7f, color);
            drawCylinder(at, 2.5f, r * 1.01f, 0.15f, shade(color, 0.75f));
            drawCylinder(at, 3.7f, r * 1.06f, 0.3f, Color{250, 250, 250, 255});
            break;
        case ObstacleKind::Glass:
            // Only the lemonade inside is solid; the glass itself is drawn last (see drawGlassShells).
            drawCylinder(at, 0.05f, r * 0.84f, 2.1f, Color{250, 206, 80, 255});
            break;
        case ObstacleKind::Bottle: {
            // Glass body with a paper label, sloping shoulders, a neck and a cap.
            drawCylinder(at, 0.0f, r, 4.2f, color);
            drawCylinder(at, 1.2f, r * 1.01f, 1.8f, Color{244, 236, 214, 255});
            drawLit(cone_, nullptr, color, placed(lift(at, 4.2f), {r, 1.6f, r}));
            drawCylinder(at, 4.2f, r * 0.34f, 2.2f, color);
            drawCylinder(at, 6.4f, r * 0.42f, 0.5f, Color{214, 168, 60, 255});
            break;
        }
    }
}

void Renderer::drawGlassShells(const Level& level) {
    // See-through things are drawn after everything solid, without blocking
    // anything, so whatever is behind the glass stays visible through it.
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    for (const Obstacle& obstacle : level.obstacles) {
        if (obstacle.kind == ObstacleKind::Glass) {
            drawCylinder(obstacle.position, 0.0f, obstacle.radius, obstacleHeight(obstacle.kind),
                         Color{210, 235, 255, 70});
        }
    }
    rlEnableDepthMask();
}

void Renderer::drawBasket(const Level& level) {
    const Color cane{255, 255, 255, 255};
    const Color rim{128, 88, 50, 255};

    for (const Wall& wall : level.walls) {
        // Each wall is a box: long axis along the wall, lengthened to close the corners.
        const Vec2 along = wall.end - wall.start;
        const float span = length(along) + 2.0f * wall.thickness;
        const Vec2 direction = along * (1.0f / length(along));
        const Vec2 middle = (wall.start + wall.end) * 0.5f;
        const Vector3 xAxis{direction.x, 0.0f, direction.y};
        const Vector3 zAxis{-direction.y, 0.0f, direction.x};
        const float depth = 2.0f * wall.thickness;

        drawLit(cube_, &sprites_.wicker, cane,
                basis(lift(middle, kBasketHeight / 2.0f), Vector3Scale(xAxis, span), {0.0f, kBasketHeight, 0.0f},
                      Vector3Scale(zAxis, depth)),
                {span / kWickerTile, kBasketHeight / kWickerTile});
        drawLit(cube_, nullptr, rim,
                basis(lift(middle, kBasketHeight + 0.12f), Vector3Scale(xAxis, span + 0.1f), {0.0f, 0.24f, 0.0f},
                      Vector3Scale(zAxis, depth + 0.16f)));
    }

    for (const Basket& basket : level.baskets) {
        // The handle: an arch of short rods from one long side over to the other.
        const int pieces = 14;
        const auto archPoint = [&](int i) {
            const float angle = kPi * static_cast<float>(i) / static_cast<float>(pieces);
            const Vec2 ground{basket.center.x, basket.center.y - std::cos(angle) * basket.halfSize.y};
            return lift(ground, kBasketHeight + std::sin(angle) * basket.halfSize.y);
        };
        for (int i = 0; i < pieces; ++i) {
            drawLit(cylinder_, nullptr, rim, rod(archPoint(i), archPoint(i + 1), 0.22f));
        }
    }
}

void Renderer::drawAnt(const Ant& ant) {
    // The ant's own directions: forward, up and to its right.
    const Vec2 ahead = fromAngle(ant.heading);
    const Vector3 forward{ahead.x, 0.0f, ahead.y};
    const Vector3 up{0.0f, 1.0f, 0.0f};
    const Vector3 right{-ahead.y, 0.0f, ahead.x};

    // A point given in the ant's own terms: so far forward, up, and to the right.
    const auto at = [&](float f, float u, float r) {
        Vector3 point = lift(ant.position, u);
        point = Vector3Add(point, Vector3Scale(forward, f));
        return Vector3Add(point, Vector3Scale(right, r));
    };
    // A body segment: a ball stretched to the given length, height and width.
    const auto segment = [&](Vector3 center, float length, float height, float width) {
        drawLit(sphere_, nullptr, kAntBody,
                basis(center, Vector3Scale(forward, length), Vector3Scale(up, height), Vector3Scale(right, width)));
    };

    segment(at(-0.27f, 0.20f, 0.0f), 0.20f, 0.14f, 0.15f);  // abdomen
    segment(at(0.00f, 0.19f, 0.0f), 0.13f, 0.09f, 0.09f);   // thorax
    segment(at(0.21f, 0.20f, 0.0f), 0.10f, 0.085f, 0.10f);  // head

    const float stride = ant.distanceWalked * kStepsPerUnit;
    for (int side = -1; side <= 1; side += 2) {
        const float s = static_cast<float>(side);
        for (int leg = 0; leg < 3; ++leg) {
            // Ants walk with a "tripod gait": the front and back legs of one
            // side move together with the middle leg of the other side.
            const bool firstTripod = (leg % 2 == 0) == (side > 0);
            const float swing = 0.07f * std::sin(stride + (firstTripod ? 0.0f : kPi));
            const float offset = 0.09f - 0.09f * static_cast<float>(leg);  // front, middle, back
            const Vector3 hip = at(offset * 0.8f, 0.17f, s * 0.07f);
            const Vector3 knee = at(offset * 1.4f + swing * 0.5f, 0.30f, s * 0.24f);
            const Vector3 foot = at(offset * 2.2f + swing, 0.0f, s * 0.36f);
            drawLit(cylinder_, nullptr, kAntLimb, rod(hip, knee, 0.014f));
            drawLit(cylinder_, nullptr, kAntLimb, rod(knee, foot, 0.012f));
        }
        // One antenna: up and out from the head, then bending forward.
        const float twitch = 0.02f * std::sin(stride * 0.7f + s);
        const Vector3 root = at(0.28f, 0.25f, s * 0.04f);
        const Vector3 elbow = at(0.40f, 0.35f + twitch, s * 0.12f);
        const Vector3 tip = at(0.54f, 0.29f + twitch, s * 0.17f);
        drawLit(cylinder_, nullptr, kAntLimb, rod(root, elbow, 0.011f));
        drawLit(cylinder_, nullptr, kAntLimb, rod(elbow, tip, 0.009f));
    }
}

std::vector<Renderer::Billboard> Renderer::collectBillboards(const World& world) const {
    std::vector<Billboard> billboards;
    const float time = static_cast<float>(GetTime());

    // Scenery: clouds high up and a ring of trees around the lawn.
    for (int i = 0; i < 9; ++i) {
        const float angle = 2.0f * kPi * (static_cast<float>(i) + scatter(i, 1.0f)) / 9.0f;
        const float height = 170.0f;
        billboards.push_back({&sprites_.cloud, lift(fromAngle(angle) * 900.0f, 380.0f + 160.0f * scatter(i, 2.0f)),
                              height});
    }
    for (int i = 0; i < 20; ++i) {
        const float angle = 2.0f * kPi * (static_cast<float>(i) + 0.6f * scatter(i, 3.0f)) / 20.0f;
        const float height = 34.0f + 16.0f * scatter(i, 4.0f);
        billboards.push_back(
            {&sprites_.tree, lift(fromAngle(angle) * (70.0f + 45.0f * scatter(i, 5.0f)), height / 2.0f), height});
    }

    billboards.push_back({&sprites_.anthill, lift(world.level().nest.position, kAnthillSize / 2.0f), kAnthillSize});

    // Food on the ground hovers and bobs slightly so it catches the eye.
    for (const Food& food : world.foods()) {
        if (food.state == FoodState::OnGround) {
            const float bob = 0.05f * std::sin(time * 2.2f + food.position.x * 1.7f);
            billboards.push_back(
                {&foodSprite(sprites_, food.kind), lift(food.position, kFoodSize / 2.0f + 0.06f + bob), kFoodSize});
        }
    }
    // Food carried by the other ants is held in front of their heads.
    for (std::size_t i = 1; i < world.ants().size(); ++i) {
        const Ant& ant = world.ants()[i];
        if (ant.alive && ant.carriedFood) {
            billboards.push_back({&foodSprite(sprites_, world.foods()[*ant.carriedFood].kind),
                                  lift(ant.position + fromAngle(ant.heading) * 0.4f, 0.3f), 0.42f});
        }
    }
    return billboards;
}

}  // namespace ant
