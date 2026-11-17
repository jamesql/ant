/// @file vec2.h
/// @brief Minimal 2D vector math used by the simulation.
///
/// The ants walk on flat ground, so all game logic is two-dimensional.
/// `Vec2::x` runs along the blanket's length and `Vec2::y` along its width.
/// (The renderer maps `Vec2::y` onto the 3D world's Z axis.)

#pragma once

#include <algorithm>
#include <cmath>

namespace ant {

/// The constant pi, as a float.
inline constexpr float kPi = 3.14159265358979323846f;

/// A point or direction on the ground.
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

/// Component-wise addition.
constexpr Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }

/// Component-wise subtraction.
constexpr Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }

/// Scales a vector by a number.
constexpr Vec2 operator*(Vec2 v, float s) { return {v.x * s, v.y * s}; }

/// Dot product: how much two vectors point the same way.
constexpr float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }

/// Length (magnitude) of a vector.
inline float length(Vec2 v) { return std::sqrt(dot(v, v)); }

/// Straight-line distance between two points.
inline float distance(Vec2 a, Vec2 b) { return length(a - b); }

/// Unit-length direction for a heading angle in radians.
/// Angle 0 points along +x; increasing angles turn toward +y (a right turn
/// from an ant's point of view).
inline Vec2 fromAngle(float radians) { return {std::cos(radians), std::sin(radians)}; }

/// The heading angle (radians) of a direction; the inverse of `fromAngle`.
inline float toAngle(Vec2 direction) { return std::atan2(direction.y, direction.x); }

/// Wraps any angle into the range [-pi, pi] so headings never grow without bound.
inline float wrapAngle(float radians) { return std::remainder(radians, 2.0f * kPi); }

/// The point on the straight segment from `a` to `b` that is nearest to `point`.
inline Vec2 closestPointOnSegment(Vec2 point, Vec2 a, Vec2 b) {
    const Vec2 along = b - a;
    const float lengthSquared = dot(along, along);
    if (lengthSquared <= 0.0f) {
        return a;  // the segment is a single point
    }
    const float t = std::clamp(dot(point - a, along) / lengthSquared, 0.0f, 1.0f);
    return a + along * t;
}

}  // namespace ant
