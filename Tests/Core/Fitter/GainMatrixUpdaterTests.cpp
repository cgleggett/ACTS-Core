// This file is part of the Acts project.
//
// Copyright (C) 2016-2019 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#define BOOST_TEST_MODULE GainMatrixUpdater Tests
#include <boost/optional/optional_io.hpp>
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <memory>
#include "Acts/EventData/Measurement.hpp"
#include "Acts/EventData/MeasurementHelpers.hpp"
#include "Acts/EventData/TrackParameters.hpp"
#include "Acts/EventData/TrackState.hpp"
#include "Acts/Fitter/GainMatrixUpdater.hpp"
#include "Acts/Surfaces/CylinderSurface.hpp"
#include "Acts/Tests/CommonHelpers/FloatComparisons.hpp"
#include "Acts/Utilities/ParameterDefinitions.hpp"

namespace Acts {
namespace Test {

using Jacobian = FreeMatrix;
using Covariance = BoundSymMatrix;

using SourceLink = MinimalSourceLink;

template <ParID_t... params>
using MeasurementType = Measurement<SourceLink, params...>;
using TrackState = TrackState<SourceLink, BoundParameters>;

// Create a test context
GeometryContext tgContext = GeometryContext();

BOOST_AUTO_TEST_CASE(gain_matrix_updater) {
  // Make dummy measurement
  auto cylinder = Surface::makeShared<CylinderSurface>(nullptr, 3, 10);

  ActsSymMatrixD<2> cov;
  cov << 0.04, 0, 0, 0.1;
  FittableMeasurement<SourceLink> meas(
      MeasurementType<ParDef::eLOC_0, ParDef::eLOC_1>(
          cylinder, {}, std::move(cov), -0.1, 0.45));

  TrackState mState{SourceLink{&meas}};

  // Make dummy track parameter
  Covariance covTrk;
  covTrk.setZero();
  covTrk.diagonal() << 0.08, 0.3, 1, 1, 1, 0;
  BoundVector parValues;
  parValues << 0.3, 0.5, 0.5 * M_PI, 0.3 * M_PI, 0.01, 0.;
  BoundParameters pars(
      tgContext,
      std::make_unique<const BoundParameters::CovMatrix_t>(std::move(covTrk)),
      parValues, cylinder);

  // "update" track state with "prediction"
  mState.parameter.predicted = std::move(pars);
  mState.parameter.jacobian = Jacobian::Identity();
  mState.parameter.pathLength = 0.;

  // Gain matrix update and filtered state
  GainMatrixUpdater<BoundParameters> gmu;

  BOOST_CHECK(!mState.parameter.filtered);
  BOOST_CHECK(!mState.measurement.calibrated);
  BOOST_CHECK(gmu(tgContext, mState));
  // filtered is set now
  BOOST_CHECK(!!mState.parameter.filtered);
  // measurement was calibrated
  BOOST_CHECK(!!mState.measurement.calibrated);
  // ref surface is same on measurements and parameters
  BOOST_CHECK_EQUAL(
      MeasurementHelpers::getSurface(*mState.measurement.calibrated),
      cylinder.get());
  BOOST_CHECK_EQUAL(&(*mState.parameter.filtered).referenceSurface(),
                    cylinder.get());

  // Check for regression. This does NOT test if the math is correct, just that
  // the result is the same as when the test was written.

  Covariance expCov;
  expCov.setZero();
  expCov.diagonal() << 0.0266667, 0.0750000, 1.0000000, 1.0000000, 1.0000000,
      0.0000000;

  BoundVector expPar;
  expPar << 0.0333333, 0.4625000, 1.5707963, 0.9424778, 0.0100000, 0.0000000;

  Vector3D expPosition;
  expPosition << 2.9998148, 0.0333326, 0.4625000;

  Vector3D expMomentum;
  expMomentum << 0.0000000, 80.9016994, 58.7785252;

  auto& filtered = *mState.parameter.filtered;

  double expChi2 = 1.33958;

  double tol = 1e-6;

  CHECK_CLOSE_ABS(expCov, *filtered.covariance(), tol);
  CHECK_CLOSE_ABS(expPar, filtered.parameters(), tol);
  CHECK_CLOSE_ABS(expPosition, filtered.position(), tol);
  CHECK_CLOSE_ABS(expMomentum, filtered.momentum(), tol);
  CHECK_CLOSE_ABS(expChi2, mState.parameter.chi2, 1e-4);
}

}  // namespace Test
}  // namespace Acts
