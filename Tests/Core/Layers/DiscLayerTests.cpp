// This file is part of the Acts project.
//
// Copyright (C) 2017-2018 Acts project team
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#define BOOST_TEST_MODULE Layer Tests

#include <boost/test/included/unit_test.hpp>
// leave blank line

#include <boost/test/data/test_case.hpp>
// leave blank line

#include <boost/test/output_test_stream.hpp>
// leave blank line

//#include <limits>
#include "Acts/Layers/DiscLayer.hpp"
#include "Acts/Surfaces/RadialBounds.hpp"
//#include "Acts/Utilities/Definitions.hpp"
#include "Acts/EventData/SingleTrackParameters.hpp"
#include "Acts/Layers/GenericApproachDescriptor.hpp"
#include "Acts/Surfaces/PlaneSurface.hpp"
#include "Acts/Tools/SurfaceArrayCreator.hpp"
#include "Acts/Utilities/VariantData.hpp"
#include "Acts/Volumes/CuboidVolumeBounds.hpp"
#include "Acts/Volumes/CylinderVolumeBounds.hpp"
#include "LayerStub.hpp"

using boost::test_tools::output_test_stream;
namespace utf = boost::unit_test;

namespace Acts {

namespace Test {
  namespace Layers {
    BOOST_AUTO_TEST_SUITE(Layers)

    /// Unit test for creating compliant/non-compliant DiscLayer object
    BOOST_AUTO_TEST_CASE(DiscLayerConstruction)
    {
      // default constructor, copy and assignment are all deleted
      // minimally need a Transform3D and a PlanarBounds object (e.g.
      // RadialBounds) to construct
      Translation3D translation{0., 1., 2.};
      auto          pTransform = make_shared_transform(translation);
      const double  minRad(10.), maxRad(5.);  // 20 x 10 disc
      auto pDisc      = std::make_shared<const RadialBounds>(minRad, maxRad);
      auto pDiscLayer = DiscLayer::create(pTransform, pDisc);
      BOOST_TEST(pDiscLayer->layerType() == LayerType::passive);
      // next level: need an array of Surfaces;
      // bounds object, rectangle type
      auto rBounds = std::make_shared<const RectangleBounds>(1., 1.);
      /// Constructor with transform pointer
      auto pNullTransform = make_shared_transform();
      const std::vector<std::shared_ptr<const Surface>> aSurfaces{
          Surface::makeShared<PlaneSurface>(pNullTransform, rBounds),
          Surface::makeShared<PlaneSurface>(pNullTransform, rBounds)};
      const double thickness(1.0);
      auto         pDiscLayerFromSurfaces
          = DiscLayer::create(pTransform, pDisc, nullptr);
      BOOST_TEST(pDiscLayerFromSurfaces->layerType() == LayerType::passive);
      // construct with thickness:
      auto pDiscLayerWithThickness
          = DiscLayer::create(pTransform, pDisc, nullptr, thickness);
      BOOST_TEST(pDiscLayerWithThickness->thickness() == thickness);
      // with an approach descriptor...
      std::unique_ptr<ApproachDescriptor> ad(
          new GenericApproachDescriptor(aSurfaces));
      auto adPtr                            = ad.get();
      auto pDiscLayerWithApproachDescriptor = DiscLayer::create(
          pTransform, pDisc, nullptr, thickness, std::move(ad));
      BOOST_TEST(pDiscLayerWithApproachDescriptor->approachDescriptor()
                 == adPtr);
      // with the layerType specified...
      auto pDiscLayerWithLayerType = DiscLayer::create(pTransform,
                                                       pDisc,
                                                       nullptr,
                                                       thickness,
                                                       std::move(ad),
                                                       LayerType::passive);
      BOOST_TEST(pDiscLayerWithLayerType->layerType() == LayerType::passive);
    }

    /// Unit test for testing Layer properties
    BOOST_AUTO_TEST_CASE(DiscLayerProperties /*, *utf::expected_failures(1)*/)
    {
      Translation3D translation{0., 1., 2.};
      auto          pTransform = make_shared_transform(translation);
      const double  minRad(10.), maxRad(5.);  // 20 x 10 disc
      auto pDisc      = std::make_shared<const RadialBounds>(minRad, maxRad);
      auto pDiscLayer = DiscLayer::create(pTransform, pDisc);
      // auto planeSurface = pDiscLayer->surfaceRepresentation();
      BOOST_TEST(pDiscLayer->surfaceRepresentation().name()
                 == std::string("Acts::DiscSurface"));
    }

    BOOST_AUTO_TEST_CASE(DiscLayer_toVariantData)
    {
      Translation3D translation{0., 1., 2.};
      auto          pTransform = make_shared_transform(translation);
      const double  minRad(10.), maxRad(5.);  // 20 x 10 disc
      auto pDisc      = std::make_shared<const RadialBounds>(minRad, maxRad);
      auto pDiscLayer = std::dynamic_pointer_cast<DiscLayer>(
          DiscLayer::create(pTransform, pDisc, nullptr, 6));

      variant_data var_data = pDiscLayer->toVariantData();
      std::cout << var_data << std::endl;

      variant_map var_map = boost::get<variant_map>(var_data);
      variant_map pl      = var_map.get<variant_map>("payload");
      BOOST_TEST(pl.get<double>("thickness") == 6);
      Transform3D act = from_variant<Transform3D>(pl.at("transform"));
      BOOST_TEST(pTransform->isApprox(act));

      auto pDiscLayer2
          = std::dynamic_pointer_cast<DiscLayer>(DiscLayer::create(var_data));

      BOOST_TEST(pDiscLayer->thickness() == pDiscLayer2->thickness());
      BOOST_TEST(pDiscLayer->transform().isApprox(pDiscLayer2->transform()));

      auto cvBoundsExp = dynamic_cast<const CylinderVolumeBounds*>(
          &(pDiscLayer->representingVolume()->volumeBounds()));
      auto cvBoundsAct = dynamic_cast<const CylinderVolumeBounds*>(
          &(pDiscLayer2->representingVolume()->volumeBounds()));

      BOOST_TEST(cvBoundsExp->innerRadius() == cvBoundsAct->innerRadius());
      BOOST_TEST(cvBoundsExp->outerRadius() == cvBoundsAct->outerRadius());
      BOOST_TEST(cvBoundsExp->halfPhiSector() == cvBoundsAct->halfPhiSector());
      BOOST_TEST(cvBoundsExp->halflengthZ() == cvBoundsAct->halflengthZ());
    }

    BOOST_AUTO_TEST_SUITE_END()
  }  // namespace Layers
}  // namespace Test

}  // namespace Acts
