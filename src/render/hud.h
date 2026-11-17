/// @file hud.h
/// @brief The HUD ("heads-up display"): everything drawn flat on top of the
///        3D view - score, clock, health, mini-map, messages, and the player
///        ant's own antennae and jaws at the edges of the screen.

#pragma once

#include <string>

#include "core/world.h"
#include "raylib.h"
#include "render/sprites.h"

namespace ant {

/// Draws the 2D overlay and remembers the little it needs to animate messages.
class Hud {
public:
    /// `sprites` must outlive the HUD.
    explicit Hud(const Sprites& sprites);

    /// Draws the whole overlay for the current frame.
    /// @param scentTexture The scent-trail picture to show on the mini-map, or null to hide trails.
    void draw(const World& world, const Texture2D* scentTexture);

private:
    /// Compares the player ant with last frame and raises a message when
    /// something happened (picked up food, delivered it, died).
    void noticeEvents(const World& world);

    /// The antennae at the top of the view and the jaws (with any carried food) at the bottom.
    void drawAntBody(const World& world) const;
    /// Colony food, the player's own tally and health.
    void drawStatusPanel(const World& world) const;
    void drawClock(const World& world) const;
    void drawMiniMap(const World& world, const Texture2D* scentTexture) const;
    /// The short message that fades out under the clock.
    void drawMessage() const;
    /// Full-screen notices: poisoned, won, lost.
    void drawOverlays(const World& world) const;

    const Sprites& sprites_;
    bool wasCarrying_ = false;
    bool wasAlive_ = true;
    int lastDeliveries_ = 0;
    std::string message_;
    double messageTime_ = -100.0;  ///< When the current message appeared (seconds since start).
};

}  // namespace ant
