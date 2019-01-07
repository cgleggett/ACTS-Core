// This file is part of the Acts project.
//
// Copyright (C) 2017-2018 Acts project team
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#define BOOST_TEST_MODULE Surface Tests

#include <boost/test/included/unit_test.hpp>
// leave blank line

#include <boost/test/data/test_case.hpp>
// leave blank line

#include <boost/test/output_test_stream.hpp>
// leave blank line

#include <limits>
#include "Acts/Layers/PlaneLayer.hpp"
#include "Acts/Material/HomogeneousSurfaceMaterial.hpp"
#include "Acts/Surfaces/InfiniteBounds.hpp"   //to get s_noBounds
#include "Acts/Surfaces/RectangleBounds.hpp"  //to get s_noBounds
#include "Acts/Surfaces/Surface.hpp"
#include "Acts/Tests/CommonHelpers/DetectorElementStub.hpp"
#include "Acts/Utilities/Definitions.hpp"
#include "SurfaceStub.hpp"

using boost::test_tools::output_test_stream;
namespace utf = boost::unit_test;

namespace Acts {
/// Mock track object with minimal methods implemented for compilation
class MockTrack
{
public:
  MockTrack(const Vector3D& mom, const Vector3D& pos) : m_mom(mom), m_pos(pos)
  {
    // nop
  }

  Vector3D
  momentum() const
  {
    return m_mom;
  }

  Vector3D
  position() const
  {
    return m_pos;
  }

private:
  Vector3D m_mom;
  Vector3D m_pos;
};

namespace Test {
  BOOST_AUTO_TEST_SUITE(Surfaces)

  /// todo: make test fixture; separate out different cases

  /// Unit test for creating compliant/non-compliant Surface object
  BOOST_AUTO_TEST_CASE(SurfaceConstruction)
  {
    // SurfaceStub s;
    BOOST_TEST(Surface::Other == SurfaceStub().type());
    SurfaceStub original;
    BOOST_TEST(Surface::Other == SurfaceStub(original).type());
    Translation3D translation{0., 1., 2.};
    Transform3D   transform(translation);
    BOOST_TEST(Surface::Other == SurfaceStub(original, transform).type());
    // need some cruft to make the next one work
    auto pTransform = make_shared_transform(translation);
    std::shared_ptr<const Acts::PlanarBounds> p
        = std::make_shared<const RectangleBounds>(5., 10.);
    DetectorElementStub detElement{pTransform, p, 0.2, nullptr};
    BOOST_TEST(Surface::Other == SurfaceStub(detElement).type());
  }

  /// Unit test for testing Surface properties
  BOOST_AUTO_TEST_CASE(SurfaceProperties, *utf::expected_failures(1))
  {
    // build a test object , 'surface'
    std::shared_ptr<const Acts::PlanarBounds> pPlanarBound
        = std::make_shared<const RectangleBounds>(5., 10.);
    Vector3D           reference{0., 1., 2.};
    Translation3D      translation{0., 1., 2.};
    auto               pTransform = make_shared_transform(translation);
    auto               pLayer = PlaneLayer::create(pTransform, pPlanarBound);
    MaterialProperties properties{0.2, 0.2, 0.2, 20., 10, 5.};
    auto               pMaterial
        = std::make_shared<const HomogeneousSurfaceMaterial>(properties);
    DetectorElementStub detElement{pTransform, pPlanarBound, 0.2, pMaterial};
    SurfaceStub         surface(detElement);
    // associatedDetectorElement
    BOOST_TEST(surface.associatedDetectorElement() == &detElement);
    // test  associatelayer, associatedLayer
    surface.associateLayer(*pLayer);
    BOOST_TEST(surface.associatedLayer() == pLayer.get());
    // associated Material is not set to the surface
    // it is set to the detector element surface though
    BOOST_TEST(surface.associatedMaterial() != pMaterial.get());
    // center()
    BOOST_TEST(reference == surface.center());
    // stream insertion operator <<
    output_test_stream output;
    output << surface;
    BOOST_TEST(!output.is_empty(false));  // no check on contents
    // insideBounds
    Vector2D localPosition{0.1, 3.0};
    BOOST_CHECK(surface.insideBounds(localPosition));
    Vector2D outside{20., 20.};
    BOOST_CHECK(!surface.insideBounds(
        outside));  // fails: m_bounds only in derived classes
    // intersectionEstimate (should delegate to derived class method of same
    // name)
    Vector3D mom{100., 200., 300.};
    auto     intersectionEstimate
        = surface.intersectionEstimate(reference, mom, forward, false);
    const Intersection ref{Vector3D{1, 1, 1}, 20., true};
    bool               trial = (ref.position == intersectionEstimate.position);
    BOOST_TEST(trial, "intersectionEstimate() delegates to derived class");
    // isFree
    BOOST_CHECK(!surface.isFree());
    // isOnSurface
    BOOST_CHECK(surface.isOnSurface(reference, mom, false));
    BOOST_CHECK(
        surface.isOnSurface(reference, mom, true));  // need to improve bounds()
    // referenceFrame()
    RotationMatrix3D unitary;
    unitary << 1, 0, 0, 0, 1, 0, 0, 0, 1;
    auto referenceFrame = surface.referenceFrame(
        reference, mom);  // need more complex case to test
    bool ok = (referenceFrame == unitary);
    BOOST_TEST(ok, "referenceFrame() returns sensible answer");
    // normal()
    auto normal = surface.Surface::normal(reference);  // needs more complex
                                                       // test
    Vector3D zero{0., 0., 0.};
    BOOST_TEST(zero == normal);
    // pathCorrection is pure virtual
    // associatedMaterial()
    MaterialProperties newProperties{0.5, 0.5, 0.5, 20., 10., 5.};
    auto               pNewMaterial
        = std::make_shared<const HomogeneousSurfaceMaterial>(newProperties);
    surface.setAssociatedMaterial(pNewMaterial);
    BOOST_TEST(surface.associatedMaterial() == pNewMaterial.get());  // passes
                                                                     // ??
    //
    auto returnedTransform = surface.transform();
    bool constructedTransformEqualsRetrievedTransform
        = returnedTransform.isApprox(*pTransform);
    BOOST_TEST(constructedTransformEqualsRetrievedTransform);
    // type() is pure virtual
  }

  BOOST_AUTO_TEST_CASE(EqualityOperators)
  {
    // build some test objects
    std::shared_ptr<const Acts::PlanarBounds> pPlanarBound
        = std::make_shared<const RectangleBounds>(5., 10.);
    Vector3D           reference{0., 1., 2.};
    Translation3D      translation1{0., 1., 2.};
    Translation3D      translation2{1., 1., 2.};
    auto               pTransform1 = make_shared_transform(translation1);
    auto               pTransform2 = make_shared_transform(translation2);
    auto               pLayer = PlaneLayer::create(pTransform1, pPlanarBound);
    MaterialProperties properties{1., 1., 1., 20., 10, 5.};
    auto               pMaterial
        = std::make_shared<const HomogeneousSurfaceMaterial>(properties);
    DetectorElementStub detElement1{pTransform1, pPlanarBound, 0.2, pMaterial};
    DetectorElementStub detElement2{pTransform1, pPlanarBound, 0.3, pMaterial};
    DetectorElementStub detElement3{pTransform2, pPlanarBound, 0.3, pMaterial};
    //
    SurfaceStub surface1(detElement1);
    SurfaceStub surface2(detElement1);  // 1 and 2 are the same
    SurfaceStub surface3(detElement2);  // 3 differs in thickness
    SurfaceStub surface4(detElement3);  // 4 has a different transform and id
    //
    bool equalSurface = (surface1 == surface2);
    BOOST_TEST(equalSurface, "Equality between similar surfaces");
    //
    // remove test for the moment,
    // surfaces do not have a concept of thickness (only detector elemetns have)
    // bool unequalSurface
    //    = (surface1 != surface3);  // only thickness is different here;
    // BOOST_TEST(unequalSurface,
    //           "Different thickness surfaces should be unequal");  // will
    //           fail
    //
    bool unequalSurface
        = (surface1 != surface4);  // bounds or transform must be different;
    BOOST_TEST(unequalSurface,
               "Different transform surfaces should be unequal");
  }
  BOOST_AUTO_TEST_SUITE_END()

}  // namespace Test

}  // namespace Acts
