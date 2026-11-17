/// @file sprites.h
/// @brief Procedurally drawn sprite textures (no image files needed).
///
/// Every function paints a small picture in memory and uploads it to the
/// graphics card. They must be called after the window exists, and the caller
/// owns the returned texture (release it with raylib's `UnloadTexture`).

#pragma once

#include "raylib.h"

namespace ant {

/// A tan bread crumb.
Texture2D makeCrumbSprite();

/// A white sugar cube.
Texture2D makeSugarSprite();

/// A red berry with a leaf.
Texture2D makeBerrySprite();

/// The anthill that marks the nest.
Texture2D makeAnthillSprite();

/// The red-and-white checked tablecloth.
/// @param widthUnits, lengthUnits Table size in world units, so the checks stay square.
Texture2D makeTableclothTexture(float widthUnits, float lengthUnits);

}  // namespace ant
