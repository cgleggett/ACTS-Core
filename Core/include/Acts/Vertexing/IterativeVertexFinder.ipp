// This file is part of the Acts project.
//
// Copyright (C) 2019 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Acts/Vertexing/VertexFitterOptions.hpp"

template <typename bfield_t, typename input_track_t, typename vfitter_t>
Acts::Result<std::vector<Acts::Vertex<input_track_t>>>
Acts::IterativeVertexFinder<bfield_t, input_track_t, vfitter_t>::find(
    const std::vector<input_track_t>& trackVector,
    const VertexFinderOptions<input_track_t>& vFinderOptions) const {
  // Original tracks
  const std::vector<input_track_t>& origTracks = trackVector;
  // Tracks for seeding
  // Note: Remains to be investigated if another container (e.g. std::list)
  // or also std::vector<input_track_t*> is a faster option since erasures
  // of tracks is quite expensive with std::vector.
  // std::vector<input_track_t*> would however also come with an overhead
  // since m_cfg.vertexFitter.fit and m_cfg.seedFinder.find take
  // vector<input_track_t> and hence a lot of copying would be required.
  std::vector<input_track_t> seedTracks = trackVector;

  // Construct the vertex fitter options from vertex finder options
  VertexFitterOptions<input_track_t> vFitterOptions(
      vFinderOptions.geoContext, vFinderOptions.magFieldContext,
      vFinderOptions.vertexConstraint);

  // List of vertices to be filled below
  std::vector<Vertex<input_track_t>> vertexCollection;

  int nInterations = 0;
  // begin iterating
  while (seedTracks.size() > 1 && nInterations < m_cfg.maxVertices) {
    /// Begin seeding
    auto seedRes = getVertexSeed(seedTracks, vFinderOptions);
    if (!seedRes.ok()) {
      return seedRes.error();
    }
    // retrieve the seed vertex as the last element in
    // the seed vertexCollection
    Vertex<input_track_t>& seedVertex = *seedRes;
    /// End seeding
    /// Now take only tracks compatible with current seed
    // Tracks used for the fit in this iteration
    std::vector<input_track_t> perigeesToFit;
    std::vector<input_track_t> perigeesToFitSplitVertex;

    // Fill vector with tracks to fit, only compatible with seed
    auto fillRes = fillPerigeesToFit(seedTracks, seedVertex, perigeesToFit,
                                     perigeesToFitSplitVertex);
    if (!fillRes.ok()) {
      return fillRes.error();
    }
    ACTS_DEBUG("Perigees used for fit: " << perigeesToFit.size());

    /// Begin vertex fit
    Vertex<input_track_t> currentVertex;
    Vertex<input_track_t> currentSplitVertex;

    if (m_cfg.useBeamConstraint && !perigeesToFit.empty()) {
      auto fitResult = m_cfg.vertexFitter.fit(perigeesToFit, vFitterOptions);
      if (fitResult.ok()) {
        currentVertex = std::move(*fitResult);
      } else {
        return fitResult.error();
      }
    } else if (!m_cfg.useBeamConstraint && perigeesToFit.size() > 1) {
      auto fitResult = m_cfg.vertexFitter.fit(perigeesToFit, vFitterOptions);
      if (fitResult.ok()) {
        currentVertex = std::move(*fitResult);
      } else {
        return fitResult.error();
      }
    }
    if (m_cfg.createSplitVertices && perigeesToFitSplitVertex.size() > 1) {
      auto fitResult =
          m_cfg.vertexFitter.fit(perigeesToFitSplitVertex, vFitterOptions);
      if (fitResult.ok()) {
        currentSplitVertex = std::move(*fitResult);
      } else {
        return fitResult.error();
      }
    }
    /// End vertex fit
    ACTS_DEBUG("Vertex position after fit: " << currentVertex.fullPosition());

    // Number degrees of freedom
    double ndf = currentVertex.fitQuality().second;
    double ndfSplitVertex = currentSplitVertex.fitQuality().second;

    // Number of significant tracks
    int nTracksAtVertex = countSignificantTracks(currentVertex);
    int nTracksAtSplitVertex = countSignificantTracks(currentSplitVertex);

    bool isGoodVertex =
        ((!m_cfg.useBeamConstraint && ndf > 0 && nTracksAtVertex >= 2) ||
         (m_cfg.useBeamConstraint && ndf > 3 && nTracksAtVertex >= 2));

    if (!isGoodVertex) {
      removeAllTracks(perigeesToFit, seedTracks);
    } else {
      if (m_cfg.reassignTracksAfterFirstFit && (!m_cfg.createSplitVertices)) {
        // vertex is good vertex here
        // but add tracks which may have been missed

        auto result = reassignTracksToNewVertex(
            vertexCollection, currentVertex, perigeesToFit, seedTracks,
            origTracks, vFitterOptions, vFinderOptions);
        if (!result.ok()) {
          return result.error();
        }
        isGoodVertex = *result;

      }  // end reassignTracksAfterFirstFit case
         // still good vertex? might have changed in the meanwhile
      if (isGoodVertex) {
        removeUsedCompatibleTracks(currentVertex, perigeesToFit, seedTracks,
                                   vFinderOptions);

        ACTS_DEBUG(
            "Number of seed tracks after removal of compatible tracks "
            "and outliers: "
            << seedTracks.size());
      }
    }  // end case if good vertex

    // now splitvertex
    bool isGoodSplitVertex = false;
    if (m_cfg.createSplitVertices) {
      isGoodSplitVertex = (ndfSplitVertex > 0 && nTracksAtSplitVertex >= 2);

      if (!isGoodSplitVertex) {
        removeAllTracks(perigeesToFitSplitVertex, seedTracks);
      } else {
        removeUsedCompatibleTracks(currentSplitVertex, perigeesToFitSplitVertex,
                                   seedTracks, vFinderOptions);
      }
    }
    // Now fill vertex collection with vertex
    if (isGoodVertex) {
      vertexCollection.push_back(currentVertex);
    }
    if (isGoodSplitVertex && m_cfg.createSplitVertices) {
      vertexCollection.push_back(currentSplitVertex);
    }

    nInterations++;

  }  // end while loop

  return vertexCollection;
}

template <typename bfield_t, typename input_track_t, typename vfitter_t>
Acts::Result<Acts::Vertex<input_track_t>>
Acts::IterativeVertexFinder<bfield_t, input_track_t, vfitter_t>::getVertexSeed(
    const std::vector<input_track_t>& seedTracks,
    const VertexFinderOptions<input_track_t>& vFinderOptions) const {
  auto res = m_cfg.seedFinder.find(seedTracks, vFinderOptions);
  if (res.ok()) {
    auto vertexCollection = *res;
    if (vertexCollection.empty()) {
      ACTS_DEBUG(
          "No seed found. Number of input tracks: " << seedTracks.size());
      return VertexingError::SeedingError;
    }
    // current seed is last element in collection
    Vertex<input_track_t> seedVertex = vertexCollection.back();
    ACTS_DEBUG("Seed found at position: ("
               << seedVertex.fullPosition()[eX] << ", "
               << seedVertex.fullPosition()[eY] << ", "
               << seedVertex.fullPosition()[eZ] << ", " << seedVertex.time()
               << "). Number of input tracks: " << seedTracks.size());
    return seedVertex;
  } else {
    ACTS_DEBUG("No seed found. Number of input tracks: " << seedTracks.size());
    return VertexingError::SeedingError;
  }
}

template <typename bfield_t, typename input_track_t, typename vfitter_t>
void Acts::IterativeVertexFinder<bfield_t, input_track_t, vfitter_t>::
    removeAllTracks(const std::vector<input_track_t>& perigeesToFit,
                    std::vector<input_track_t>& seedTracks) const {
  auto seedBegin = seedTracks.begin();
  auto seedEnd = seedTracks.end();

  for (const auto& fitPerigee : perigeesToFit) {
    const BoundParameters& fitPerigeeParams = m_extractParameters(fitPerigee);

    bool trackFound = false;
    for (auto seedIter = seedBegin; seedIter != seedEnd;) {
      const BoundParameters& seedParams = m_extractParameters(*seedIter);
      if (fitPerigeeParams == seedParams) {
        seedIter = seedTracks.erase(seedIter);
        seedBegin = seedTracks.begin();
        seedEnd = seedTracks.end();
        trackFound = true;
        break;
      }
    }

    if (!trackFound) {
      ACTS_WARNING("Track (perigeeToFit) not found in seedTracks!");
    }
  }
}

template <typename bfield_t, typename input_track_t, typename vfitter_t>
Acts::Result<double> Acts::
    IterativeVertexFinder<bfield_t, input_track_t, vfitter_t>::getCompatibility(
        const BoundParameters& params, const Vertex<input_track_t>& vertex,
        const VertexFinderOptions<input_track_t>& vFinderOptions) const {
  // Linearize track
  auto result = m_cfg.linFactory.linearizeTrack(
      vFinderOptions.geoContext, vFinderOptions.magFieldContext, &params,
      vertex.fullPosition(), m_cfg.propagator);
  if (!result.ok()) {
    return result.error();
  }

  auto linTrack = std::move(*result);

  // Calculate reduced weight
  ActsSymMatrixD<2> weightReduced =
      linTrack.covarianceAtPCA.template block<2, 2>(0, 0);

  ActsSymMatrixD<2> errorVertexReduced =
      (linTrack.positionJacobian *
       (vertex.fullCovariance() * linTrack.positionJacobian.transpose()))
          .template block<2, 2>(0, 0);
  weightReduced += errorVertexReduced;
  weightReduced = weightReduced.inverse();

  // Calculate compatibility / chi2
  Vector2D trackParameters2D =
      linTrack.parametersAtPCA.template block<2, 1>(0, 0);
  double compatibility =
      trackParameters2D.dot(weightReduced * trackParameters2D);

  return compatibility;
}

template <typename bfield_t, typename input_track_t, typename vfitter_t>
Acts::Result<void>
Acts::IterativeVertexFinder<bfield_t, input_track_t, vfitter_t>::
    removeUsedCompatibleTracks(
        Vertex<input_track_t>& myVertex,
        std::vector<input_track_t>& perigeesToFit,
        std::vector<input_track_t>& seedTracks,
        const VertexFinderOptions<input_track_t>& vFinderOptions) const {
  std::vector<TrackAtVertex<input_track_t>> tracksAtVertex = myVertex.tracks();

  auto seedBegin = seedTracks.begin();
  auto seedEnd = seedTracks.end();

  auto perigeesToFitBegin = perigeesToFit.begin();
  auto perigeesToFitEnd = perigeesToFit.end();

  for (const auto& trackAtVtx : tracksAtVertex) {
    // remove track from seedTracks if compatible
    bool found = false;
    for (auto seedIter = seedBegin; seedIter != seedEnd; ++seedIter) {
      if (m_extractParameters(*seedIter) ==
          m_extractParameters(trackAtVtx.originalTrack)) {
        found = true;
        if (trackAtVtx.trackWeight > m_cfg.cutOffTrackWeight) {
          seedTracks.erase(seedIter);
          seedBegin = seedTracks.begin();
          seedEnd = seedTracks.end();
        }
        break;
      }
    }
    if (!found) {
      ACTS_WARNING("Track trackAtVtx not found in seedTracks!");
    }

    // remove track from perigeesToFit if compatible
    found = false;
    for (auto perigeesToFitIter = perigeesToFitBegin;
         perigeesToFitIter != perigeesToFitEnd; ++perigeesToFitIter) {
      if (m_extractParameters(*perigeesToFitIter) ==
          m_extractParameters(trackAtVtx.originalTrack)) {
        found = true;
        if (trackAtVtx.trackWeight > m_cfg.cutOffTrackWeight) {
          perigeesToFit.erase(perigeesToFitIter);
          perigeesToFitBegin = perigeesToFit.begin();
          perigeesToFitEnd = perigeesToFit.end();
        }
        break;
      }
    }
    if (!found) {
      ACTS_WARNING("Track trackAtVtx not found in perigeesToFit!");
    }
  }  // end iteration over tracksAtVertex

  ACTS_DEBUG("After removal of tracks belonging to vertex, "
             << seedTracks.size() << " seed tracks left.");

  // Now start considering outliers
  // perigeesToFit that are left here were below
  // m_cfg.cutOffTrackWeight threshold and are hence outliers
  ACTS_DEBUG("Number of outliers: " << perigeesToFit.size());

  auto trackAtVtxBegin = tracksAtVertex.begin();
  auto trackAtVtxEnd = tracksAtVertex.end();

  for (const auto& myPerigeeToFit : perigeesToFit) {
    // calculate chi2 w.r.t. last fitted vertex
    auto result = getCompatibility(m_extractParameters(myPerigeeToFit),
                                   myVertex, vFinderOptions);

    if (!result.ok()) {
      return result.error();
    }

    double chi2 = *result;

    // check if sufficiently compatible with last fitted vertex
    // (quite loose constraint)
    if (chi2 < m_cfg.maximumChi2cutForSeeding) {
      for (auto seedIter = seedBegin; seedIter != seedEnd; ++seedIter) {
        if (m_extractParameters(*seedIter) ==
            m_extractParameters(myPerigeeToFit)) {
          seedTracks.erase(seedIter);
          seedBegin = seedTracks.begin();
          seedEnd = seedTracks.end();

          ACTS_DEBUG(
              "Outlier track found. However, still sufficiently "
              "compatible with last fitted vertex. Remove from seeds.");
          break;
        }
      }
    } else {
      // Remove track from current vertex
      for (auto trackAtVtxIter = trackAtVtxBegin;
           trackAtVtxIter != trackAtVtxEnd; ++trackAtVtxIter) {
        if (m_extractParameters(trackAtVtxIter->originalTrack) ==
            m_extractParameters(myPerigeeToFit)) {
          ACTS_DEBUG(
              "Outlier track found which is not compatible with last "
              "fitted vertex. Remove from tracksAtVertex.");

          tracksAtVertex.erase(trackAtVtxIter);
          trackAtVtxBegin = tracksAtVertex.begin();
          trackAtVtxEnd = tracksAtVertex.end();
          break;
        }
      }
    }
  }

  // set updated (possibly with removed outliers) tracksAtVertex to vertex
  myVertex.setTracksAtVertex(tracksAtVertex);

  return {};
}

template <typename bfield_t, typename input_track_t, typename vfitter_t>
Acts::Result<void> Acts::IterativeVertexFinder<
    bfield_t, input_track_t,
    vfitter_t>::fillPerigeesToFit(const std::vector<input_track_t>& perigeeList,
                                  const Vertex<input_track_t>& seedVertex,
                                  std::vector<input_track_t>& perigeesToFitOut,
                                  std::vector<input_track_t>&
                                      perigeesToFitSplitVertexOut) const {
  int numberOfTracks = perigeeList.size();

  // Count how many tracks are used for fit
  int count = 0;
  // Fill perigeesToFit vector with tracks compatible with seed
  for (const auto& sTrack : perigeeList) {
    if (numberOfTracks <= 2) {
      perigeesToFitOut.push_back(sTrack);
      ++count;
    } else if (numberOfTracks <= 4 && !m_cfg.createSplitVertices) {
      perigeesToFitOut.push_back(sTrack);
      ++count;
    } else if (numberOfTracks <= 4 * m_cfg.splitVerticesTrkInvFraction &&
               m_cfg.createSplitVertices) {
      // Only few tracks left, put them into fit regardless their position
      if (count % m_cfg.splitVerticesTrkInvFraction == 0) {
        perigeesToFitOut.push_back(sTrack);
        ++count;
      } else {
        perigeesToFitSplitVertexOut.push_back(sTrack);
        ++count;
      }
    }
    // still large amount of tracks available, check compatibility
    else {
      // check first that distance is not too large
      const BoundParameters& sTrackParams = m_extractParameters(sTrack);
      double distance =
          m_cfg.ipEst.calculateDistance(sTrackParams, seedVertex.position());

      if (sTrackParams.covariance() == nullptr) {
        return VertexingError::NoCovariance;
      }

      double error = sqrt((*(sTrackParams.covariance()))(eLOC_D0, eLOC_D0) +
                          (*(sTrackParams.covariance()))(eLOC_Z0, eLOC_Z0));

      if (error == 0.) {
        ACTS_WARNING("Error is zero. Setting to 1.");
        error = 1.;
      }

      if (distance / error < m_cfg.significanceCutSeeding) {
        if (count % m_cfg.splitVerticesTrkInvFraction == 0 ||
            !m_cfg.createSplitVertices) {
          perigeesToFitOut.push_back(sTrack);
          ++count;
        } else {
          perigeesToFitSplitVertexOut.push_back(sTrack);
          ++count;
        }
      }
    }
  }
  return {};
}

template <typename bfield_t, typename input_track_t, typename vfitter_t>
Acts::Result<bool>
Acts::IterativeVertexFinder<bfield_t, input_track_t, vfitter_t>::
    reassignTracksToNewVertex(
        std::vector<Vertex<input_track_t>>& vertexCollection,
        Vertex<input_track_t>& currentVertex,
        std::vector<input_track_t>& perigeesToFit,
        std::vector<input_track_t>& seedTracks,
        const std::vector<input_track_t>& origTracks,
        const VertexFitterOptions<input_track_t>& vFitterOptions,
        const VertexFinderOptions<input_track_t>& vFinderOptions) const {
  int numberOfAddedTracks = 0;

  // iterate over all vertices and check if tracks need to be reassigned
  // to new (current) vertex
  for (auto& vertexIt : vertexCollection) {
    // tracks at vertexIt
    std::vector<TrackAtVertex<input_track_t>> tracksAtVertex =
        vertexIt.tracks();
    auto tracksBegin = tracksAtVertex.begin();
    auto tracksEnd = tracksAtVertex.end();

    for (auto tracksIter = tracksBegin; tracksIter != tracksEnd;) {
      // consider only tracks that are not too tightly assigned to other
      // vertex
      if (tracksIter->trackWeight > m_cfg.cutOffTrackWeight) {
        tracksIter++;
        continue;
      }
      // use original perigee parameter of course
      BoundParameters trackPerigee =
          m_extractParameters(tracksIter->originalTrack);

      // compute compatibility
      auto resultNew =
          getCompatibility(trackPerigee, currentVertex, vFinderOptions);
      if (!resultNew.ok()) {
        return Result<bool>::failure(resultNew.error());
      }
      double chi2NewVtx = *resultNew;

      auto resultOld = getCompatibility(trackPerigee, vertexIt, vFinderOptions);
      if (!resultOld.ok()) {
        return Result<bool>::failure(resultOld.error());
      }
      double chi2OldVtx = *resultOld;

      ACTS_DEBUG("Compatibility to new vertex: " << chi2NewVtx);
      ACTS_DEBUG("Compatibility to old vertex: " << chi2OldVtx);

      if (chi2NewVtx < chi2OldVtx) {
        perigeesToFit.push_back(tracksIter->originalTrack);
        // origTrack was already deleted from seedTracks previously
        // (when assigned to old vertex)
        // add it now back to seedTracks to be able to consistently
        // delete it later
        // when all tracks used to fit current vertex are deleted
        seedTracks.push_back(*std::find_if(
            origTracks.begin(), origTracks.end(),
            [&trackPerigee, this](auto origTrack) {
              return trackPerigee == m_extractParameters(origTrack);
            }));

        numberOfAddedTracks += 1;

        // remove track from old vertex
        tracksIter = tracksAtVertex.erase(tracksIter);
        tracksBegin = tracksAtVertex.begin();
        tracksEnd = tracksAtVertex.end();

      }  // end chi2NewVtx < chi2OldVtx

      else {
        // go and check next track
        ++tracksIter;
      }
    }  // end loop over tracks at old vertexIt

    vertexIt.setTracksAtVertex(tracksAtVertex);
  }  // end loop over all vertices

  ACTS_DEBUG("Added " << numberOfAddedTracks
                      << " tracks from old (other) vertices for new fit");

  // override current vertex with new fit
  // set first to default vertex to be able to check if still good vertex
  // later
  currentVertex = Vertex<input_track_t>();
  if (m_cfg.useBeamConstraint && !perigeesToFit.empty()) {
    auto fitResult = m_cfg.vertexFitter.fit(perigeesToFit, vFitterOptions);
    if (fitResult.ok()) {
      currentVertex = std::move(*fitResult);
    } else {
      return Result<bool>::success(false);
    }
  } else if (!m_cfg.useBeamConstraint && perigeesToFit.size() > 1) {
    auto fitResult = m_cfg.vertexFitter.fit(perigeesToFit, vFitterOptions);
    if (fitResult.ok()) {
      currentVertex = std::move(*fitResult);
    } else {
      return Result<bool>::success(false);
    }
  }

  // Number degrees of freedom
  double ndf = currentVertex.fitQuality().second;

  // Number of significant tracks
  int nTracksAtVertex = countSignificantTracks(currentVertex);

  bool isGoodVertex =
      ((!m_cfg.useBeamConstraint && ndf > 0 && nTracksAtVertex >= 2) ||
       (m_cfg.useBeamConstraint && ndf > 3 && nTracksAtVertex >= 2));

  if (!isGoodVertex) {
    removeAllTracks(perigeesToFit, seedTracks);

    ACTS_DEBUG("Going to new  iteration with "
               << seedTracks.size() << "seed tracks after BAD vertex.");
  }

  return Result<bool>::success(isGoodVertex);
}

template <typename bfield_t, typename input_track_t, typename vfitter_t>
int Acts::IterativeVertexFinder<bfield_t, input_track_t, vfitter_t>::
    countSignificantTracks(const Vertex<input_track_t>& vtx) const {
  return std::count_if(vtx.tracks().begin(), vtx.tracks().end(),
                       [this](TrackAtVertex<input_track_t> trk) {
                         return trk.trackWeight > m_cfg.cutOffTrackWeight;
                       });
}
