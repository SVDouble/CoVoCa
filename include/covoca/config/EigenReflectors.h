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

    static Eigen::Vector3d to(const ReflType& value) noexcept {
        return Eigen::Map<const Eigen::Vector3d>(value.data());
    }

    static ReflType from(const Eigen::Vector3d& value) {
        return {value.x(), value.y(), value.z()};
    }
};

template <> struct Reflector<Eigen::VectorXd> {
    using ReflType = std::vector<double>;

    static Eigen::VectorXd to(const ReflType& value) noexcept {
        if (value.empty()) {
            return {};
        }
        return Eigen::Map<const Eigen::VectorXd>(value.data(), static_cast<Eigen::Index>(value.size()));
    }

    static ReflType from(const Eigen::VectorXd& value) {
        if (value.size() == 0) {
            return {};
        }
        return {value.data(), value.data() + value.size()};
    }
};

template <int Rows, int Cols> struct EigenMatrixReflector {
    using ReflType = std::array<std::array<double, static_cast<std::size_t>(Cols)>, static_cast<std::size_t>(Rows)>;

    static Eigen::Matrix<double, Rows, Cols> to(const ReflType& rows) noexcept {
        Eigen::Matrix<double, Rows, Cols> matrix;
        for (int row = 0; row < Rows; ++row) {
            for (int col = 0; col < Cols; ++col) {
                matrix(row, col) = rows[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
            }
        }
        return matrix;
    }

    static ReflType from(const Eigen::Matrix<double, Rows, Cols>& matrix) {
        ReflType rows{};
        for (int row = 0; row < Rows; ++row) {
            for (int col = 0; col < Cols; ++col) {
                rows[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] = matrix(row, col);
            }
        }
        return rows;
    }
};

template <> struct Reflector<Eigen::Matrix3d> : EigenMatrixReflector<3, 3> {};
template <> struct Reflector<Eigen::Matrix4d> : EigenMatrixReflector<4, 4> {};

} // namespace rfl
