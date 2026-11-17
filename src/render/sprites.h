/// @file sprites.h
/// @brief Every picture the game uses, painted in code (no image files).
///
/// A *texture* is a picture stored on the graphics card. A *sprite* is a
/// texture drawn as a flat cut-out in the world or on the HUD.

#pragma once

#include "core/level.h"
#include "raylib.h"

namespace ant {

/// All textures used by the renderer and HUD.
struct Sprites {
    Texture2D crumb;       ///< Food: a bread crumb.
    Texture2D sugar;       ///< Food: a sugar cube.
    Texture2D berry;       ///< Food: a red berry.
    Texture2D anthill;     ///< The nest marker.
    Texture2D tree;        ///< Distant scenery.
    Texture2D cloud;       ///< Distant scenery.
    Texture2D shadow;      ///< A soft dark blob, drawn under objects.
    Texture2D puddle;      ///< A white disc with a soft rim; tinted green for poison.
    Texture2D wicker;      ///< Woven cane for the picnic basket.
    Texture2D vignette;    ///< Darkened screen corners, drawn over the 3D view.
    Texture2D blanket;     ///< The red-and-cream gingham picnic blanket.
    Texture2D grass;       ///< The lawn.
};

/// Paints every texture. Call once, after the window exists.
/// @param blanketWidth, blanketLength Blanket size in world units, so the gingham squares stay square.
Sprites loadSprites(float blanketWidth, float blanketLength);

/// Releases every texture in `sprites`.
void unloadSprites(const Sprites& sprites);

/// The sprite for a kind of food.
const Texture2D& foodSprite(const Sprites& sprites, FoodKind kind);

}  // namespace ant
