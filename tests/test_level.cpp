/// @file test_level.cpp
/// @brief Tests for the picnic level generator. Because every map is random,
///        most tests check a property across many seeds.

#include <queue>
#include <vector>

#include "core/level.h"
#include "test_framework.h"

using namespace ant;

namespace {

constexpr std::uint32_t kSeedsChecked = 100;
constexpr float kAntRadius = 0.3f;

/// True if `point` lies inside the basket's rectangle.
bool isInside(const Basket& basket, Vec2 point) {
    const Vec2 offset = point - basket.center;
    return std::fabs(offset.x) < basket.halfSize.x && std::fabs(offset.y) < basket.halfSize.y;
}

/// True if an ant could stand at `point`: on the map, not overlapping
/// anything solid, and not in poison.
bool canStand(const Level& level, Vec2 point) {
    if (std::fabs(point.x) > level.areaHalfSize.x - kAntRadius || std::fabs(point.y) > level.areaHalfSize.y - kAntRadius) {
        return false;
    }
    for (const Obstacle& obstacle : level.obstacles) {
        if (distance(point, obstacle.position) < obstacle.radius + kAntRadius) {
            return false;
        }
    }
    for (const Wall& wall : level.walls) {
        if (distance(point, closestPointOnSegment(point, wall.start, wall.end)) < wall.thickness + kAntRadius) {
            return false;
        }
    }
    for (const Poison& poison : level.poisons) {
        if (distance(point, poison.position) < poison.radius) {
            return false;
        }
    }
    return true;
}

/// How many pieces of food an ant could walk to from the nest without
/// passing through anything solid or poisonous.
///
/// Works by "flood fill": lay a fine grid over the map, start at the nest,
/// and keep spreading to neighbouring grid points an ant could stand on.
/// Food counts as reachable if the flood got close enough to touch it.
int countReachableFood(const Level& level) {
    const float cell = 0.2f;
    const int columns = static_cast<int>(2.0f * level.areaHalfSize.x / cell);
    const int rows = static_cast<int>(2.0f * level.areaHalfSize.y / cell);
    const auto pointAt = [&](int column, int row) {
        return Vec2{(static_cast<float>(column) + 0.5f) * cell - level.areaHalfSize.x,
                    (static_cast<float>(row) + 0.5f) * cell - level.areaHalfSize.y};
    };

    std::vector<char> reached(static_cast<std::size_t>(columns) * rows, 0);
    std::queue<std::pair<int, int>> frontier;
    const auto visit = [&](int column, int row) {
        if (column < 0 || row < 0 || column >= columns || row >= rows) {
            return;
        }
        char& mark = reached[static_cast<std::size_t>(row) * columns + column];
        if (mark == 0 && canStand(level, pointAt(column, row))) {
            mark = 1;
            frontier.push({column, row});
        }
    };

    visit(static_cast<int>((level.nest.position.x + level.areaHalfSize.x) / cell),
          static_cast<int>((level.nest.position.y + level.areaHalfSize.y) / cell));
    while (!frontier.empty()) {
        const auto [column, row] = frontier.front();
        frontier.pop();
        visit(column + 1, row);
        visit(column - 1, row);
        visit(column, row + 1);
        visit(column, row - 1);
    }

    int reachable = 0;
    for (const Food& food : level.foods) {
        bool touched = false;
        for (int row = 0; row < rows && !touched; ++row) {
            for (int column = 0; column < columns && !touched; ++column) {
                touched = reached[static_cast<std::size_t>(row) * columns + column] != 0 &&
                          distance(pointAt(column, row), food.position) < kAntRadius + food.radius;
            }
        }
        reachable += touched ? 1 : 0;
    }
    return reachable;
}

}  // namespace

TEST(level_places_requested_food_counts) {
    for (std::uint32_t seed = 0; seed < kSeedsChecked; ++seed) {
        const Level level = makePicnicLevel(seed, 6, 8);
        int inBasket = 0;
        for (const Food& food : level.foods) {
            inBasket += isInside(level.baskets.front(), food.position) ? 1 : 0;
        }
        CHECK(level.foods.size() == 14);
        CHECK(inBasket == 8);
    }
}

TEST(level_same_seed_gives_the_same_map) {
    const Level a = makePicnicLevel(42);
    const Level b = makePicnicLevel(42);
    CHECK(a.nest.position.x == b.nest.position.x);
    CHECK(a.baskets.front().center.x == b.baskets.front().center.x);
    CHECK(a.obstacles.size() == b.obstacles.size());
    CHECK(a.obstacles.back().position.y == b.obstacles.back().position.y);
    for (std::size_t i = 0; i < a.foods.size(); ++i) {
        CHECK(a.foods[i].position.x == b.foods[i].position.x);
        CHECK(a.foods[i].position.y == b.foods[i].position.y);
    }
}

TEST(level_different_seeds_give_different_maps) {
    const Level a = makePicnicLevel(1);
    const Level b = makePicnicLevel(2);
    CHECK(a.nest.position.x != b.nest.position.x || a.nest.position.y != b.nest.position.y);
    CHECK(a.baskets.front().center.x != b.baskets.front().center.x);
    CHECK(a.obstacles.front().position.x != b.obstacles.front().position.x);
    CHECK(a.foods.front().position.x != b.foods.front().position.x);
}

TEST(level_nest_is_on_the_grass_and_basket_on_the_blanket) {
    for (std::uint32_t seed = 0; seed < kSeedsChecked; ++seed) {
        const Level level = makePicnicLevel(seed);
        const Vec2 nest = level.nest.position;
        const bool offBlanket = std::fabs(nest.x) > level.blanketHalfSize.x || std::fabs(nest.y) > level.blanketHalfSize.y;
        const bool onMap = std::fabs(nest.x) + level.nest.radius < level.areaHalfSize.x &&
                           std::fabs(nest.y) + level.nest.radius < level.areaHalfSize.y;
        CHECK(offBlanket && onMap);

        const Basket& basket = level.baskets.front();
        CHECK(std::fabs(basket.center.x) + basket.halfSize.x < level.blanketHalfSize.x);
        CHECK(std::fabs(basket.center.y) + basket.halfSize.y < level.blanketHalfSize.y);
        CHECK(distance(basket.center, nest) > 15.0f);  // a real journey, never next door
    }
}

TEST(level_things_have_walking_room_between_them) {
    for (std::uint32_t seed = 0; seed < kSeedsChecked; ++seed) {
        const Level level = makePicnicLevel(seed);
        CHECK(level.obstacles.size() >= 5);
        CHECK(level.poisons.size() >= 2);
        for (std::size_t i = 0; i < level.obstacles.size(); ++i) {
            for (std::size_t j = i + 1; j < level.obstacles.size(); ++j) {
                const float gap = distance(level.obstacles[i].position, level.obstacles[j].position) -
                                  level.obstacles[i].radius - level.obstacles[j].radius;
                CHECK(gap > 2.0f * kAntRadius);
            }
        }
    }
}

TEST(level_ant_starts_in_the_open_facing_the_picnic) {
    for (std::uint32_t seed = 0; seed < kSeedsChecked; ++seed) {
        const Level level = makePicnicLevel(seed);
        CHECK(canStand(level, level.antStart));
        // Walking straight ahead from the start gets closer to the middle of the blanket.
        const Vec2 ahead = level.antStart + fromAngle(level.antStartHeading);
        CHECK(length(ahead) < length(level.antStart));
    }
}

TEST(level_every_piece_of_food_can_be_reached_from_the_nest) {
    // The important guarantee for a random map: no food is walled in, cut
    // off by poison, or otherwise impossible to fetch. This includes the
    // food in the basket, which is only reachable through its doorway.
    for (std::uint32_t seed = 0; seed < kSeedsChecked; ++seed) {
        const Level level = makePicnicLevel(seed);
        CHECK(countReachableFood(level) == static_cast<int>(level.foods.size()));
    }
}

TEST(level_basket_doorway_faces_the_nest) {
    // The doorway is in the corner of the basket nearest the nest.
    for (std::uint32_t seed = 0; seed < kSeedsChecked; ++seed) {
        const Level level = makePicnicLevel(seed);
        const Basket& basket = level.baskets.front();
        const Vec2 toNest = level.nest.position - basket.center;
        const Vec2 toDoor = basket.door - basket.center;
        CHECK(toDoor.x * toNest.x >= 0.0f);  // same side left-right
        CHECK(toDoor.y * toNest.y >= 0.0f);  // same side up-down
        CHECK(basket.doorWidth > 2.0f * (level.walls.front().thickness + kAntRadius));
    }
}
