// Copyright (c) 2017 Franka Emika GmbH
// Use of this source code is governed by the Apache-2.0 license, see LICENSE
#pragma once

#include <Eigen/Dense>

namespace franka {
std::array<double, 3> combineCenterOfMass(double m_ee,
                                          const std::array<double, 3>& F_x_Cee,
                                          double m_load,
                                          const std::array<double, 3>& F_x_Cload);

Eigen::Matrix3d skewSymmetricMatrixFromVector(Eigen::Vector3d& input);

std::array<double, 9> combineInertiaTensor(double m_ee,
                                           const std::array<double, 3>& F_x_Cee,
                                           const std::array<double, 9>& I_ee,
                                           double m_load,
                                           const std::array<double, 3>& F_x_Cload,
                                           const std::array<double, 9>& I_load,
                                           double m_total,
                                           const std::array<double, 3>& F_x_total);
}  // namespace franka