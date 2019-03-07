// This file is part of the Acts project.
//
// Copyright (C) 2018 Acts project team
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

///////////////////////////////////////////////////////////////////
// SurfaceMaterialMapper.hpp, Acts project MaterialPlugins
///////////////////////////////////////////////////////////////////

#pragma once

#include "Acts/Extrapolator/MaterialInteractor.hpp"
#include "Acts/Extrapolator/Navigator.hpp"
#include "Acts/Extrapolator/SurfaceCollector.hpp"
#include "Acts/Material/SurfaceMaterial.hpp"
#include "Acts/Plugins/MaterialMapping/AccumulatedSurfaceMaterial.hpp"
#include "Acts/Propagator/Propagator.hpp"
#include "Acts/Propagator/StraightLineStepper.hpp"
#include "Acts/Surfaces/Surface.hpp"
#include "Acts/Utilities/GeometryID.hpp"
#include "Acts/Utilities/Logger.hpp"

namespace Acts {

class TrackingGeometry;

/// @brief selector for finding
struct MaterialSurface
{
  bool
  operator()(const Surface& sf) const
  {
    return (sf.surfaceMaterial() != nullptr);
  }
};

/// @brief SurfaceMaterialMapper
///
/// This is the main feature tool to map material information
/// from a 3D geometry onto the TrackingGeometry with its surface
/// material description.
///
/// The process runs as such:
///
///  1) TrackingGeometry is parsed and for each Surface with
///     ProtoSurfaceMaterial a local store is initialized
///     the identification is done hereby through the Surface::GeometryID
///
///  2) A Cache is generated that is used to keep the filling thread local,
///     the filling is protected with std::mutex
///
///  3) A number of N material tracks is read in, each track has :
///       origin, direction, material steps < position, step length, x0, l0, a,
///       z, rho >
///
///       for each track:
///          surfaces along the origin/direction path are collected
///          the closest material steps are assigned
///
///  4) Each 'hit' bin per event is counted and averaged at the end of the run
///
class SurfaceMaterialMapper
{
public:
  using StraightLinePropagator = Propagator<StraightLineStepper, Navigator>;

  /// @struct Config
  ///
  /// Nested Configuration struct for the material mapper
  struct Config
  {
    /// Mapping range
    std::array<double, 2> etaRange = {{-6., 6.}};
    /// Mapping output to debug stream
    bool mapperDebugOutput = false;
  };

  /// @struct State
  ///
  /// Nested State struct which is used for the mapping prococess
  struct State
  {
    /// The accumulated material per geometry ID
    std::map<GeometryID, AccumulatedSurfaceMaterial> accumulatedMaterial;
    /// The created surface material from it
    std::map<GeometryID, std::unique_ptr<const SurfaceMaterial>>
        surfaceMaterial;
  };

  /// Delete the Default constructor
  SurfaceMaterialMapper() = delete;

  /// Constructor with config object
  ///
  /// @param cfg Configuration struct
  /// @param propagator The straight line propagator
  /// @param log The logger
  SurfaceMaterialMapper(const Config&                 cfg,
                        StraightLinePropagator        propagator,
                        std::unique_ptr<const Logger> slogger
                        = getDefaultLogger("SurfaceMaterialMapper",
                                           Logging::INFO));

  /// @brief helper method that creates the cache for the mapping
  ///
  /// @param[in] tGeometry The geometry which should be mapped
  ///
  /// This method takes a TrackingGeometry,
  /// finds all surfaces with material proxis
  /// and returns you a Cache object tO be used
  State
  createState(const TrackingGeometry& tGeometry) const;

  /// @brief Method to finalize the maps
  ///
  /// It calls the final run averaging and then transforms
  /// the AccumulatedSurface material class to a surface material
  /// class type
  ///
  /// @param mState
  void
  finalizeMaps(State& mState) const;

  /// Process/map a single track
  ///
  /// @param mState The current state map
  /// @param mTrack The material track to be mapped
  ///
  /// @note the RecordedMaterialProperties of the track are assumed
  /// to be ordered from the starting position along the starting direction
  void
  mapMaterialTrack(State& mState, const RecordedMaterialTrack& mTrack) const;

private:
  /// @brief finds all surfaces with ProtoSurfaceMaterial of a volume
  ///
  /// @param mState The state to be filled
  /// @param tVolume is current TrackingVolume
  void
  resolveMaterialSurfaces(State& mState, const TrackingVolume& tVolume) const;

  /// @brief check and insert
  ///
  /// @param mState is the map to be filled
  /// @param surface is the surface to be checked for a Proxy
  void
  checkAndInsert(State& /*mState*/, const Surface& surface) const;

  /// Standard logger method
  const Logger&
  logger() const
  {
    return *m_logger;
  }

  /// The configuration object
  Config m_cfg;

  /// The straight line propagator
  StraightLinePropagator m_propagator;

  /// The logging instance
  std::unique_ptr<const Logger> m_logger;
};
}  // namespace Acts
