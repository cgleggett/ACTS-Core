// This file is part of the Acts project.
//
// Copyright (C) 2017-2018 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#define BOOST_TEST_MODULE Layer Tests

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
// leave blank line

#include <boost/test/data/test_case.hpp>
// leave blank line

#include <boost/test/output_test_stream.hpp>
// leave blank line

//#include <limits>
#include "Acts/EventData/SingleTrackParameters.hpp"
#include "Acts/Geometry/CuboidVolumeBounds.hpp"
#include "Acts/Geometry/GenericApproachDescriptor.hpp"
#include "Acts/Geometry/GeometryContext.hpp"
#include "Acts/Geometry/Layer.hpp"
#include "Acts/Geometry/SurfaceArrayCreator.hpp"
#include "Acts/Surfaces/PlaneSurface.hpp"
#include "Acts/Surfaces/RectangleBounds.hpp"
#include "LayerStub.hpp"

using boost::test_tools::output_test_stream;
namespace utf = boost::unit_test;

namespace Acts {

namespace Test {

// Create a test context
GeometryContext tgContext = GeometryContext();

namespace Layers {

BOOST_AUTO_TEST_SUITE(Layers)

/// Unit test for creating compliant/non-compliant Layer object
BOOST_AUTO_TEST_CASE(LayerConstruction) {
  // Descendant Layer objects also inherit from Surface objects, which
  // delete the default constructor
  //
  /// Minimum possible construction (default constructor is deleted)
  LayerStub minallyConstructed(nullptr);
  BOOST_CHECK(minallyConstructed.constructedOk());
  /// Need an approach descriptor for the next level of complexity:
  std::vector<std::shared_ptr<const Surface>> aSurfaces{
      Surface::makeShared<SurfaceStub>(), Surface::makeShared<SurfaceStub>()};
  std::unique_ptr<ApproachDescriptor> ad(
      new GenericApproachDescriptor(aSurfaces));
  const double thickness(1.0);
  LayerStub approachDescriptorConstructed(nullptr, thickness, std::move(ad));
  /// Construction with (minimal) approach descriptor
  BOOST_CHECK(approachDescriptorConstructed.constructedOk());
  // Copy construction is deleted
}

/// Unit test for testing Layer properties
BOOST_AUTO_TEST_CASE(LayerProperties, *utf::expected_failures(1)) {
  // Make a dummy layer to play with
  // bounds object, rectangle type
  auto rBounds = std::make_shared<const RectangleBounds>(1., 1.);
  /// Constructor with transform pointer
  auto pNullTransform = std::make_shared<const Transform3D>();
  const std::vector<std::shared_ptr<const Surface>> aSurfaces{
      Surface::makeShared<PlaneSurface>(pNullTransform, rBounds),
      Surface::makeShared<PlaneSurface>(pNullTransform, rBounds)};
  std::unique_ptr<ApproachDescriptor> ad(
      new GenericApproachDescriptor(aSurfaces));
  auto adPtr = ad.get();
  const double thickness(1.0);
  LayerStub layerStub(nullptr, thickness, std::move(ad));
  //
  /// surfaceArray()
  BOOST_CHECK_EQUAL(layerStub.surfaceArray(), nullptr);
  /// thickness()
  BOOST_CHECK_EQUAL(layerStub.thickness(), thickness);
  // onLayer() is templated; can't find implementation!
  /// isOnLayer() (delegates to the Surface 'isOnSurface()')
  const Vector3D pos{0.0, 0.0, 0.0};
  const Vector3D pos2{100., 100., std::nan("")};
  BOOST_CHECK(layerStub.isOnLayer(tgContext, pos));
  // this should fail, but does not, but possibly my fault in SurfaceStub
  // implementation:
  BOOST_CHECK(!layerStub.isOnLayer(tgContext, pos2));
  /// approachDescriptor(), retrieved as a pointer.
  BOOST_CHECK_EQUAL(layerStub.approachDescriptor(), adPtr);
  const Vector3D gpos{0., 0., 1.0};
  const Vector3D direction{0., 0., -1.};
  /// nextLayer()
  BOOST_CHECK(!(layerStub.nextLayer(tgContext, gpos, direction)));
  /// trackingVolume()
  BOOST_CHECK(!layerStub.trackingVolume());
  // BOOST_TEST_CHECKPOINT("Before ending test");
  // deletion results in "memory access violation at address: 0x00000071: no
  // mapping at fault address"
  // delete abstractVolumePtr;
  /// layerType()
  BOOST_CHECK_EQUAL(layerStub.layerType(), LayerType::passive);
}

BOOST_AUTO_TEST_SUITE_END()
}  // namespace Layers
}  // namespace Test

}  // namespace Acts
