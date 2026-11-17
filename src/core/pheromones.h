/// @file pheromones.h
/// @brief Scent trails: how real ants tell each other where things are.
///
/// Ants are nearly blind. They navigate by smell, leaving chemical marks
/// called *pheromones* on the ground as they walk. We simulate two scents:
///
///   - **Home scent** is laid by ants that are out searching. It is strongest
///     near the nest, so following it "uphill" leads home.
///   - **Food scent** is laid by ants carrying food. It is strongest near
///     where the food was found, so following it uphill leads to food.
///
/// The ground is divided into a grid of small square cells. Each cell stores
/// how strong each scent is there (0 = none, 1 = as strong as it gets).
/// Over time scent **evaporates** (fades away) and **diffuses** (spreads into
/// neighbouring cells), so unused trails disappear and busy trails stay strong.

#pragma once

#include <vector>

#include "core/vec2.h"

namespace ant {

/// The two kinds of scent.
enum class Scent { Home, Food };

/// Numbers that define how scent behaves.
struct PheromoneTuning {
    float cellSize = 0.5f;   ///< Width of one grid cell, in world units.
    float lifetime = 45.0f;  ///< Seconds for a scent to fade to about 37% (1/e) of its strength.
    float diffusion = 0.3f;  ///< How quickly scent evens out with neighbouring cells, per second.
};

/// The scent grid covering the whole walkable area.
class PheromoneField {
public:
    /// Creates an empty field covering [-areaHalfSize, +areaHalfSize].
    explicit PheromoneField(Vec2 areaHalfSize, PheromoneTuning tuning = {});

    /// Marks the cell at (`column`, `row`) as solid: it holds no scent and scent
    /// cannot spread through it. Used for cells covered by walls and obstacles.
    void setSolid(int column, int row) { solid_[row * columns_ + column] = 1; }

    /// The world position of the middle of a grid cell.
    Vec2 cellCenter(int column, int row) const;

    float cellSize() const { return tuning_.cellSize; }

    /// Adds `amount` of `scent` to the cell containing `position`.
    /// A cell never holds more than 1. Positions outside the area or in a solid cell are ignored.
    void deposit(Scent scent, Vec2 position, float amount);

    /// The strength of `scent` at `position` (0 outside the area).
    float sample(Scent scent, Vec2 position) const;

    /// Lets `dt` seconds pass: scent spreads a little and fades a little.
    void update(float dt);

    int columns() const { return columns_; }
    int rows() const { return rows_; }

    /// The strength of `scent` in one grid cell (used for drawing the trails).
    float cell(Scent scent, int column, int row) const { return grid(scent)[row * columns_ + column]; }

private:
    /// The grid for one scent, stored row by row.
    const std::vector<float>& grid(Scent scent) const { return scent == Scent::Home ? home_ : food_; }
    std::vector<float>& grid(Scent scent) { return scent == Scent::Home ? home_ : food_; }

    /// Index of the cell containing `position`, or -1 if it is outside the area.
    int cellIndex(Vec2 position) const;

    /// Spreads and fades one grid.
    void updateGrid(std::vector<float>& cells, float dt);

    Vec2 areaHalfSize_;
    PheromoneTuning tuning_;
    int columns_ = 0;
    int rows_ = 0;
    std::vector<float> home_;
    std::vector<float> food_;
    std::vector<float> scratch_;  ///< Working copy used while spreading scent.
    std::vector<char> solid_;     ///< 1 for cells that scent cannot enter.
};

}  // namespace ant
