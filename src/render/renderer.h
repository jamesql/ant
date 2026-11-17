/// @file renderer.h
/// @brief Draws a `World` from the player ant's point of view, plus the HUD.
///
/// The renderer only ever *reads* the world. All game rules live in the core
/// library; deleting this folder would not change how the game plays.

#pragma once

#include <vector>

#include "core/world.h"
#include "raylib.h"
#include "render/hud.h"
#include "render/lighting.h"
#include "render/sprites.h"

namespace ant {

/// Owns every graphics resource and draws one frame at a time.
/// Must be created after the window is open and destroyed before it closes.
class Renderer {
public:
    /// Creates all textures, shapes and shaders for `world`'s level.
    explicit Renderer(const World& world);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /// Draws the sky, the 3D scene and the HUD. Call between raylib's
    /// `BeginDrawing()` and `EndDrawing()`.
    void draw(const World& world);

    /// Shows or hides the scent trails (in the world and on the mini-map).
    void toggleScentView() { showScent_ = !showScent_; }

private:
    /// A sprite to draw standing upright and facing the camera.
    struct Billboard {
        const Texture2D* texture;
        Vector3 center;
        float height;
    };

    /// Repaints the scent texture from the world's pheromone grid.
    void updateScentTexture(const World& world);

    void drawSky() const;
    /// Grass and blanket.
    void drawGround(const Level& level);
    /// Everything painted flat onto the ground: poison, scent trails, shadows.
    void drawGroundDecals(const World& world);
    void drawNest(const Nest& nest);
    void drawObstacle(const Obstacle& obstacle, std::size_t index);
    void drawBasket(const Level& level);
    void drawAnt(const Ant& ant);
    /// The see-through shells of the drinking glasses (drawn last).
    void drawGlassShells(const Level& level);
    /// Collects every sprite in the scene: scenery, anthill, food.
    std::vector<Billboard> collectBillboards(const World& world) const;

    /// Draws a shape with sunlight and haze. `texture` may be null for a plain colour.
    void drawLit(const Mesh& mesh, const Texture2D* texture, Color color, const Matrix& transform,
                 Vector2 textureRepeats = {1.0f, 1.0f});
    /// Draws a flat, unlit, see-through picture lying on the ground.
    void drawDecal(const Texture2D& texture, Color tint, Vec2 center, Vec2 lengthAxis, float length, float width,
                   float height);
    /// A lit upright cylinder standing on `base`.
    void drawCylinder(Vec2 base, float bottom, float radius, float height, Color color);
    /// A soft round shadow for something `height` tall standing at `base`.
    void drawShadow(Vec2 base, float radius, float height);

    Sprites sprites_;
    LightingShader lighting_;
    Shader cutout_;
    Material litMaterial_;   ///< Uses the lighting shader.
    Material flatMaterial_;  ///< Uses raylib's plain shader (no light, no haze).
    Texture2D white_;        ///< A plain white texture, for untextured shapes.
    Mesh plane_;             ///< 1 x 1 flat square, centred, facing up.
    Mesh cube_;              ///< 1 x 1 x 1 box, centred.
    Mesh cylinder_;          ///< Radius 1, height 1, standing on its base.
    Mesh cone_;              ///< Radius 1, height 1, standing on its base.
    Mesh sphere_;            ///< Radius 1, centred.
    Texture2D scentTexture_; ///< One pixel per pheromone grid cell, repainted every frame.
    std::vector<Color> scentPixels_;
    bool showScent_ = true;
    Camera3D camera_{};
    Hud hud_;
};

}  // namespace ant
