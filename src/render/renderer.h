/// @file renderer.h
/// @brief Draws a `World` from the ant's point of view, plus the on-screen HUD.
///
/// The renderer only ever *reads* the world. All game rules live in the core
/// library; deleting this file would not change how the game plays.

#pragma once

#include "core/world.h"
#include "raylib.h"

namespace ant {

/// Owns the textures and draws one frame at a time.
/// Must be created after the window is open and destroyed before it closes.
class Renderer {
public:
    /// Creates all textures. `level` supplies the table size for the tablecloth.
    explicit Renderer(const Level& level);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /// Draws the 3D scene and the HUD. Call between raylib's
    /// `BeginDrawing()` and `EndDrawing()`.
    void draw(const World& world) const;

private:
    /// The first-person 3D view: table, obstacles, food and nest.
    void drawScene(const World& world) const;
    /// Score, clock, carried food, mini-map and win/lose banner.
    void drawHud(const World& world) const;
    /// A small top-down map in the corner of the screen.
    void drawMiniMap(const World& world) const;
    /// The sprite texture for a kind of food.
    const Texture2D& foodSprite(FoodKind kind) const;

    Texture2D crumb_;
    Texture2D sugar_;
    Texture2D berry_;
    Texture2D anthill_;
    Model tablecloth_;  ///< A flat textured rectangle; owns the tablecloth texture.
};

}  // namespace ant
