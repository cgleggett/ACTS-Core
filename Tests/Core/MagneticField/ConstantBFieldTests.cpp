// This file is part of the Acts project.
//
// Copyright (C) 2017-2018 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

/// @file ConstantBField_tests.cpp

// clang-format off
#define BOOST_TEST_MODULE Constant magnetic field tests
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on

#include "Acts/MagneticField/ConstantBField.hpp"
#include "Acts/MagneticField/MagneticFieldContext.hpp"
#include "Acts/Utilities/Definitions.hpp"
#include "Acts/Utilities/Units.hpp"

namespace bdata = boost::unit_test::data;
namespace tt = boost::test_tools;
using namespace Acts::UnitLiterals;

namespace Acts {
namespace Test {

// Create a test context
MagneticFieldContext mfContext = MagneticFieldContext();

/// @brief unit test for construction of constant magnetic field
///
/// Tests the correct behavior and consistency of
/// -# ConstantBField::ConstantBField(double Bx,double By,double Bz)
/// -# ConstantBField::ConstantBField(Vector3D B)
/// -# ConstantBField::getField(const double* xyz, double* B) const
/// -# ConstantBField::getField(const Vector3D& pos) const
BOOST_DATA_TEST_CASE(ConstantBField_components,
                     bdata::random(-2_T, 2_T) ^ bdata::random(-1_T, 4_T) ^
                         bdata::random(0_T, 10_T) ^ bdata::random(-10_m, 10_m) ^
                         bdata::random(-10_m, 10_m) ^
                         bdata::random(-10_m, 10_m) ^ bdata::xrange(10),
                     x, y, z, bx, by, bz, index) {
  (void)index;
  BOOST_TEST_CONTEXT("Eigen interface") {
    const Vector3D Btrue(bx, by, bz);
    const Vector3D pos(x, y, z);
    const ConstantBField BField(Btrue);

    ConstantBField::Cache bCache(mfContext);

    BOOST_CHECK_EQUAL(Btrue, BField.getField(pos));
    BOOST_CHECK_EQUAL(Btrue, BField.getField(Vector3D(0, 0, 0)));
    BOOST_CHECK_EQUAL(Btrue, BField.getField(-2 * pos));

    BOOST_CHECK_EQUAL(Btrue, BField.getField(pos, bCache));
    BOOST_CHECK_EQUAL(Btrue, BField.getField(Vector3D(0, 0, 0), bCache));
    BOOST_CHECK_EQUAL(Btrue, BField.getField(-2 * pos, bCache));
  }

  BOOST_TEST_CONTEXT("C-array initialised - Eigen retrieved") {
    const ConstantBField BField(bx, by, bz);
    const Vector3D Btrue(bx, by, bz);
    const Vector3D pos(x, y, z);

    ConstantBField::Cache bCache(mfContext);

    BOOST_CHECK_EQUAL(Btrue, BField.getField(pos));
    BOOST_CHECK_EQUAL(Btrue, BField.getField(Vector3D(0, 0, 0)));
    BOOST_CHECK_EQUAL(Btrue, BField.getField(-2 * pos));

    BOOST_CHECK_EQUAL(Btrue, BField.getField(pos, bCache));
    BOOST_CHECK_EQUAL(Btrue, BField.getField(Vector3D(0, 0, 0), bCache));
    BOOST_CHECK_EQUAL(Btrue, BField.getField(-2 * pos, bCache));
  }
}

/// @brief unit test for update of constant magnetic field
///
/// Tests the correct behavior and consistency of
/// -# ConstantBField::setField(double Bx, double By, double Bz)
/// -# ConstantBField::setField(const Vector3D& B)
/// -# ConstantBField::getField(const double* xyz, double* B) const
/// -# ConstantBField::getField(const Vector3D& pos) const
BOOST_DATA_TEST_CASE(ConstantBField_update,
                     bdata::random(-2_T, 2_T) ^ bdata::random(-1_T, 4_T) ^
                         bdata::random(0_T, 10_T) ^ bdata::random(-10_m, 10_m) ^
                         bdata::random(-10_m, 10_m) ^
                         bdata::random(-10_m, 10_m) ^ bdata::xrange(10),
                     x, y, z, bx, by, bz, index) {
  (void)index;
  ConstantBField BField(0, 0, 0);

  BOOST_TEST_CONTEXT("Eigen interface") {
    const Vector3D Btrue(bx, by, bz);
    const Vector3D pos(x, y, z);
    BField.setField(Btrue);

    ConstantBField::Cache bCache(mfContext);

    BOOST_CHECK_EQUAL(Btrue, BField.getField(pos));
    BOOST_CHECK_EQUAL(Btrue, BField.getField(Vector3D(0, 0, 0)));
    BOOST_CHECK_EQUAL(Btrue, BField.getField(-2 * pos));

    BOOST_CHECK_EQUAL(Btrue, BField.getField(pos, bCache));
    BOOST_CHECK_EQUAL(Btrue, BField.getField(Vector3D(0, 0, 0), bCache));
    BOOST_CHECK_EQUAL(Btrue, BField.getField(-2 * pos, bCache));
  }

  BOOST_TEST_CONTEXT("C-array initialised - Eigen retrieved") {
    const Vector3D Btrue(bx, by, bz);
    const Vector3D pos(x, y, z);
    BField.setField(bx, by, bz);

    ConstantBField::Cache bCache(mfContext);

    BOOST_CHECK_EQUAL(Btrue, BField.getField(pos));
    BOOST_CHECK_EQUAL(Btrue, BField.getField(Vector3D(0, 0, 0)));
    BOOST_CHECK_EQUAL(Btrue, BField.getField(-2 * pos));

    BOOST_CHECK_EQUAL(Btrue, BField.getField(pos, bCache));
    BOOST_CHECK_EQUAL(Btrue, BField.getField(Vector3D(0, 0, 0), bCache));
    BOOST_CHECK_EQUAL(Btrue, BField.getField(-2 * pos, bCache));
  }
}

}  // namespace Test
}  // namespace Acts
