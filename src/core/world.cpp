/// @file world.cpp
/// @brief Implementation of the game rules.

#include "core/world.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/instinct.h"

namespace ant {
namespace {

/// Spacing between the points tested by `isPathClear`.
constexpr float kPathProbeSpacing = 0.25f;

}  // namespace

World::World(Level level, AntTuning tuning)
    : level_(std::move(level)),
      tuning_(tuning),
      ants_(static_cast<std::size_t>(std::max(1, level_.antCount))),
      pheromones_(level_.areaHalfSize),
      timeRemaining_(level_.timeLimit) {
    markSolidScentCells();
    for (std::size_t i = 1; i < ants_.size(); ++i) {
        spawnAtNest(i);
    }
    ants_[0].position = level_.antStart;
    ants_[0].heading = level_.antStartHeading;
}

void World::markSolidScentCells() {
    // A wall thinner than a grid cell could slip between cell centres, so a
    // cell counts as walled if its centre is within half a cell of the wall.
    const float reach = 0.5f * pheromones_.cellSize() + 0.01f;
    for (int row = 0; row < pheromones_.rows(); ++row) {
        for (int column = 0; column < pheromones_.columns(); ++column) {
            const Vec2 center = pheromones_.cellCenter(column, row);
            bool solid = isBlocked(center);
            for (const Wall& wall : level_.walls) {
                const float gap = distance(center, closestPointOnSegment(center, wall.start, wall.end));
                solid = solid || gap < std::max(wall.thickness, reach);
            }
            if (solid) {
                pheromones_.setSolid(column, row);
            }
        }
    }
}

void World::spawnAtNest(std::size_t index) {
    // Each ant leaves in its own direction, fanned around the nest, so the
    // colony spreads out instead of marching in a single file.
    const float fan = static_cast<float>(index) * 2.39996f;  // the "golden angle" spreads points evenly
    Ant fresh;
    fresh.heading = wrapAngle(fan + elapsedTime_);
    fresh.position = level_.nest.position + fromAngle(fresh.heading) * (level_.nest.radius * 0.5f);
    fresh.deliveries = ants_[index].deliveries;
    fresh.distanceWalked = ants_[index].distanceWalked;
    ants_[index] = fresh;
    resolveCollisions(ants_[index]);
}

void World::step(const AntAction& playerAction, float dt) {
    actionBuffer_.resize(ants_.size());
    actionBuffer_[0] = playerAction;
    for (std::size_t i = 1; i < ants_.size(); ++i) {
        actionBuffer_[i] = instinctAction(*this, i);
    }
    step(actionBuffer_, dt);
}

void World::step(std::span<const AntAction> actions, float dt) {
    if (status_ != GameStatus::Playing) {
        return;
    }

    for (std::size_t i = 0; i < ants_.size(); ++i) {
        Ant& ant = ants_[i];
        if (!ant.alive) {
            ant.respawnTimer -= dt;
            if (ant.respawnTimer <= 0.0f) {
                spawnAtNest(i);
            }
            continue;
        }
        move(ant, i < actions.size() ? actions[i] : AntAction{}, dt);
        resolveCollisions(ant);
        applyPoison(ant, dt);
        if (ant.alive) {
            pickUpFood(ant);
            deliverFood(ant);
            layScent(ant, dt);
        }
    }

    pheromones_.update(dt);
    elapsedTime_ += dt;
    timeRemaining_ = std::max(0.0f, timeRemaining_ - dt);
    if (status_ == GameStatus::Playing && timeRemaining_ <= 0.0f) {
        status_ = GameStatus::Lost;
    }
}

void World::move(Ant& ant, const AntAction& action, float dt) {
    const float forward = std::clamp(action.forward, -1.0f, 1.0f);
    const float turn = std::clamp(action.turn, -1.0f, 1.0f);

    ant.heading = wrapAngle(ant.heading + turn * tuning_.turnSpeed * dt);

    const float speed = tuning_.moveSpeed * (ant.carriedFood ? tuning_.carrySpeedFactor : 1.0f);
    const float stride = forward * speed * dt;
    ant.position = ant.position + fromAngle(ant.heading) * stride;
    ant.distanceWalked += std::fabs(stride);
}

void World::resolveCollisions(Ant& ant) const {
    // Moves the ant so it is at least `minDistance` from `solidPoint`, along
    // the line joining the two (the shortest way out).
    const auto pushAway = [&](Vec2 solidPoint, float minDistance) {
        const Vec2 offset = ant.position - solidPoint;
        const float dist = length(offset);
        if (dist < minDistance) {
            // Exactly on top of the point there is no "away" direction; pick +x.
            const Vec2 away = dist > 0.0f ? offset * (1.0f / dist) : Vec2{1.0f, 0.0f};
            ant.position = solidPoint + away * minDistance;
        }
    };

    // Two passes, so being pushed out of one thing into another (a corner) is also fixed.
    for (int pass = 0; pass < 2; ++pass) {
        for (const Obstacle& obstacle : level_.obstacles) {
            pushAway(obstacle.position, obstacle.radius + tuning_.radius);
        }
        for (const Wall& wall : level_.walls) {
            pushAway(closestPointOnSegment(ant.position, wall.start, wall.end), wall.thickness + tuning_.radius);
        }
    }

    // The edge of the map is an invisible wall.
    const Vec2 limit = level_.areaHalfSize - Vec2{tuning_.radius, tuning_.radius};
    ant.position.x = std::clamp(ant.position.x, -limit.x, limit.x);
    ant.position.y = std::clamp(ant.position.y, -limit.y, limit.y);
}

void World::applyPoison(Ant& ant, float dt) {
    if (!isPoisoned(ant.position)) {
        ant.health = std::min(1.0f, ant.health + dt / tuning_.healTime);
        return;
    }
    ant.health -= dt / tuning_.poisonKillTime;
    if (ant.health > 0.0f) {
        return;
    }
    // The ant dies. Whatever it carried is left lying where it fell.
    if (ant.carriedFood) {
        Food& dropped = level_.foods[*ant.carriedFood];
        dropped.state = FoodState::OnGround;
        dropped.position = ant.position;
        ant.carriedFood.reset();
    }
    ant.health = 0.0f;
    ant.alive = false;
    ant.respawnTimer = tuning_.respawnDelay;
}

void World::pickUpFood(Ant& ant) {
    if (ant.carriedFood) {
        return;
    }
    for (std::size_t i = 0; i < level_.foods.size(); ++i) {
        Food& food = level_.foods[i];
        const bool touching = distance(ant.position, food.position) <= tuning_.radius + food.radius;
        if (food.state == FoodState::OnGround && touching) {
            food.state = FoodState::Carried;
            ant.carriedFood = i;
            ant.scentReserve = 1.0f;  // fresh food: start a strong food trail from here
            return;
        }
    }
}

void World::deliverFood(Ant& ant) {
    if (distance(ant.position, level_.nest.position) > level_.nest.radius) {
        return;
    }
    ant.scentReserve = 1.0f;  // at home: the next home trail starts strong
    if (!ant.carriedFood) {
        return;
    }
    level_.foods[*ant.carriedFood].state = FoodState::Delivered;
    ant.carriedFood.reset();
    ++ant.deliveries;
    ++deliveredCount_;
    if (deliveredCount_ == static_cast<int>(level_.foods.size())) {
        status_ = GameStatus::Won;
    }
}

void World::layScent(Ant& ant, float dt) {
    // Searching ants mark the way home; ants with food mark the way to food.
    const Scent scent = ant.carriedFood ? Scent::Food : Scent::Home;
    pheromones_.deposit(scent, ant.position, tuning_.scentRate * ant.scentReserve * dt);
    ant.scentReserve *= std::exp(-dt / tuning_.scentFadeTime);
}

bool World::isBlocked(Vec2 point) const {
    if (std::fabs(point.x) > level_.areaHalfSize.x || std::fabs(point.y) > level_.areaHalfSize.y) {
        return true;
    }
    for (const Obstacle& obstacle : level_.obstacles) {
        if (distance(point, obstacle.position) < obstacle.radius) {
            return true;
        }
    }
    for (const Wall& wall : level_.walls) {
        if (distance(point, closestPointOnSegment(point, wall.start, wall.end)) < wall.thickness) {
            return true;
        }
    }
    return false;
}

bool World::isPoisoned(Vec2 point) const {
    for (const Poison& poison : level_.poisons) {
        if (distance(point, poison.position) < poison.radius) {
            return true;
        }
    }
    return false;
}

bool World::isPathClear(Vec2 from, Vec2 to) const {
    const int probes = static_cast<int>(distance(from, to) / kPathProbeSpacing) + 1;
    for (int i = 0; i <= probes; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(probes);
        if (isBlocked(from + (to - from) * t)) {
            return false;
        }
    }
    return true;
}

}  // namespace ant
