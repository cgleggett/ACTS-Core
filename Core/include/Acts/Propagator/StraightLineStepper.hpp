// This file is part of the Acts project.
//
// Copyright (C) 2016-2019 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cmath>
#include <functional>

#include "Acts/EventData/TrackParameters.hpp"
#include "Acts/Geometry/GeometryContext.hpp"
#include "Acts/Propagator/detail/ConstrainedStep.hpp"
#include "Acts/Surfaces/Surface.hpp"
#include "Acts/Utilities/Definitions.hpp"
#include "Acts/Utilities/Intersection.hpp"
#include "Acts/Utilities/Result.hpp"

namespace Acts {

/// @brief straight line stepper based on Surface intersection
///
/// The straight line stepper is a simple navigation stepper
/// to be used to navigate through the tracking geometry. It can be
/// used for simple material mapping, navigation validation
class StraightLineStepper {
 private:
  // This struct is a meta-function which normally maps to BoundParameters...
  template <typename T, typename S>
  struct s {
    using type = BoundParameters;
  };

  // ...unless type S is int, in which case it maps to Curvilinear parameters
  template <typename T>
  struct s<T, int> {
    using type = CurvilinearParameters;
  };

 public:
  using cstep = detail::ConstrainedStep;

  using Corrector = VoidIntersectionCorrector;
  using Jacobian = FreeMatrix;
  using Covariance = FreeSymMatrix;
  using BoundState = std::tuple<BoundParameters, BoundMatrix, double>;
  using CurvilinearState = std::tuple<CurvilinearParameters, BoundMatrix, double>;

  /// State for track parameter propagation
  ///
  struct State {
    /// Delete the default constructor
    State() = delete;

    /// Constructor from the initial track parameters
    ///
    /// @tparam parameters_t the Type of the track parameters
    ///
    /// @param [in] gctx is the context object for the geometery
    /// @param [in] mctx is the context object for the magnetic field
    /// @param [in] par The track parameters at start
    /// @param [in] ndir is the navigation direction
    /// @param [in] ssize is the (absolute) maximum step size
    template <typename parameters_t, std::enable_if_t<std::is_same<typename parameters_t::CovMatrix_t, BoundSymMatrix>::value, int> = 0>
    explicit State(std::reference_wrapper<const GeometryContext> gctx,
                   std::reference_wrapper<const MagneticFieldContext> /*mctx*/,
                   const parameters_t& par, NavigationDirection ndir = forward,
                   double ssize = std::numeric_limits<double>::max())
        : pos(par.position()),
          dir(par.momentum().normalized()),
          p(par.momentum().norm()),
          q(par.charge()),
          t0(par.time()),
          navDir(ndir),
          stepSize(ndir * std::abs(ssize)),
          geoContext(gctx) {
      if (par.covariance()) { // TODO: constructors might be combined but a covariance setter is then templated
		  // Set the covariance transport flag to true
		  covTransport = true;
		  // Get the covariance          
		  par.referenceSurface().initJacobianToGlobal(gctx, jacToGlobal,
									   par.position(), par.momentum().normalized(), par.parameters());
		  cov = jacToGlobal * (*par.covariance()) * jacToGlobal.transpose();
      }
    }
    
    /// Constructor from the initial track parameters
    ///
    /// @tparam parameters_t the Type of the track parameters
    ///
    /// @param [in] gctx is the context object for the geometery
    /// @param [in] mctx is the context object for the magnetic field
    /// @param [in] par The track parameters at start
    /// @param [in] ndir is the navigation direction
    /// @param [in] ssize is the (absolute) maximum step size
    template <typename parameters_t, std::enable_if_t<std::is_same<typename parameters_t::CovMatrix_t, FreeSymMatrix>::value, int> = 0>
    explicit State(std::reference_wrapper<const GeometryContext> gctx,
                   std::reference_wrapper<const MagneticFieldContext> /*mctx*/,
                   const parameters_t& par, NavigationDirection ndir = forward,
                   double ssize = std::numeric_limits<double>::max())
        : pos(par.position()),
          dir(par.momentum().normalized()),
          p(par.momentum().norm()),
          q(par.charge()),
          t0(par.time()),
          navDir(ndir),
          stepSize(ndir * std::abs(ssize)),
          geoContext(gctx) {
      if (par.covariance()) {
		  // Set the covariance transport flag to true
		  covTransport = true;
		  // Get the covariance
          cov = *par.covariance();
      }
    }
    
    /// Pure transport jacobian part from runge kutta integration
    Jacobian jacTransport = Jacobian::Identity();

    /// The full jacobian of the transport entire transport
    Jacobian jacobian = Jacobian::Identity();

	boost::optional<BoundToFreeMatrix> jacToGlobal;

    /// The propagation derivative
    FreeVector derivative = FreeVector::Zero();

    /// Boolean to indiciate if you need covariance transport
    bool covTransport = false;
    Covariance cov = Covariance::Zero();

    /// Global particle position
    Vector3D pos = Vector3D(0., 0., 0.);

    /// Momentum direction (normalized)
    Vector3D dir = Vector3D(1., 0., 0.);

    /// Momentum
    double p = 0.;

    /// Save the charge: neutral as default for SL stepper
    double q = 0.;

    /// @note The time is split into a starting and a propagated time to avoid
    /// machine precision related errors
    /// Starting time
    const double t0;
    /// Propagated time
    double dt = 0.;

    /// Navigation direction, this is needed for searching
    NavigationDirection navDir;

    /// accummulated path length state
    double pathAccumulated = 0.;

    /// adaptive step size of the runge-kutta integration
    cstep stepSize = std::numeric_limits<double>::max();

    // Cache the geometry context of this propagation
    std::reference_wrapper<const GeometryContext> geoContext;
  };

  /// Always use the same propagation state type, independently of the initial
  /// track parameter type and of the target surface
  using state_type = State;

  /// Return parameter types depend on the propagation mode:
  /// - when propagating to a surface we return BoundParameters
  /// - otherwise CurvilinearParameters
  template <typename parameters_t, typename surface_t = int>
  using return_parameter_type = typename s<parameters_t, surface_t>::type;

  /// Constructor
  StraightLineStepper() = default;

  /// Get the field for the stepping, this gives back a zero field
  ///
  /// @param [in,out] state is the propagation state associated with the track
  ///                 the magnetic field cell is used (and potentially updated)
  /// @param [in] pos is the field position
  Vector3D getField(State& /*state*/, const Vector3D& /*pos*/) const {
    // get the field from the cell
    return Vector3D(0., 0., 0.);
  }

  /// Global particle position accessor
  Vector3D position(const State& state) const { return state.pos; }

  /// Momentum direction accessor
  Vector3D direction(const State& state) const { return state.dir; }

  /// Momentum accessor
  double momentum(const State& state) const { return state.p; }

  /// Charge access
  double charge(const State& state) const { return state.q; }

  /// Time access
  double time(const State& state) const { return state.t0 + state.dt; }

  /// Tests if the state reached a surface
  ///
  /// @param [in] state State that is tests
  /// @param [in] surface Surface that is tested
  ///
  /// @return Boolean statement if surface is reached by state
  bool surfaceReached(const State& state, const Surface* surface) const {
    return surface->isOnSurface(state.geoContext, position(state),
                                direction(state), true);
  }

	FreeToBoundMatrix freeToCurvilinearJacobian(State& state)
	{
		// Optimized trigonometry on the propagation direction
		const double x = state.dir(0);  // == cos(phi) * sin(theta)
		const double y = state.dir(1);  // == sin(phi) * sin(theta)
		const double z = state.dir(2);  // == cos(theta)
		// can be turned into cosine/sine
		const double cosTheta = z;
		const double sinTheta = sqrt(x * x + y * y);
		const double invSinTheta = 1. / sinTheta;
		const double cosPhi = x * invSinTheta;
		const double sinPhi = y * invSinTheta;
		// prepare the jacobian to curvilinear
		FreeToBoundMatrix jacToCurv = FreeToBoundMatrix::Zero();
		if (std::abs(cosTheta) < s_curvilinearProjTolerance) {
		  // We normally operate in curvilinear coordinates defined as follows
		  jacToCurv(0, 0) = -sinPhi;
		  jacToCurv(0, 1) = cosPhi;
		  jacToCurv(1, 0) = -cosPhi * cosTheta;
		  jacToCurv(1, 1) = -sinPhi * cosTheta;
		  jacToCurv(1, 2) = sinTheta;
		} else {
		  // Under grazing incidence to z, the above coordinate system definition
		  // becomes numerically unstable, and we need to switch to another one
		  const double c = sqrt(y * y + z * z);
		  const double invC = 1. / c;
		  jacToCurv(0, 1) = -z * invC;
		  jacToCurv(0, 2) = y * invC;
		  jacToCurv(1, 0) = c;
		  jacToCurv(1, 1) = -x * y * invC;
		  jacToCurv(1, 2) = -x * z * invC;
		}
		// Time parameter
		jacToCurv(5, 3) = 1.;
		// Directional and momentum parameters for curvilinear
		jacToCurv(2, 4) = -sinPhi * invSinTheta;
		jacToCurv(2, 5) = cosPhi * invSinTheta;
		jacToCurv(3, 6) = -invSinTheta;
		jacToCurv(4, 7) = 1.;
		
		return jacToCurv;
	}

  /// Create and return the bound state at the current position
  ///
  /// @brief It does not check if the transported state is at the surface, this
  /// needs to be guaranteed by the propagator
  ///
  /// @param [in] state State that will be presented as @c BoundState
  /// @param [in] surface The surface to which we bind the state
  /// @param [in] reinitialize Boolean flag whether reinitialization is needed,
  /// i.e. if this is an intermediate state of a larger propagation
  ///
  /// @return A bound state:
  ///   - the parameters at the surface
  ///   - the stepwise jacobian towards it (from last bound)
  ///   - and the path length (from start - for ordering)
  BoundState boundState(State& state, const Surface& surface,
                        bool reinitialize) const {
    // Transport the covariance to here
    std::unique_ptr<const BoundSymMatrix> covPtr = nullptr;
    BoundMatrix jacobian = BoundMatrix::Identity();
    if (state.covTransport) {
		covarianceTransport(state, surface, reinitialize);
		
		// Initialize the transport final frame jacobian
		FreeToBoundMatrix jacToLocal = FreeToBoundMatrix::Zero();
		// initalize the jacobian to local, returns the transposed ref frame
		auto rframeT = surface.initJacobianToLocal(state.geoContext, jacToLocal,
                                               state.pos, state.dir);
                                               
    		jacobian = jacToLocal * state.jacobian * jacToLocal.transpose();
		
		
      covPtr = std::make_unique<const BoundSymMatrix>(jacToLocal * state.cov * jacToLocal.transpose(););
    }
    // Create the bound parameters
    BoundParameters parameters(state.geoContext, std::move(covPtr), state.pos,
                               state.p * state.dir, state.q,
                               state.t0 + state.dt, surface.getSharedPtr());
    // Create the bound state
    BoundState bState{std::move(parameters), jacobian,
                      state.pathAccumulated};
    // Reset the jacobian to identity
    if (reinitialize) {
      state.jacobian = Jacobian::Identity();
    }
    /// Return the State
    return bState;
  }

  /// Create and return a curvilinear state at the current position
  ///
  /// @brief This creates a curvilinear state.
  ///
  /// @param [in] state State that will be presented as @c CurvilinearState
  /// @param [in] reinitialize Boolean flag whether reinitialization is needed,
  /// i.e. if this is an intermediate state of a larger propagation
  ///
  /// @return A curvilinear state:
  ///   - the curvilinear parameters at given position
  ///   - the stepweise jacobian towards it (from last bound)
  ///   - and the path length (from start - for ordering)
  CurvilinearState curvilinearState(State& state, bool reinitialize) const {
    // Transport the covariance to here
    std::unique_ptr<const BoundSymMatrix> covPtr = nullptr;
    BoundMatrix jacobian = BoundMatrix::Identity();
    if (state.covTransport) {
		covarianceTransport(state, reinitialize);
		FreeToBoundMatrix jacToCurv = freeToBoundJacobian(state);
		jacobian = jacToCurv * state.jacobian * jacToCurv.transpose();
      covPtr = std::make_unique<BoundSymMatrix>(jacToCurv * state.cov * jacToCurv.transpose());
    }
    // Create the curvilinear parameters
    CurvilinearParameters parameters(std::move(covPtr), state.pos,
                                     state.p * state.dir, state.q,
                                     state.t0 + state.dt);
    // Create the bound state
    CurvilinearState curvState{std::move(parameters), jacobian,
                               state.pathAccumulated};
    // Reset the jacobian to identity
    if (reinitialize) {
      state.jacobian = Jacobian::Identity();
    }
    /// Return the State
    return curvState;
  }

  /// Method to update a stepper state to the some parameters
  ///
  /// @param [in,out] state State object that will be updated
  /// @param [in] pars Parameters that will be written into @p state
  void update(State& state, const BoundParameters& pars) const {
    const auto& mom = pars.momentum();
    state.pos = pars.position();
    state.dir = mom.normalized();
    state.p = mom.norm();
    state.dt = pars.time();

    if (pars.covariance() != nullptr) {	
      state.cov = pars.globalCovariance(state.geoContext);
    }
  }

  /// Method to update momentum, direction and p
  ///
  /// @param [in,out] state State object that will be updated
  /// @param [in] uposition the updated position
  /// @param [in] udirection the updated direction
  /// @param [in] up the updated momentum value
  /// @param [in] time the updated time value
  void update(State& state, const Vector3D& uposition,
              const Vector3D& udirection, double up, double time) const {
    state.pos = uposition;
    state.dir = udirection;
    state.p = up;
    state.dt = time;
  }

  /// Return a corrector
  VoidIntersectionCorrector corrector(State& /*state*/) const {
    return VoidIntersectionCorrector();
  }

  /// Method for on-demand transport of the covariance
  /// to a new curvilinear frame at current  position,
  /// or direction of the state - for the moment a dummy method
  ///
  /// @param [in,out] state State of the stepper
  /// @param [in] reinitialize is a flag to steer whether the
  ///        state should be reinitialized at the new
  ///        position
  void covarianceTransport(State& state, bool reinitialize = false) const {
    // Transport the covariance
	ActsRowVectorD<3> normVec(state.dir);
    const FreeRowVector sfactors =
        normVec * state.jacTransport.template topLeftCorner<3, FreeParsDim>();
    // The full jacobian is
	const Jacobian jacFull  = state.jacTransport - state.derivative * sfactors;
	
    // Apply the actual covariance transport
    state.cov = (jacFull * state.cov * jacFull.transpose());
    // Reinitialize if asked to do so
    // this is useful for interruption calls
    if (reinitialize) {
      // reset the jacobian
      state.jacTransport = Jacobian::Identity();
    }
    // Store The global and bound jacobian (duplication for the moment)
    state.jacobian = jacFull * state.jacobian;
  }

  /// Method for on-demand transport of the covariance
  /// to a new curvilinear frame at current  position,
  /// or direction of the state - for the moment a dummy method
  ///
  /// @tparam surface_t the surface type - ignored here
  ///
  /// @param [in,out] state The stepper state
  /// @param [in] surface is the surface to which the covariance is
  ///        forwarded to
  /// @param [in] reinitialize is a flag to steer whether the
  ///        state should be reinitialized at the new
  ///        position
  /// @note no check is done if the position is actually on the surface
  ///
  void covarianceTransport(State& state, const Surface& surface,
                           bool reinitialize = false) const {
    // calculate the form factors for the derivatives
    const FreeRowVector sVec = surface.derivativeFactors(
        state.geoContext, state.pos, state.dir, rframeT, state.jacTransport);
    // the full jacobian is
    const Jacobian jacFull = state.jacTransport - state.derivative * sVec;
    // Apply the actual covariance transport
    state.cov = (jacFull * state.cov * jacFull.transpose());
    // Reinitialize if asked to do so
    // this is useful for interruption calls
    if (reinitialize) {
      // reset the jacobian
      state.jacTransport = FreeMatrix::Identity();
      // reset the derivative
      state.derivative = FreeVector::Zero();
    }
    // Store The global and bound jacobian (duplication for the moment)
    state.jacobian = jacFull * state.jacobian;
  }

  /// Perform a straight line propagation step
  ///
  /// @param [in,out] state is the propagation state associated with the track
  /// parameters that are being propagated.
  ///                The state contains the desired step size,
  ///                it can be negative during backwards track propagation,
  ///                and since we're using an adaptive algorithm, it can
  ///                be modified by the stepper class during propagation.
  ///
  /// @return the step size taken
  template <typename propagator_state_t>
  Result<double> step(propagator_state_t& state) const {
    // use the adjusted step size
    const auto h = state.stepping.stepSize;
    // time propagates along distance as 1/b = sqrt(1 + m²/p²)
    const auto dtds = std::hypot(1, state.options.mass / state.stepping.p);
    // Update the track parameters according to the equations of motion
    state.stepping.pos += h * state.stepping.dir;
    state.stepping.dt += h * dtds;
    // Propagate the jacobian
    if (state.stepping.covTransport) {
      // The step transport matrix in global coordinates
      FreeMatrix D = FreeMatrix::Identity();
      D.block<3, 3>(0, 4) = ActsSymMatrixD<3>::Identity() * h;
      // Extend the calculation by the time propagation
      // Evaluate dt/dlambda
      D(3, 7) = h * state.options.mass * state.options.mass /
                (state.stepping.p * dtds);
      // Set the derivative factor the time
      state.stepping.derivative(3) = dtds;
      // Update jacobian and derivative
      state.stepping.jacTransport = D * state.stepping.jacTransport;
      state.stepping.derivative.template head<3>() = state.stepping.dir;
    }
    // state the path length
    state.stepping.pathAccumulated += h;

    // return h
    return h;
  }
};

}  // namespace Acts
