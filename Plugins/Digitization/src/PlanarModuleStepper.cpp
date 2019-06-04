// This file is part of the Acts project.
//
// Copyright (C) 2016-2018 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

///////////////////////////////////////////////////////////////////
// PlanarModuleStepper.cpp, Acts project
///////////////////////////////////////////////////////////////////

#include "Acts/Plugins/Digitization/PlanarModuleStepper.hpp"
#include <limits>
#include "Acts/Plugins/Digitization/DigitizationModule.hpp"
#include "Acts/Surfaces/Surface.hpp"
#include "Acts/Utilities/Definitions.hpp"
#include "Acts/Utilities/Intersection.hpp"

Acts::PlanarModuleStepper::PlanarModuleStepper(
    std::unique_ptr<const Logger> mlogger)
    : m_logger(std::move(mlogger)) {}

std::vector<Acts::DigitizationStep> Acts::PlanarModuleStepper::cellSteps(
    const GeometryContext& gctx, const DigitizationModule& dmodule,
    const Vector3D& startPoint, const Vector3D& endPoint) const {
  // create the return vector
  std::vector<DigitizationStep> cSteps;

  // get the test surfaces for bin intersections
  auto& stepSurfaces = dmodule.stepSurfaces(startPoint, endPoint);

  // the track direction
  Vector3D trackDirection((endPoint - startPoint).normalized());

  // the intersections through the surfaces, start one is the first valid one
  std::vector<Acts::Intersection> stepIntersections;
  stepIntersections.reserve(stepSurfaces.size() + 1);

  // run them - and check for the fast exit
  for (auto& sSurface : stepSurfaces) {
    // try it out by intersecting, but do not force the direction
    Acts::Intersection sIntersection =
        sSurface->intersectionEstimate(gctx, startPoint, trackDirection, true,
                                       std::numeric_limits<double>::max());
    if (sIntersection) {
      // now record
      stepIntersections.push_back(sIntersection);
      ACTS_VERBOSE("Boundary Surface intersected with = "
                   << sIntersection.position.x() << ", "
                   << sIntersection.position.y() << ", "
                   << sIntersection.position.z());
    }
  }
  // last one is also valid - now sort
  stepIntersections.push_back(Intersection(
      endPoint, (startPoint - endPoint).norm(), IntersectionStatus::reachable));
  std::sort(stepIntersections.begin(), stepIntersections.end());

  Vector3D lastPosition = startPoint;
  // reserve the right amount
  cSteps.reserve(stepIntersections.size());
  for (auto& sIntersection : stepIntersections) {
    // create the new digitization step
    cSteps.push_back(
        dmodule.digitizationStep(lastPosition, sIntersection.position));
    lastPosition = sIntersection.position;
  }
  // return all the steps
  return cSteps;
}

// calculate the steps caused by this track - fast simulation interface
std::vector<Acts::DigitizationStep> Acts::PlanarModuleStepper::cellSteps(
    const GeometryContext& gctx, const Acts::DigitizationModule& dmodule,
    const Vector2D& moduleIntersection, const Vector3D& trackDirection) const {
  // first, intersect the boundary surfaces
  auto boundarySurfaces = dmodule.boundarySurfaces();
  // intersect them - fast exit for cases where
  // readout and counter readout are hit
  Vector3D intersection3D(moduleIntersection.x(), moduleIntersection.y(), 0.);
  size_t attempts = 0;
  // the collected intersections
  std::vector<Acts::Intersection> boundaryIntersections;
  // run them - and check for the fast exit
  for (auto& bSurface : boundarySurfaces) {
    // count as an attempt
    ++attempts;
    // try it out by intersecting, but do not force the direction
    Acts::Intersection bIntersection = bSurface->intersectionEstimate(
        gctx, intersection3D, trackDirection, true,
        std::numeric_limits<double>::max());
    if (bIntersection) {
      // now record
      boundaryIntersections.push_back(bIntersection);
      ACTS_VERBOSE("Boundary Surface intersected with = "
                   << bIntersection.position.x() << ", "
                   << bIntersection.position.y() << ", "
                   << bIntersection.position.z());
    }
    // fast break in case of readout/counter surface hit
    // the first two attempts are the module faces, if they are hit,
    // the stepper has run ok.
    if (attempts == 2 && boundaryIntersections.size() == attempts) {
      break;
    }
  }
  // Post-process if we have more than 2 intersections
  // only first or last can be wrong after resorting
  if (boundaryIntersections.size() > 2) {
    ACTS_VERBOSE(
        "More than 2 Boundary Surfaces intersected, this is an edge "
        "case, resolving ... ");
    std::sort(boundaryIntersections.begin(), boundaryIntersections.end());
  }
  // if for some reason the intersection does not work
  if (boundaryIntersections.empty()) {
    return std::vector<Acts::DigitizationStep>();
  }
  // return
  return cellSteps(gctx, dmodule, boundaryIntersections[0].position,
                   boundaryIntersections[1].position);
}
