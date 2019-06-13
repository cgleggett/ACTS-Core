f// This file is part of the Acts project.
//
// Copyright (C) 2016-2019 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <functional>
#include <memory>
#include "Acts/EventData/Measurement.hpp"
#include "Acts/EventData/MeasurementHelpers.hpp"
#include "Acts/EventData/TrackParameters.hpp"
#include "Acts/EventData/TrackState.hpp"
#include "Acts/EventData/TrackStateSorters.hpp"
#include "Acts/Fitter/detail/VoidKalmanComponents.hpp"
#include "Acts/Geometry/GeometryContext.hpp"
#include "Acts/MagneticField/MagneticFieldContext.hpp"
#include "Acts/Propagator/AbortList.hpp"
#include "Acts/Propagator/ActionList.hpp"
#include "Acts/Propagator/Propagator.hpp"
#include "Acts/Propagator/detail/ConstrainedStep.hpp"
#include "Acts/Propagator/detail/StandardAborters.hpp"
#include "Acts/Utilities/CalibrationContext.hpp"
#include "Acts/Utilities/Definitions.hpp"
#include "Acts/Utilities/Logger.hpp"

namespace Acts {

/// @brief Options struct how the Fitter is called
///
/// It contains the context of the fitter call and the optional
/// surface where to express the fit result
///
/// @note the context objects must be provided
struct KalmanFitterOptions {
  /// Deleted default constructor
  KalmanFitterOptions() = delete;

  /// PropagatorOptions with context
  ///
  /// @param gctx The goemetry context for this fit
  /// @param mctx The magnetic context for this fit
  /// @param cctx The calibration context for this fit
  /// @param rSurface The reference surface for the fit to be expressed at
  KalmanFitterOptions(std::reference_wrapper<const GeometryContext> gctx,
                      std::reference_wrapper<const MagneticFieldContext> mctx,
                      std::reference_wrapper<const CalibrationContext> cctx,
                      const Surface* rSurface = nullptr)
      : geoContext(gctx),
        magFieldContext(mctx),
        calibrationContext(cctx),
        referenceSurface(rSurface) {}

  /// Context object for the geometry
  std::reference_wrapper<const GeometryContext> geoContext;
  /// Context object for the magnetic field
  std::reference_wrapper<const MagneticFieldContext> magFieldContext;
  /// context object for the calibration
  std::reference_wrapper<const CalibrationContext> calibrationContext;

  /// The reference Surface
  const Surface* referenceSurface = nullptr;
};

/// @brief Kalman fitter implementation of Acts as a plugin
/// to the Propgator
///
/// @tparam propagator_t Type of the propagation class
/// @tparam updator_t Type of the kalman updator class
/// @tparam smoother_t Type of the kalman smoother class
/// @tparam calibrator_t Type of the calibrator class
/// @tparam input_converter_t Type of the input converter class
/// @tparam output_converter_t Type of the output converter class
///
/// The Kalman filter contains an Actor and a Sequencer sub-class.
/// The Sequencer has to be part of the Navigator of the Propagator
/// in order to initialize and provide the measurement surfaces.
///
/// The Actor is part of the Propagation call and does the Kalman update
/// and eventually the smoothing.  Updator, Smoother and Calibrator are
/// given to the Actor for further use:
/// - The Updator is the implemented kalman updator formalism, it
///   runs via a visitor pattern through the measurements.
/// - The Smoother is called at the end of the forward fit by the Actor.
/// - The Calibrator is a dedicated calibration algorithm that allows
///   to calibrate measurements using track information, this could be
///    e.g. sagging for wires, module deformations, etc.
///
/// Measurements are not required to be ordered for the KalmanFilter,
/// measurement ordering needs to be figured out by the navigation of
/// the propagator.
///
/// The Input converter is a converter that transforms the input
/// measurement/track/segments into a set of FittableMeasurements
///
/// The Output converter is a converter that transforms the
/// set of track states into a given track/track particle class
///
/// The void components are provided mainly for unit testing.
template <typename propagator_t, typename updator_t = VoidKalmanUpdator,
          typename smoother_t = VoidKalmanSmoother,
          typename calibrator_t = VoidMeasurementCalibrator,
          typename input_converter_t = VoidKalmanComponents,
          typename output_converter_t = VoidKalmanComponents>
class KalmanFitter {
 public:
  /// Shorthand definition
  using MeasurementSurfaces = std::multimap<const Layer*, const Surface*>;

  /// Default constructor is deleted
  KalmanFitter() = delete;

  /// Constructor from arguments
  KalmanFitter(propagator_t pPropagator,
               std::unique_ptr<const Logger> logger =
                   getDefaultLogger("KalmanFilter", Logging::INFO),
               input_converter_t pInputCnv = input_converter_t(),
               output_converter_t pOutputCnv = output_converter_t())
      : m_propagator(std::move(pPropagator)),
        m_inputConverter(std::move(pInputCnv)),
        m_outputConverter(std::move(pOutputCnv)),
        m_logger(std::move(logger)) {}

  /// Fit implementation of the foward filter, calls the
  /// the forward filter and backward smoother
  ///
  /// @tparam source_link_t Source link type identifying uncalibrated input
  /// measurements.
  /// @tparam start_parameters_t Type of the initial parameters
  /// @tparam parameters_t Type of parameters used for local parameters
  ///
  /// @param sourcelinks The fittable uncalibrated measurements
  /// @param sParameters The initial track parameters
  /// @param kfOptions KalmanOptions steering the fit
  /// @note The input measurements are given in the form of @c SourceLinks. It's
  /// @c calibrator_t's job to turn them into calibrated measurements used in
  /// the fit.
  ///
  /// @return the output as an output track
  template <typename source_link_t, typename start_parameters_t,
            typename parameters_t = BoundParameters>
  auto fit(const std::vector<source_link_t>& sourcelinks,
           const start_parameters_t& sParameters,
           const KalmanFitterOptions& kfOptions) const {
    static_assert(SourceLinkConcept<source_link_t>,
                  "Source link does not fulfill SourceLinkConcept");

    // To be able to find measurements later, we put them into a map
    // We need to copy input SourceLinks anyways, so the map can own them.
    ACTS_VERBOSE("Preparing " << sourcelinks.size() << " input measurements");
    std::map<const Surface*, source_link_t> inputMeasurements;
    for (const auto& sl : sourcelinks) {
      const Surface* srf = &sl.referenceSurface();
      inputMeasurements[srf] = sl;  // copy!
    }

    // Create the ActionList and AbortList
    using KalmanActor = Actor<source_link_t, parameters_t>;
    using KalmanResult = typename KalmanActor::result_type;
    using Actors = ActionList<KalmanActor>;
    using Aborters = AbortList<>;

    // Create relevant options for the propagation options
    PropagatorOptions<Actors, Aborters> kalmanOptions(
        kfOptions.geoContext, kfOptions.magFieldContext);

    // Catch the actor and set the measurements
    auto& kalmanActor = kalmanOptions.actionList.template get<KalmanActor>();
    kalmanActor.m_logger = m_logger.get();
    kalmanActor.inputMeasurements = std::move(inputMeasurements);
    kalmanActor.targetSurface = kfOptions.referenceSurface;

    // Run the fitter
    const auto& result =
        m_propagator.template propagate(sParameters, kalmanOptions).value();

    /// Get the result of the fit
    auto kalmanResult = result.template get<KalmanResult>();

    // Return the converted Track
    return m_outputConverter(std::move(kalmanResult));
  }

 private:
  /// The propgator for the transport and material update
  propagator_t m_propagator;

  /// The input converter to Fittable measurements
  input_converter_t m_inputConverter;

  /// The output converter into a given format
  output_converter_t m_outputConverter;

  /// Logger getter to support macros
  const Logger& logger() const { return *m_logger; }

  /// Owned logging instance
  std::unique_ptr<const Logger> m_logger;

  /// @brief Propagator Actor plugin for the KalmanFilter
  ///
  /// @tparam source_link_t is an type fulfilling the @c SourceLinkConcept
  /// @tparam parameters_t The type of parameters used for "local" paremeters.
  ///
  /// The KalmanActor does not rely on the measurements to be
  /// sorted along the track.
  template <typename source_link_t, typename parameters_t>
  class Actor {
   public:
    using TrackStateType = TrackState<source_link_t, parameters_t>;

    /// Explicit constructor with updator and calibrator
    Actor(updator_t pUpdator = updator_t(), smoother_t pSmoother = smoother_t(),
          calibrator_t pCalibrator = calibrator_t())
        : m_updator(std::move(pUpdator)),
          m_smoother(std::move(pSmoother)),
          m_calibrator(std::move(pCalibrator)) {}

    /// Simple result struct to be returned
    /// It mainly acts as an internal state which is
    /// created for every propagation/extrapolation step
    struct this_result {
      // Fitted states that the actor has handled.
      std::vector<TrackStateType> fittedStates = {};

      // The optional Parameters at the provided surface
      boost::optional<BoundParameters> fittedParameters;

      // Counter for handled states
      size_t processedStates = 0;

      // Indicator if smoothing has been done.
      bool smoothed = false;

      // Indicator if initialization has been performed.
      bool initialized = false;

      // Measurement surfaces without hits
      std::vector<const Surface*> missedActiveSurfaces = {};
    };

    /// Broadcast the result_type
    using result_type = this_result;

    /// The target surface
    const Surface* targetSurface = nullptr;

    /// Allows retrieving measurements for a surface
    std::map<const Surface*, source_link_t> inputMeasurements;

    /// @brief Kalman actor operation
    ///
    /// @tparam propagator_state_t is the type of Propagagor state
    /// @tparam stepper_t Type of the stepper
    ///
    /// @param state is the mutable propagator state object
    /// @param stepper The stepper in use
    /// @param result is the mutable result state object
    template <typename propagator_state_t, typename stepper_t>
    void operator()(propagator_state_t& state, const stepper_t& stepper,
                    result_type& result) const {
      // Initialization:
      // - Only when track states are not set
      if (!result.initialized) {
        // -> Move the TrackState vector
        // -> Feed the KalmanSequencer with the measurements to be fitted
        initialize(state, stepper, result);
        result.initialized = true;
      }

      // Update:
      // - Waiting for a current surface that appears in the measurement list
      auto surface = state.navigation.currentSurface;
      if (surface and not result.smoothed) {
        // Check if the surface is in the measurement map
        // -> Get the measurement / calibrate
        // -> Create the predicted state
        // -> Perform the kalman update
        // -> Check outlier behavior (@todo)
        // -> Fill strack state information & update stepper information
        filter(surface, state, stepper, result);
      }

      // Finalization:
      // - When all track states have been handled
      if (result.processedStates == inputMeasurements.size() and
          not result.smoothed) {
        // -> Sort the track states (as now the path length is set)
        // -> Call the smoothing
        // -> Set a stop condition when all track states have been handled
        finalize(state, stepper, result);
      }
      // Post-finalization:
      // - Progress to target/reference surface and built the final track
      // parameters
      if (result.smoothed and targetReached(state, stepper, *targetSurface)) {
        // Transport & bind the parameter to the final surface
        auto fittedState =
            stepper.boundState(state.stepping, *targetSurface, true);
        // Assign the fitted parameters
        result.fittedParameters = std::get<BoundParameters>(fittedState);
        // Break the navigation for stopping the Propagation
        state.navigation.navigationBreak = true;
      }
    }

    /// @brief Kalman actor operation : initialize
    ///
    /// @tparam propagator_state_t is the type of Propagagor state
    /// @tparam stepper_t Type of the stepper
    ///
    /// @param state is the mutable propagator state object
    /// @param stepper The stepper in use
    /// @param result is the mutable result state object
    template <typename propagator_state_t, typename stepper_t>
    void initialize(propagator_state_t& /*state*/, const stepper_t& /*stepper*/,
                    result_type& /*result*/) const {}

    /// @brief Kalman actor operation : update
    ///
    /// @tparam propagator_state_t is the type of Propagagor state
    /// @tparam stepper_t Type of the stepper
    ///
    /// @param surface The surface where the update happens
    /// @param state The mutable propagator state object
    /// @param stepper The stepper in use
    /// @param result The mutable result state object
    template <typename propagator_state_t, typename stepper_t>
    void filter(const Surface* surface, propagator_state_t& state,
                const stepper_t& stepper, result_type& result) const {
      // Try to find the surface in the measurement surfaces
      auto sourcelink_it = inputMeasurements.find(surface);
      if (sourcelink_it != inputMeasurements.end()) {
        // Screen output message
        ACTS_VERBOSE("Measurement surface " << surface->geoID().toString()
                                            << " detected.");

        // create track state on the vector from sourcelink
        result.fittedStates.emplace_back(sourcelink_it->second);
        TrackStateType& trackState = result.fittedStates.back();

        // Transport & bind the state to the current surface
        std::tuple<BoundParameters,
                   typename TrackStateType::Parameters::CovMatrix_t, double>
            boundState = stepper.boundState(state.stepping, *surface, true);
        // Fill the track state
        trackState.parameter.predicted = std::get<0>(boundState);
        trackState.parameter.jacobian = std::get<1>(boundState);
        trackState.parameter.pathLength = std::get<2>(boundState);

        // If the update is successful, set covariance and
        if (m_updator(state.geoContext, trackState)) {
          // Update the stepping state
          ACTS_VERBOSE("Filtering step successful, updated parameters are : \n"
                       << *trackState.parameter.filtered);
          // update stepping state using filtered parameters
          // after kalman update
          stepper.update(state.stepping, *trackState.parameter.filtered);
        }
        // We count the processed state
        ++result.processedStates;
      } else if (surface->associatedDetectorElement() != nullptr) {
        // Count the missed surface
        ACTS_VERBOSE("Detected hole on " << surface->geoID().toString());
        result.missedActiveSurfaces.push_back(surface);
      }
    }

    /// @brief Kalman actor operation : finalize
    ///
    /// @tparam propagator_state_t is the type of Propagagor state
    /// @tparam stepper_t Type of the stepper
    ///
    /// @param state is the mutable propagator state object
    /// @param stepper The stepper in use
    /// @param result is the mutable result state object
    template <typename propagator_state_t, typename stepper_t>
    void finalize(propagator_state_t& state, const stepper_t& stepper,
                  result_type& result) const {
      // Remember you smoothed the track states
      result.smoothed = true;

      // Sort the TrackStates according to the path length
      TrackStatePathLengthSorter plSorter;
      std::sort(result.fittedStates.begin(), result.fittedStates.end(),
                plSorter);
      // Screen output for debugging
      ACTS_VERBOSE("Apply smoothing on " << result.fittedStates.size()
                                         << " filtered track states.");
      // Smooth the track states and obtain the last smoothed track parameters
      const auto& smoothedPars =
          m_smoother(state.geoContext, result.fittedStates);
      // Update the stepping parameters - in order to progress to destination
      if (smoothedPars) {
        // Update the stepping state
        ACTS_VERBOSE(
            "Smoothing successful, updating stepping state, "
            "set target surface.");
        stepper.update(state.stepping, smoothedPars.get());
        // Reverse the propagation direction
        state.stepping.stepSize =
            detail::ConstrainedStep(-1. * state.options.maxStepSize);
        state.options.direction = backward;
      }
    }

    /// Pointer to a logger that is owned by the parent, KalmanFilter
    const Logger* m_logger;

    /// Getter for the logger, to support logging macros
    const Logger& logger() const { return *m_logger; }

    /// The Kalman updator
    updator_t m_updator;

    /// The Kalman smoother
    smoother_t m_smoother;

    /// The Measuremetn calibrator
    calibrator_t m_calibrator;

    /// The Surface beeing
    detail::SurfaceReached targetReached;
  };
};

}  // namespace Acts
