/// @file lighting.h
/// @brief The GPU shaders: sunlight with distance haze, and sprite cut-outs.
///
/// A *shader* is a tiny program the graphics card runs for every pixel.
///
/// The **lighting shader** does two things:
///   - **Sunlight**: surfaces facing the sun are brighter than those facing away,
///     which is what makes a cylinder look round instead of flat.
///   - **Haze (fog)**: far-away surfaces fade toward the horizon colour, which
///     gives a sense of depth and hides the edge of the world.
///
/// The **cut-out shader** is for sprites: it throws away their transparent
/// pixels completely, so a sprite never hides what is behind its empty corners.

#pragma once

#include "core/vec2.h"
#include "raylib.h"

namespace ant {

/// Direction on the ground in which shadows fall (unit length).
inline constexpr Vec2 kShadowDirection{0.7474f, 0.6644f};

/// How long a shadow is, per unit of the object's height.
inline constexpr float kShadowLengthPerHeight = 0.75f;

/// The lighting shader plus the addresses of the values we change while drawing.
struct LightingShader {
    Shader shader;
    int cameraPositionLocation;  ///< Where the camera is (needed for the haze).
    int textureScaleLocation;    ///< How many times a surface's texture repeats across it.
};

/// Compiles the lighting shader and sets its fixed values (sun direction and
/// fog colour). Release `shader` with raylib's `UnloadShader`.
LightingShader loadLightingShader();

/// Tells the lighting shader where the camera is. Call once per frame.
void setCameraPosition(const LightingShader& lighting, Vector3 cameraPosition);

/// Sets how many times the texture repeats across the next surfaces drawn.
void setTextureScale(const LightingShader& lighting, Vector2 repeats);

/// Compiles the sprite cut-out shader. Release it with raylib's `UnloadShader`.
Shader loadCutoutShader();

}  // namespace ant
