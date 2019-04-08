// This file is part of the Acts project.
//
// Copyright (C) 2018 Acts project team
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

///  Boost include(s)
#define BOOST_TEST_MODULE SurfaceMaterialProxy Tests
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <climits>
#include "Acts/Material/ProtoSurfaceMaterial.hpp"

namespace Acts {

namespace Test {

  /// Test the constructors
  BOOST_AUTO_TEST_CASE(ProtoSurfaceMaterial_construction_test)
  {
    BinUtility smpBU(10, -10., 10., open, binX);
    smpBU += BinUtility(10, -10., 10., open, binY);

    // Constructor from arguments
    ProtoSurfaceMaterial smp(smpBU);
    // Copy constructor
    ProtoSurfaceMaterial smpCopy(smp);
    // Copy move constructor
    ProtoSurfaceMaterial smpCopyMoved(std::move(smpCopy));
  }

}  // namespace Test
}  // namespace Acts
