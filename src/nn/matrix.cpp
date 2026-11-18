/// @file matrix.cpp
/// @brief Implementation of the matrix type.

#include "nn/matrix.h"

namespace ant {

Matrix::Matrix(int rows, int columns)
    : rows_(rows), columns_(columns), values_(static_cast<std::size_t>(rows) * columns, 0.0f) {}

std::vector<float> multiply(const Matrix& matrix, const std::vector<float>& vector) {
    std::vector<float> result(static_cast<std::size_t>(matrix.rows()), 0.0f);
    for (int row = 0; row < matrix.rows(); ++row) {
        float sum = 0.0f;
        for (int column = 0; column < matrix.columns(); ++column) {
            sum += matrix.at(row, column) * vector[static_cast<std::size_t>(column)];
        }
        result[static_cast<std::size_t>(row)] = sum;
    }
    return result;
}

std::vector<float> multiplyTransposed(const Matrix& matrix, const std::vector<float>& vector) {
    std::vector<float> result(static_cast<std::size_t>(matrix.columns()), 0.0f);
    for (int row = 0; row < matrix.rows(); ++row) {
        for (int column = 0; column < matrix.columns(); ++column) {
            result[static_cast<std::size_t>(column)] += matrix.at(row, column) * vector[static_cast<std::size_t>(row)];
        }
    }
    return result;
}

}  // namespace ant
