// This file is part of the Acts project.
//
// Copyright (C) 2019 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// clang-format off
#define BOOST_TEST_MODULE PlanarModuleStepper Tests
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on

#include "Acts/Utilities/Units.hpp"
#include "Acts/Geometry/GeometryContext.hpp"
#include "Acts/Surfaces/Surface.hpp"
#include "Acts/Surfaces/PlanarBounds.hpp"
#include "Acts/Surfaces/RectangleBounds.hpp"
#include "Acts/Tests/CommonHelpers/FloatComparisons.hpp"
#include "Acts/Plugins/Digitization/CartesianSegmentation.hpp"
#include "Acts/Plugins/Digitization/DigitizationModule.hpp"
#include "Acts/Plugins/Digitization/PlanarModuleStepper.hpp"

namespace bdata = boost::unit_test::data;
namespace tt = boost::test_tools;
using namespace Acts::UnitLiterals;

namespace Acts {
namespace Test {

double halfX = 5_mm;
double halfY = 10_mm;
size_t ntests = 100;
size_t nbinsx = 100;
size_t nbinsy = 200;
double hThickness = 75_um;
double lAngle = 0.1;
double tanAlpha = tan(lAngle);
double sguardX = 2 * hThickness * abs(tanAlpha);

// Module bounds
auto moduleBounds = std::make_shared<const RectangleBounds>(halfX, halfY);
auto cSegmentation =
    std::make_shared<const CartesianSegmentation>(moduleBounds, nbinsx, nbinsy);

// Create digitisation modules
// (1) positive readout
DigitizationModule pdModule(cSegmentation, hThickness, 1, lAngle, 0., true);
// (2) negative readout
DigitizationModule ndModule(cSegmentation, hThickness, -1, lAngle, 0., true);
std::vector<DigitizationModule> testModules = {std::move(pdModule),
                                               std::move(ndModule)};

/// The Planar module stepper
PlanarModuleStepper pmStepper;

// Create a test context
GeometryContext tgContext = GeometryContext();

/// The following test checks test cases where the entry and exit is
/// guaranteed to be in on the readout/counter plane
BOOST_DATA_TEST_CASE(
    readout_counter_test,
    bdata::random((bdata::seed = 0,
                   bdata::distribution = std::uniform_real_distribution<>(
                       -halfX + sguardX, halfX - sguardX))) ^
        bdata::random((bdata::seed = 1,
                       bdata::distribution = std::uniform_real_distribution<>(
                           -halfX + sguardX, halfX - sguardX))) ^
        bdata::random((bdata::seed = 2,
                       bdata::distribution =
                           std::uniform_real_distribution<>(-halfY, halfY))) ^
        bdata::random((bdata::seed = 3,
                       bdata::distribution =
                           std::uniform_int_distribution<>(-halfY, halfY))) ^
        bdata::xrange(ntests),
    entryX, entryY, exitX, exitY, index) {
  // avoid warning with void
  (void)index;

  // Entry and exit point
  Vector3D entry(entryX, entryY, -hThickness);
  Vector3D exit(exitX, exitY, hThickness);

  // test the module flavours
  for (auto& dm : testModules) {
    // retrieve the digitiztion steps
    auto cSteps = pmStepper.cellSteps(tgContext, dm, entry, exit);
    BOOST_TEST(cSteps.size() != 0);

    // Test if the longitudinal distance between first and last step
    // is equal/close to the thickness of the module
    auto fPosition = cSteps.begin()->stepEntry;
    auto lPosition = cSteps.rbegin()->stepExit;
    double zDiff = (lPosition - fPosition).z();

    CHECK_CLOSE_REL(zDiff, 2 * hThickness, 10e-6);
  }
}

}  // namespace Test
}  // namespace Acts
