// This file is part of the Acts project.
//
// Copyright (C) 2016-2018 Acts project team
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

///////////////////////////////////////////////////////////////////
// PassiveLayerBuilder.h, Acts project
///////////////////////////////////////////////////////////////////

#pragma once

#include "Acts/Layers/Layer.hpp"
#include "Acts/Material/MaterialProperties.hpp"
#include "Acts/Tools/ILayerBuilder.hpp"
#include "Acts/Utilities/GeometryContext.hpp"
#include "Acts/Utilities/Logger.hpp"

namespace Acts {

/// @class PassiveLayerBuilder
///
/// The PassiveLayerBuilder is able to build cylinder & disc layers with given
/// dimensions and material. The specifications of the the layers have to be
/// given by the configuration struct.

class PassiveLayerBuilder : public ILayerBuilder
{
public:
  /// @struct Config
  /// Configuration struct for the passive layer builder
  /// This nested struct is used to configure the layer building
  struct Config
  {
    /// String based identification
    std::string layerIdentification;

    // The layer in central position
    std::vector<double> centralLayerRadii;        ///< central layer specs
    std::vector<double> centralLayerHalflengthZ;  ///< central layer specs
    std::vector<double> centralLayerThickness;    ///< central layer specs
    std::vector<std::shared_ptr<const SurfaceMaterial>>
        centralLayerMaterial;  ///< central layer specs

    // The layers at p/e side
    std::vector<double> posnegLayerPositionZ;  ///< p/n layer specs
    std::vector<double> posnegLayerRmin;       ///< p/n layer specs
    std::vector<double> posnegLayerRmax;       ///< p/n layer specs
    std::vector<double> posnegLayerThickness;  ///< p/n layer specs
    std::vector<std::shared_ptr<const SurfaceMaterial>>
        posnegLayerMaterial;  ///< p/n  layer specs
  };

  /// Constructor
  ///
  /// @param plConfig is the ocnfiguration struct that steers behavior
  /// @param logger logging instance
  PassiveLayerBuilder(const Config&                 plConfig,
                      std::unique_ptr<const Logger> logger
                      = getDefaultLogger("PassiveLayerBuilder", Logging::INFO));

  /// Destructor
  ~PassiveLayerBuilder() override = default;

  /// LayerBuilder interface method
  ///
  /// @param gctx ist the geometry context under
  /// which the geometry is built
  ///
  /// @return  the layers at negative side
  const LayerVector
  negativeLayers(const GeometryContext& gctx) const override;

  /// LayerBuilder interface method
  ///
  /// @param gctx ist the geometry context under
  /// which the geometry is built
  ///
  /// @return the layers at the central sector
  const LayerVector
  centralLayers(const GeometryContext& gctx) const override;

  /// LayerBuilder interface method
  ///
  /// @param gctx ist the geometry context under
  /// which the geometry is built
  ///
  /// @return  the layers at positive side
  const LayerVector
  positiveLayers(const GeometryContext& gctx) const override;

  /// Name identification
  /// @return the string based identification
  const std::string&
  identification() const override
  {
    return m_cfg.layerIdentification;
  }

  /// Set configuration method
  ///
  /// @param plConfig is a configuration struct
  /// it overwrites the current configuration
  void
  setConfiguration(const Config& plConfig);

  /// Get configuration method
  Config
  getConfiguration() const;

  /// Set logging instance
  ///
  /// @param newLogger the logger instance
  void
  setLogger(std::unique_ptr<const Logger> newLogger);

protected:
  Config m_cfg;  //!< configuration

private:
  /// Helper interface method
  ///
  /// @param gctx ist the geometry context under
  /// which the geometry is built
  /// @param side is the side of the layer to be built
  ///
  /// @return  the layers at positive side
  const LayerVector
  endcapLayers(const GeometryContext& gctx, int side) const;

  const Logger&
  logger() const
  {
    return *m_logger;
  }

  /// logging instance
  std::unique_ptr<const Logger> m_logger;
};

inline PassiveLayerBuilder::Config
PassiveLayerBuilder::getConfiguration() const
{
  return m_cfg;
}

}  // namespace