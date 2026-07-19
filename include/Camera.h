#pragma once

#include <Eigen/Dense>

class Camera {
public:
  // Constructors
  Camera();

  Camera(const Eigen::Matrix3d &_K, const Eigen::Matrix3d &_R,
         const Eigen::Vector3d &_t);

  // Setters
  void setIntrinsics(const Eigen::Matrix3d &_K);
  void setRotation(const Eigen::Matrix3d &_R);
  void setTranslation(const Eigen::Vector3d &_t);

  // Getters
  const Eigen::Matrix3d &getIntrinsics() const;
  const Eigen::Matrix3d &getRotation() const;
  const Eigen::Vector3d &getTranslation() const;
  const Eigen::Matrix<double, 3, 4> &getProjectionMatrix() const;

  // Camera center in world coordinates
  Eigen::Vector3d getCameraCenter() const;

  // World -> Camera coordinates
  Eigen::Vector3d worldToCamera(const Eigen::Vector3d &_point_world) const;

  // Camera -> World coordinates
  Eigen::Vector3d cameraToWorld(const Eigen::Vector3d &_point_camera) const;

  // Project a 3D world point to image pixel coordinates
  Eigen::Vector2d projectPoint(const Eigen::Vector3d &_point_world) const;

  // Project a 3D world point to homogeneous image coordinates (x, y, z_cam)
  Eigen::Vector3d projectPointHomogeneous(const Eigen::Vector3d &_point_world) const;

private:
  void updateProjectionMatrix();

private:
  // Intrinsics
  Eigen::Matrix3d m_K;

  // Extrinsics (world -> camera)
  Eigen::Matrix3d m_R;
  Eigen::Vector3d m_t;

  // Projection matrix
  Eigen::Matrix<double, 3, 4> m_P;
};