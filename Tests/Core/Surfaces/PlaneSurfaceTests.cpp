// This file is part of the Acts project.
//
// Copyright (C) 2017-2018 Acts project team
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#define BOOST_TEST_MODULE PlaneSurface Tests

#include <boost/test/included/unit_test.hpp>
// leave blank line

#include <boost/test/data/test_case.hpp>
// leave blank line

#include <boost/test/output_test_stream.hpp>
// leave blank line

#include <limits>
#include "Acts/Surfaces/PlaneSurface.hpp"
#include "Acts/Surfaces/RectangleBounds.hpp"
#include "Acts/Surfaces/TriangleBounds.hpp"
#include "Acts/Tests/CommonHelpers/DetectorElementStub.hpp"
#include "Acts/Utilities/Definitions.hpp"
#include "Acts/Utilities/VariantData.hpp"

namespace tt = boost::test_tools;
using boost::test_tools::output_test_stream;
namespace utf = boost::unit_test;

namespace Acts {

namespace Test {
  BOOST_AUTO_TEST_SUITE(PlaneSurfaces)
  /// Unit test for creating compliant/non-compliant PlaneSurface object
  BOOST_AUTO_TEST_CASE(PlaneSurfaceConstruction)
  {
    // PlaneSurface default constructor is deleted
    // bounds object, rectangle type
    auto rBounds = std::make_shared<const RectangleBounds>(3., 4.);
    /// Constructor with transform pointer, null or valid, alpha and symmetry
    /// indicator
    Translation3D translation{0., 1., 2.};
    auto          pTransform     = make_shared_transform(translation);
    auto          pNullTransform = make_shared_transform();
    // constructor with nullptr transform
    BOOST_TEST(
        Surface::makeShared<PlaneSurface>(pNullTransform, rBounds)->type()
        == Surface::Plane);
    // constructor with transform
    BOOST_TEST(Surface::makeShared<PlaneSurface>(pTransform, rBounds)->type()
               == Surface::Plane);
    /// Copy constructor
    auto planeSurfaceObject
        = Surface::makeShared<PlaneSurface>(pTransform, rBounds);
    auto copiedPlaneSurface
        = Surface::makeShared<PlaneSurface>(*planeSurfaceObject);
    BOOST_TEST(copiedPlaneSurface->type() == Surface::Plane);
    BOOST_TEST(*copiedPlaneSurface == *planeSurfaceObject);
    //
    /// Copied and transformed
    auto copiedTransformedPlaneSurface
        = Surface::makeShared<PlaneSurface>(*planeSurfaceObject, *pTransform);
    BOOST_TEST(copiedTransformedPlaneSurface->type() == Surface::Plane);

    /// Construct with nullptr bounds
    DetectorElementStub detElem;
    BOOST_CHECK_THROW(auto nullBounds
                      = Surface::makeShared<PlaneSurface>(nullptr, detElem),
                      AssertionFailureException);
  }
  //
  /// Unit test for testing PlaneSurface properties
  BOOST_AUTO_TEST_CASE(PlaneSurfaceProperties)
  {
    double withinOnePercent = 0.01;
    // bounds object, rectangle type
    auto rBounds = std::make_shared<const RectangleBounds>(3., 4.);
    /// Test clone method
    Translation3D translation{0., 1., 2.};
    auto          pTransform = make_shared_transform(translation);
    // auto pNullTransform = make_shared_transform();
    auto planeSurfaceObject
        = Surface::makeShared<PlaneSurface>(pTransform, rBounds);
    //
    auto pClonedPlaneSurface = planeSurfaceObject->clone();
    BOOST_TEST(pClonedPlaneSurface->type() == Surface::Plane);
    // Test clone method with translation
    auto pClonedShiftedPlaneSurface
        = planeSurfaceObject->clone(pTransform.get());
    // Does it exist at all in a decent state?
    BOOST_TEST(pClonedShiftedPlaneSurface->type() == Surface::Plane);
    // Is it in the right place?
    Translation3D translation2{0., 2., 4.};
    auto          pTransform2 = make_shared_transform(translation2);
    auto          planeSurfaceObject2
        = Surface::makeShared<PlaneSurface>(pTransform2, rBounds);
    // these two surfaces should be equivalent now (prematurely testing equality
    // also)
    BOOST_TEST(*pClonedShiftedPlaneSurface == *planeSurfaceObject2);
    // and, trivially, the shifted cloned surface should be different from the
    // original
    BOOST_TEST(*pClonedShiftedPlaneSurface != *planeSurfaceObject);
    //
    /// Test type (redundant)
    BOOST_TEST(planeSurfaceObject->type() == Surface::Plane);
    //
    /// Test binningPosition
    Vector3D binningPosition{0., 1., 2.};
    BOOST_TEST(planeSurfaceObject->binningPosition(BinningValue::binX)
               == binningPosition);
    //
    /// Test referenceFrame
    Vector3D         globalPosition{2.0, 2.0, 0.0};
    Vector3D         momentum{1.e6, 1.e6, 1.e6};
    RotationMatrix3D expectedFrame;
    expectedFrame << 1., 0., 0., 0., 1., 0., 0., 0., 1.;

    BOOST_TEST(planeSurfaceObject->referenceFrame(globalPosition, momentum)
                   .isApprox(expectedFrame));
    //
    /// Test normal, given 3D position
    Vector3D normal3D(0., 0., 1.);
    BOOST_TEST(planeSurfaceObject->normal() == normal3D);
    //
    /// Test bounds
    BOOST_TEST(planeSurfaceObject->bounds().type() == SurfaceBounds::Rectangle);

    /// Test localToGlobal
    Vector2D localPosition{1.5, 1.7};
    planeSurfaceObject->localToGlobal(localPosition, momentum, globalPosition);
    //
    // expected position is the translated one
    Vector3D expectedPosition{
        1.5 + translation.x(), 1.7 + translation.y(), translation.z()};

    BOOST_TEST(globalPosition.isApprox(expectedPosition, withinOnePercent));
    //
    /// Testing globalToLocal
    planeSurfaceObject->globalToLocal(globalPosition, momentum, localPosition);
    Vector2D expectedLocalPosition{1.5, 1.7};

    BOOST_TEST(localPosition.isApprox(expectedLocalPosition, withinOnePercent),
               "Testing globalToLocal");

    /// Test isOnSurface
    Vector3D offSurface{0, 1, -2.};
    BOOST_TEST(planeSurfaceObject->isOnSurface(globalPosition, momentum, true)
               == true);
    BOOST_TEST(planeSurfaceObject->isOnSurface(offSurface, momentum, true)
               == false);
    //
    /// intersectionEstimate
    Vector3D direction{0., 0., 1.};
    auto     intersect = planeSurfaceObject->intersectionEstimate(
        offSurface, direction, forward, true);
    Intersection expectedIntersect{Vector3D{0, 1, 2}, 4., true, 0};
    BOOST_TEST(intersect.valid);
    BOOST_TEST(intersect.position == expectedIntersect.position);
    BOOST_TEST(intersect.pathLength == expectedIntersect.pathLength);
    BOOST_TEST(intersect.distance == expectedIntersect.distance);
    //
    /// Test pathCorrection
    // BOOST_TEST(planeSurfaceObject.pathCorrection(offSurface, momentum)
    //               == 0.40218866453252877,
    //           tt::tolerance(0.01));
    //
    /// Test name
    BOOST_TEST(planeSurfaceObject->name() == std::string("Acts::PlaneSurface"));
    //
    /// Test dump
    // TODO 2017-04-12 msmk: check how to correctly check output
    //    boost::test_tools::output_test_stream dumpOuput;
    //    planeSurfaceObject.dump(dumpOuput);
    //    BOOST_TEST(dumpOuput.is_equal(
    //      "Acts::PlaneSurface\n"
    //      "    Center position  (x, y, z) = (0.0000, 1.0000, 2.0000)\n"
    //      "    Rotation:             colX = (1.000000, 0.000000, 0.000000)\n"
    //      "                          colY = (0.000000, 1.000000, 0.000000)\n"
    //      "                          colZ = (0.000000, 0.000000, 1.000000)\n"
    //      "    Bounds  : Acts::ConeBounds: (tanAlpha, minZ, maxZ, averagePhi,
    //      halfPhiSector) = (0.4142136, 0.0000000, inf, 0.0000000,
    //      3.1415927)"));
  }

  BOOST_AUTO_TEST_CASE(EqualityOperators)
  {
    // rectangle bounds
    auto          rBounds = std::make_shared<const RectangleBounds>(3., 4.);
    Translation3D translation{0., 1., 2.};
    auto          pTransform = make_shared_transform(translation);
    auto          planeSurfaceObject
        = Surface::makeShared<PlaneSurface>(pTransform, rBounds);
    auto planeSurfaceObject2
        = Surface::makeShared<PlaneSurface>(pTransform, rBounds);
    //
    /// Test equality operator
    BOOST_TEST(*planeSurfaceObject == *planeSurfaceObject2);
    //
    BOOST_TEST_CHECKPOINT(
        "Create and then assign a PlaneSurface object to the existing one");
    /// Test assignment
    auto assignedPlaneSurface
        = Surface::makeShared<PlaneSurface>(nullptr, nullptr);
    *assignedPlaneSurface = *planeSurfaceObject;
    /// Test equality of assigned to original
    BOOST_TEST(*assignedPlaneSurface == *planeSurfaceObject);
  }

  BOOST_AUTO_TEST_CASE(PlaneSurface_Serialization)
  {
    // build
    auto rectBounds = std::make_shared<const RectangleBounds>(5, 10);
    auto idTrf      = make_shared_transform(Transform3D::Identity());
    auto rot = make_shared_transform(AngleAxis3D(M_PI / 4., Vector3D::UnitZ()));
    auto rectSrf = Surface::makeShared<PlaneSurface>(rot, rectBounds);
    variant_data rectVariant = rectSrf->toVariantData();
    std::cout << rectVariant << std::endl;

    // rebuild from variant
    auto rectSrfRec = Surface::makeShared<PlaneSurface>(rectVariant);
    auto rectBoundsRec
        = dynamic_cast<const RectangleBounds*>(&rectSrfRec->bounds());
    BOOST_CHECK_CLOSE(
        rectBounds->halflengthX(), rectBoundsRec->halflengthX(), 1e-4);
    BOOST_CHECK_CLOSE(
        rectBounds->halflengthY(), rectBoundsRec->halflengthY(), 1e-4);
    BOOST_TEST(rot->isApprox(rectSrfRec->transform(), 1e-4));

    std::array<Vector2D, 3> vertices
        = {{Vector2D(1, 1), Vector2D(1, -1), Vector2D(-1, 1)}};
    auto triangleBounds = std::make_shared<const TriangleBounds>(vertices);
    auto triangleSrf = Surface::makeShared<PlaneSurface>(rot, triangleBounds);
    variant_data triangleVariant = triangleSrf->toVariantData();
    std::cout << triangleVariant << std::endl;

    // rebuild
    auto triangleSrfRec = Surface::makeShared<PlaneSurface>(triangleVariant);
    auto triangleBoundsRec
        = dynamic_cast<const TriangleBounds*>(&triangleSrfRec->bounds());
    for (size_t i = 0; i < 3; i++) {
      Vector2D exp = triangleBounds->vertices().at(i);
      Vector2D act = triangleBoundsRec->vertices().at(i);
      BOOST_CHECK_CLOSE(exp.x(), act.x(), 1e-4);
      BOOST_CHECK_CLOSE(exp.y(), act.y(), 1e-4);
    }
    BOOST_TEST(rot->isApprox(triangleSrfRec->transform(), 1e-4));
  }

  BOOST_AUTO_TEST_SUITE_END()

}  // namespace Test

}  // namespace Acts
