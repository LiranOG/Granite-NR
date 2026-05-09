/**
 * @file granite/amr/amr.hpp
 * @brief Adaptive Mesh Refinement module — public interface.
 *
 * Block-structured AMR with Berger-Oliger subcycling, gradient-based
 * refinement tagging, tracking spheres, and inter-level restrictions.
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#pragma once

#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace granite::amr {

struct AMRParams {
    int max_levels = 15;            ///< Maximum refinement levels
    int refinement_ratio = 2;       ///< Ratio between consecutive levels
    int regrid_interval = 4;        ///< Coarse steps between regridding
    int buffer_width = 4;           ///< Buffer cells around tagged region
    Real refine_threshold = 0.1;    ///< Gradient threshold for tagging
    Real derefine_threshold = 0.05; ///< Threshold for de-refinement
    bool subcycling = true;         ///< Berger-Oliger time subcycling
};

struct TrackingSphere {
    std::array<Real, DIM> center;
    Real radius;
    int min_level; ///< Minimum AMR level to force within sphere
};

/// Refinement criterion function type
using TaggingFunction = std::function<bool(const GridBlock& block, int i, int j, int k)>;

/// Signature for single-level physics evolution step, used by AMRHierarchy::subcycle
using EvolutionStepFunc = std::function<void(std::vector<GridBlock*>& blocks, Real dt)>;

/**
 * @class AMRHierarchy
 * @brief Manages the block-structured AMR grid hierarchy.
 */
class AMRHierarchy {
public:
    explicit AMRHierarchy(const AMRParams& params, const SimulationParams& sim_params);
    ~AMRHierarchy() = default;

    /// Initialize the hierarchy recursively with internal tagging
    void initialize(const TaggingFunction& tagger);

    /// Core Berger-Oliger recursive timestep subcycling loop
    void subcycle(int level, const EvolutionStepFunc& evolve_func, const TaggingFunction& tagger);

    /// Regrid: tag cells, cluster, create child blocks, and prolongate recursively
    void regrid(int level, const TaggingFunction& tagger);

    /// Get all blocks at a given refinement level
    std::vector<GridBlock*> getLevel(int level);
    const std::vector<GridBlock*> getLevel(int level) const;

    /// Get all blocks across all levels
    std::vector<GridBlock*> getAllBlocks();

    /// Number of active refinement levels
    int numLevels() const;

    /// Total number of active blocks
    int numBlocks() const;

    /// Inject a time step into a specific level (must be called before subcycle).
    /// This is the mechanism by which main.cpp keeps level dt in sync with the
    /// global CFL-controlled dt.
    void setLevelDt(int level, Real dt);

    /// Fill ghost zones (inter-block communication + boundary conditions)
    void fillGhostZones(int level);

    /// Prolongation: 4th-order polynomial interpolation from coarse to fine level
    void prolongate(const GridBlock& coarse, GridBlock& fine) const;

    /// Restriction: cell-averaging injection with reflux conservation
    void restrict_data(const GridBlock& fine, GridBlock& coarse) const;

    /// Add a tracking sphere (e.g., around moving BH punctures)
    void addTrackingSphere(std::array<Real, DIM> center, Real radius, int min_level);

    /// Dynamically update centers of existing tracking spheres
    void updateTrackingSpheres(const std::vector<std::array<Real, DIM>>& new_centers);

    /// Compute load-balancing distribution (Hilbert SFC)
    void redistributeBlocks();

    /// Get the effective resolution at a given spatial point
    Real effectiveResolution(const std::array<Real, DIM>& point) const;

    const AMRParams& params() const { return params_; }

private:
    AMRParams params_;
    SimulationParams sim_params_;

    struct Level {
        int level_id;
        std::vector<std::unique_ptr<GridBlock>> blocks;
        Real dt;           ///< Time step for this level
        Real current_time; ///< Local evolution time
    };

    std::vector<Level> levels_;
    std::vector<TrackingSphere> tracking_spheres_;
};

// ===========================================================================
// Default tagging functions
// ===========================================================================

TaggingFunction gradientChiTagger(Real threshold);
TaggingFunction gradientRhoTagger(Real threshold);
TaggingFunction gradientLapseTagger(Real threshold);
TaggingFunction compositeTagger(std::vector<TaggingFunction> taggers);

} // namespace granite::amr
