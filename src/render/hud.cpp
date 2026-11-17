/// @file hud.cpp
/// @brief Drawing of the 2D overlay.
///
/// All sizes are in screen pixels. Text uses raylib's built-in pixel font at
/// multiples of 10 pixels, where it is perfectly crisp.

#include "render/hud.h"

#include <algorithm>
#include <cmath>

#include "render/palette.h"

namespace ant {
namespace {

constexpr float kMargin = 16.0f;         ///< Gap between panels and the screen edge.
constexpr float kMessageSeconds = 2.5f;  ///< How long a message stays on screen.
constexpr float kMapScale = 4.5f;        ///< Mini-map pixels per world unit.

constexpr Color kPanel{18, 24, 34, 185};
constexpr Color kPanelEdge{255, 255, 255, 46};
constexpr Color kText{244, 244, 240, 255};
constexpr Color kTextDim{190, 198, 210, 255};
constexpr Color kGood{120, 214, 110, 255};
constexpr Color kWarn{250, 176, 60, 255};
constexpr Color kBad{236, 80, 70, 255};
constexpr Color kGrassMap{92, 148, 78, 255};

/// The current time in seconds, as a float.
float now() { return static_cast<float>(GetTime()); }

/// A rounded, see-through dark panel that other things are drawn on.
void drawPanel(Rectangle area) {
    const float roundness = 20.0f / std::min(area.width, area.height);  // about a 10-pixel corner
    DrawRectangleRounded(area, roundness, 8, kPanel);
    DrawRectangleRoundedLinesEx(area, roundness, 8, 2.0f, kPanelEdge);
}

/// Text with a soft drop shadow so it stays readable over any background.
void drawText(const char* text, float x, float y, int size, Color color) {
    const int ix = static_cast<int>(x);
    const int iy = static_cast<int>(y);
    DrawText(text, ix + 2, iy + 2, size, Fade(BLACK, 0.55f * static_cast<float>(color.a) / 255.0f));
    DrawText(text, ix, iy, size, color);
}

/// Like `drawText`, but centred horizontally on `centerX`.
void drawTextCentered(const char* text, float centerX, float y, int size, Color color) {
    drawText(text, centerX - static_cast<float>(MeasureText(text, size)) / 2.0f, y, size, color);
}

/// A horizontal bar filled from the left by `fraction` (0 to 1).
void drawBar(Rectangle area, float fraction, Color fill) {
    DrawRectangleRounded(area, 1.0f, 6, Fade(BLACK, 0.45f));
    const float width = area.width * std::clamp(fraction, 0.0f, 1.0f);
    if (width >= area.height) {  // too short a bar cannot be rounded cleanly
        DrawRectangleRounded({area.x, area.y, width, area.height}, 1.0f, 6, fill);
    }
}

/// The everyday name of a kind of food.
const char* foodName(FoodKind kind) {
    switch (kind) {
        case FoodKind::Crumb: return "a crumb";
        case FoodKind::Sugar: return "a sugar cube";
        case FoodKind::Berry: return "a berry";
    }
    return "food";
}

}  // namespace

Hud::Hud(const Sprites& sprites) : sprites_(sprites) {}

void Hud::draw(const World& world, const Texture2D* scentTexture) {
    noticeEvents(world);

    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());

    // Darkened corners focus the eye on the middle of the view.
    DrawTexturePro(sprites_.vignette, {0.0f, 0.0f, 128.0f, 128.0f}, {0.0f, 0.0f, width, height}, {0.0f, 0.0f}, 0.0f,
                   WHITE);
    // The view turns sickly green as the player ant's health drops.
    const float sickness = 1.0f - world.player().health;
    if (sickness > 0.01f) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Color{90, 200, 40, 255}, 0.45f * sickness));
    }

    drawAntBody(world);
    drawStatusPanel(world);
    drawClock(world);
    drawMiniMap(world, scentTexture);
    drawMessage();

    const char* controls = "W/S walk    A/D turn    P scent trails    R restart    Esc quit";
    const float controlsWidth = static_cast<float>(MeasureText(controls, 20)) + 28.0f;
    drawPanel({kMargin, height - kMargin - 36.0f, controlsWidth, 36.0f});
    drawText(controls, kMargin + 14.0f, height - kMargin - 28.0f, 20, kTextDim);

    drawOverlays(world);
}

void Hud::noticeEvents(const World& world) {
    const Ant& player = world.player();
    const bool carrying = player.carriedFood.has_value();
    const auto say = [&](const char* text) {
        message_ = text;
        messageTime_ = GetTime();
    };

    if (player.deliveries < lastDeliveries_) {
        message_.clear();  // the game was restarted
    } else if (player.deliveries > lastDeliveries_) {
        say(TextFormat("Delivered!  The colony has %d of %d", world.deliveredCount(),
                       static_cast<int>(world.foods().size())));
    } else if (carrying && !wasCarrying_) {
        say(TextFormat("Picked up %s - carry it home", foodName(world.foods()[*player.carriedFood].kind)));
    } else if (wasAlive_ && !player.alive) {
        say("Poisoned!");
    }

    wasCarrying_ = carrying;
    wasAlive_ = player.alive;
    lastDeliveries_ = player.deliveries;
}

void Hud::drawAntBody(const World& world) const {
    const Ant& player = world.player();
    if (!player.alive) {
        return;
    }
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    const float unit = height / 720.0f;  // everything here scales with the window
    const float middle = width / 2.0f;
    const float stride = player.distanceWalked * 9.0f;
    const float sway = (6.0f * std::sin(stride) + 3.0f * std::sin(now() * 1.5f)) * unit;

    // Antennae: two curved feelers reaching forward from above the eyes.
    for (int side = -1; side <= 1; side += 2) {
        const float s = static_cast<float>(side);
        const Vector2 root{middle + s * 170.0f * unit, -20.0f * unit};
        const Vector2 bend{middle + s * 265.0f * unit, 110.0f * unit};
        const Vector2 tip{middle + s * 335.0f * unit + sway, 150.0f * unit + s * sway * 0.5f};
        DrawSplineSegmentBezierQuadratic(root, bend, tip, 9.0f * unit, kInk);
        DrawCircleV(tip, 6.5f * unit, kInk);
    }

    // Carried food sits between the jaws; empty jaws rest lower, mostly out of view.
    const bool carrying = player.carriedFood.has_value();
    const float lift = (carrying ? 0.0f : 70.0f * unit) + 4.0f * unit * std::sin(stride * 2.0f);
    if (carrying) {
        const Texture2D& sprite = foodSprite(sprites_, world.foods()[*player.carriedFood].kind);
        const float size = 200.0f * unit;
        DrawTexturePro(sprite, {0.0f, 0.0f, static_cast<float>(sprite.width), static_cast<float>(sprite.height)},
                       {middle - size / 2.0f, height - size * 0.78f + lift, size, size}, {0.0f, 0.0f}, 0.0f, WHITE);
    }
    for (int side = -1; side <= 1; side += 2) {
        const float s = static_cast<float>(side);
        const Vector2 root{middle + s * 210.0f * unit, height + 20.0f * unit + lift};
        const Vector2 bend{middle + s * 175.0f * unit, height - 150.0f * unit + lift};
        const Vector2 tip{middle + s * 62.0f * unit, height - 118.0f * unit + lift};
        DrawSplineSegmentBezierQuadratic(root, bend, tip, 18.0f * unit, kInk);
        DrawCircleV(tip, 9.0f * unit, kInk);
    }
}

void Hud::drawStatusPanel(const World& world) const {
    const Ant& player = world.player();
    const int total = static_cast<int>(world.foods().size());
    const Rectangle panel{kMargin, kMargin, 320.0f, 150.0f};
    const float left = panel.x + 16.0f;
    const float barWidth = panel.width - 32.0f;
    drawPanel(panel);

    drawText("COLONY FOOD", left, panel.y + 14.0f, 20, kTextDim);
    const char* count = TextFormat("%d / %d", world.deliveredCount(), total);
    drawText(count, panel.x + panel.width - 16.0f - static_cast<float>(MeasureText(count, 30)), panel.y + 8.0f, 30,
             kText);
    drawBar({left, panel.y + 46.0f, barWidth, 12.0f},
            total > 0 ? static_cast<float>(world.deliveredCount()) / static_cast<float>(total) : 0.0f, kWarn);

    int alive = 0;
    for (const Ant& ant : world.ants()) {
        alive += ant.alive ? 1 : 0;
    }
    drawText(TextFormat("You carried %d", player.deliveries), left, panel.y + 70.0f, 20, kText);
    const char* ants = TextFormat("Ants %d", alive);
    drawText(ants, panel.x + panel.width - 16.0f - static_cast<float>(MeasureText(ants, 20)), panel.y + 70.0f, 20,
             kTextDim);

    const Color healthColor = player.health > 0.6f ? kGood : (player.health > 0.3f ? kWarn : kBad);
    drawText("HEALTH", left, panel.y + 104.0f, 20, kTextDim);
    drawBar({left + 96.0f, panel.y + 108.0f, barWidth - 96.0f, 12.0f}, player.health, healthColor);
}

void Hud::drawClock(const World& world) const {
    const float middle = static_cast<float>(GetScreenWidth()) / 2.0f;
    const Rectangle panel{middle - 90.0f, kMargin, 180.0f, 64.0f};
    const int seconds = static_cast<int>(std::ceil(world.timeRemaining()));

    // The clock turns orange, then pulses red, as time runs short.
    Color color = kText;
    if (seconds <= 10) {
        color = Fade(kBad, 0.7f + 0.3f * std::sin(now() * 10.0f));
    } else if (seconds <= 30) {
        color = kWarn;
    }

    drawPanel(panel);
    drawTextCentered(TextFormat("%d:%02d", seconds / 60, seconds % 60), middle, panel.y + 8.0f, 30, color);
    drawBar({panel.x + 16.0f, panel.y + 44.0f, panel.width - 32.0f, 8.0f},
            world.timeRemaining() / world.level().timeLimit, color);
}

void Hud::drawMiniMap(const World& world, const Texture2D* scentTexture) const {
    const Level& level = world.level();
    const float mapWidth = 2.0f * level.areaHalfSize.x * kMapScale;
    const float mapHeight = 2.0f * level.areaHalfSize.y * kMapScale;
    const Rectangle panel{static_cast<float>(GetScreenWidth()) - kMargin - mapWidth - 16.0f, kMargin,
                          mapWidth + 16.0f, mapHeight + 16.0f};
    const Vector2 origin{panel.x + 8.0f, panel.y + 8.0f};

    // Converts a ground point to a mini-map pixel.
    const auto toMap = [&](Vec2 point) {
        return Vector2{origin.x + (point.x + level.areaHalfSize.x) * kMapScale,
                       origin.y + (point.y + level.areaHalfSize.y) * kMapScale};
    };
    const auto dot = [&](Vec2 point, float radius, Color color) {
        DrawCircleV(toMap(point), radius + 1.0f, kInk);
        DrawCircleV(toMap(point), radius, color);
    };

    drawPanel(panel);
    DrawRectangleV(origin, {mapWidth, mapHeight}, kGrassMap);
    const Vector2 blanketCorner = toMap(level.blanketHalfSize * -1.0f);
    const Vector2 blanketSize{2.0f * level.blanketHalfSize.x * kMapScale, 2.0f * level.blanketHalfSize.y * kMapScale};
    DrawRectangleV(blanketCorner, blanketSize, Color{226, 150, 150, 255});
    DrawRectangleLinesEx({blanketCorner.x, blanketCorner.y, blanketSize.x, blanketSize.y}, 2.0f, kClothRed);

    for (const Poison& poison : level.poisons) {
        DrawCircleV(toMap(poison.position), poison.radius * kMapScale, Color{110, 200, 50, 255});
    }
    if (scentTexture != nullptr) {
        DrawTexturePro(*scentTexture,
                       {0.0f, 0.0f, static_cast<float>(scentTexture->width), static_cast<float>(scentTexture->height)},
                       {origin.x, origin.y, mapWidth, mapHeight}, {0.0f, 0.0f}, 0.0f, WHITE);
    }
    DrawCircleV(toMap(level.nest.position), level.nest.radius * kMapScale, kNestDirt);
    for (std::size_t i = 0; i < level.obstacles.size(); ++i) {
        const Obstacle& obstacle = level.obstacles[i];
        DrawCircleV(toMap(obstacle.position), obstacle.radius * kMapScale, obstacleColor(obstacle.kind, i));
    }
    for (const Wall& wall : level.walls) {
        DrawLineEx(toMap(wall.start), toMap(wall.end), 3.0f, Color{150, 104, 58, 255});
    }
    for (const Food& food : world.foods()) {
        if (food.state == FoodState::OnGround) {
            dot(food.position, 2.5f, foodColor(food.kind));
        }
    }
    for (std::size_t i = 1; i < world.ants().size(); ++i) {
        if (world.ants()[i].alive) {
            DrawCircleV(toMap(world.ants()[i].position), 2.0f, kInk);
        }
    }

    // The player: an arrowhead pointing the way the ant faces.
    const Ant& player = world.player();
    const Vector2 center = toMap(player.position);
    const Vec2 facing = fromAngle(player.heading);
    const auto arrow = [&](float size, Color color) {
        const Vector2 tip{center.x + facing.x * size, center.y + facing.y * size};
        const Vector2 backLeft{center.x - facing.x * size * 0.7f + facing.y * size * 0.7f,
                               center.y - facing.y * size * 0.7f - facing.x * size * 0.7f};
        const Vector2 backRight{center.x - facing.x * size * 0.7f - facing.y * size * 0.7f,
                                center.y - facing.y * size * 0.7f + facing.x * size * 0.7f};
        DrawTriangle(tip, backLeft, backRight, color);
    };
    arrow(9.0f, kInk);
    arrow(6.5f, WHITE);
}

void Hud::drawMessage() const {
    const float age = static_cast<float>(GetTime() - messageTime_);
    if (message_.empty() || age > kMessageSeconds) {
        return;
    }
    // The message drifts upward and fades out over its last second.
    const float fade = std::clamp(kMessageSeconds - age, 0.0f, 1.0f);
    const float middle = static_cast<float>(GetScreenWidth()) / 2.0f;
    drawTextCentered(message_.c_str(), middle, 104.0f - 10.0f * age, 30, Fade(Color{255, 232, 120, 255}, fade));
}

void Hud::drawOverlays(const World& world) const {
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    const float middle = width / 2.0f;
    const Ant& player = world.player();

    if (world.status() == GameStatus::Playing) {
        if (!player.alive) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Color{20, 50, 10, 255}, 0.55f));
            drawTextCentered("POISONED", middle, height / 2.0f - 60.0f, 60, kGood);
            drawTextCentered(TextFormat("A new ant hatches in %d...", static_cast<int>(std::ceil(player.respawnTimer))),
                             middle, height / 2.0f + 16.0f, 30, kText);
        }
        return;
    }

    const bool won = world.status() == GameStatus::Won;
    const int total = static_cast<int>(world.foods().size());
    const int secondsLeft = static_cast<int>(std::ceil(world.timeRemaining()));  // matches the clock
    const Rectangle card{middle - 330.0f, height / 2.0f - 130.0f, 660.0f, 260.0f};

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
    drawPanel(card);
    drawTextCentered(won ? "THE COLONY FEASTS!" : "OUT OF TIME", middle, card.y + 34.0f, 50, won ? kGood : kBad);
    drawTextCentered(won ? TextFormat("All %d pieces home with %d:%02d to spare", total, secondsLeft / 60,
                                      secondsLeft % 60)
                         : TextFormat("%d of %d pieces made it home", world.deliveredCount(), total),
                     middle, card.y + 112.0f, 30, kText);
    drawTextCentered(TextFormat("You carried %d of them yourself", player.deliveries), middle, card.y + 154.0f, 20,
                     kTextDim);
    drawTextCentered("Press R to play again", middle, card.y + 204.0f, 20,
                     Fade(kText, 0.6f + 0.4f * std::sin(now() * 4.0f)));
}

}  // namespace ant
