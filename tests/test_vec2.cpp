/// @file test_vec2.cpp
/// @brief Tests for the 2D vector helpers.

#include "core/vec2.h"
#include "test_framework.h"

using namespace ant;

namespace {
constexpr float kTolerance = 1e-5f;
}

TEST(vec2_arithmetic_is_component_wise) {
    const Vec2 sum = Vec2{1.0f, 2.0f} + Vec2{3.0f, 5.0f};
    const Vec2 scaled = Vec2{1.0f, -2.0f} * 3.0f;
    CHECK(sum.x == 4.0f && sum.y == 7.0f);
    CHECK(scaled.x == 3.0f && scaled.y == -6.0f);
}

TEST(vec2_length_and_distance_use_pythagoras) {
    CHECK_NEAR(length({3.0f, 4.0f}), 5.0f, kTolerance);
    CHECK_NEAR(distance({1.0f, 1.0f}, {4.0f, 5.0f}), 5.0f, kTolerance);
}

TEST(vec2_from_angle_gives_unit_directions) {
    const Vec2 ahead = fromAngle(0.0f);
    const Vec2 right = fromAngle(kPi / 2.0f);
    CHECK_NEAR(ahead.x, 1.0f, kTolerance);
    CHECK_NEAR(ahead.y, 0.0f, kTolerance);
    CHECK_NEAR(right.x, 0.0f, kTolerance);
    CHECK_NEAR(right.y, 1.0f, kTolerance);
}

TEST(vec2_wrap_angle_stays_within_half_turn) {
    CHECK_NEAR(wrapAngle(2.0f * kPi + 0.5f), 0.5f, kTolerance);
    CHECK_NEAR(wrapAngle(-2.0f * kPi - 0.5f), -0.5f, kTolerance);
    CHECK_NEAR(wrapAngle(1.0f), 1.0f, kTolerance);
}
