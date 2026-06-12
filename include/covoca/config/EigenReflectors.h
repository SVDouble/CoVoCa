#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include <Eigen/Dense>

#include <rfl/internal/has_reflector.hpp>

/// rfl::Reflector specializations so Eigen vector/matrix types can be used directly
/// as struct fields in rfl-serialized models (YAML/JSON arrays in, Eigen types out).
namespace rfl {

template <> struct Reflector<Eigen::Vector3d> {
    using ReflType = std::array<double, 3>;

    /**
     * Converts a serialized 3-vector into `Eigen::Vector3d`.
     *
     * Args:
     *   value: YAML/JSON array `[x, y, z]`.
     *
     * Returns:
     *   Eigen vector with matching coefficients.
     */
    static Eigen::Vector3d to(const ReflType& value) noexcept {
        return Eigen::Map<const Eigen::Vector3d>(value.data());
    }

    /**
     * Converts `Eigen::Vector3d` into its serialized representation.
     *
     * Args:
     *   value: Eigen vector to serialize.
     *
     * Returns:
     *   Fixed-size array `[x, y, z]`.
     */
    static ReflType from(const Eigen::Vector3d& value) {
        return {value.x(), value.y(), value.z()};
    }
};

template <> struct Reflector<Eigen::VectorXd> {
    using ReflType = std::vector<double>;

    /**
     * Converts a serialized dynamic vector into `Eigen::VectorXd`.
     *
     * Args:
     *   value: YAML/JSON array of coefficients.
     *
     * Returns:
     *   Eigen dynamic vector with matching coefficients.
     */
    static Eigen::VectorXd to(const ReflType& value) noexcept {
        if (value.empty()) {
            return {};
        }
        const auto size = static_cast<Eigen::Index>(value.size());
        return Eigen::Map<const Eigen::VectorXd>(value.data(), size);
    }

    /**
     * Converts `Eigen::VectorXd` into its serialized representation.
     *
     * Args:
     *   value: Eigen vector to serialize.
     *
     * Returns:
     *   YAML/JSON-ready coefficient array.
     */
    static ReflType from(const Eigen::VectorXd& value) {
        if (value.size() == 0) {
            return {};
        }
        return {value.data(), value.data() + value.size()};
    }
};

template <int Rows, int Cols> struct EigenMatrixReflector {
    static constexpr std::size_t kRows = Rows;
    static constexpr std::size_t kCols = Cols;

    using ReflType = std::array<std::array<double, kCols>, kRows>;

    /**
     * Converts serialized row-major matrix data into an Eigen matrix.
     *
     * Args:
     *   rows: YAML/JSON nested array, one inner array per matrix row.
     *
     * Returns:
     *   Eigen matrix with matching coefficients.
     */
    static Eigen::Matrix<double, Rows, Cols> to(const ReflType& rows) noexcept {
        Eigen::Matrix<double, Rows, Cols> matrix;
        for (Eigen::Index row = 0; row < Rows; ++row) {
            for (Eigen::Index col = 0; col < Cols; ++col) {
                matrix(row, col) = rows[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
            }
        }
        return matrix;
    }

    /**
     * Converts an Eigen matrix into serialized row-major data.
     *
     * Args:
     *   matrix: Eigen matrix to serialize.
     *
     * Returns:
     *   YAML/JSON-ready nested row array.
     */
    static ReflType from(const Eigen::Matrix<double, Rows, Cols>& matrix) {
        ReflType rows{};
        for (Eigen::Index row = 0; row < Rows; ++row) {
            for (Eigen::Index col = 0; col < Cols; ++col) {
                rows[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] = matrix(row, col);
            }
        }
        return rows;
    }
};

template <> struct Reflector<Eigen::Matrix3d> : EigenMatrixReflector<3, 3> {};
template <> struct Reflector<Eigen::Matrix4d> : EigenMatrixReflector<4, 4> {};

} // namespace rfl
