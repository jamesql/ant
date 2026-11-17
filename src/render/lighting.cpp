/// @file lighting.cpp
/// @brief Source code and set-up of the shaders.

#include "render/lighting.h"

#include "render/palette.h"

namespace ant {
namespace {

/// Runs once per corner (vertex) of every triangle: works out where the
/// corner is in the world and on screen, and passes its facing direction on.
constexpr const char* kLightingVertex = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;

uniform mat4 mvp;           // model space -> screen
uniform mat4 matModel;      // model space -> world
uniform mat4 matNormal;     // rotates facing directions into the world
uniform vec2 textureScale;  // how many times the texture repeats

out vec3 worldPosition;
out vec2 texCoord;
out vec3 worldNormal;

void main() {
    worldPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    texCoord = vertexTexCoord * textureScale;
    worldNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

/// Runs once per pixel: surface colour x light, then blended toward the haze.
constexpr const char* kLightingFragment = R"(
#version 330
in vec3 worldPosition;
in vec2 texCoord;
in vec3 worldNormal;

uniform sampler2D texture0;  // the surface's picture
uniform vec4 colDiffuse;     // the surface's tint
uniform vec3 cameraPosition;
uniform vec3 toSun;          // unit direction from the surface toward the sun
uniform vec3 fogColor;

out vec4 finalColor;

const vec3 skyLight = vec3(0.60, 0.66, 0.78);  // soft light from the whole sky
const vec3 sunLight = vec3(0.62, 0.55, 0.42);  // warm direct sunlight
const float fogDensity = 0.006;

void main() {
    vec4 surface = texture(texture0, texCoord) * colDiffuse;
    vec3 normal = normalize(worldNormal);

    // How directly the surface faces the sun: 1 = straight at it, 0 = side-on or away.
    float facingSun = max(dot(normal, toSun), 0.0);
    // Surfaces facing up catch more sky light than those facing sideways.
    float facingSky = 0.75 + 0.25 * normal.y;
    vec3 lit = surface.rgb * (skyLight * facingSky + sunLight * facingSun);

    float distanceToCamera = length(cameraPosition - worldPosition);
    float fog = 1.0 - exp(-pow(distanceToCamera * fogDensity, 2.0));
    finalColor = vec4(mix(lit, fogColor, fog), surface.a);
}
)";

/// Runs once per sprite pixel: draws solid pixels, discards see-through ones.
constexpr const char* kCutoutFragment = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

out vec4 finalColor;

void main() {
    vec4 color = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    if (color.a < 0.4) discard;
    finalColor = color;
}
)";

}  // namespace

LightingShader loadLightingShader() {
    LightingShader lighting;
    lighting.shader = LoadShaderFromMemory(kLightingVertex, kLightingFragment);
    lighting.cameraPositionLocation = GetShaderLocation(lighting.shader, "cameraPosition");
    lighting.textureScaleLocation = GetShaderLocation(lighting.shader, "textureScale");

    // The sun sits high, opposite the direction shadows fall.
    const float toSun[3] = {-0.4497f, 0.8000f, -0.3970f};
    const float fog[3] = {kHorizon.r / 255.0f, kHorizon.g / 255.0f, kHorizon.b / 255.0f};
    SetShaderValue(lighting.shader, GetShaderLocation(lighting.shader, "toSun"), toSun, SHADER_UNIFORM_VEC3);
    SetShaderValue(lighting.shader, GetShaderLocation(lighting.shader, "fogColor"), fog, SHADER_UNIFORM_VEC3);
    setTextureScale(lighting, {1.0f, 1.0f});
    return lighting;
}

void setCameraPosition(const LightingShader& lighting, Vector3 cameraPosition) {
    SetShaderValue(lighting.shader, lighting.cameraPositionLocation, &cameraPosition, SHADER_UNIFORM_VEC3);
}

void setTextureScale(const LightingShader& lighting, Vector2 repeats) {
    SetShaderValue(lighting.shader, lighting.textureScaleLocation, &repeats, SHADER_UNIFORM_VEC2);
}

Shader loadCutoutShader() {
    return LoadShaderFromMemory(nullptr, kCutoutFragment);  // null = raylib's standard vertex shader
}

}  // namespace ant
