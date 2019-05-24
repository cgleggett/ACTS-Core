// This file is part of the Acts project.
//
// Copyright (C) 2018 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

/////////////////////////////////////////////////////////////////
// DiscSurface.ipp, Acts project
///////////////////////////////////////////////////////////////////

inline const Vector2D DiscSurface::localPolarToCartesian(
    const Vector2D& lpolar) const {
  return Vector2D(lpolar[eLOC_R] * cos(lpolar[eLOC_PHI]),
                  lpolar[eLOC_R] * sin(lpolar[eLOC_PHI]));
}

inline const Vector2D DiscSurface::localCartesianToPolar(
    const Vector2D& lcart) const {
  return Vector2D(
      sqrt(lcart[eLOC_X] * lcart[eLOC_X] + lcart[eLOC_Y] * lcart[eLOC_Y]),
      atan2(lcart[eLOC_Y], lcart[eLOC_X]));
}

inline void DiscSurface::initJacobianToGlobal(const GeometryContext& gctx,
                                              BoundToFreeMatrix& jacobian,
                                              const Vector3D& gpos,
                                              const Vector3D& dir,
                                              const BoundVector& pars) const {
  // The trigonometry required to convert the direction to spherical
  // coordinates and then compute the sines and cosines again can be
  // surprisingly expensive from a performance point of view.
  //
  // Here, we can avoid it because the direction is by definition a unit
  // vector, with the following coordinate conversions...
  const double x = dir(0);  // == cos(phi) * sin(theta)
  const double y = dir(1);  // == sin(phi) * sin(theta)
  const double z = dir(2);  // == cos(theta)

  // ...which we can invert to directly get the sines and cosines:
  const double cos_theta = z;
  const double sin_theta = sqrt(x * x + y * y);
  const double inv_sin_theta = 1. / sin_theta;
  const double cos_phi = x * inv_sin_theta;
  const double sin_phi = y * inv_sin_theta;
  // retrieve the reference frame
  const auto rframe = referenceFrame(gctx, gpos, dir);

  // special polar coordinates for the Disc
  double lrad = pars[eLOC_0];
  double lphi = pars[eLOC_1];
  double lcos_phi = cos(lphi);
  double lsin_phi = sin(lphi);
  // the local error components - rotated from reference frame
  jacobian.block<3, 1>(0, eLOC_0) =
      lcos_phi * rframe.block<3, 1>(0, 0) + lsin_phi * rframe.block<3, 1>(0, 1);
  jacobian.block<3, 1>(0, eLOC_1) =
      lrad * (lcos_phi * rframe.block<3, 1>(0, 1) -
              lsin_phi * rframe.block<3, 1>(0, 0));
  // the momentum components
  jacobian(3, ePHI) = (-sin_theta) * sin_phi;
  jacobian(3, eTHETA) = cos_theta * cos_phi;
  jacobian(4, ePHI) = sin_theta * cos_phi;
  jacobian(4, eTHETA) = cos_theta * sin_phi;
  jacobian(5, eTHETA) = (-sin_theta);
  jacobian(6, eQOP) = 1;
  jacobian(7, eT) = 1;
}

inline const RotationMatrix3D DiscSurface::initJacobianToLocal(
    const GeometryContext& gctx, FreeToBoundMatrix& jacobian,
    const Vector3D& gpos, const Vector3D& dir) const {
  using VectorHelpers::perp;
  using VectorHelpers::phi;
  // Optimized trigonometry on the propagation direction
  const double x = dir(0);  // == cos(phi) * sin(theta)
  const double y = dir(1);  // == sin(phi) * sin(theta)
  // component expressions - global
  const double inv_sin_theta_2 = 1. / (x * x + y * y);
  const double cos_phi_over_sin_theta = x * inv_sin_theta_2;
  const double sin_phi_over_sin_theta = y * inv_sin_theta_2;
  const double inv_sin_theta = sqrt(inv_sin_theta_2);
  // The measurement frame of the surface
  RotationMatrix3D rframeT = referenceFrame(gctx, gpos, dir).transpose();
  // calculate the transformation to local coorinates
  const Vector3D pos_loc = transform(gctx).inverse() * gpos;
  const double lr = perp(pos_loc);
  const double lphi = phi(pos_loc);
  const double lcphi = cos(lphi);
  const double lsphi = sin(lphi);
  // rotate into the polar coorindates
  auto lx = rframeT.block<1, 3>(0, 0);
  auto ly = rframeT.block<1, 3>(1, 0);
  jacobian.block<1, 3>(0, 0) = lcphi * lx + lsphi * ly;
  jacobian.block<1, 3>(1, 0) = (lcphi * ly - lsphi * lx) / lr;
  // Directional and momentum elements for reference frame surface
  jacobian(ePHI, 3) = -sin_phi_over_sin_theta;
  jacobian(ePHI, 4) = cos_phi_over_sin_theta;
  jacobian(eTHETA, 5) = -inv_sin_theta;
  jacobian(eQOP, 6) = 1;
  jacobian(eT, 7) = 1;
  // return the transposed reference frame
  return rframeT;
}

inline Intersection DiscSurface::intersectionEstimate(
    const GeometryContext& gctx, const Vector3D& gpos, const Vector3D& gdir,
    const BoundaryCheck& bcheck, double bwdTolerance, CorrFnc correct) const {
  // Call the common method for planar surfaces
  return planarIntersectionEstimate(*this, gctx, gpos, gdir, bcheck,
                                    bwdTolerance, correct);
}

inline const Vector3D DiscSurface::normal(const GeometryContext& gctx,
                                          const Vector2D& /*unused*/) const {
  // fast access via tranform matrix (and not rotation())
  const auto& tMatrix = transform(gctx).matrix();
  return Vector3D(tMatrix(0, 2), tMatrix(1, 2), tMatrix(2, 2));
}

inline const Vector3D DiscSurface::binningPosition(
    const GeometryContext& gctx, BinningValue /*unused*/) const {
  return center(gctx);
}

inline double DiscSurface::pathCorrection(const GeometryContext& gctx,
                                          const Vector3D& pos,
                                          const Vector3D& mom) const {
  /// we can ignore the global position here
  return 1. / std::abs(Surface::normal(gctx, pos).dot(mom.normalized()));
}
