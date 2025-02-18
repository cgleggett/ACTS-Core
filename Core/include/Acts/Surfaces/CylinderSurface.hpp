// This file is part of the Acts project.
//
// Copyright (C) 2016-2018 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

///////////////////////////////////////////////////////////////////
// CylinderSurface.h, Acts project
///////////////////////////////////////////////////////////////////

#pragma once

#include <cmath>

#include "Acts/Geometry/GeometryContext.hpp"
#include "Acts/Geometry/GeometryStatics.hpp"
#include "Acts/Surfaces/CylinderBounds.hpp"
#include "Acts/Surfaces/PolyhedronRepresentation.hpp"
#include "Acts/Surfaces/Surface.hpp"
#include "Acts/Utilities/Definitions.hpp"
#include "Acts/Utilities/detail/RealQuadraticEquation.hpp"

namespace Acts {

/// @class CylinderSurface
///
/// Class for a CylinderSurface in the TrackingGeometry.
/// It inherits from Surface.
///
/// The cylinder surface has a special role in the TrackingGeometry,
/// since it builds the surfaces of all TrackingVolumes at container level
/// for a cylindrical tracking geometry.
///
/// @image html CylinderSurface.png

class CylinderSurface : public Surface {
  friend Surface;

 protected:
  /// Deleted default constructor
  CylinderSurface() = delete;

  /// Constructor from Transform3D, radius and halflength
  ///
  /// @param htrans transform to position the surface, can be nullptr
  /// @note if htrans == nullptr, the cylinder is positioned around (0.,0.,0.)
  /// @param radius is the radius of the cylinder
  /// @param hlength is the half length of the cylinder in z
  CylinderSurface(std::shared_ptr<const Transform3D> htrans, double radius,
                  double hlength);

  /// Constructor from Transform3D, radius halfphi, and halflength
  ///
  /// @param htrans transform to position the surface, can be nullptr
  /// @note if htrans == nullptr, the cylinder is positioned around (0.,0.,0.)
  /// @param radius is the radius of the cylinder
  /// @param hphi is the half length in phi of the cylinder
  /// @param hlength is the half length of the cylinder in z
  CylinderSurface(std::shared_ptr<const Transform3D> htrans, double radius,
                  double hphi, double hlength);

  /// Constructor from DetectorElementBase: Element proxy
  ///
  /// @param cbounds are the provided cylinder bounds (shared)
  /// @param detelement is the linked detector element to this surface
  CylinderSurface(std::shared_ptr<const CylinderBounds> cbounds,
                  const DetectorElementBase& detelement);

  /// Constructor from Transform3D and CylinderBounds
  ///
  /// @param htrans transform to position the surface, can be nullptr
  /// @note if htrans == nullptr, the cylinder is positioned around (0.,0.,0.)
  /// @param cbounds is a shared pointer to a cylindeer bounds object,
  /// it must exist (assert test)
  CylinderSurface(std::shared_ptr<const Transform3D> htrans,
                  const std::shared_ptr<const CylinderBounds>& cbounds);

  /// Copy constructor
  ///
  /// @param other is the source cylinder for the copy
  CylinderSurface(const CylinderSurface& other);

  /// Copy constructor - with shift
  ///
  /// @param gctx The current geometry context object, e.g. alignment
  /// @param other is the source cone surface
  /// @param transf is the additional transfrom applied after copying
  CylinderSurface(const GeometryContext& gctx, const CylinderSurface& other,
                  const Transform3D& transf);

 public:
  /// Destructor - defaulted
  ~CylinderSurface() override = default;

  /// Assignment operator
  ///
  /// @param other is the source cylinder for the copy
  CylinderSurface& operator=(const CylinderSurface& other);

  /// Clone method into a concrete type of CylinderSurface with shift
  ///
  /// @param gctx The current geometry context object, e.g. alignment
  /// @param shift applied to the surface
  std::shared_ptr<CylinderSurface> clone(const GeometryContext& gctx,
                                         const Transform3D& shift) const;

  /// The binning position method - is overloaded for r-type binning
  ///
  /// @param gctx The current geometry context object, e.g. alignment
  /// @param bValue is the type of global binning to be done
  ///
  /// @return is the global position to be used for binning
  const Vector3D binningPosition(const GeometryContext& gctx,
                                 BinningValue bValue) const final;

  /// Return the measurement frame - this is needed for alignment, in particular
  /// The measurement frame of a cylinder is the tangential plane at a given
  /// position
  ///
  /// @param gctx The current geometry context object, e.g. alignment
  /// @param gpos is the position where the measurement frame is defined
  /// @param mom is the momentum vector (ignored)
  /// @return rotation matrix that defines the measurement frame
  const RotationMatrix3D referenceFrame(const GeometryContext& gctx,
                                        const Vector3D& gpos,
                                        const Vector3D& mom) const final;

  /// Return the surface type
  SurfaceType type() const override;

  /// Return method for surface normal information
  /// @note for a Cylinder a local position is always required for the normal
  /// vector
  ///
  /// @param gctx The current geometry context object, e.g. alignment
  /// @param lpos is the local postion for which the normal vector is requested
  /// @return normal vector at the local position
  const Vector3D normal(const GeometryContext& gctx,
                        const Vector2D& lpos) const final;

  /// Return method for surface normal information
  /// @note for a Cylinder a local position is always required for the normal
  /// vector
  ///
  /// @param gctx The current geometry context object, e.g. alignment
  /// @param gpos is the global postion for which the normal vector is requested
  /// @return normal vector at the global position
  const Vector3D normal(const GeometryContext& gctx,
                        const Vector3D& gpos) const final;

  /// Normal vector return without argument
  using Surface::normal;

  /// Return method for the rotational symmetry axis
  ///
  /// @param gctx The current geometry context object, e.g. alignment
  ///
  /// @return  the z-Axis of transform
  virtual const Vector3D rotSymmetryAxis(const GeometryContext& gctx) const;

  /// This method returns the CylinderBounds by reference
  const CylinderBounds& bounds() const final;

  /// Local to global transformation
  ///
  /// @param gctx The current geometry context object, e.g. alignment
  /// @param lpos is the local position to be transformed
  /// @param mom is the global momentum (ignored in this operation)
  /// @param gpos is the global position shich is filled
  void localToGlobal(const GeometryContext& gctx, const Vector2D& lpos,
                     const Vector3D& mom, Vector3D& gpos) const final;

  /// Global to local transfomration
  ///
  /// @param gctx The current geometry context object, e.g. alignment
  /// @param gpos is the global position to be transformed
  /// @param mom is the global momentum (ignored in this operation)
  /// @param lpos is hte local position to be filled
  ///
  /// @return is a boolean indicating if the transformation succeeded
  bool globalToLocal(const GeometryContext& gctx, const Vector3D& gpos,
                     const Vector3D& mom, Vector2D& lpos) const final;

  /// Straight line intersection schema - provides closest intersection
  ///  and (signed) path length
  ///
  /// @param gctx The current geometry context object, e.g. alignment
  /// @param gpos is the global position as a starting point
  /// @param gdir is the global direction at the starting point, expected to
  ///  be normalized
  /// @param navDir The navigation direction with respect to the momentum
  /// @param bcheck is the boundary check
  /// @param correct is an optional correction function pointer that
  ///        allows to update the intial estimate
  ///
  ///  <b>mathematical motivation:</b>
  ///
  /// The cylinder is given by :
  ///   - cylinder center: ccenter (C)
  ///   - the direction of the cylinder axis: cdirection (DZ)
  ///   - the radius r
  /// The line is given by :
  ///   - a reference position : lposition (L0)
  ///   - the line direction: ldirection (DL)
  ///   the parametric form for the line is then : L(t) = L0 + t * DL
  ///
  /// Any point P on infinite cylinder if :
  ///      ((P - C) x DZ)^2 = r^2 * DZ^2
  /// We know that DZ is a unit vector:
  ///   DZ^2 == 1
  /// When expanded with the line equation, this is  :
  ///      ((L0 - C) x DZ + t * (DL x DZ))^2 = r^2 * DZ^2
  /// which is a quadratic equation in the form (X + t * Y)^2 = d, where :
  ///  X = (L0 - C) x DZ
  ///  Y = DL x DZ
  ///  d = r^2 * (DZ)^2
  /// Now, expand the equation :
  /// t^2 * (Y . Y) + t * (2 * (X . Y)) + (X . X) - d = 0
  /// => second order equation in the form : a*t^2 + b*t + c = 0 where
  /// a = (Y . Y)
  /// b = 2 * (X . Y)
  /// c = (X . X) - d
  /// finally solve the second order equation : a*t^2 + b*t + c = 0
  /// reinsertion into the line equation.
  ///
  /// @return is the intersection object
  Intersection intersectionEstimate(const GeometryContext& gctx,
                                    const Vector3D& gpos, const Vector3D& gdir,
                                    NavigationDirection navDir = forward,
                                    const BoundaryCheck& bcheck = false,
                                    CorrFnc correct = nullptr) const final;

  /// Path correction due to incident of the track
  ///
  /// @param gctx The current geometry context object, e.g. alignment
  /// @param gpos is the global position as a starting point
  /// @param mom is the global momentum at the starting point
  ///
  /// @return is the correction factor due to incident
  double pathCorrection(const GeometryContext& gctx, const Vector3D& gpos,
                        const Vector3D& mom) const final;

  /// Return method for properly formatted output string
  std::string name() const override;

  /// Return a PolyhedronRepresentation for this object
  ///
  /// @param gctx The current geometry context object, e.g. alignment
  /// @param l0div Number of divisions along l0 (phi)
  /// @param l1div Number of divisions along l1 (z)
  virtual PolyhedronRepresentation polyhedronRepresentation(
      const GeometryContext& gctx, size_t l0div = 10, size_t l1div = 1) const;

 protected:
  std::shared_ptr<const CylinderBounds> m_bounds;  //!< bounds (shared)

 private:
  /// Clone method implementation
  ///
  /// @param gctx The current geometry context object, e.g. alignment
  /// @param shift applied to the surface
  CylinderSurface* clone_impl(const GeometryContext& gctx,
                              const Transform3D& shift) const override;
};

#include "Acts/Surfaces/detail/CylinderSurface.ipp"

}  // namespace Acts
