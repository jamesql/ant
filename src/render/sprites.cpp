/// @file sprites.cpp
/// @brief Texture painting.
///
/// Object sprites (food, anthill) are 64x64 pixel art on a transparent
/// background, with a dark outline, drawn so the object rests on the bottom
/// edge. Scenery and effect textures are smooth instead of pixelated.

#include "render/sprites.h"

#include <algorithm>
#include <cmath>

#include "render/palette.h"

namespace ant {
namespace {

/// Width and height of the object sprites, in pixels.
constexpr int kSize = 64;

/// A blank, fully transparent square canvas.
Image blankCanvas(int size = kSize) { return GenImageColor(size, size, BLANK); }

/// Direct access to a canvas's pixels (row by row, left to right).
Color* pixels(Image& image) { return static_cast<Color*>(image.data); }

/// Blends two colours: `t = 0` gives `a`, `t = 1` gives `b`.
Color mix(Color a, Color b, float t) {
    const auto channel = [t](unsigned char from, unsigned char to) {
        return static_cast<unsigned char>(static_cast<float>(from) + (static_cast<float>(to) - static_cast<float>(from)) * t);
    };
    return {channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), channel(a.a, b.a)};
}

/// Draws a one-pixel dark outline around everything painted so far, which
/// makes sprites read clearly against any background.
void addOutline(Image& image) {
    Color* original = LoadImageColors(image);
    const auto solid = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < image.width && y < image.height && original[y * image.width + x].a > 0;
    };
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const bool touchesShape = solid(x - 1, y) || solid(x + 1, y) || solid(x, y - 1) || solid(x, y + 1);
            if (!solid(x, y) && touchesShape) {
                ImageDrawPixel(&image, x, y, kInk);
            }
        }
    }
    UnloadImageColors(original);
}

/// How a finished picture should be sampled when drawn larger or smaller.
enum class Sampling {
    Pixelated,  ///< Hard pixel edges: the pixel-art look for object sprites.
    Smooth,     ///< Blended: for scenery and soft effects.
    Detailed,   ///< Blended with mipmaps: for large surfaces seen at a shallow angle.
};

/// Uploads a finished picture to the graphics card and frees the CPU copy.
Texture2D finish(Image image, Sampling sampling) {
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    switch (sampling) {
        case Sampling::Pixelated:
            SetTextureFilter(texture, TEXTURE_FILTER_POINT);
            break;
        case Sampling::Smooth:
            SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
            break;
        case Sampling::Detailed:
            // Mipmaps are pre-shrunk copies; they stop distant detail from shimmering.
            GenTextureMipmaps(&texture);
            SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_8X);
            break;
    }
    return texture;
}

Texture2D makeCrumb() {
    Image image = blankCanvas();
    const Color crust{186, 128, 62, 255};
    const Color body{226, 176, 104, 255};
    const Color light{244, 208, 146, 255};
    const Color speck{142, 92, 44, 255};
    // An irregular blob built from overlapping circles: crust, then body, then highlight.
    ImageDrawCircle(&image, 30, 42, 19, crust);
    ImageDrawCircle(&image, 43, 46, 14, crust);
    ImageDrawCircle(&image, 19, 48, 12, crust);
    ImageDrawCircle(&image, 29, 40, 15, body);
    ImageDrawCircle(&image, 41, 45, 10, body);
    ImageDrawCircle(&image, 20, 47, 8, body);
    ImageDrawCircle(&image, 25, 35, 6, light);
    ImageDrawCircle(&image, 34, 44, 2, speck);
    ImageDrawCircle(&image, 44, 49, 2, speck);
    ImageDrawCircle(&image, 20, 50, 2, speck);
    addOutline(image);
    return finish(image, Sampling::Pixelated);
}

Texture2D makeSugar() {
    Image image = blankCanvas();
    const Color top{255, 255, 255, 255};
    const Color front{232, 238, 248, 255};
    const Color side{196, 208, 228, 255};
    const Color sparkle{255, 255, 255, 255};
    const int slant = 12;  // how far the top face leans back
    // A cube seen from above and to the left: three faces in three shades.
    ImageDrawRectangle(&image, 8, 26, 34, 34, front);
    for (int i = 0; i < slant; ++i) {
        ImageDrawRectangle(&image, 8 + slant - i, 14 + i, 34, 1, top);    // top face, one row at a time
        ImageDrawRectangle(&image, 42 + i, 26 - i, 1, 34, side);          // side face, one column at a time
    }
    ImageDrawRectangle(&image, 14, 32, 3, 3, sparkle);
    ImageDrawRectangle(&image, 30, 44, 3, 3, sparkle);
    ImageDrawRectangle(&image, 20, 52, 2, 2, sparkle);
    ImageDrawRectangle(&image, 8, 58, 34, 2, side);  // a hint of shade along the bottom
    addOutline(image);
    return finish(image, Sampling::Pixelated);
}

Texture2D makeBerry() {
    Image image = blankCanvas();
    const Color dark{150, 18, 44, 255};
    const Color skin{214, 40, 60, 255};
    const Color glow{240, 86, 96, 255};
    const Color shine{255, 214, 218, 255};
    const Color leaf{58, 150, 66, 255};
    const Color leafDark{38, 112, 52, 255};
    ImageDrawCircle(&image, 32, 41, 20, dark);
    ImageDrawCircle(&image, 30, 39, 18, skin);
    ImageDrawCircle(&image, 27, 35, 11, glow);
    ImageDrawCircle(&image, 23, 31, 4, shine);
    ImageDrawRectangle(&image, 31, 9, 3, 12, leafDark);  // stalk
    ImageDrawCircle(&image, 24, 20, 6, leafDark);
    ImageDrawCircle(&image, 40, 19, 7, leaf);
    ImageDrawCircle(&image, 25, 19, 4, leaf);
    addOutline(image);
    return finish(image, Sampling::Pixelated);
}

Texture2D makeAnthill() {
    Image image = blankCanvas();
    const Color dirt{150, 104, 62, 255};
    const Color dirtLight{178, 130, 82, 255};
    const Color dirtDark{112, 76, 44, 255};
    const Color hole{34, 22, 14, 255};
    // A mound: each pixel row is a strip whose width follows half an ellipse.
    const int top = 12;
    const int base = kSize - 2;
    for (int y = top; y < base; ++y) {
        const float t = static_cast<float>(y - top) / static_cast<float>(base - top);  // 0 at peak, 1 at base
        const int halfWidth = static_cast<int>(29.0f * std::sqrt(1.0f - (1.0f - t) * (1.0f - t)));
        const int lightWidth = halfWidth * 2 / 5;
        ImageDrawRectangle(&image, 32 - halfWidth, y, 2 * halfWidth, 1, y % 7 == 0 ? dirtDark : dirt);
        ImageDrawRectangle(&image, 32 - halfWidth + 2, y, lightWidth, 1, dirtLight);  // sunlit left flank
    }
    ImageDrawCircle(&image, 34, 22, 7, dirtDark);  // rim of the entrance
    ImageDrawCircle(&image, 34, 23, 5, hole);
    ImageDrawCircle(&image, 16, 54, 2, dirtDark);  // pebbles
    ImageDrawCircle(&image, 46, 48, 2, dirtDark);
    ImageDrawCircle(&image, 52, 57, 2, dirtLight);
    addOutline(image);
    return finish(image, Sampling::Pixelated);
}

Texture2D makeTree() {
    Image image = blankCanvas(128);
    const Color trunk{104, 82, 66, 255};
    const Color shade{66, 112, 84, 255};
    const Color leaves{92, 142, 98, 255};
    const Color sunlit{128, 172, 118, 255};
    ImageDrawRectangle(&image, 57, 78, 14, 50, trunk);
    ImageDrawCircle(&image, 64, 54, 42, shade);
    ImageDrawCircle(&image, 40, 66, 26, shade);
    ImageDrawCircle(&image, 90, 64, 28, shade);
    ImageDrawCircle(&image, 58, 46, 32, leaves);
    ImageDrawCircle(&image, 84, 54, 22, leaves);
    ImageDrawCircle(&image, 48, 36, 18, sunlit);
    ImageDrawCircle(&image, 70, 30, 12, sunlit);
    return finish(image, Sampling::Smooth);
}

Texture2D makeCloud() {
    Image image = GenImageColor(256, 128, BLANK);
    const Color shade{226, 234, 244, 235};
    const Color white{255, 255, 255, 240};
    ImageDrawCircle(&image, 60, 86, 30, shade);
    ImageDrawCircle(&image, 118, 72, 46, shade);
    ImageDrawCircle(&image, 176, 82, 38, shade);
    ImageDrawCircle(&image, 214, 94, 24, shade);
    ImageDrawCircle(&image, 62, 80, 28, white);
    ImageDrawCircle(&image, 116, 62, 44, white);
    ImageDrawCircle(&image, 172, 74, 36, white);
    ImageDrawCircle(&image, 212, 90, 22, white);
    ImageDrawRectangle(&image, 0, 108, 256, 20, BLANK);  // flat underside
    return finish(image, Sampling::Smooth);
}

/// Paints a square picture of one colour whose opacity depends only on the
/// distance from the centre. `opacityAt` receives that distance (0 at the
/// centre, 1 at the middle of an edge, about 1.41 in a corner) and returns
/// an opacity from 0 to 1.
template <typename OpacityAt>
Texture2D makeRadial(Color color, OpacityAt opacityAt) {
    const int size = 128;
    Image image = blankCanvas(size);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) / (size / 2.0f) - 1.0f;
            const float dy = (static_cast<float>(y) + 0.5f) / (size / 2.0f) - 1.0f;
            const float opacity = std::clamp(opacityAt(std::sqrt(dx * dx + dy * dy)), 0.0f, 1.0f);
            color.a = static_cast<unsigned char>(opacity * 255.0f);
            pixels(image)[y * size + x] = color;
        }
    }
    return finish(image, Sampling::Smooth);
}

Texture2D makeShadow() {
    // Darkest in the middle, fading smoothly to nothing at the rim.
    return makeRadial(Color{20, 14, 30, 0}, [](float d) {
        const float fade = std::clamp(1.0f - d, 0.0f, 1.0f);
        return 0.5f * fade * fade * (3.0f - 2.0f * fade);
    });
}

Texture2D makeVignette() {
    // Clear in the middle, darkening toward the corners of the screen.
    return makeRadial(Color{8, 14, 24, 0}, [](float d) {
        const float edge = std::clamp((d - 0.7f) / 0.7f, 0.0f, 1.0f);
        return 0.5f * edge * edge;
    });
}

Texture2D makePuddle() {
    // Solid almost to the rim, then a short soft edge.
    return makeRadial(WHITE, [](float d) { return (1.0f - d) / 0.12f; });
}

Texture2D makeWicker() {
    // Woven cane: horizontal strands pass alternately over and under
    // vertical stakes, shifting by one stake on every row.
    const int size = 64;
    const int strand = 8;  // height of one strand, and width of one stake, in pixels
    const Color over{206, 160, 98, 255};
    const Color under{160, 116, 66, 255};
    const Color gap{104, 72, 40, 255};
    Image image = blankCanvas(size);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool onTop = (x / strand + y / strand) % 2 == 0;
            const bool edge = y % strand == 0 || x % strand == 0;
            pixels(image)[y * size + x] = edge ? gap : (onTop ? over : under);
        }
    }
    return finish(image, Sampling::Detailed);
}

Texture2D makeBlanket(float widthUnits, float lengthUnits) {
    // Gingham: red stripes run both ways. Where two stripes cross the cloth
    // is deep red, where one runs it is pink, elsewhere it is cream.
    const int pixelsPerUnit = 32;
    const int width = static_cast<int>(widthUnits) * pixelsPerUnit;
    const int height = static_cast<int>(lengthUnits) * pixelsPerUnit;
    const Color pink = mix(kClothCream, kClothRed, 0.5f);
    Image image = GenImageColor(width, height, kClothCream);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int stripes = (x / pixelsPerUnit) % 2 + (y / pixelsPerUnit) % 2;
            Color color = stripes == 2 ? kClothRed : (stripes == 1 ? pink : kClothCream);
            if ((x + y) % 4 == 0 || (x - y) % 4 == 0) {
                color = mix(color, BLACK, 0.05f);  // faint diagonal weave of the fabric
            }
            pixels(image)[y * width + x] = color;
        }
    }
    return finish(image, Sampling::Detailed);
}

Texture2D makeGrass() {
    // Cloud-like noise turned into patches of lighter and darker green.
    Image image = GenImagePerlinNoise(512, 512, 0, 0, 10.0f);
    const Color dark{74, 138, 66, 255};
    const Color light{122, 178, 84, 255};
    for (int i = 0; i < image.width * image.height; ++i) {
        pixels(image)[i] = mix(dark, light, static_cast<float>(pixels(image)[i].r) / 255.0f);
    }
    return finish(image, Sampling::Detailed);
}

}  // namespace

Sprites loadSprites(float blanketWidth, float blanketLength) {
    Sprites sprites;
    sprites.crumb = makeCrumb();
    sprites.sugar = makeSugar();
    sprites.berry = makeBerry();
    sprites.anthill = makeAnthill();
    sprites.tree = makeTree();
    sprites.cloud = makeCloud();
    sprites.shadow = makeShadow();
    sprites.vignette = makeVignette();
    sprites.puddle = makePuddle();
    sprites.wicker = makeWicker();
    sprites.blanket = makeBlanket(blanketWidth, blanketLength);
    sprites.grass = makeGrass();
    return sprites;
}

void unloadSprites(const Sprites& sprites) {
    for (const Texture2D& texture : {sprites.crumb, sprites.sugar, sprites.berry, sprites.anthill, sprites.tree,
                                     sprites.cloud, sprites.shadow, sprites.vignette, sprites.puddle, sprites.wicker, sprites.blanket,
                                     sprites.grass}) {
        UnloadTexture(texture);
    }
}

const Texture2D& foodSprite(const Sprites& sprites, FoodKind kind) {
    switch (kind) {
        case FoodKind::Crumb: return sprites.crumb;
        case FoodKind::Sugar: return sprites.sugar;
        case FoodKind::Berry: return sprites.berry;
    }
    return sprites.crumb;
}

}  // namespace ant
