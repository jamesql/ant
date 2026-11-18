/// @file matrix.h
/// @brief A matrix: a rectangular grid of numbers.
///
/// In a neural network, a matrix holds the weights of one layer: one row per
/// neuron, one column per input. See docs/NEURAL_NETWORK.md, section 2.3.

#pragma once

#include <vector>

namespace ant {

/// A grid of floats with `rows` rows and `columns` columns, all starting at zero.
class Matrix {
public:
    Matrix() = default;
    Matrix(int rows, int columns);

    int rows() const { return rows_; }
    int columns() const { return columns_; }

    /// The number at `row`, `column` (both counted from 0).
    float& at(int row, int column) { return values_[static_cast<std::size_t>(row) * columns_ + column]; }
    float at(int row, int column) const { return values_[static_cast<std::size_t>(row) * columns_ + column]; }

private:
    int rows_ = 0;
    int columns_ = 0;
    std::vector<float> values_;  ///< Stored row by row.
};

/// Matrix times vector: for every row, multiply each entry by the matching
/// entry of `vector` and add them up. The result has one number per row.
///
/// With a layer's weights as `matrix` and its inputs as `vector`, row `i` of
/// the result is neuron `i`'s weighted sum (before the bias is added).
/// `vector` must have exactly `matrix.columns()` entries.
std::vector<float> multiply(const Matrix& matrix, const std::vector<float>& vector);

}  // namespace ant
