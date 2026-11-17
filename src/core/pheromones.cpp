/// @file pheromones.cpp
/// @brief Implementation of the scent grid.

#include "core/pheromones.h"

#include <algorithm>
#include <cmath>

namespace ant {

PheromoneField::PheromoneField(Vec2 areaHalfSize, PheromoneTuning tuning)
    : areaHalfSize_(areaHalfSize),
      tuning_(tuning),
      columns_(static_cast<int>(std::ceil(2.0f * areaHalfSize.x / tuning.cellSize))),
      rows_(static_cast<int>(std::ceil(2.0f * areaHalfSize.y / tuning.cellSize))),
      home_(static_cast<std::size_t>(columns_) * rows_, 0.0f),
      food_(home_),
      scratch_(home_),
      solid_(home_.size(), 0) {}

Vec2 PheromoneField::cellCenter(int column, int row) const {
    return {(static_cast<float>(column) + 0.5f) * tuning_.cellSize - areaHalfSize_.x,
            (static_cast<float>(row) + 0.5f) * tuning_.cellSize - areaHalfSize_.y};
}

int PheromoneField::cellIndex(Vec2 position) const {
    const int column = static_cast<int>(std::floor((position.x + areaHalfSize_.x) / tuning_.cellSize));
    const int row = static_cast<int>(std::floor((position.y + areaHalfSize_.y) / tuning_.cellSize));
    if (column < 0 || row < 0 || column >= columns_ || row >= rows_) {
        return -1;
    }
    return row * columns_ + column;
}

void PheromoneField::deposit(Scent scent, Vec2 position, float amount) {
    const int index = cellIndex(position);
    if (index >= 0 && solid_[index] == 0) {
        float& cell = grid(scent)[index];
        cell = std::min(1.0f, cell + amount);
    }
}

float PheromoneField::sample(Scent scent, Vec2 position) const {
    const int index = cellIndex(position);
    return index >= 0 ? grid(scent)[index] : 0.0f;
}

void PheromoneField::update(float dt) {
    updateGrid(home_, dt);
    updateGrid(food_, dt);
}

void PheromoneField::updateGrid(std::vector<float>& cells, float dt) {
    // Diffusion: every cell moves a little toward the average of its four
    // neighbours. Evaporation: every cell then keeps a fixed fraction of
    // itself; exp(-dt / lifetime) is that fraction for a step of dt seconds.
    // Solid cells take no part: scent neither enters nor crosses them.
    const float blend = std::min(1.0f, tuning_.diffusion * dt);
    const float keep = std::exp(-dt / tuning_.lifetime);

    scratch_ = cells;
    for (int row = 0; row < rows_; ++row) {
        for (int column = 0; column < columns_; ++column) {
            const int index = row * columns_ + column;
            if (solid_[index] != 0) {
                continue;
            }
            const float here = scratch_[index];
            // A neighbour that is off the grid or solid counts as the cell itself,
            // so no scent flows that way.
            const auto at = [&](int c, int r) {
                const bool inside = c >= 0 && r >= 0 && c < columns_ && r < rows_;
                return inside && solid_[r * columns_ + c] == 0 ? scratch_[r * columns_ + c] : here;
            };
            const float neighbours =
                (at(column - 1, row) + at(column + 1, row) + at(column, row - 1) + at(column, row + 1)) / 4.0f;
            cells[index] = (here + (neighbours - here) * blend) * keep;
        }
    }
}

}  // namespace ant
