// This file is part of the Acts project.
//
// Copyright (C) 2019 Acts project team
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

template <typename B, typename C, typename E, typename A>
Acts::EigenStepper<B, C, E, A>::EigenStepper(B bField)
  : m_bField(std::move(bField))
{
}

template <typename B, typename C, typename E, typename A>
template <typename options_t>
std::pair<bool,double>
Acts::EigenStepper<B, C, E, A>::targetSurface(State& state,
                const Surface*     surface,
                const options_t&   navOpts,
                const Corrector& navCorr) const
{
    // Intersect the surface
    auto surfaceIntersect = surface->surfaceIntersectionEstimate(state.geoContext,
        position(state), direction(state), navOpts, navCorr);
    if (surfaceIntersect) {
      double ssize = surfaceIntersect.intersection.pathLength;
      // update the stepsize
      updateStep(state, navCorr, ssize, true);
      return std::move(std::make_pair(true, ssize));
    }
    return std::move(std::make_pair(false, std::numeric_limits<double>::max()));
}

template <typename B, typename C, typename E, typename A>
void
Acts::EigenStepper<B, C, E, A>::updateStep(State& state,
             const Corrector& navCorr,
             double             stepSize,
             bool               release) const
{
  state.stepSize.update(stepSize, cstep::actor, release);
  navCorr(state.stepSize);
}

template <typename B, typename C, typename E, typename A>
void
Acts::EigenStepper<B, C, E, A>::updateStep(State& state,
             double      stepSize,
             cstep::Type type) const
{
  state.stepSize.update(stepSize, type);
}

template <typename B, typename C, typename E, typename A>
auto
Acts::EigenStepper<B, C, E, A>::boundState(State& state,
                                           const Surface& surface,
                                           bool           reinitialize) const
    -> BoundState
{
  // Transport the covariance to here
  std::unique_ptr<const Covariance> covPtr = nullptr;
  if (state.covTransport) {
    covarianceTransport(state, surface, reinitialize);
    covPtr = std::make_unique<const Covariance>(state.cov);
  }
  // Create the bound parameters
  BoundParameters parameters(state.geoContext,
                             std::move(covPtr),
                             state.pos,
                             state.p * state.dir,
                             state.q,
                             surface.getSharedPtr());
  // Create the bound state
  BoundState bState{
      std::move(parameters), state.jacobian, state.pathAccumulated};
  // Reset the jacobian to identity
  if (reinitialize) {
    state.jacobian = Jacobian::Identity();
  }
  /// Return the State
  return bState;
}

template <typename B, typename C, typename E, typename A>
auto
Acts::EigenStepper<B, C, E, A>::curvilinearState(State& state,
                                                 bool reinitialize) const
    -> CurvilinearState
{
  // Transport the covariance to here
  std::unique_ptr<const Covariance> covPtr = nullptr;
  if (state.covTransport) {
    covarianceTransport(state, reinitialize);
    covPtr = std::make_unique<const Covariance>(state.cov);
  }
  // Create the curvilinear parameters
  CurvilinearParameters parameters(
      std::move(covPtr), state.pos, state.p * state.dir, state.q);
  // Create the bound state
  CurvilinearState curvState{
      std::move(parameters), state.jacobian, state.pathAccumulated};
  // Reset the jacobian to identity
  if (reinitialize) {
    state.jacobian = Jacobian::Identity();
  }
  /// Return the State
  return curvState;
}

template <typename B, typename C, typename E, typename A>
void
Acts::EigenStepper<B, C, E, A>::update(State& state,
                                       const BoundParameters& pars) const
{
  const auto& mom = pars.momentum();
  state.pos       = pars.position();
  state.dir       = mom.normalized();
  state.p         = mom.norm();
  if (pars.covariance() != nullptr) {
    state.cov = (*(pars.covariance()));
  }
}

template <typename B, typename C, typename E, typename A>
void
Acts::EigenStepper<B, C, E, A>::update(State& state,
                                       const Vector3D& uposition,
                                       const Vector3D& udirection,
                                       double          up) const
{
  state.pos = uposition;
  state.dir = udirection;
  state.p   = up;
}

template <typename B, typename C, typename E, typename A>
void
Acts::EigenStepper<B, C, E, A>::covarianceTransport(State& state,
                                                    bool reinitialize) const
{
  // Optimized trigonometry on the propagation direction
  const double x = state.dir(0);  // == cos(phi) * sin(theta)
  const double y = state.dir(1);  // == sin(phi) * sin(theta)
  const double z = state.dir(2);  // == cos(theta)
  // can be turned into cosine/sine
  const double cosTheta    = z;
  const double sinTheta    = sqrt(x * x + y * y);
  const double invSinTheta = 1. / sinTheta;
  const double cosPhi      = x * invSinTheta;
  const double sinPhi      = y * invSinTheta;
  // prepare the jacobian to curvilinear
  ActsMatrixD<5, 7> jacToCurv = ActsMatrixD<5, 7>::Zero();
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
    const double c    = sqrt(y * y + z * z);
    const double invC = 1. / c;
    jacToCurv(0, 1) = -z * invC;
    jacToCurv(0, 2) = y * invC;
    jacToCurv(1, 0) = c;
    jacToCurv(1, 1) = -x * y * invC;
    jacToCurv(1, 2) = -x * z * invC;
  }
  // Directional and momentum parameters for curvilinear
  jacToCurv(2, 3) = -sinPhi * invSinTheta;
  jacToCurv(2, 4) = cosPhi * invSinTheta;
  jacToCurv(3, 5) = -invSinTheta;
  jacToCurv(4, 6) = 1;
  // Apply the transport from the steps on the jacobian
  state.jacToGlobal = state.jacTransport * state.jacToGlobal;
  // Transport the covariance
  ActsRowVectorD<3>       normVec(state.dir);
  const ActsRowVectorD<5> sfactors
      = normVec * state.jacToGlobal.template topLeftCorner<3, 5>();
  // The full jacobian is ([to local] jacobian) * ([transport] jacobian)
  const ActsMatrixD<5, 5> jacFull
      = jacToCurv * (state.jacToGlobal - state.derivative * sfactors);
  // Apply the actual covariance transport
  state.cov = (jacFull * state.cov * jacFull.transpose());
  // Reinitialize if asked to do so
  // this is useful for interruption calls
  if (reinitialize) {
    // reset the jacobians
    state.jacToGlobal  = ActsMatrixD<7, 5>::Zero();
    state.jacTransport = ActsMatrixD<7, 7>::Identity();
    // fill the jacobian to global for next transport
    state.jacToGlobal(0, eLOC_0) = -sinPhi;
    state.jacToGlobal(0, eLOC_1) = -cosPhi * cosTheta;
    state.jacToGlobal(1, eLOC_0) = cosPhi;
    state.jacToGlobal(1, eLOC_1) = -sinPhi * cosTheta;
    state.jacToGlobal(2, eLOC_1) = sinTheta;
    state.jacToGlobal(3, ePHI)   = -sinTheta * sinPhi;
    state.jacToGlobal(3, eTHETA) = cosTheta * cosPhi;
    state.jacToGlobal(4, ePHI)   = sinTheta * cosPhi;
    state.jacToGlobal(4, eTHETA) = cosTheta * sinPhi;
    state.jacToGlobal(5, eTHETA) = -sinTheta;
    state.jacToGlobal(6, eQOP)   = 1;
  }
  // Store The global and bound jacobian (duplication for the moment)
  state.jacobian = jacFull * state.jacobian;
}

template <typename B, typename C, typename E, typename A>
void
Acts::EigenStepper<B, C, E, A>::covarianceTransport(State& state,
                                                    const Surface& surface,
                                                    bool reinitialize) const
{
  using VectorHelpers::phi;
  using VectorHelpers::theta;
  // Initialize the transport final frame jacobian
  ActsMatrixD<5, 7> jacToLocal = ActsMatrixD<5, 7>::Zero();
  // initalize the jacobian to local, returns the transposed ref frame
  auto rframeT = surface.initJacobianToLocal(
      state.geoContext, jacToLocal, state.pos, state.dir);
  // Update the jacobian with the transport from the steps
  state.jacToGlobal = state.jacTransport * state.jacToGlobal;
  // calculate the form factors for the derivatives
  const ActsRowVectorD<5> sVec = surface.derivativeFactors(
      state.geoContext, state.pos, state.dir, rframeT, state.jacToGlobal);
  // the full jacobian is ([to local] jacobian) * ([transport] jacobian)
  const ActsMatrixD<5, 5> jacFull
      = jacToLocal * (state.jacToGlobal - state.derivative * sVec);
  // Apply the actual covariance transport
  state.cov = (jacFull * state.cov * jacFull.transpose());
  // Reinitialize if asked to do so
  // this is useful for interruption calls
  if (reinitialize) {
    // reset the jacobians
    state.jacToGlobal  = ActsMatrixD<7, 5>::Zero();
    state.jacTransport = ActsMatrixD<7, 7>::Identity();
    // reset the derivative
    state.derivative = ActsVectorD<7>::Zero();
    // fill the jacobian to global for next transport
    Vector2D loc{0., 0.};
    surface.globalToLocal(state.geoContext, state.pos, state.dir, loc);
    ActsVectorD<5> pars;
    pars << loc[eLOC_0], loc[eLOC_1], phi(state.dir), theta(state.dir),
        state.q / state.p;
    surface.initJacobianToGlobal(
        state.geoContext, state.jacToGlobal, state.pos, state.dir, pars);
  }
  // Store The global and bound jacobian (duplication for the moment)
  state.jacobian = jacFull * state.jacobian;
}

template <typename B, typename C, typename E, typename A>
template <typename propagator_state_t>
Acts::Result<double>
Acts::EigenStepper<B, C, E, A>::step(propagator_state_t& state) const
{
  // Runge-Kutta integrator state
  auto&  sd             = state.stepping.stepData;
  double error_estimate = 0.;
  double h2, half_h;

  // First Runge-Kutta point (at current position)
  sd.B_first = getField(state.stepping, state.stepping.pos);
  if (!state.stepping.extension.validExtensionForStep(state, *this,state.stepping)
      || !state.stepping.extension.k1(state, *this, state.stepping, sd.k1, sd.B_first)) {
    return 0.;
  }

  // The following functor starts to perform a Runge-Kutta step of a certain
  // size, going up to the point where it can return an estimate of the local
  // integration error. The results are stated in the local variables above,
  // allowing integration to continue once the error is deemed satisfactory
  const auto tryRungeKuttaStep = [&](const double h) -> bool {
    // State the square and half of the step size
    h2     = h * h;
    half_h = h * 0.5;

    // Second Runge-Kutta point
    const Vector3D pos1
        = state.stepping.pos + half_h * state.stepping.dir + h2 * 0.125 * sd.k1;
    sd.B_middle = getField(state.stepping, pos1);
    if (!state.stepping.extension.k2(
            state, *this, state.stepping, sd.k2, sd.B_middle, half_h, sd.k1)) {
      return false;
    }

    // Third Runge-Kutta point
    if (!state.stepping.extension.k3(
            state, *this, state.stepping, sd.k3, sd.B_middle, half_h, sd.k2)) {
      return false;
    }

    // Last Runge-Kutta point
    const Vector3D pos2
        = state.stepping.pos + h * state.stepping.dir + h2 * 0.5 * sd.k3;
    sd.B_last = getField(state.stepping, pos2);
    if (!state.stepping.extension.k4(
            state, *this, state.stepping, sd.k4, sd.B_last, h, sd.k3)) {
      return false;
    }

    // Compute and check the local integration error estimate
    error_estimate = std::max(
        h2 * (sd.k1 - sd.k2 - sd.k3 + sd.k4).template lpNorm<1>(), 1e-20);
    return (error_estimate <= state.options.tolerance);
  };

  double stepSizeScaling;

  // Select and adjust the appropriate Runge-Kutta step size as given
  // ATL-SOFT-PUB-2009-001
  while (!tryRungeKuttaStep(state.stepping.stepSize)) {
    stepSizeScaling
        = std::min(std::max(0.25,
                            std::pow((state.options.tolerance
                                      / std::abs(2. * error_estimate)),
                                     0.25)),
                   4.);
    if (stepSizeScaling == 1.) {
      break;
    }
    state.stepping.stepSize = state.stepping.stepSize * stepSizeScaling;

    // If step size becomes too small the particle remains at the initial
    // place
    if (state.stepping.stepSize * state.stepping.stepSize
        < state.options.stepSizeCutOff * state.options.stepSizeCutOff) {
      // Not moving due to too low momentum needs an aborter
      return EigenStepperError::StepSizeStalled;
    }
  }

  // use the adjusted step size
  const double h = state.stepping.stepSize;

  // When doing error propagation, update the associated Jacobian matrix
  if (state.stepping.covTransport) {
    // The step transport matrix in global coordinates
    ActsMatrixD<7, 7> D;
    if (!state.stepping.extension.finalize(state, *this, state.stepping, h, D)) {
      return EigenStepperError::StepInvalid;
    }

    // for moment, only update the transport part
    state.stepping.jacTransport = D * state.stepping.jacTransport;
  } else {
    if (!state.stepping.extension.finalize(state, *this, state.stepping, h)) {
      return EigenStepperError::StepInvalid;
    }
  }

  // Update the track parameters according to the equations of motion
  state.stepping.pos
      += h * state.stepping.dir + h2 / 6. * (sd.k1 + sd.k2 + sd.k3);
  state.stepping.dir += h / 6. * (sd.k1 + 2. * (sd.k2 + sd.k3) + sd.k4);
  state.stepping.dir /= state.stepping.dir.norm();
  state.stepping.derivative.template head<3>()     = state.stepping.dir;
  state.stepping.derivative.template segment<3>(3) = sd.k4;
  state.stepping.pathAccumulated += h;

  return h;
}
