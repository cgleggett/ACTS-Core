// This file is part of the Acts project.
//
// Copyright (C) 2017-2019 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <array>
#include "Acts/EventData/TrackParameters.hpp"
#include "Acts/Geometry/GeometryContext.hpp"
#include "Acts/Surfaces/PlaneSurface.hpp"
#include "Acts/Surfaces/Surface.hpp"
#include "Acts/Propagator/Propagator.hpp"

namespace Acts {

namespace IntegrationTest {

using Jacobian = BoundMatrix;
using Covariance = BoundSymMatrix;

template <typename action_list_t = ActionList<>,
          typename aborter_list_t = AbortList<>>
struct RiddersPropagatorOptions : PropagatorOptions<action_list_t, aborter_list_t>{
	
	/// Copy Constructor
  RiddersPropagatorOptions(
      const RiddersPropagatorOptions<action_list_t, aborter_list_t>&
          rpo) = default;

  /// Constructor with GeometryContext
  ///
  /// @param gctx The current geometry context object, e.g. alignment
  /// @param mctx The current magnetic fielc context object
  RiddersPropagatorOptions(
      std::reference_wrapper<const GeometryContext> gctx,
      std::reference_wrapper<const MagneticFieldContext> mctx)
      : PropagatorOptions<action_list_t, aborter_list_t>(gctx, mctx) {}
      
  std::vector<double> deviations = {-2e-4, -1e-4, 1e-4, 2e-4};
  
  /// @brief Expand the Options with extended aborters
  ///
  /// @tparam extended_aborter_list_t Type of the new aborter list
  ///
  /// @param aborters The new aborter list to be used (internally)
  template <typename extended_aborter_list_t>
  RiddersPropagatorOptions<action_list_t, extended_aborter_list_t> extend(
      extended_aborter_list_t aborters) const {
    RiddersPropagatorOptions<action_list_t, extended_aborter_list_t> eoptions(
        this->geoContext, this->magFieldContext);
    // Copy the options over
    eoptions.direction = this->direction;
    eoptions.absPdgCode = this->absPdgCode;
    eoptions.mass = this->mass;
    eoptions.maxSteps = this->maxSteps;
    eoptions.maxStepSize = this->maxStepSize;
    eoptions.targetTolerance = this->targetTolerance;
    eoptions.pathLimit = this->pathLimit;
    eoptions.loopProtection = this->loopProtection;
    eoptions.loopFraction = this->loopFraction;
    // Output option
    eoptions.debug = this->debug;
    eoptions.debugString = this->debugString;
    eoptions.debugPfxWidth = this->debugPfxWidth;
    eoptions.debugMsgWidth = this->debugMsgWidth;
    // Stepper options
    eoptions.tolerance = this->tolerance;
    eoptions.stepSizeCutOff = this->stepSizeCutOff;
    // Action / abort list
    eoptions.actionList = std::move(this->actionList);
    eoptions.abortList = std::move(aborters);
    eoptions.deviations = std::move(this->deviations);
    // And return the options
    return eoptions;
  }
};

template <typename propagator_t>
class RiddersPropagator
{
public:
	RiddersPropagator(propagator_t& propagator) : m_propagator(propagator){}
	
	template<typename stepper_t, typename navigator_t>
	RiddersPropagator(stepper_t stepper, navigator_t navigator  = navigator_t()) : m_propagator(Propagator(stepper, navigator)) {}
	
//~ template <typename parameters_t, typename action_list_t,
		//~ typename aborter_list_t,
		//~ template <typename, typename> class propagator_options_t,
		//~ typename path_aborter_t = detail::PathLimitReached>
//~ Result<action_list_t_result_t<
  //~ typename stepper_t::template return_parameter_type<parameters_t>,
  //~ action_list_t>>
//~ propagate(
  //~ const parameters_t& start,
  //~ const propagator_options_t<action_list_t, aborter_list_t>& options) const
  //~ {
	  	//~ const Surface& surface = nominalResult->referenceSurface();

  //~ }
  
  template <typename parameters_t, typename surface_t, typename action_list_t,
            typename aborter_list_t,
            template <typename, typename> class propagator_options_t,
            typename target_aborter_t = detail::SurfaceReached,
            typename path_aborter_t = detail::PathLimitReached>
  Covariance
  propagate(
      const parameters_t& start, const surface_t& target,
      const RiddersPropagatorOptions<action_list_t, aborter_list_t>& options) const
  {
	 
	const auto& nominalResult = m_propagator.template propagate<parameters_t, surface_t, action_list_t, aborter_list_t, propagator_options_t, target_aborter_t, path_aborter_t>(start, target, options).value().endParameters;
	const BoundVector& nominalParameters = nominalResult->parameters();
	
    // - for planar surfaces the dest surface is a perfect destination
	// surface for the numerical propagation, as reference frame
	// aligns with the referenceSurface.transform().rotation() at
	// at any given time
	//
	// - for straw & cylinder, where the error is given
	// in the reference frame that re-aligns with a slightly different
	// intersection solution
	
	options.pathLimit *= 2.;
	
	std::array<std::vector<BoundVector>, Acts::BoundParsDim> derivatives;

	for(unsigned int i = 0; i < Acts::BoundParsDim; i++)
	{
		derivatives[i] = wiggleDimension(options, start, i, target, nominalParameters);
	}
	
	return calculateCovariance(derivatives, start.covariance());
  }

private:
	template <typename action_list_t, typename aborter_list_t, typename parameters_t, typename surface_t> // TODO: Is the templated surface needed?
	std::vector<BoundVector>
	wiggleDimension(RiddersPropagatorOptions<action_list_t, aborter_list_t>& options, const parameters_t& startPars, const unsigned int param, const surface_t& target, const BoundVector& nominal) const
	{
		// variation in x/y/phi/qop/t
		std::vector<BoundVector> derivatives;
		derivatives.reserve(options.deviations.size());
		for (double h : options.deviations) {
		  parameters_t tp = startPars;
		  
		  // Treatment for theta
		  if(param == Acts::eTHETA)
		  {
			  const double current_theta = tp.template get<Acts::eTHETA>();
			  if (current_theta + h > M_PI) {
				h = M_PI - current_theta;
			  }
			  if (current_theta + h < 0) {
				h = -current_theta;
			  } 
			  
		  }
		  
		  tp.template set<param>(options.geoContext,
										tp.template get<param>() + h);
		  const auto& r = m_propagator.propagate(tp, target, options).value();
		  derivatives.push_back((r.endParameters->parameters() - nominal) / h);
		}
		return derivatives;
	}

	template <typename action_list_t, typename aborter_list_t>
	Covariance
	calculateCovariance(RiddersPropagatorOptions<action_list_t, aborter_list_t>& options, const std::array<std::vector<BoundVector>, Acts::BoundParsDim>& derivatives, const Covariance& startCov) const
	{
		Jacobian jacobian;
		jacobian.setIdentity();
		jacobian.col(Acts::eLOC_0) = fitLinear(derivatives[Acts::eLOC_0], options.deviations);
		jacobian.col(Acts::eLOC_1) = fitLinear(derivatives[Acts::eLOC_1], options.deviations);
		jacobian.col(Acts::ePHI) = fitLinear(derivatives[Acts::ePHI], options.deviations);
		jacobian.col(Acts::eTHETA) = fitLinear(derivatives[Acts::eTHETA], options.deviations);
		jacobian.col(Acts::eQOP) = fitLinear(derivatives[Acts::eQOP], options.deviations);
		jacobian.col(Acts::eT) = fitLinear(derivatives[Acts::eT], options.deviations);
		return jacobian * startCov * jacobian.transpose();
    }

  template <unsigned long int N>
  static BoundVector fitLinear(const std::vector<BoundVector>& values,
                               const std::array<double, N>& h) {
    BoundVector A;
    BoundVector C;
    A.setZero();
    C.setZero();
    double B = 0;
    double D = 0;

    for (unsigned int i = 0; i < N; ++i) {
      A += h.at(i) * values.at(i);
      B += h.at(i);
      C += values.at(i);
      D += h.at(i) * h.at(i);
    }

    BoundVector b = (N * A - B * C) / (N * D - B * B);
    BoundVector a = (C - B * b) / N;

    return a;
  }

	propagator_t m_propagator;
};



template <typename T>
struct covariance_validation_fixture {
 public:
  covariance_validation_fixture(T propagator)
      : m_propagator(std::move(propagator)) {}

  /// Numerical transport of covariance using the ridder's algorithm
  /// this is for covariance propagation validation
  /// it can either be used for curvilinear transport
  template <typename StartParameters, typename EndParameters, typename U>
  Covariance calculateCovariance(const StartParameters& startPars,
                                 const Covariance& startCov,
                                 const EndParameters& endPars,
                                 const U& options) const {
    // steps for estimating derivatives
    const std::array<double, 4> h_steps = {{-2e-4, -1e-4, 1e-4, 2e-4}};

    // nominal propagation
    const auto& nominal = endPars.parameters();
    const Surface& dest = endPars.referenceSurface();

    // - for planar surfaces the dest surface is a perfect destination
    // surface for the numerical propagation, as reference frame
    // aligns with the referenceSurface.transform().rotation() at
    // at any given time
    //
    // - for straw & cylinder, where the error is given
    // in the reference frame that re-aligns with a slightly different
    // intersection solution

    // avoid stopping before the surface because of path length reached
    U var_options = options;
    var_options.pathLimit *= 2;

    // variation in x
    std::vector<BoundVector> x_derivatives;
    x_derivatives.reserve(h_steps.size());
    for (double h : h_steps) {
      StartParameters tp = startPars;
      tp.template set<Acts::eLOC_0>(options.geoContext,
                                    tp.template get<Acts::eLOC_0>() + h);
      const auto& r = m_propagator.propagate(tp, dest, var_options).value();
      x_derivatives.push_back((r.endParameters->parameters() - nominal) / h);
    }

    // variation in y
    std::vector<BoundVector> y_derivatives;
    y_derivatives.reserve(h_steps.size());
    for (double h : h_steps) {
      StartParameters tp = startPars;
      tp.template set<Acts::eLOC_1>(options.geoContext,
                                    tp.template get<Acts::eLOC_1>() + h);
      const auto& r = m_propagator.propagate(tp, dest, var_options).value();
      y_derivatives.push_back((r.endParameters->parameters() - nominal) / h);
    }

    // variation in phi
    std::vector<BoundVector> phi_derivatives;
    phi_derivatives.reserve(h_steps.size());
    for (double h : h_steps) {
      StartParameters tp = startPars;
      tp.template set<Acts::ePHI>(options.geoContext,
                                  tp.template get<Acts::ePHI>() + h);
      const auto& r = m_propagator.propagate(tp, dest, var_options).value();
      phi_derivatives.push_back((r.endParameters->parameters() - nominal) / h);
    }

    // variation in theta
    std::vector<BoundVector> theta_derivatives;
    theta_derivatives.reserve(h_steps.size());
    for (double h : h_steps) {
      StartParameters tp = startPars;
      const double current_theta = tp.template get<Acts::eTHETA>();
      if (current_theta + h > M_PI) {
        h = M_PI - current_theta;
      }
      if (current_theta + h < 0) {
        h = -current_theta;
      }
      tp.template set<Acts::eTHETA>(options.geoContext,
                                    tp.template get<Acts::eTHETA>() + h);
      const auto& r = m_propagator.propagate(tp, dest, var_options).value();
      theta_derivatives.push_back((r.endParameters->parameters() - nominal) /
                                  h);
    }

    // variation in q/p
    std::vector<BoundVector> qop_derivatives;
    qop_derivatives.reserve(h_steps.size());
    for (double h : h_steps) {
      StartParameters tp = startPars;
      tp.template set<Acts::eQOP>(options.geoContext,
                                  tp.template get<Acts::eQOP>() + h);
      const auto& r = m_propagator.propagate(tp, dest, var_options).value();
      qop_derivatives.push_back((r.endParameters->parameters() - nominal) / h);
    }

    // variation in t
    std::vector<BoundVector> t_derivatives;
    t_derivatives.reserve(h_steps.size());
    for (double h : h_steps) {
      StartParameters tp = startPars;
      tp.template set<Acts::eT>(options.geoContext,
                                tp.template get<Acts::eT>() + h);
      const auto& r = m_propagator.propagate(tp, dest, var_options).value();
      t_derivatives.push_back((r.endParameters->parameters() - nominal) / h);
    }

    Jacobian jacobian;
    jacobian.setIdentity();
    jacobian.col(Acts::eLOC_0) = fitLinear(x_derivatives, h_steps);
    jacobian.col(Acts::eLOC_1) = fitLinear(y_derivatives, h_steps);
    jacobian.col(Acts::ePHI) = fitLinear(phi_derivatives, h_steps);
    jacobian.col(Acts::eTHETA) = fitLinear(theta_derivatives, h_steps);
    jacobian.col(Acts::eQOP) = fitLinear(qop_derivatives, h_steps);
    jacobian.col(Acts::eT) = fitLinear(t_derivatives, h_steps);
    return jacobian * startCov * jacobian.transpose();
  }

 private:
  template <unsigned long int N>
  static BoundVector fitLinear(const std::vector<BoundVector>& values,
                               const std::array<double, N>& h) {
    BoundVector A;
    BoundVector C;
    A.setZero();
    C.setZero();
    double B = 0;
    double D = 0;

    for (unsigned int i = 0; i < N; ++i) {
      A += h.at(i) * values.at(i);
      B += h.at(i);
      C += values.at(i);
      D += h.at(i) * h.at(i);
    }

    BoundVector b = (N * A - B * C) / (N * D - B * B);
    BoundVector a = (C - B * b) / N;

    return a;
  }

  T m_propagator;
};

}  // namespace IntegrationTest

}  // namespace Acts