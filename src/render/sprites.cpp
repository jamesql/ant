/// @file sprites.cpp
/// @brief Sprite painting. Each sprite is a 64x64 picture on a transparent
///        background, drawn so the object rests on the bottom edge.

#include "render/sprites.h"

#include <cmath>

namespace ant {
namespace {

/// Width and height of every sprite, in pixels.
constexpr int kSize = 64;

/// Uploads a finished picture to the graphics card and frees the CPU copy.
/// Point filtering keeps the chunky pixel-art look when sprites are close up.
Texture2D finish(Image image) {
    const Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    return texture;
}

/// A blank, fully transparent canvas.
Image blankCanvas() { return GenImageColor(kSize, kSize, BLANK); }

}  // namespace

Texture2D makeCrumbSprite() {
    Image image = blankCanvas();
    const Color crust{196, 140, 72, 255};
    const Color crumb{232, 190, 120, 255};
    const Color speck{150, 100, 50, 255};
    // An irregular blob built from overlapping circles.
    ImageDrawCircle(&image, 30, 42, 20, crust);
    ImageDrawCircle(&image, 42, 46, 15, crust);
    ImageDrawCircle(&image, 20, 48, 13, crust);
    ImageDrawCircle(&image, 30, 40, 15, crumb);
    ImageDrawCircle(&image, 40, 46, 10, crumb);
    ImageDrawCircle(&image, 24, 36, 2, speck);
    ImageDrawCircle(&image, 36, 44, 2, speck);
    ImageDrawCircle(&image, 44, 50, 2, speck);
    return finish(image);
}

Texture2D makeSugarSprite() {
    Image image = blankCanvas();
    const Color shade{190, 205, 225, 255};
    const Color face{250, 250, 255, 255};
    const Color sparkle{215, 228, 245, 255};
    // A cube seen slightly from above: a shaded back face and a bright front face.
    ImageDrawRectangle(&image, 18, 18, 36, 36, shade);
    ImageDrawRectangle(&image, 10, 26, 36, 36, face);
    ImageDrawRectangleLines(&image, Rectangle{10, 26, 36, 36}, 2, shade);
    ImageDrawRectangle(&image, 18, 34, 4, 4, sparkle);
    ImageDrawRectangle(&image, 32, 46, 4, 4, sparkle);
    return finish(image);
}

Texture2D makeBerrySprite() {
    Image image = blankCanvas();
    const Color skin{200, 30, 50, 255};
    const Color shadow{150, 15, 40, 255};
    const Color shine{255, 170, 180, 255};
    const Color leaf{50, 150, 60, 255};
    ImageDrawCircle(&image, 32, 40, 22, shadow);
    ImageDrawCircle(&image, 30, 38, 20, skin);
    ImageDrawCircle(&image, 23, 30, 5, shine);
    ImageDrawRectangle(&image, 30, 8, 4, 12, leaf);
    ImageDrawCircle(&image, 40, 14, 6, leaf);
    return finish(image);
}

Texture2D makeAnthillSprite() {
    Image image = blankCanvas();
    const Color dirt{140, 95, 55, 255};
    const Color darkDirt{105, 70, 40, 255};
    const Color hole{35, 22, 12, 255};
    // A mound: each pixel row is a horizontal strip whose width follows half an ellipse.
    const int top = 14;
    for (int y = top; y < kSize; ++y) {
        const float t = static_cast<float>(y - top) / static_cast<float>(kSize - top);  // 0 at peak, 1 at base
        const int halfWidth = static_cast<int>(30.0f * std::sqrt(1.0f - (1.0f - t) * (1.0f - t)));
        ImageDrawRectangle(&image, 32 - halfWidth, y, 2 * halfWidth, 1, y % 6 < 2 ? darkDirt : dirt);
    }
    ImageDrawCircle(&image, 32, 22, 6, hole);  // the entrance
    return finish(image);
}

Texture2D makeTableclothTexture(float widthUnits, float lengthUnits) {
    const int pixelsPerUnit = 16;
    const int checkPixels = pixelsPerUnit;  // each check is 1x1 world unit
    const Image image = GenImageChecked(static_cast<int>(widthUnits) * pixelsPerUnit,
                                        static_cast<int>(lengthUnits) * pixelsPerUnit,
                                        checkPixels, checkPixels,
                                        Color{205, 45, 50, 255}, Color{245, 240, 232, 255});
    return finish(image);
}

}  // namespace ant
