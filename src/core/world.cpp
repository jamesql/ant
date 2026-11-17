/// @file world.cpp
/// @brief Implementation of the game rules.

#include "core/world.h"

#include <algorithm>
#include <utility>

namespace ant {

World::World(Level level, AntTuning tuning)
    : level_(std::move(level)), tuning_(tuning), timeRemaining_(level_.timeLimit) {
    ant_.position = level_.antStart;
    ant_.heading = level_.antStartHeading;
}

void World::step(const AntAction& action, float dt) {
    if (status_ != GameStatus::Playing) {
        return;
    }

    const float forward = std::clamp(action.forward, -1.0f, 1.0f);
    const float turn = std::clamp(action.turn, -1.0f, 1.0f);

    ant_.heading = wrapAngle(ant_.heading + turn * tuning_.turnSpeed * dt);

    const float speed = tuning_.moveSpeed * (ant_.carriedFood ? tuning_.carrySpeedFactor : 1.0f);
    ant_.position = ant_.position + fromAngle(ant_.heading) * (forward * speed * dt);

    resolveCollisions();
    pickUpFood();
    deliverFood();

    timeRemaining_ = std::max(0.0f, timeRemaining_ - dt);
    if (status_ == GameStatus::Playing && timeRemaining_ <= 0.0f) {
        status_ = GameStatus::Lost;
    }
}

void World::resolveCollisions() {
    // Push the ant out of any obstacle it overlaps, along the line joining
    // the two centres (the shortest way out of a circle).
    for (const Obstacle& obstacle : level_.obstacles) {
        const float minDistance = obstacle.radius + tuning_.radius;
        const Vec2 offset = ant_.position - obstacle.position;
        const float dist = length(offset);
        if (dist < minDistance) {
            // If the ant is exactly at the centre there is no "away" direction; pick +x.
            const Vec2 away = dist > 0.0f ? offset * (1.0f / dist) : Vec2{1.0f, 0.0f};
            ant_.position = obstacle.position + away * minDistance;
        }
    }

    // The table edge is an invisible wall: the ant cannot fall off.
    const Vec2 limit = level_.tableHalfSize - Vec2{tuning_.radius, tuning_.radius};
    ant_.position.x = std::clamp(ant_.position.x, -limit.x, limit.x);
    ant_.position.y = std::clamp(ant_.position.y, -limit.y, limit.y);
}

void World::pickUpFood() {
    if (ant_.carriedFood) {
        return;
    }
    for (std::size_t i = 0; i < level_.foods.size(); ++i) {
        Food& food = level_.foods[i];
        const bool touching = distance(ant_.position, food.position) <= tuning_.radius + food.radius;
        if (food.state == FoodState::OnTable && touching) {
            food.state = FoodState::Carried;
            ant_.carriedFood = i;
            return;
        }
    }
}

void World::deliverFood() {
    if (!ant_.carriedFood) {
        return;
    }
    if (distance(ant_.position, level_.nest.position) > level_.nest.radius) {
        return;
    }
    level_.foods[*ant_.carriedFood].state = FoodState::Delivered;
    ant_.carriedFood.reset();
    ++deliveredCount_;
    if (deliveredCount_ == static_cast<int>(level_.foods.size())) {
        status_ = GameStatus::Won;
    }
}

}  // namespace ant
