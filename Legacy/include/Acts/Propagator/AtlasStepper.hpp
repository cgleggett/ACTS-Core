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

// This is based original stepper code from the ATLAS RungeKuttePropagagor
namespace Acts {

/// @brief the AtlasStepper implementation for the
template <typename bfield_t>
class AtlasStepper {
  // This struct is a meta-function which normally maps to BoundParameters...
  template <typename T, typename S>
  struct s {
    using type = BoundParameters;
  };

  // Unless S is int, then it maps to CurvilinearParameters ...
  template <typename T>
  struct s<T, int> {
    using type = CurvilinearParameters;
  };

 public:
  using cstep = detail::ConstrainedStep;

  using Jacobian = BoundMatrix;
  using Covariance = BoundSymMatrix;
  using BoundState = std::tuple<BoundParameters, Jacobian, double>;
  using CurvilinearState = std::tuple<CurvilinearParameters, Jacobian, double>;

  using Corrector = VoidIntersectionCorrector;

  /// @brief Nested State struct for the local caching
  struct State {
    /// Default constructor - deleted
    State() = delete;

    /// Constructor
    ///
    /// @tparams Type of TrackParameters
    ///
    /// @param[in] gctx The geometry contex tof this call
    /// @param[in] mctx The magnetic field context of this call
    /// @param[in] pars Input parameters
    /// @param[in] ndir The navigation direction w.r.t. parameters
    /// @param[in] ssize the steps size limitation
    template <typename Parameters>
    State(std::reference_wrapper<const GeometryContext> gctx,
          std::reference_wrapper<const MagneticFieldContext> mctx,
          const Parameters& pars, NavigationDirection ndir = forward,
          double ssize = std::numeric_limits<double>::max())
        : state_ready(false),
          navDir(ndir),
          useJacobian(false),
          step(0.),
          maxPathLength(0.),
          mcondition(false),
          needgradient(false),
          newfield(true),
          field(0., 0., 0.),
          covariance(nullptr),
          t0(pars.time()),
          stepSize(ndir * std::abs(ssize)),
          fieldCache(mctx),
          geoContext(gctx) {
      // The rest of this constructor is copy&paste of AtlasStepper::update() -
      // this is a nasty but working solution for the stepper state without
      // functions

      const ActsVectorD<3> pos = pars.position();
      const auto Vp = pars.parameters();

      double Sf, Cf, Ce, Se;
      Sf = sin(Vp(2));
      Cf = cos(Vp(2));
      Se = sin(Vp(3));
      Ce = cos(Vp(3));

      pVector[0] = pos(0);
      pVector[1] = pos(1);
      pVector[2] = pos(2);
      pVector[3] = 0.;
      pVector[4] = Cf * Se;
      pVector[5] = Sf * Se;
      pVector[6] = Ce;
      pVector[7] = Vp[4];

      // @todo: remove magic numbers - is that the charge ?
      if (std::abs(pVector[7]) < .000000000000001) {
        pVector[7] < 0. ? pVector[7] = -.000000000000001
                        : pVector[7] = .000000000000001;
      }

      // prepare the jacobian if we have a covariance
      if (pars.covariance()) {
        // copy the covariance matrix
        covariance = new ActsSymMatrixD<BoundParsDim>(*pars.covariance());
        covTransport = true;
        useJacobian = true;
        const auto transform = pars.referenceFrame(geoContext);

        pVector[8] = transform(0, eLOC_0);
        pVector[16] = transform(0, eLOC_1);
        pVector[24] = 0.;
        pVector[32] = 0.;
        pVector[40] = 0.;
        pVector[48] = 0.;  // dX /

        pVector[9] = transform(1, eLOC_0);
        pVector[17] = transform(1, eLOC_1);
        pVector[25] = 0.;
        pVector[33] = 0.;
        pVector[41] = 0.;
        pVector[49] = 0.;  // dY /

        pVector[10] = transform(2, eLOC_0);
        pVector[18] = transform(2, eLOC_1);
        pVector[26] = 0.;
        pVector[34] = 0.;
        pVector[42] = 0.;
        pVector[50] = 0.;  // dZ /

        pVector[11] = 0.;
        pVector[19] = 0.;
        pVector[27] = 0.;
        pVector[35] = 0.;
        pVector[43] = 0.;
        pVector[51] = 1.;  // dT/

        pVector[12] = 0.;
        pVector[20] = 0.;
        pVector[28] = -Sf * Se;  // - sin(phi) * cos(theta)
        pVector[36] = Cf * Ce;   // cos(phi) * cos(theta)
        pVector[44] = 0.;
        pVector[52] = 0.;  // dAx/

        pVector[13] = 0.;
        pVector[21] = 0.;
        pVector[29] = Cf * Se;  // cos(phi) * sin(theta)
        pVector[37] = Sf * Ce;  // sin(phi) * cos(theta)
        pVector[45] = 0.;
        pVector[53] = 0.;  // dAy/

        pVector[14] = 0.;
        pVector[22] = 0.;
        pVector[30] = 0.;
        pVector[38] = -Se;  // - sin(theta)
        pVector[46] = 0.;
        pVector[54] = 0.;  // dAz/

        pVector[15] = 0.;
        pVector[23] = 0.;
        pVector[31] = 0.;
        pVector[39] = 0.;
        pVector[47] = 1.;
        pVector[55] = 0.;  // dCM/

        pVector[56] = 0.;
        pVector[57] = 0.;
        pVector[58] = 0.;

        // special treatment for surface types
        const auto& surface = pars.referenceSurface();
        // the disc needs polar coordinate adaptations
        if (surface.type() == Surface::Disc) {
          double lCf = cos(Vp[1]);
          double lSf = sin(Vp[1]);
          double Ax[3] = {transform(0, 0), transform(1, 0), transform(2, 0)};
          double Ay[3] = {transform(0, 1), transform(1, 1), transform(2, 1)};
          double d0 = lCf * Ax[0] + lSf * Ay[0];
          double d1 = lCf * Ax[1] + lSf * Ay[1];
          double d2 = lCf * Ax[2] + lSf * Ay[2];
          pVector[8] = d0;
          pVector[9] = d1;
          pVector[10] = d2;
          pVector[16] = Vp[0] * (lCf * Ay[0] - lSf * Ax[0]);
          pVector[17] = Vp[0] * (lCf * Ay[1] - lSf * Ax[1]);
          pVector[18] = Vp[0] * (lCf * Ay[2] - lSf * Ax[2]);
        }
        // the line needs components that relate direction change
        // with global frame change
        if (surface.type() == Surface::Perigee ||
            surface.type() == Surface::Straw) {
          // sticking to the nomenclature of the original RkPropagator
          // - axis pointing along the drift/transverse direction
          double B[3] = {transform(0, 0), transform(1, 0), transform(2, 0)};
          // - axis along the straw
          double A[3] = {transform(0, 1), transform(1, 1), transform(2, 1)};
          // - normal vector of the reference frame
          double C[3] = {transform(0, 2), transform(1, 2), transform(2, 2)};

          // projection of direction onto normal vector of reference frame
          double PC = pVector[4] * C[0] + pVector[5] * C[1] + pVector[6] * C[2];
          double Bn = 1. / PC;

          double Bx2 = -A[2] * pVector[29];
          double Bx3 = A[1] * pVector[38] - A[2] * pVector[37];

          double By2 = A[2] * pVector[28];
          double By3 = A[2] * pVector[36] - A[0] * pVector[38];

          double Bz2 = A[0] * pVector[29] - A[1] * pVector[28];
          double Bz3 = A[0] * pVector[37] - A[1] * pVector[36];

          double B2 = B[0] * Bx2 + B[1] * By2 + B[2] * Bz2;
          double B3 = B[0] * Bx3 + B[1] * By3 + B[2] * Bz3;

          Bx2 = (Bx2 - B[0] * B2) * Bn;
          Bx3 = (Bx3 - B[0] * B3) * Bn;
          By2 = (By2 - B[1] * B2) * Bn;
          By3 = (By3 - B[1] * B3) * Bn;
          Bz2 = (Bz2 - B[2] * B2) * Bn;
          Bz3 = (Bz3 - B[2] * B3) * Bn;

          //  /dPhi      |     /dThe       |
          pVector[24] = Bx2 * Vp[0];
          pVector[32] = Bx3 * Vp[0];  // dX/
          pVector[25] = By2 * Vp[0];
          pVector[33] = By3 * Vp[0];  // dY/
          pVector[26] = Bz2 * Vp[0];
          pVector[34] = Bz3 * Vp[0];  // dZ/
        }
      }
      // now declare the state as ready
      state_ready = true;
    }

    // optimisation that init is not called twice
    bool state_ready = false;
    // configuration
    NavigationDirection navDir;
    bool useJacobian;
    double step;
    double maxPathLength;
    bool mcondition;
    bool needgradient;
    bool newfield;
    // internal parameters to be used
    Vector3D field;
    double pVector[59];

    /// Storage pattern of pVector
    ///                   /dL0    /dL1    /dPhi   /dThe   /dCM   /dT
    /// X  ->P[0]  dX /   P[ 8]   P[16]   P[24]   P[32]   P[40]  P[48]
    /// Y  ->P[1]  dY /   P[ 9]   P[17]   P[25]   P[33]   P[41]  P[49]
    /// Z  ->P[2]  dZ /   P[10]   P[18]   P[26]   P[34]   P[42]  P[50]
    /// T  ->P[3]  dT/	  P[11]   P[19]   P[27]   P[35]   P[43]  P[51]
    /// Ax ->P[4]  dAx/   P[12]   P[20]   P[28]   P[36]   P[44]  P[52]
    /// Ay ->P[5]  dAy/   P[13]   P[21]   P[29]   P[37]   P[45]  P[53]
    /// Az ->P[6]  dAz/   P[14]   P[22]   P[30]   P[38]   P[46]  P[54]
    /// CM ->P[7]  dCM/   P[15]   P[23]   P[31]   P[39]   P[47]  P[55]
    /// Cache: P[56] - P[58]

    // result
    double parameters[BoundParsDim] = {0., 0., 0., 0., 0., 0.};
    const Covariance* covariance;
    Covariance cov = Covariance::Zero();
    bool covTransport = false;
    double jacobian[BoundParsDim * BoundParsDim];

    // accummulated path length cache
    double pathAccumulated = 0.;
    // Starting time
    const double t0;

    // adaptive step size of the runge-kutta integration
    cstep stepSize = std::numeric_limits<double>::max();

    /// It caches the current magnetic field cell and stays (and interpolates)
    ///  within as long as this is valid. See step() code for details.
    typename bfield_t::Cache fieldCache;

    /// Cache the geometry context
    std::reference_wrapper<const GeometryContext> geoContext;

    /// Debug output
    /// the string where debug messages are stored (optionally)
    bool debug = false;
    std::string debugString = "";
    /// buffer & formatting for consistent output
    size_t debugPfxWidth = 30;
    size_t debugMsgWidth = 50;
  };

  template <typename T, typename S = int>
  using return_parameter_type = typename s<T, S>::type;

  AtlasStepper(bfield_t bField = bfield_t()) : m_bField(std::move(bField)){};

  /// Get the field for the stepping
  /// It checks first if the access is still within the Cell,
  /// and updates the cell if necessary, then it takes the field
  /// from the cell
  /// @param [in,out] state is the stepper state associated with the track
  ///                 the magnetic field cell is used (and potentially updated)
  /// @param [in] pos is the field position
  Vector3D getField(State& state, const Vector3D& pos) const {
    // get the field from the cell
    state.field = m_bField.getField(pos, state.fieldCache);
    return state.field;
  }

  Vector3D position(const State& state) const {
    return Vector3D(state.pVector[0], state.pVector[1], state.pVector[2]);
  }

  Vector3D direction(const State& state) const {
    return Vector3D(state.pVector[4], state.pVector[5], state.pVector[6]);
  }

  double momentum(const State& state) const {
    return 1. / std::abs(state.pVector[7]);
  }

  /// Charge access
  double charge(const State& state) const {
    return state.pVector[7] > 0. ? 1. : -1.;
  }

  /// Time access
  double time(const State& state) const { return state.t0 + state.pVector[3]; }

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

  /// Create and return the bound state at the current position
  ///
  ///
  /// @param [in] state State that will be presented as @c BoundState
  /// @param [in] surface The surface to which we bind the state
  /// @param [in] reinitialize Boolean flag whether reinitialization is needed,
  /// i.e. if this is an intermediate state of a larger propagation
  ///
  /// @return A bound state:
  ///   - the parameters at the surface
  ///   - the stepwise jacobian towards it
  ///   - and the path length (from start - for ordering)
  BoundState boundState(State& state, const Surface& surface,
                        bool /*unused*/) const {
    // the convert method invalidates the state (in case it's reused)
    state.state_ready = false;

    /// The transport of the position
    Acts::Vector3D gp(state.pVector[0], state.pVector[1], state.pVector[2]);
    Acts::Vector3D mom(state.pVector[4], state.pVector[5], state.pVector[6]);
    mom /= std::abs(state.pVector[7]);

    // The transport of the covariance
    std::unique_ptr<const Covariance> cov = nullptr;
    std::optional<Covariance> covOpt = std::nullopt;
    if (state.covTransport) {
      covarianceTransport(state, surface, true);
      covOpt = state.cov;
    }

    // Fill the end parameters
    BoundParameters parameters(state.geoContext, std::move(covOpt), gp, mom,
                               charge(state), state.t0 + state.pVector[3],
                               surface.getSharedPtr());

    return BoundState(std::move(parameters), state.jacobian,
                      state.pathAccumulated);
  }

  /// Create and return a curvilinear state at the current position
  ///
  ///
  /// @param [in] state State that will be presented as @c CurvilinearState
  /// @param [in] reinitialize Boolean flag whether reinitialization is needed
  /// i.e. if this is an intermediate state of a larger propagation
  ///
  /// @return A curvilinear state:
  ///   - the curvilinear parameters at given position
  ///   - the stepweise jacobian towards it
  ///   - and the path length (from start - for ordering)
  CurvilinearState curvilinearState(State& state, bool /*unused*/) const {
    // the convert method invalidates the state (in case it's reused)
    state.state_ready = false;
    //
    Acts::Vector3D gp(state.pVector[0], state.pVector[1], state.pVector[2]);
    Acts::Vector3D mom(state.pVector[4], state.pVector[5], state.pVector[6]);
    mom /= std::abs(state.pVector[7]);

    std::optional<Covariance> covOpt = std::nullopt;
    if (state.covTransport) {
      covarianceTransport(state, true);
      covOpt = state.cov;
    }

    CurvilinearParameters parameters(std::move(covOpt), gp, mom, charge(state),
                                     state.t0 + state.pVector[3]);

    return CurvilinearState(std::move(parameters), state.jacobian,
                            state.pathAccumulated);
  }

  /// The state update method
  ///
  /// @param [in,out] state The stepper state for
  /// @param [in] pars The new track parameters at start
  void update(State& state, const BoundParameters& pars) const {
    // state is ready - noting to do
    if (state.state_ready) {
      return;
    }

    const ActsVectorD<3> pos = pars.position();
    const auto Vp = pars.parameters();

    double Sf, Cf, Ce, Se;
    Sf = sin(Vp(2));
    Cf = cos(Vp(2));
    Se = sin(Vp(3));
    Ce = cos(Vp(3));

    state.pVector[0] = pos(0);
    state.pVector[1] = pos(1);
    state.pVector[2] = pos(2);
    state.pVector[3] = pars.time();
    state.pVector[4] = Cf * Se;
    state.pVector[5] = Sf * Se;
    state.pVector[6] = Ce;
    state.pVector[7] = Vp[4];

    // @todo: remove magic numbers - is that the charge ?
    if (std::abs(state.pVector[7]) < .000000000000001) {
      state.pVector[7] < 0. ? state.pVector[7] = -.000000000000001
                            : state.pVector[7] = .000000000000001;
    }

    // prepare the jacobian if we have a covariance
    if (pars.covariance()) {
      // copy the covariance matrix
      state.covariance = new ActsSymMatrixD<BoundParsDim>(*pars.covariance());
      state.covTransport = true;
      state.useJacobian = true;
      const auto transform = pars.referenceFrame(state.geoContext);

      state.pVector[8] = transform(0, eLOC_0);
      state.pVector[16] = transform(0, eLOC_1);
      state.pVector[24] = 0.;
      state.pVector[32] = 0.;
      state.pVector[40] = 0.;
      state.pVector[48] = 0.;  // dX /

      state.pVector[9] = transform(1, eLOC_0);
      state.pVector[17] = transform(1, eLOC_1);
      state.pVector[25] = 0.;
      state.pVector[33] = 0.;
      state.pVector[41] = 0.;
      state.pVector[49] = 0.;  // dY /

      state.pVector[10] = transform(2, eLOC_0);
      state.pVector[18] = transform(2, eLOC_1);
      state.pVector[26] = 0.;
      state.pVector[34] = 0.;
      state.pVector[42] = 0.;
      state.pVector[50] = 0.;  // dZ /

      state.pVector[11] = 0.;
      state.pVector[19] = 0.;
      state.pVector[27] = 0.;
      state.pVector[35] = 0.;
      state.pVector[43] = 0.;
      state.pVector[51] = 1.;  // dT/

      state.pVector[12] = 0.;
      state.pVector[20] = 0.;
      state.pVector[28] = -Sf * Se;  // - sin(phi) * cos(theta)
      state.pVector[36] = Cf * Ce;   // cos(phi) * cos(theta)
      state.pVector[44] = 0.;
      state.pVector[52] = 0.;  // dAx/

      state.pVector[13] = 0.;
      state.pVector[21] = 0.;
      state.pVector[29] = Cf * Se;  // cos(phi) * sin(theta)
      state.pVector[37] = Sf * Ce;  // sin(phi) * cos(theta)
      state.pVector[45] = 0.;
      state.pVector[53] = 0.;  // dAy/

      state.pVector[14] = 0.;
      state.pVector[22] = 0.;
      state.pVector[30] = 0.;
      state.pVector[38] = -Se;  // - sin(theta)
      state.pVector[46] = 0.;
      state.pVector[54] = 0.;  // dAz/

      state.pVector[15] = 0.;
      state.pVector[23] = 0.;
      state.pVector[31] = 0.;
      state.pVector[39] = 0.;
      state.pVector[47] = 1.;
      state.pVector[55] = 0.;  // dCM/

      state.pVector[56] = 0.;
      state.pVector[57] = 0.;
      state.pVector[58] = 0.;

      // special treatment for surface types
      const auto& surface = pars.referenceSurface();
      // the disc needs polar coordinate adaptations
      if (surface.type() == Surface::Disc) {
        double lCf = cos(Vp[1]);
        double lSf = sin(Vp[1]);
        double Ax[3] = {transform(0, 0), transform(1, 0), transform(2, 0)};
        double Ay[3] = {transform(0, 1), transform(1, 1), transform(2, 1)};
        double d0 = lCf * Ax[0] + lSf * Ay[0];
        double d1 = lCf * Ax[1] + lSf * Ay[1];
        double d2 = lCf * Ax[2] + lSf * Ay[2];
        state.pVector[8] = d0;
        state.pVector[9] = d1;
        state.pVector[10] = d2;
        state.pVector[16] = Vp[0] * (lCf * Ay[0] - lSf * Ax[0]);
        state.pVector[17] = Vp[0] * (lCf * Ay[1] - lSf * Ax[1]);
        state.pVector[18] = Vp[0] * (lCf * Ay[2] - lSf * Ax[2]);
      }
      // the line needs components that relate direction change
      // with global frame change
      if (surface.type() == Surface::Perigee ||
          surface.type() == Surface::Straw) {
        // sticking to the nomenclature of the original RkPropagator
        // - axis pointing along the drift/transverse direction
        double B[3] = {transform(0, 0), transform(1, 0), transform(2, 0)};
        // - axis along the straw
        double A[3] = {transform(0, 1), transform(1, 1), transform(2, 1)};
        // - normal vector of the reference frame
        double C[3] = {transform(0, 2), transform(1, 2), transform(2, 2)};

        // projection of direction onto normal vector of reference frame
        double PC = state.pVector[4] * C[0] + state.pVector[5] * C[1] +
                    state.pVector[6] * C[2];
        double Bn = 1. / PC;

        double Bx2 = -A[2] * state.pVector[29];
        double Bx3 = A[1] * state.pVector[38] - A[2] * state.pVector[37];

        double By2 = A[2] * state.pVector[28];
        double By3 = A[2] * state.pVector[36] - A[0] * state.pVector[38];

        double Bz2 = A[0] * state.pVector[29] - A[1] * state.pVector[28];
        double Bz3 = A[0] * state.pVector[37] - A[1] * state.pVector[36];

        double B2 = B[0] * Bx2 + B[1] * By2 + B[2] * Bz2;
        double B3 = B[0] * Bx3 + B[1] * By3 + B[2] * Bz3;

        Bx2 = (Bx2 - B[0] * B2) * Bn;
        Bx3 = (Bx3 - B[0] * B3) * Bn;
        By2 = (By2 - B[1] * B2) * Bn;
        By3 = (By3 - B[1] * B3) * Bn;
        Bz2 = (Bz2 - B[2] * B2) * Bn;
        Bz3 = (Bz3 - B[2] * B3) * Bn;

        //  /dPhi      |     /dThe       |
        state.pVector[24] = Bx2 * Vp[0];
        state.pVector[32] = Bx3 * Vp[0];  // dX/
        state.pVector[25] = By2 * Vp[0];
        state.pVector[33] = By3 * Vp[0];  // dY/
        state.pVector[26] = Bz2 * Vp[0];
        state.pVector[34] = Bz3 * Vp[0];  // dZ/
      }
    }
    // now declare the state as ready
    state.state_ready = true;
  }

  /// Method to update momentum, direction and p
  ///
  /// @param uposition the updated position
  /// @param udirection the updated direction
  /// @param p the updated momentum value
  void update(State& state, const Vector3D& uposition,
              const Vector3D& udirection, double up, double time) const {
    // update the vector
    state.pVector[0] = uposition[0];
    state.pVector[1] = uposition[1];
    state.pVector[2] = uposition[2];
    state.pVector[3] = time;
    state.pVector[4] = udirection[0];
    state.pVector[5] = udirection[1];
    state.pVector[6] = udirection[2];
    state.pVector[7] = charge(state) / up;
  }

  /// Return a corrector
  VoidIntersectionCorrector corrector(State& /*unused*/) const {
    return VoidIntersectionCorrector();
  }

  /// Method for on-demand transport of the covariance
  /// to a new curvilinear frame at current  position,
  /// or direction of the state
  ///
  /// @param [in,out] state State of the stepper
  /// @param [in] reinitialize is a flag to steer whether the state should be
  /// reinitialized at the new position
  ///
  /// @return the full transport jacobian
  void covarianceTransport(State& state, bool /*unused*/) const {
    double P[59];
    for (unsigned int i = 0; i < 59; ++i) {
      P[i] = state.pVector[i];
    }

    double p = 1. / P[7];
    P[40] *= p;
    P[41] *= p;
    P[42] *= p;
    P[44] *= p;
    P[45] *= p;
    P[46] *= p;

    double An = sqrt(P[4] * P[4] + P[5] * P[5]);
    double Ax[3];
    if (An != 0.) {
      Ax[0] = -P[5] / An;
      Ax[1] = P[4] / An;
      Ax[2] = 0.;
    } else {
      Ax[0] = 1.;
      Ax[1] = 0.;
      Ax[2] = 0.;
    }

    double Ay[3] = {-Ax[1] * P[6], Ax[0] * P[6], An};
    double S[3] = {P[4], P[5], P[6]};

    double A = P[4] * S[0] + P[5] * S[1] + P[6] * S[2];
    if (A != 0.) {
      A = 1. / A;
    }
    S[0] *= A;
    S[1] *= A;
    S[2] *= A;

    double s0 = P[8] * S[0] + P[9] * S[1] + P[10] * S[2];
    double s1 = P[16] * S[0] + P[17] * S[1] + P[18] * S[2];
    double s2 = P[24] * S[0] + P[25] * S[1] + P[26] * S[2];
    double s3 = P[32] * S[0] + P[33] * S[1] + P[34] * S[2];
    double s4 = P[40] * S[0] + P[41] * S[1] + P[42] * S[2];

    P[8] -= (s0 * P[4]);
    P[9] -= (s0 * P[5]);
    P[10] -= (s0 * P[6]);
    P[12] -= (s0 * P[56]);
    P[13] -= (s0 * P[57]);
    P[14] -= (s0 * P[58]);
    P[16] -= (s1 * P[4]);
    P[17] -= (s1 * P[5]);
    P[18] -= (s1 * P[6]);
    P[20] -= (s1 * P[56]);
    P[21] -= (s1 * P[57]);
    P[22] -= (s1 * P[58]);
    P[24] -= (s2 * P[4]);
    P[25] -= (s2 * P[5]);
    P[26] -= (s2 * P[6]);
    P[28] -= (s2 * P[56]);
    P[29] -= (s2 * P[57]);
    P[30] -= (s2 * P[58]);
    P[32] -= (s3 * P[4]);
    P[33] -= (s3 * P[5]);
    P[34] -= (s3 * P[6]);
    P[36] -= (s3 * P[56]);
    P[37] -= (s3 * P[57]);
    P[38] -= (s3 * P[58]);
    P[40] -= (s4 * P[4]);
    P[41] -= (s4 * P[5]);
    P[42] -= (s4 * P[6]);
    P[44] -= (s4 * P[56]);
    P[45] -= (s4 * P[57]);
    P[46] -= (s4 * P[58]);

    double P3, P4, C = P[4] * P[4] + P[5] * P[5];
    if (C > 1.e-20) {
      C = 1. / C;
      P3 = P[4] * C;
      P4 = P[5] * C;
      C = -sqrt(C);
    } else {
      C = -1.e10;
      P3 = 1.;
      P4 = 0.;
    }

    // Jacobian production
    //
    state.jacobian[0] = Ax[0] * P[8] + Ax[1] * P[9];    // dL0/dL0
    state.jacobian[1] = Ax[0] * P[16] + Ax[1] * P[17];  // dL0/dL1
    state.jacobian[2] = Ax[0] * P[24] + Ax[1] * P[25];  // dL0/dPhi
    state.jacobian[3] = Ax[0] * P[32] + Ax[1] * P[33];  // dL0/dThe
    state.jacobian[4] = Ax[0] * P[40] + Ax[1] * P[41];  // dL0/dCM
    state.jacobian[5] = 0.;                             // dL0/dT

    state.jacobian[6] = Ay[0] * P[8] + Ay[1] * P[9] + Ay[2] * P[10];  // dL1/dL0
    state.jacobian[7] =
        Ay[0] * P[16] + Ay[1] * P[17] + Ay[2] * P[18];  // dL1/dL1
    state.jacobian[8] =
        Ay[0] * P[24] + Ay[1] * P[25] + Ay[2] * P[26];  // dL1/dPhi
    state.jacobian[9] =
        Ay[0] * P[32] + Ay[1] * P[33] + Ay[2] * P[34];  // dL1/dThe
    state.jacobian[10] =
        Ay[0] * P[40] + Ay[1] * P[41] + Ay[2] * P[42];  // dL1/dCM
    state.jacobian[11] = 0.;                            // dL1/dT

    state.jacobian[12] = P3 * P[13] - P4 * P[12];  // dPhi/dL0
    state.jacobian[13] = P3 * P[21] - P4 * P[20];  // dPhi/dL1
    state.jacobian[14] = P3 * P[29] - P4 * P[28];  // dPhi/dPhi
    state.jacobian[15] = P3 * P[37] - P4 * P[36];  // dPhi/dThe
    state.jacobian[16] = P3 * P[45] - P4 * P[44];  // dPhi/dCM
    state.jacobian[17] = 0.;                       // dPhi/dT

    state.jacobian[18] = C * P[14];  // dThe/dL0
    state.jacobian[19] = C * P[22];  // dThe/dL1
    state.jacobian[20] = C * P[30];  // dThe/dPhi
    state.jacobian[21] = C * P[38];  // dThe/dThe
    state.jacobian[22] = C * P[46];  // dThe/dCM
    state.jacobian[23] = 0.;         // dThe/dT

    state.jacobian[24] = 0.;     // dCM /dL0
    state.jacobian[25] = 0.;     // dCM /dL1
    state.jacobian[26] = 0.;     // dCM /dPhi
    state.jacobian[27] = 0.;     // dCM /dTheta
    state.jacobian[28] = P[47];  // dCM /dCM
    state.jacobian[29] = 0.;     // dCM/dT

    state.jacobian[30] = 0.;  // dT/dL0
    state.jacobian[31] = 0.;  // dT/dL1
    state.jacobian[32] = 0.;  // dT/dPhi
    state.jacobian[33] = 0.;  // dT/dThe
    state.jacobian[34] = 0.;  // dT/dCM
    state.jacobian[35] = 1.;  // dT/dT

    Eigen::Map<
        Eigen::Matrix<double, BoundParsDim, BoundParsDim, Eigen::RowMajor>>
        J(state.jacobian);
    state.cov = J * (*state.covariance) * J.transpose();
  }

  /// Method for on-demand transport of the covariance
  /// to a new curvilinear frame at current position,
  /// or direction of the state
  ///
  /// @tparam surface_t the Surface type
  ///
  /// @param [in,out] state State of the stepper
  /// @param [in] surface is the surface to which the covariance is forwarded to
  /// @param [in] reinitialize is a flag to steer whether the state should be
  /// reinitialized at the new position
  void covarianceTransport(State& state, const Surface& surface,
                           bool /*unused*/) const {
    Acts::Vector3D gp(state.pVector[0], state.pVector[1], state.pVector[2]);
    Acts::Vector3D mom(state.pVector[4], state.pVector[5], state.pVector[6]);
    mom /= std::abs(state.pVector[7]);

    double p = 1. / state.pVector[7];
    state.pVector[40] *= p;
    state.pVector[41] *= p;
    state.pVector[42] *= p;
    state.pVector[44] *= p;
    state.pVector[45] *= p;
    state.pVector[46] *= p;

    const auto fFrame = surface.referenceFrame(state.geoContext, gp, mom);

    double Ax[3] = {fFrame(0, 0), fFrame(1, 0), fFrame(2, 0)};
    double Ay[3] = {fFrame(0, 1), fFrame(1, 1), fFrame(2, 1)};
    double S[3] = {fFrame(0, 2), fFrame(1, 2), fFrame(2, 2)};

    // this is the projection of direction onto the local normal vector
    double A = state.pVector[4] * S[0] + state.pVector[5] * S[1] +
               state.pVector[6] * S[2];

    if (A != 0.) {
      A = 1. / A;
    }

    S[0] *= A;
    S[1] *= A;
    S[2] *= A;

    double s0 = state.pVector[8] * S[0] + state.pVector[9] * S[1] +
                state.pVector[10] * S[2];
    double s1 = state.pVector[16] * S[0] + state.pVector[17] * S[1] +
                state.pVector[18] * S[2];
    double s2 = state.pVector[24] * S[0] + state.pVector[25] * S[1] +
                state.pVector[26] * S[2];
    double s3 = state.pVector[32] * S[0] + state.pVector[33] * S[1] +
                state.pVector[34] * S[2];
    double s4 = state.pVector[40] * S[0] + state.pVector[41] * S[1] +
                state.pVector[42] * S[2];

    // in case of line-type surfaces - we need to take into account that
    // the reference frame changes with variations of all local
    // parameters
    if (surface.type() == Surface::Straw ||
        surface.type() == Surface::Perigee) {
      // vector from position to center
      double x = state.pVector[0] - surface.center(state.geoContext).x();
      double y = state.pVector[1] - surface.center(state.geoContext).y();
      double z = state.pVector[2] - surface.center(state.geoContext).z();

      // this is the projection of the direction onto the local y axis
      double d = state.pVector[4] * Ay[0] + state.pVector[5] * Ay[1] +
                 state.pVector[6] * Ay[2];

      // this is cos(beta)
      double a = (1. - d) * (1. + d);
      if (a != 0.) {
        a = 1. / a;  // i.e. 1./(1-d^2)
      }

      // that's the modified norm vector
      double X = d * Ay[0] - state.pVector[4];  //
      double Y = d * Ay[1] - state.pVector[5];  //
      double Z = d * Ay[2] - state.pVector[6];  //

      // d0 to d1
      double d0 = state.pVector[12] * Ay[0] + state.pVector[13] * Ay[1] +
                  state.pVector[14] * Ay[2];
      double d1 = state.pVector[20] * Ay[0] + state.pVector[21] * Ay[1] +
                  state.pVector[22] * Ay[2];
      double d2 = state.pVector[28] * Ay[0] + state.pVector[29] * Ay[1] +
                  state.pVector[30] * Ay[2];
      double d3 = state.pVector[36] * Ay[0] + state.pVector[37] * Ay[1] +
                  state.pVector[38] * Ay[2];
      double d4 = state.pVector[44] * Ay[0] + state.pVector[45] * Ay[1] +
                  state.pVector[46] * Ay[2];

      s0 = (((state.pVector[8] * X + state.pVector[9] * Y +
              state.pVector[10] * Z) +
             x * (d0 * Ay[0] - state.pVector[12])) +
            (y * (d0 * Ay[1] - state.pVector[13]) +
             z * (d0 * Ay[2] - state.pVector[14]))) *
           (-a);

      s1 = (((state.pVector[16] * X + state.pVector[17] * Y +
              state.pVector[18] * Z) +
             x * (d1 * Ay[0] - state.pVector[20])) +
            (y * (d1 * Ay[1] - state.pVector[21]) +
             z * (d1 * Ay[2] - state.pVector[22]))) *
           (-a);
      s2 = (((state.pVector[24] * X + state.pVector[25] * Y +
              state.pVector[26] * Z) +
             x * (d2 * Ay[0] - state.pVector[28])) +
            (y * (d2 * Ay[1] - state.pVector[29]) +
             z * (d2 * Ay[2] - state.pVector[30]))) *
           (-a);
      s3 = (((state.pVector[32] * X + state.pVector[33] * Y +
              state.pVector[34] * Z) +
             x * (d3 * Ay[0] - state.pVector[36])) +
            (y * (d3 * Ay[1] - state.pVector[37]) +
             z * (d3 * Ay[2] - state.pVector[38]))) *
           (-a);
      s4 = (((state.pVector[40] * X + state.pVector[41] * Y +
              state.pVector[42] * Z) +
             x * (d4 * Ay[0] - state.pVector[44])) +
            (y * (d4 * Ay[1] - state.pVector[45]) +
             z * (d4 * Ay[2] - state.pVector[46]))) *
           (-a);
    }

    state.pVector[8] -= (s0 * state.pVector[4]);
    state.pVector[9] -= (s0 * state.pVector[5]);
    state.pVector[10] -= (s0 * state.pVector[6]);
    state.pVector[12] -= (s0 * state.pVector[56]);
    state.pVector[13] -= (s0 * state.pVector[57]);
    state.pVector[14] -= (s0 * state.pVector[58]);

    state.pVector[16] -= (s1 * state.pVector[4]);
    state.pVector[17] -= (s1 * state.pVector[5]);
    state.pVector[18] -= (s1 * state.pVector[6]);
    state.pVector[20] -= (s1 * state.pVector[56]);
    state.pVector[21] -= (s1 * state.pVector[57]);
    state.pVector[22] -= (s1 * state.pVector[58]);

    state.pVector[24] -= (s2 * state.pVector[4]);
    state.pVector[25] -= (s2 * state.pVector[5]);
    state.pVector[26] -= (s2 * state.pVector[6]);
    state.pVector[28] -= (s2 * state.pVector[56]);
    state.pVector[29] -= (s2 * state.pVector[57]);
    state.pVector[30] -= (s2 * state.pVector[58]);

    state.pVector[32] -= (s3 * state.pVector[4]);
    state.pVector[33] -= (s3 * state.pVector[5]);
    state.pVector[34] -= (s3 * state.pVector[6]);
    state.pVector[36] -= (s3 * state.pVector[56]);
    state.pVector[37] -= (s3 * state.pVector[57]);
    state.pVector[38] -= (s3 * state.pVector[58]);

    state.pVector[40] -= (s4 * state.pVector[4]);
    state.pVector[41] -= (s4 * state.pVector[5]);
    state.pVector[42] -= (s4 * state.pVector[6]);
    state.pVector[44] -= (s4 * state.pVector[56]);
    state.pVector[45] -= (s4 * state.pVector[57]);
    state.pVector[46] -= (s4 * state.pVector[58]);

    double P3, P4,
        C = state.pVector[4] * state.pVector[4] +
            state.pVector[5] * state.pVector[5];
    if (C > 1.e-20) {
      C = 1. / C;
      P3 = state.pVector[4] * C;
      P4 = state.pVector[5] * C;
      C = -sqrt(C);
    } else {
      C = -1.e10;
      P3 = 1.;
      P4 = 0.;
    }

    double MA[3] = {Ax[0], Ax[1], Ax[2]};
    double MB[3] = {Ay[0], Ay[1], Ay[2]};
    // Jacobian production of transport and to_local
    if (surface.type() == Surface::Disc) {
      // the vector from the disc surface to the p
      const auto& sfc = surface.center(state.geoContext);
      double d[3] = {state.pVector[0] - sfc(0), state.pVector[1] - sfc(1),
                     state.pVector[2] - sfc(2)};
      // this needs the transformation to polar coordinates
      double RC = d[0] * Ax[0] + d[1] * Ax[1] + d[2] * Ax[2];
      double RS = d[0] * Ay[0] + d[1] * Ay[1] + d[2] * Ay[2];
      double R2 = RC * RC + RS * RS;

      // inverse radius
      double Ri = 1. / sqrt(R2);
      MA[0] = (RC * Ax[0] + RS * Ay[0]) * Ri;
      MA[1] = (RC * Ax[1] + RS * Ay[1]) * Ri;
      MA[2] = (RC * Ax[2] + RS * Ay[2]) * Ri;
      MB[0] = (RC * Ay[0] - RS * Ax[0]) * (Ri = 1. / R2);
      MB[1] = (RC * Ay[1] - RS * Ax[1]) * Ri;
      MB[2] = (RC * Ay[2] - RS * Ax[2]) * Ri;
    }

    state.jacobian[0] = MA[0] * state.pVector[8] + MA[1] * state.pVector[9] +
                        MA[2] * state.pVector[10];  // dL0/dL0
    state.jacobian[1] = MA[0] * state.pVector[16] + MA[1] * state.pVector[17] +
                        MA[2] * state.pVector[18];  // dL0/dL1
    state.jacobian[2] = MA[0] * state.pVector[24] + MA[1] * state.pVector[25] +
                        MA[2] * state.pVector[26];  // dL0/dPhi
    state.jacobian[3] = MA[0] * state.pVector[32] + MA[1] * state.pVector[33] +
                        MA[2] * state.pVector[34];  // dL0/dThe
    state.jacobian[4] = MA[0] * state.pVector[40] + MA[1] * state.pVector[41] +
                        MA[2] * state.pVector[42];  // dL0/dCM
    state.jacobian[5] = 0.;                         // dL0/dT

    state.jacobian[6] = MB[0] * state.pVector[8] + MB[1] * state.pVector[9] +
                        MB[2] * state.pVector[10];  // dL1/dL0
    state.jacobian[7] = MB[0] * state.pVector[16] + MB[1] * state.pVector[17] +
                        MB[2] * state.pVector[18];  // dL1/dL1
    state.jacobian[8] = MB[0] * state.pVector[24] + MB[1] * state.pVector[25] +
                        MB[2] * state.pVector[26];  // dL1/dPhi
    state.jacobian[9] = MB[0] * state.pVector[32] + MB[1] * state.pVector[33] +
                        MB[2] * state.pVector[34];  // dL1/dThe
    state.jacobian[10] = MB[0] * state.pVector[40] + MB[1] * state.pVector[41] +
                         MB[2] * state.pVector[42];  // dL1/dCM
    state.jacobian[11] = 0.;                         // dL1/dT

    state.jacobian[12] =
        P3 * state.pVector[13] - P4 * state.pVector[12];  // dPhi/dL0
    state.jacobian[13] =
        P3 * state.pVector[21] - P4 * state.pVector[20];  // dPhi/dL1
    state.jacobian[14] =
        P3 * state.pVector[29] - P4 * state.pVector[28];  // dPhi/dPhi
    state.jacobian[15] =
        P3 * state.pVector[37] - P4 * state.pVector[36];  // dPhi/dThe
    state.jacobian[16] =
        P3 * state.pVector[45] - P4 * state.pVector[44];  // dPhi/dCM
    state.jacobian[17] = 0.;                              // dPhi/dT

    state.jacobian[18] = C * state.pVector[14];  // dThe/dL0
    state.jacobian[19] = C * state.pVector[22];  // dThe/dL1
    state.jacobian[20] = C * state.pVector[30];  // dThe/dPhi
    state.jacobian[21] = C * state.pVector[38];  // dThe/dThe
    state.jacobian[22] = C * state.pVector[46];  // dThe/dCM
    state.jacobian[23] = 0.;                     // dThe/dT

    state.jacobian[24] = 0.;                 // dCM /dL0
    state.jacobian[25] = 0.;                 // dCM /dL1
    state.jacobian[26] = 0.;                 // dCM /dPhi
    state.jacobian[27] = 0.;                 // dCM /dTheta
    state.jacobian[28] = state.pVector[47];  // dCM /dCM
    state.jacobian[29] = 0.;                 // dCM/dT

    state.jacobian[30] = 0.;  // dT/dL0
    state.jacobian[31] = 0.;  // dT/dL1
    state.jacobian[32] = 0.;  // dT/dPhi
    state.jacobian[33] = 0.;  // dT/dThe
    state.jacobian[34] = 0.;  // dT/dCM
    state.jacobian[35] = 1.;  // dT/dT

    Eigen::Map<
        Eigen::Matrix<double, BoundParsDim, BoundParsDim, Eigen::RowMajor>>
        J(state.jacobian);

    state.cov = J * (*state.covariance) * J.transpose();
  }

  /// Perform the actual step on the state
  ///
  /// @param state is the provided stepper state (caller keeps thread locality)
  template <typename propagator_state_t>
  Result<double> step(propagator_state_t& state) const {
    // we use h for keeping the nominclature with the original atlas code
    double h = state.stepping.stepSize;
    bool Jac = state.stepping.useJacobian;

    double* R = &(state.stepping.pVector[0]);  // Coordinates
    double* A = &(state.stepping.pVector[4]);  // Directions
    double* sA = &(state.stepping.pVector[56]);
    // Invert mometum/2.
    double Pi = 0.5 * state.stepping.pVector[7];
    //    double dltm = 0.0002 * .03;
    Vector3D f0, f;

    // if new field is required get it
    if (state.stepping.newfield) {
      const Vector3D pos(R[0], R[1], R[2]);
      f0 = getField(state.stepping, pos);
    } else {
      f0 = state.stepping.field;
    }

    bool Helix = false;
    // if (std::abs(S) < m_cfg.helixStep) Helix = true;

    while (h != 0.) {
      double S3 = (1. / 3.) * h, S4 = .25 * h, PS2 = Pi * h;

      // First point
      //
      double H0[3] = {f0[0] * PS2, f0[1] * PS2, f0[2] * PS2};
      double A0 = A[1] * H0[2] - A[2] * H0[1];
      double B0 = A[2] * H0[0] - A[0] * H0[2];
      double C0 = A[0] * H0[1] - A[1] * H0[0];
      double A2 = A0 + A[0];
      double B2 = B0 + A[1];
      double C2 = C0 + A[2];
      double A1 = A2 + A[0];
      double B1 = B2 + A[1];
      double C1 = C2 + A[2];

      // Second point
      //
      if (!Helix) {
        const Vector3D pos(R[0] + A1 * S4, R[1] + B1 * S4, R[2] + C1 * S4);
        f = getField(state.stepping, pos);
      } else {
        f = f0;
      }

      double H1[3] = {f[0] * PS2, f[1] * PS2, f[2] * PS2};
      double A3 = (A[0] + B2 * H1[2]) - C2 * H1[1];
      double B3 = (A[1] + C2 * H1[0]) - A2 * H1[2];
      double C3 = (A[2] + A2 * H1[1]) - B2 * H1[0];
      double A4 = (A[0] + B3 * H1[2]) - C3 * H1[1];
      double B4 = (A[1] + C3 * H1[0]) - A3 * H1[2];
      double C4 = (A[2] + A3 * H1[1]) - B3 * H1[0];
      double A5 = 2. * A4 - A[0];
      double B5 = 2. * B4 - A[1];
      double C5 = 2. * C4 - A[2];

      // Last point
      //
      if (!Helix) {
        const Vector3D pos(R[0] + h * A4, R[1] + h * B4, R[2] + h * C4);
        f = getField(state.stepping, pos);
      } else {
        f = f0;
      }

      double H2[3] = {f[0] * PS2, f[1] * PS2, f[2] * PS2};
      double A6 = B5 * H2[2] - C5 * H2[1];
      double B6 = C5 * H2[0] - A5 * H2[2];
      double C6 = A5 * H2[1] - B5 * H2[0];

      // Test approximation quality on give step and possible step reduction
      //
      double EST = 2. * (std::abs((A1 + A6) - (A3 + A4)) +
                         std::abs((B1 + B6) - (B3 + B4)) +
                         std::abs((C1 + C6) - (C3 + C4)));
      if (EST > 0.0002) {
        h *= .5;
        //        dltm = 0.;
        continue;
      }

      //      if (EST < dltm) h *= 2.;

      // Parameters calculation
      //
      double A00 = A[0], A11 = A[1], A22 = A[2];

      A[0] = 2. * A3 + (A0 + A5 + A6);
      A[1] = 2. * B3 + (B0 + B5 + B6);
      A[2] = 2. * C3 + (C0 + C5 + C6);

      double D = (A[0] * A[0] + A[1] * A[1]) + (A[2] * A[2] - 9.);
      double Sl = 2. / h;
      D = (1. / 3.) - ((1. / 648.) * D) * (12. - D);

      R[0] += (A2 + A3 + A4) * S3;
      R[1] += (B2 + B3 + B4) * S3;
      R[2] += (C2 + C3 + C4) * S3;
      A[0] *= D;
      A[1] *= D;
      A[2] *= D;
      sA[0] = A6 * Sl;
      sA[1] = B6 * Sl;
      sA[2] = C6 * Sl;

      // Evaluate the time propagation
      state.stepping.pVector[3] +=
          h * std::hypot(1, state.options.mass / momentum(state.stepping));
      state.stepping.field = f;
      state.stepping.newfield = false;

      if (Jac) {
        // Jacobian calculation
        //
        double* d2A = &state.stepping.pVector[28];
        double* d3A = &state.stepping.pVector[36];
        double* d4A = &state.stepping.pVector[44];
        double d2A0 = H0[2] * d2A[1] - H0[1] * d2A[2];
        double d2B0 = H0[0] * d2A[2] - H0[2] * d2A[0];
        double d2C0 = H0[1] * d2A[0] - H0[0] * d2A[1];
        double d3A0 = H0[2] * d3A[1] - H0[1] * d3A[2];
        double d3B0 = H0[0] * d3A[2] - H0[2] * d3A[0];
        double d3C0 = H0[1] * d3A[0] - H0[0] * d3A[1];
        double d4A0 = (A0 + H0[2] * d4A[1]) - H0[1] * d4A[2];
        double d4B0 = (B0 + H0[0] * d4A[2]) - H0[2] * d4A[0];
        double d4C0 = (C0 + H0[1] * d4A[0]) - H0[0] * d4A[1];
        double d2A2 = d2A0 + d2A[0];
        double d2B2 = d2B0 + d2A[1];
        double d2C2 = d2C0 + d2A[2];
        double d3A2 = d3A0 + d3A[0];
        double d3B2 = d3B0 + d3A[1];
        double d3C2 = d3C0 + d3A[2];
        double d4A2 = d4A0 + d4A[0];
        double d4B2 = d4B0 + d4A[1];
        double d4C2 = d4C0 + d4A[2];
        double d0 = d4A[0] - A00;
        double d1 = d4A[1] - A11;
        double d2 = d4A[2] - A22;
        double d2A3 = (d2A[0] + d2B2 * H1[2]) - d2C2 * H1[1];
        double d2B3 = (d2A[1] + d2C2 * H1[0]) - d2A2 * H1[2];
        double d2C3 = (d2A[2] + d2A2 * H1[1]) - d2B2 * H1[0];
        double d3A3 = (d3A[0] + d3B2 * H1[2]) - d3C2 * H1[1];
        double d3B3 = (d3A[1] + d3C2 * H1[0]) - d3A2 * H1[2];
        double d3C3 = (d3A[2] + d3A2 * H1[1]) - d3B2 * H1[0];
        double d4A3 = ((A3 + d0) + d4B2 * H1[2]) - d4C2 * H1[1];
        double d4B3 = ((B3 + d1) + d4C2 * H1[0]) - d4A2 * H1[2];
        double d4C3 = ((C3 + d2) + d4A2 * H1[1]) - d4B2 * H1[0];
        double d2A4 = (d2A[0] + d2B3 * H1[2]) - d2C3 * H1[1];
        double d2B4 = (d2A[1] + d2C3 * H1[0]) - d2A3 * H1[2];
        double d2C4 = (d2A[2] + d2A3 * H1[1]) - d2B3 * H1[0];
        double d3A4 = (d3A[0] + d3B3 * H1[2]) - d3C3 * H1[1];
        double d3B4 = (d3A[1] + d3C3 * H1[0]) - d3A3 * H1[2];
        double d3C4 = (d3A[2] + d3A3 * H1[1]) - d3B3 * H1[0];
        double d4A4 = ((A4 + d0) + d4B3 * H1[2]) - d4C3 * H1[1];
        double d4B4 = ((B4 + d1) + d4C3 * H1[0]) - d4A3 * H1[2];
        double d4C4 = ((C4 + d2) + d4A3 * H1[1]) - d4B3 * H1[0];
        double d2A5 = 2. * d2A4 - d2A[0];
        double d2B5 = 2. * d2B4 - d2A[1];
        double d2C5 = 2. * d2C4 - d2A[2];
        double d3A5 = 2. * d3A4 - d3A[0];
        double d3B5 = 2. * d3B4 - d3A[1];
        double d3C5 = 2. * d3C4 - d3A[2];
        double d4A5 = 2. * d4A4 - d4A[0];
        double d4B5 = 2. * d4B4 - d4A[1];
        double d4C5 = 2. * d4C4 - d4A[2];
        double d2A6 = d2B5 * H2[2] - d2C5 * H2[1];
        double d2B6 = d2C5 * H2[0] - d2A5 * H2[2];
        double d2C6 = d2A5 * H2[1] - d2B5 * H2[0];
        double d3A6 = d3B5 * H2[2] - d3C5 * H2[1];
        double d3B6 = d3C5 * H2[0] - d3A5 * H2[2];
        double d3C6 = d3A5 * H2[1] - d3B5 * H2[0];
        double d4A6 = d4B5 * H2[2] - d4C5 * H2[1];
        double d4B6 = d4C5 * H2[0] - d4A5 * H2[2];
        double d4C6 = d4A5 * H2[1] - d4B5 * H2[0];

        double* dR = &state.stepping.pVector[24];
        dR[0] += (d2A2 + d2A3 + d2A4) * S3;
        dR[1] += (d2B2 + d2B3 + d2B4) * S3;
        dR[2] += (d2C2 + d2C3 + d2C4) * S3;
        d2A[0] = ((d2A0 + 2. * d2A3) + (d2A5 + d2A6)) * (1. / 3.);
        d2A[1] = ((d2B0 + 2. * d2B3) + (d2B5 + d2B6)) * (1. / 3.);
        d2A[2] = ((d2C0 + 2. * d2C3) + (d2C5 + d2C6)) * (1. / 3.);

        dR = &state.stepping.pVector[32];
        dR[0] += (d3A2 + d3A3 + d3A4) * S3;
        dR[1] += (d3B2 + d3B3 + d3B4) * S3;
        dR[2] += (d3C2 + d3C3 + d3C4) * S3;
        d3A[0] = ((d3A0 + 2. * d3A3) + (d3A5 + d3A6)) * (1. / 3.);
        d3A[1] = ((d3B0 + 2. * d3B3) + (d3B5 + d3B6)) * (1. / 3.);
        d3A[2] = ((d3C0 + 2. * d3C3) + (d3C5 + d3C6)) * (1. / 3.);

        dR = &state.stepping.pVector[40];
        dR[0] += (d4A2 + d4A3 + d4A4) * S3;
        dR[1] += (d4B2 + d4B3 + d4B4) * S3;
        dR[2] += (d4C2 + d4C3 + d4C4) * S3;
        d4A[0] = ((d4A0 + 2. * d4A3) + (d4A5 + d4A6 + A6)) * (1. / 3.);
        d4A[1] = ((d4B0 + 2. * d4B3) + (d4B5 + d4B6 + B6)) * (1. / 3.);
        d4A[2] = ((d4C0 + 2. * d4C3) + (d4C5 + d4C6 + C6)) * (1. / 3.);
      }
      state.stepping.pathAccumulated += h;
      return h;
    }

    // that exit path should actually not happen
    state.stepping.pathAccumulated += h;
    return h;
  }

 private:
  bfield_t m_bField;
};

}  // namespace Acts
