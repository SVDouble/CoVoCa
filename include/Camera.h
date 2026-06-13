#pragma once

#include <iostream>
#include <Eigen/Dense>

class Camera
{
public:
    // Constructors
    Camera()
        : m_K(Eigen::Matrix3d::Identity()),
          m_R(Eigen::Matrix3d::Identity()),
          m_t(Eigen::Vector3d::Zero())
    {
        updateProjectionMatrix();
    }

    Camera(const Eigen::Matrix3d &_K,
           const Eigen::Matrix3d &_R,
           const Eigen::Vector3d &_t)
        : m_K(_K), m_R(_R), m_t(_t)
    {
        updateProjectionMatrix();
    }

    // Setters
    void setIntrinsics(const Eigen::Matrix3d &_K)
    {
        m_K = _K;
        updateProjectionMatrix();
    }

    void setRotation(const Eigen::Matrix3d &_R)
    {
        m_R = _R;
        updateProjectionMatrix();
    }

    void setTranslation(const Eigen::Vector3d &_t)
    {
        m_t = _t;
        updateProjectionMatrix();
    }

    // Getters
    const Eigen::Matrix3d &getIntrinsics() const { return m_K; }
    const Eigen::Matrix3d &getRotation() const { return m_R; }
    const Eigen::Vector3d &getTranslation() const { return m_t; }
    const Eigen::Matrix<double, 3, 4> &getProjectionMatrix() const { return m_P; }

    // Camera center in world coordinates
    Eigen::Vector3d getCameraCenter() const
    {
        return -m_R.transpose() * m_t;
    }

    // World -> Camera coordinates
    Eigen::Vector3d worldToCamera(const Eigen::Vector3d &_point_world) const
    {
        return m_R * _point_world + m_t;
    }

    // Camera -> World coordinates
    Eigen::Vector3d cameraToWorld(const Eigen::Vector3d &_point_camera) const
    {
        return m_R.transpose() * (_point_camera - m_t);
    }

    // Project a 3D world point to image pixel coordinates
    Eigen::Vector2d projectPoint(const Eigen::Vector3d &_point_world) const
    {
        Eigen::Vector4d point_h;
        point_h << _point_world, 1.0;

        Eigen::Vector3d pixel_h = m_P * point_h;

        return Eigen::Vector2d(
            pixel_h(0) / pixel_h(2),
            pixel_h(1) / pixel_h(2));
    }

private:
    void updateProjectionMatrix()
    {
        Eigen::Matrix<double, 3, 4> extrinsic;
        extrinsic.block<3, 3>(0, 0) = m_R;
        extrinsic.col(3) = m_t;

        m_P = m_K * extrinsic;
    }

private:
    // Intrinsics
    Eigen::Matrix3d m_K;

    // Extrinsics (world -> camera)
    Eigen::Matrix3d m_R;
    Eigen::Vector3d m_t;

    // Projection matrix
    Eigen::Matrix<double, 3, 4> m_P;
};