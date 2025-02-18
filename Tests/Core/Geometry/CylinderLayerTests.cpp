// This file is part of the Acts project.
//
// Copyright (C) 2017-2018 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// clang-format off
#define BOOST_TEST_MODULE Layer Tests
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/output_test_stream.hpp>
// clang-format on

#include "Acts/EventData/SingleTrackParameters.hpp"
#include "Acts/Geometry/CylinderLayer.hpp"
#include "Acts/Geometry/GenericApproachDescriptor.hpp"
#include "Acts/Surfaces/CylinderBounds.hpp"
#include "Acts/Surfaces/PlaneSurface.hpp"
#include "Acts/Surfaces/RectangleBounds.hpp"
#include "Acts/Tests/CommonHelpers/FloatComparisons.hpp"
#include "Acts/Geometry/SurfaceArrayCreator.hpp"
#include "Acts/Utilities/Definitions.hpp"
#include "Acts/Geometry/CuboidVolumeBounds.hpp"

#include "LayerStub.hpp"

using boost::test_tools::output_test_stream;
namespace utf = boost::unit_test;

namespace Acts {

namespace Test {

// Create a test context
GeometryContext tgContext = GeometryContext();

namespace Layers {
BOOST_AUTO_TEST_SUITE(Layers)

/// Unit test for creating compliant/non-compliant CylinderLayer object
BOOST_AUTO_TEST_CASE(CylinderLayerConstruction) {
  // default constructor, copy and assignment are all deleted
  // minimally need a Transform3D and a PlanarBounds object (e.g.
  // CylinderBounds) to construct
  Translation3D translation{0., 1., 2.};
  auto pTransform = std::make_shared<const Transform3D>(translation);
  double radius(0.5), halfz(10.);
  auto pCylinder = std::make_shared<const CylinderBounds>(radius, halfz);
  auto pCylinderLayer = CylinderLayer::create(pTransform, pCylinder);
  BOOST_CHECK_EQUAL(pCylinderLayer->layerType(), LayerType::passive);
  // next level: need an array of Surfaces;
  // bounds object, rectangle type
  auto rBounds = std::make_shared<const RectangleBounds>(1., 1.);
  /// Constructor with transform pointer
  auto pNullTransform = std::make_shared<const Transform3D>();
  const std::vector<std::shared_ptr<const Surface>> aSurfaces{
      Surface::makeShared<PlaneSurface>(pNullTransform, rBounds),
      Surface::makeShared<PlaneSurface>(pNullTransform, rBounds)};
  const double thickness(1.0);
  auto pCylinderLayerFromSurfaces =
      CylinderLayer::create(pTransform, pCylinder, nullptr);
  BOOST_CHECK_EQUAL(pCylinderLayerFromSurfaces->layerType(),
                    LayerType::passive);
  // construct with thickness:
  auto pCylinderLayerWithThickness =
      CylinderLayer::create(pTransform, pCylinder, nullptr, thickness);
  CHECK_CLOSE_REL(pCylinderLayerWithThickness->thickness(), thickness, 1e-6);
  // with an approach descriptor...
  std::unique_ptr<ApproachDescriptor> ad(
      new GenericApproachDescriptor(aSurfaces));
  auto adPtr = ad.get();
  auto pCylinderLayerWithApproachDescriptor = CylinderLayer::create(
      pTransform, pCylinder, nullptr, thickness, std::move(ad));
  BOOST_CHECK_EQUAL(pCylinderLayerWithApproachDescriptor->approachDescriptor(),
                    adPtr);
  // with the layerType specified...
  auto pCylinderLayerWithLayerType =
      CylinderLayer::create(pTransform, pCylinder, nullptr, thickness,
                            std::move(ad), LayerType::passive);
  BOOST_CHECK_EQUAL(pCylinderLayerWithLayerType->layerType(),
                    LayerType::passive);
}

/// Unit test for testing Layer properties
BOOST_AUTO_TEST_CASE(CylinderLayerProperties /*, *utf::expected_failures(1)*/) {
  Translation3D translation{0., 1., 2.};
  auto pTransform = std::make_shared<const Transform3D>(translation);
  double radius(0.5), halfz(10.);
  auto pCylinder = std::make_shared<const CylinderBounds>(radius, halfz);
  auto pCylinderLayer = CylinderLayer::create(pTransform, pCylinder);
  // auto planeSurface = pCylinderLayer->surfaceRepresentation();
  BOOST_CHECK_EQUAL(pCylinderLayer->surfaceRepresentation().name(),
                    std::string("Acts::CylinderSurface"));
}

BOOST_AUTO_TEST_SUITE_END()
}  // namespace Layers
}  // namespace Test

}  // namespace Acts
