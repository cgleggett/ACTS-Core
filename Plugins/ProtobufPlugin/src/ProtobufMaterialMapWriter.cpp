// This file is part of the ACTS project.
//
// Copyright (C) 2018 ACTS project team
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ACTS/Plugins/ProtobufPlugin/ProtobufMaterialMapWriter.hpp"
#include "ACTS/Utilities/GeometryID.hpp"

#include "ACTS/Material/SurfaceMaterial.hpp"
#include "ACTS/Material/BinnedSurfaceMaterial.hpp"
#include "ACTS/Material/HomogeneousSurfaceMaterial.hpp"
#include "ACTS/Utilities/BinUtility.hpp"
#include "ACTS/Utilities/ThrowAssert.hpp"

// this is a private include
#include "gen/MaterialMap.pb.h"

// google/protobuf
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/message_lite.h>

#include <iostream>
#include <map>
#include <stdio.h>

void
Acts::ProtobufMaterialMapWriter::write(
  const std::map<GeometryID, SurfaceMaterial*>& surfaceMaterialMap) const
{
  //std::cout << "i'm totally writing right now" << std::endl;

  auto outfd = fopen(m_cfg.outfile.c_str(), "w+");
  auto outstream
      = std::make_unique<google::protobuf::io::FileOutputStream>(fileno(outfd));

  Acts::protobuf::MaterialMap matMapMsg;
  for(const auto& item : surfaceMaterialMap) {
    const GeometryID & geo_id = item.first;
    const SurfaceMaterial* srfMat = item.second;

    matMapMsg.Clear();
    matMapMsg.set_geo_id(geo_id.value());
    matMapMsg.set_vol_id(geo_id.value(GeometryID::volume_mask));
    matMapMsg.set_lay_id(geo_id.value(GeometryID::layer_mask));
    matMapMsg.set_app_id(geo_id.value(GeometryID::approach_mask));
    matMapMsg.set_sen_id(geo_id.value(GeometryID::sensitive_mask));

    auto binnedSrfMat = dynamic_cast<const BinnedSurfaceMaterial*>(srfMat);
    auto homogenousSrfMat = dynamic_cast<const HomogeneousSurfaceMaterial*>(srfMat);

    if(binnedSrfMat != nullptr) {
      const BinUtility& binUtility = binnedSrfMat->binUtility();
      size_t rows = binUtility.bins(0);
      size_t cols = binUtility.bins(1);
      int nBins = rows*cols;
      matMapMsg.set_rows(rows);
      matMapMsg.set_cols(cols);

      for (size_t b0 = 0; b0 < rows; ++b0) {
        for (size_t b1 = 0; b1 < cols; ++b1) {
          const MaterialProperties* matProp = binnedSrfMat->material(b0, b1);
          //std::cout << "matProp: " << matProp << std::endl;
          Acts::protobuf::MaterialMap::MaterialProperties* matPropMsg = matMapMsg.add_bins();

          if (matProp != nullptr) {
            const Material& mat = matProp->material();
            
            matPropMsg->set_thickness(matProp->thickness());
            matPropMsg->set_x0(mat.X0());
            matPropMsg->set_l0(mat.L0());
            matPropMsg->set_a(mat.A());
            matPropMsg->set_z(mat.Z());
            matPropMsg->set_rho(mat.rho());
          }
        }
      }

      throw_assert(matMapMsg.bins_size() == nBins, "Unexpected number of bins written");
    }
    else if(homogenousSrfMat != nullptr) {

      matMapMsg.set_rows(1);
      matMapMsg.set_cols(1);
      
      
      const MaterialProperties* matProp = homogenousSrfMat->material(Vector2D(0, 0));
      const Material& mat = matProp->material();
      
      Acts::protobuf::MaterialMap::MaterialProperties* matPropMsg = matMapMsg.add_bins();
      matPropMsg->set_thickness(matProp->thickness());
      matPropMsg->set_x0(mat.X0());
      matPropMsg->set_l0(mat.L0());
      matPropMsg->set_a(mat.A());
      matPropMsg->set_z(mat.Z());
      matPropMsg->set_rho(mat.rho());

      throw_assert(matMapMsg.bins_size() == 1, "HomogeneousSurfaceMaterial should only have one bin");
    }
    else {
      throw std::runtime_error("SurfaceMaterial in map is not currently supported");
    }

    writeDelimitedTo(matMapMsg, outstream.get());


  }

  //fclose(outfd);
}

// see
// https://stackoverflow.com/questions/2340730/are-there-c-equivalents-for-the-protocol-buffers-delimited-i-o-functions-in-ja/22927149
bool
Acts::ProtobufMaterialMapWriter::writeDelimitedTo(
    const google::protobuf::MessageLite& message,
    google::protobuf::io::ZeroCopyOutputStream* rawOutput) const 
{

  google::protobuf::io::CodedOutputStream output(rawOutput);

  // Write the size.
  const int size = message.ByteSize();
  output.WriteVarint32(size);

  uint8_t* buffer = output.GetDirectBufferForNBytesAndAdvance(size);
  if (buffer != NULL) {
    // Optimization:  The message fits in one buffer, so use the faster
    // direct-to-array serialization path.
    message.SerializeWithCachedSizesToArray(buffer);
  } else {
    // Slightly-slower path when the message is multiple buffers.
    message.SerializeWithCachedSizes(&output);
    if (output.HadError()) return false;
  }

  return true;
}
