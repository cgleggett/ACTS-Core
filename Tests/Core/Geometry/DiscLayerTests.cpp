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
#include "Acts/Geometry/DiscLayer.hpp"
#include "Acts/Geometry/GenericApproachDescriptor.hpp"
#include "Acts/Surfaces/RadialBounds.hpp"
#include "Acts/Surfaces/PlaneSurface.hpp"
#include "Acts/Surfaces/RectangleBounds.hpp"
#include "Acts/Tests/CommonHelpers/FloatComparisons.hpp"
#include "Acts/Geometry/SurfaceArrayCreator.hpp"
#include "Acts/Geometry/CuboidVolumeBounds.hpp"
#include "Acts/Geometry/CylinderVolumeBounds.hpp"
#include "Acts/Geometry/GeometryContext.hpp"

#include "LayerStub.hpp"

using boost::test_tools::output_test_stream;
namespace utf = boost::unit_test;

namespace Acts {

namespace Test {

// Create a test context
GeometryContext tgContext = GeometryContext();

namespace Layers {
BOOST_AUTO_TEST_SUITE(Layers)

/// Unit test for creating compliant/non-compliant DiscLayer object
BOOST_AUTO_TEST_CASE(DiscLayerConstruction) {
  // default constructor, copy and assignment are all deleted
  // minimally need a Transform3D and a PlanarBounds object (e.g.
  // RadialBounds) to construct
  Translation3D translation{0., 1., 2.};
  auto pTransform = std::make_shared<const Transform3D>(translation);
  const double minRad(10.), maxRad(5.);  // 20 x 10 disc
  auto pDisc = std::make_shared<const RadialBounds>(minRad, maxRad);
  auto pDiscLayer = DiscLayer::create(pTransform, pDisc);
  BOOST_CHECK_EQUAL(pDiscLayer->layerType(), LayerType::passive);
  // next level: need an array of Surfaces;
  // bounds object, rectangle type
  auto rBounds = std::make_shared<const RectangleBounds>(1., 1.);
  /// Constructor with transform pointer
  auto pNullTransform = std::make_shared<const Transform3D>();
  const std::vector<std::shared_ptr<const Surface>> aSurfaces{
      Surface::makeShared<PlaneSurface>(pNullTransform, rBounds),
      Surface::makeShared<PlaneSurface>(pNullTransform, rBounds)};
  const double thickness(1.0);
  auto pDiscLayerFromSurfaces = DiscLayer::create(pTransform, pDisc, nullptr);
  BOOST_CHECK_EQUAL(pDiscLayerFromSurfaces->layerType(), LayerType::passive);
  // construct with thickness:
  auto pDiscLayerWithThickness =
      DiscLayer::create(pTransform, pDisc, nullptr, thickness);
  BOOST_CHECK_EQUAL(pDiscLayerWithThickness->thickness(), thickness);
  // with an approach descriptor...
  std::unique_ptr<ApproachDescriptor> ad(
      new GenericApproachDescriptor(aSurfaces));
  auto adPtr = ad.get();
  auto pDiscLayerWithApproachDescriptor =
      DiscLayer::create(pTransform, pDisc, nullptr, thickness, std::move(ad));
  BOOST_CHECK_EQUAL(pDiscLayerWithApproachDescriptor->approachDescriptor(),
                    adPtr);
  // with the layerType specified...
  auto pDiscLayerWithLayerType = DiscLayer::create(
      pTransform, pDisc, nullptr, thickness, std::move(ad), LayerType::passive);
  BOOST_CHECK_EQUAL(pDiscLayerWithLayerType->layerType(), LayerType::passive);
}

/// Unit test for testing Layer properties
BOOST_AUTO_TEST_CASE(DiscLayerProperties /*, *utf::expected_failures(1)*/) {
  Translation3D translation{0., 1., 2.};
  auto pTransform = std::make_shared<const Transform3D>(translation);
  const double minRad(10.), maxRad(5.);  // 20 x 10 disc
  auto pDisc = std::make_shared<const RadialBounds>(minRad, maxRad);
  auto pDiscLayer = DiscLayer::create(pTransform, pDisc);
  // auto planeSurface = pDiscLayer->surfaceRepresentation();
  BOOST_CHECK_EQUAL(pDiscLayer->surfaceRepresentation().name(),
                    std::string("Acts::DiscSurface"));
}

BOOST_AUTO_TEST_SUITE_END()
}  // namespace Layers
}  // namespace Test

}  // namespace Acts
