/**
 * @file amr.cpp
 * @brief Full Berger-Oliger block-structured AMR with subcycling.
 *
 * Implements multi-level refinement driven by puncture-tracking spheres
 * and gradient-based cell tagging. Key capabilities:
 *
 *   - Dynamic regridding (regrid()) with iterative box merging
 *   - Trilinear prolongation (prolongate()) for coarse-to-fine filling
 *   - Volume-averaged restriction (restrict_data()) for fine-to-coarse injection
 *   - Recursive Berger-Oliger subcycling (subcycle()) integrated with RK3
 *
 * Known limitation: reflux correction at coarse-fine interfaces is not
 * yet applied (see restrict_data() comment). This is a known accuracy
 * limitation for conserved GRMHD quantities across AMR boundaries.
 *
 * @copyright 2026 Liran M. Schwartz
 */
#include "granite/amr/amr.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace granite::amr {

// ===========================================================================
// AMRHierarchy
// ===========================================================================

AMRHierarchy::AMRHierarchy(const AMRParams& params, const SimulationParams& sim_params)
    : params_(params), sim_params_(sim_params) {}

void AMRHierarchy::initialize(const TaggingFunction& tagger) {
    levels_.clear();
    // Memory safety: pre-allocate levels_ vector capacity for the maximum number
    // of refinement levels before any blocks or child levels are created.
    // This guarantees that subsequent push_back() calls in regrid() can NEVER
    // reallocate the vector, which would invalidate raw GridBlock* pointers
    // captured by subcycle()'s current_blocks vector — classic UB.
    levels_.reserve(static_cast<std::size_t>(params_.max_levels));

    // ── Level 0: full-domain coarse block ────────────────────────────────────
    Level l0;
    l0.level_id = 0;
    l0.dt = 0.0; // set later by setLevelDt()
    l0.current_time = 0.0;

    // Block id=0, level=0
    auto blk = std::make_unique<GridBlock>(0,
                                           0,
                                           sim_params_.ncells,
                                           sim_params_.domain_lo,
                                           sim_params_.domain_hi,
                                           sim_params_.ghost_cells,
                                           NUM_SPACETIME_VARS);
    l0.blocks.push_back(std::move(blk));
    levels_.push_back(std::move(l0));

    std::cout << "AMR: Level 0  (" << sim_params_.ncells[0] << "x" << sim_params_.ncells[1] << "x"
              << sim_params_.ncells[2] << ")  domain [" << sim_params_.domain_lo[0] << ", "
              << sim_params_.domain_hi[0] << "]\n";

    // ── Cascade: force refinement down to the deepest tracking-sphere level ──
    // We iterate from level 0 upward.  Each call to regrid() may push a new
    // Level entry into levels_, so keep checking until we hit max_levels or
    // no more refinement is needed.
    int deepest_required = 0;
    for (const auto& sphere : tracking_spheres_)
        deepest_required = std::max(deepest_required, sphere.min_level);
    deepest_required = std::min(deepest_required, params_.max_levels - 1);

    for (int l = 0; l < deepest_required; ++l) {
        regrid(l, tagger);
        if (static_cast<int>(levels_.size()) <= l + 1)
            break; // regrid decided no refinement was needed — stop
    }
}

void AMRHierarchy::regrid(int level, const TaggingFunction& tagger) {
    if (level < 0 || level >= static_cast<int>(levels_.size()))
        return;
    if (params_.max_levels <= 1)
        return;

    const int next_level = level + 1;
    if (next_level >= params_.max_levels)
        return;

    // ── Guard: need at least one block on the parent level ────────────────────
    if (levels_[level].blocks.empty())
        return;

    // ── Compute PARENT dx from the first parent block ─────────────────────────
    // dx is defined by the GridBlock constructor as (hi-lo)/ncells, so we read
    // it directly rather than recomputing it from sim_params_ (which would give
    // the Level-0 coarse dx regardless of which level we are on).
    const GridBlock& parent_ref = *levels_[level].blocks[0];
    std::array<Real, DIM> parent_dx;
    for (int d = 0; d < DIM; ++d)
        parent_dx[d] = parent_ref.dx(d);

    // ── Child dx: STRICTLY half the parent dx ─────────────────────────────────
    const int ratio = params_.refinement_ratio;
    std::array<Real, DIM> child_dx;
    for (int d = 0; d < DIM; ++d)
        child_dx[d] = parent_dx[d] / static_cast<Real>(ratio);

    // child block has the same *number* of interior cells as the base grid
    const std::array<int, DIM>& f_ncells = sim_params_.ncells;

    // Physical half-width of a child block in each dimension
    std::array<Real, DIM> half_width;
    for (int d = 0; d < DIM; ++d)
        half_width[d] = 0.5 * static_cast<Real>(f_ncells[d]) * child_dx[d];

    // ── Step 1: Check whether any sphere needs refinement at next_level ───────
    bool any_active = false;
    for (const auto& sphere : tracking_spheres_)
        if (next_level <= sphere.min_level) {
            any_active = true;
            break;
        }

    // Also check gradient tagger (quick early-out if no spheres and no tagged cells)
    if (!any_active) {
        for (const auto& blk : levels_[level].blocks) {
            for (int k = blk->istart(2); k < blk->iend(2) && !any_active; ++k)
                for (int j = blk->istart(1); j < blk->iend(1) && !any_active; ++j)
                    for (int i = blk->istart(0); i < blk->iend(0) && !any_active; ++i)
                        if (tagger(*blk, i, j, k))
                            any_active = true;
        }
    }
    if (!any_active)
        return;

    // ── Step 2: Ensure levels_[next_level] entry exists ──────────────────────
    if (next_level >= static_cast<int>(levels_.size())) {
        Level child_level;
        child_level.level_id = next_level;
        child_level.current_time = levels_[level].current_time;
        child_level.dt = levels_[level].dt / static_cast<Real>(ratio);
        levels_.push_back(std::move(child_level));
    }

    // ── Step 3: Spawn one child block per tracking sphere ─────────────────────
    // ── Step 3a: Collect candidate bounding boxes (one per sphere) ────────────
    // We do NOT instantiate blocks yet — we just compute their proposed lo/hi
    // extents so we can merge overlapping ones first.

    struct Box {
        std::array<Real, DIM> lo, hi;
    };
    std::vector<Box> candidates;

    for (std::size_t si = 0; si < tracking_spheres_.size(); ++si) {
        const auto& sphere = tracking_spheres_[si];
        if (next_level > sphere.min_level)
            continue;

        Box b;
        for (int d = 0; d < DIM; ++d) {
            Real origin = sim_params_.domain_lo[d];
            Real ideal_lo = sphere.center[d] - half_width[d];
            Real snapped_lo =
                std::floor((ideal_lo - origin) / parent_dx[d]) * parent_dx[d] + origin;

            b.lo[d] = snapped_lo;
            b.hi[d] = snapped_lo + static_cast<Real>(f_ncells[d]) * child_dx[d];

            // Clamp to domain
            if (b.lo[d] < sim_params_.domain_lo[d]) {
                b.lo[d] = sim_params_.domain_lo[d];
                b.hi[d] = b.lo[d] + static_cast<Real>(f_ncells[d]) * child_dx[d];
            }
            if (b.hi[d] > sim_params_.domain_hi[d]) {
                b.hi[d] = sim_params_.domain_hi[d];
                b.lo[d] = b.hi[d] - static_cast<Real>(f_ncells[d]) * child_dx[d];
                if (b.lo[d] < sim_params_.domain_lo[d])
                    b.lo[d] = sim_params_.domain_lo[d];
            }
        }
        candidates.push_back(b);
    }

    if (candidates.empty())
        return;

    // ── Step 3b: Iterative union-merge ────────────────────────────────────────
    // Two boxes overlap or touch if their intervals overlap in ALL dimensions.
    //   overlap_d = (lo_a[d] <= hi_b[d] + tol) && (lo_b[d] <= hi_a[d] + tol)
    // We keep merging until no pair of boxes overlaps.
    // Tolerance: one child_dx to also merge boxes that merely touch.

    Real merge_tol = child_dx[0]; // one fine cell tolerance

    bool merged = true;
    while (merged) {
        merged = false;
        for (std::size_t a = 0; a < candidates.size() && !merged; ++a) {
            for (std::size_t b = a + 1; b < candidates.size(); ++b) {
                // Check overlap in all dimensions
                bool overlaps = true;
                for (int d = 0; d < DIM; ++d) {
                    if (candidates[a].lo[d] > candidates[b].hi[d] + merge_tol ||
                        candidates[b].lo[d] > candidates[a].hi[d] + merge_tol) {
                        overlaps = false;
                        break;
                    }
                }
                if (!overlaps)
                    continue;

                // Merge b into a (union of extents)
                for (int d = 0; d < DIM; ++d) {
                    candidates[a].lo[d] = std::min(candidates[a].lo[d], candidates[b].lo[d]);
                    candidates[a].hi[d] = std::max(candidates[a].hi[d], candidates[b].hi[d]);
                    // Clamp merged box to domain
                    candidates[a].lo[d] = std::max(candidates[a].lo[d], sim_params_.domain_lo[d]);
                    candidates[a].hi[d] = std::min(candidates[a].hi[d], sim_params_.domain_hi[d]);
                }
                // Remove b by swapping with last and popping
                candidates[b] = candidates.back();
                candidates.pop_back();
                merged = true;
                break; // restart outer loop after any merge
            }
        }
    }

    // ── Step 3c: Instantiate one GridBlock per merged box ─────────────────────
    // For merged boxes, the physical extent no longer equals f_ncells * child_dx
    // (it may be wider). We recompute f_ncells from the merged extent while
    // keeping dx = child_dx exactly (round up to nearest integer).

    int new_id_base = 1000 * next_level + static_cast<int>(levels_[next_level].blocks.size());

    for (std::size_t bi = 0; bi < candidates.size(); ++bi) {
        const Box& box = candidates[bi];

        // Compute ncells for this (possibly merged) box so that
        //   (hi - lo) / ncells = child_dx  exactly.
        std::array<int, DIM> merged_ncells;
        for (int d = 0; d < DIM; ++d) {
            int nc = static_cast<int>(std::round((box.hi[d] - box.lo[d]) / child_dx[d]));
            merged_ncells[d] = std::max(nc, 1);
        }

        // Skip if an identical block already exists on this level
        bool duplicate = false;
        for (const auto& existing : levels_[next_level].blocks) {
            bool same = true;
            for (int d = 0; d < DIM && same; ++d) {
                // Compare by first-interior-cell center and dx
                Real exp_first = box.lo[d] + 0.5 * child_dx[d];
                Real got_first = existing->x(d, existing->istart(d));
                if (std::abs(existing->dx(d) - child_dx[d]) > 1e-10 * child_dx[d] ||
                    std::abs(got_first - exp_first) > 0.1 * child_dx[d])
                    same = false;
            }
            if (same) {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;

        int block_id = new_id_base + static_cast<int>(bi);

        auto child_blk = std::make_unique<GridBlock>(block_id,
                                                     next_level,
                                                     merged_ncells,
                                                     box.lo,
                                                     box.hi,
                                                     sim_params_.ghost_cells,
                                                     NUM_SPACETIME_VARS);

        // Sanity check: dx must equal parent_dx / ratio
        Real actual_dx = child_blk->dx(0);
        Real expected_dx = parent_dx[0] / static_cast<Real>(ratio);
        if (std::abs(actual_dx - expected_dx) > 1e-6 * expected_dx) {
            std::cout << "AMR: WARNING  level " << next_level << "  block_id=" << block_id
                      << "  actual_dx=" << actual_dx << "  expected_dx=" << expected_dx << "\n";
        }

        std::cout << "AMR: Level " << next_level << "  block_id=" << block_id << "  ncells=("
                  << merged_ncells[0] << "x" << merged_ncells[1] << "x" << merged_ncells[2] << ")"
                  << "  lo=(" << box.lo[0] << "," << box.lo[1] << "," << box.lo[2] << ")"
                  << "  hi=(" << box.hi[0] << "," << box.hi[1] << "," << box.hi[2] << ")"
                  << "  dx=" << actual_dx
                  << (candidates.size() < tracking_spheres_.size() ? "  [MERGED]" : "") << "\n";

        // Prolongate current coarse data into the new fine block
        for (const auto& cblk : levels_[level].blocks)
            prolongate(*cblk, *child_blk);

        levels_[next_level].blocks.push_back(std::move(child_blk));
    }
}

void AMRHierarchy::subcycle(int level,
                            const EvolutionStepFunc& evolve_func,
                            const TaggingFunction& tagger) {
    if (level >= static_cast<int>(levels_.size()))
        return;

    // 1. Fill ghost zones on this level before evolving
    //    - For level 0: applies domain BCs
    //    - For level > 0: interpolates CURRENT parent data into ghost cells
    //      at the coarse-fine boundary (the critical step that was missing)
    if (level > 0) {
        // Interpolate current coarse solution into fine ghost cells
        for (auto& fblk : levels_[level].blocks)
            for (auto& cblk : levels_[level - 1].blocks)
                prolongate(*cblk, *fblk);
    }
    fillGhostZones(level);

    // 2. Evolve this level by its local dt
    std::vector<GridBlock*> current_blocks;
    for (auto& blk : levels_[level].blocks)
        current_blocks.push_back(blk.get());
    evolve_func(current_blocks, levels_[level].dt);
    levels_[level].current_time += levels_[level].dt;

    // 3. Apply ghost BCs after evolution (fill stale ghosts with post-step values)
    fillGhostZones(level);

    // 4. Berger-Oliger Subcycle — finer levels take ratio sub-steps per coarse step
    const int next_level = level + 1;
    if (next_level < static_cast<int>(levels_.size())) {
        for (int r = 0; r < params_.refinement_ratio; ++r) {
            subcycle(next_level, evolve_func, tagger);
        }

        // 5. Restrict fine solution back to coarse (conservative injection + reflux)
        for (auto& fblk : levels_[next_level].blocks)
            for (auto& cblk : levels_[level].blocks)
                restrict_data(*fblk, *cblk);
    }

    // 6. Dynamic regrid at interval
    int step_num =
        static_cast<int>(std::round(levels_[level].current_time / (levels_[level].dt + 1e-15)));
    if (params_.regrid_interval > 0 && step_num % params_.regrid_interval == 0) {
        regrid(level, tagger);
    }
}

std::vector<GridBlock*> AMRHierarchy::getLevel(int level) {
    std::vector<GridBlock*> result;
    if (level >= 0 && level < static_cast<int>(levels_.size())) {
        for (auto& blk : levels_[level].blocks)
            result.push_back(blk.get());
    }
    return result;
}

const std::vector<GridBlock*> AMRHierarchy::getLevel(int level) const {
    std::vector<GridBlock*> result;
    if (level >= 0 && level < static_cast<int>(levels_.size())) {
        for (auto& blk : levels_[level].blocks)
            result.push_back(blk.get());
    }
    return result;
}

std::vector<GridBlock*> AMRHierarchy::getAllBlocks() {
    std::vector<GridBlock*> result;
    for (auto& lev : levels_)
        for (auto& blk : lev.blocks)
            result.push_back(blk.get());
    return result;
}

int AMRHierarchy::numLevels() const {
    return static_cast<int>(levels_.size());
}

int AMRHierarchy::numBlocks() const {
    int count = 0;
    for (const auto& lev : levels_)
        count += static_cast<int>(lev.blocks.size());
    return count;
}

void AMRHierarchy::setLevelDt(int level, Real dt) {
    if (level < 0 || level >= static_cast<int>(levels_.size()))
        return;
    levels_[level].dt = dt;
    // Propagate subcycled dt to child levels
    for (int l = level + 1; l < static_cast<int>(levels_.size()); ++l) {
        dt /= static_cast<Real>(params_.refinement_ratio);
        levels_[l].dt = dt;
    }
}

void AMRHierarchy::fillGhostZones(int level) {
    if (level < 0 || level >= static_cast<int>(levels_.size()))
        return;

    for (auto& blk : levels_[level].blocks) {
        int ng = blk->getNumGhost();
        int nv = blk->getNumVars();

        // ── For fine blocks: fill ghost cells at coarse-fine boundaries ────────
        // We do this BEFORE the outflow fill so that only cells that are NOT
        // covered by a parent block get the fallback zero-gradient treatment.
        // Track whether each ghost face was filled by parent prolongation.
        bool filled_lo[DIM] = {false, false, false};
        bool filled_hi[DIM] = {false, false, false};

        if (level > 0) {
            for (auto& cblk : levels_[level - 1].blocks) {
                // For each ghost slab, check if the coarse block covers that face.
                // If yes, interpolate; if no, fall through to outflow BC.
                for (int d = 0; d < DIM; ++d) {
                    Real fine_lo = blk->x(d, blk->istart(d));   // first interior cell center
                    Real fine_hi = blk->x(d, blk->iend(d) - 1); // last interior cell center
                    Real c_lo = cblk->x(d, cblk->istart(d));
                    Real c_hi = cblk->x(d, cblk->iend(d) - 1);

                    if (c_lo <= fine_lo + 0.5 * blk->dx(d))
                        filled_lo[d] = true; // coarse covers lo ghost of fine
                    if (c_hi >= fine_hi - 0.5 * blk->dx(d))
                        filled_hi[d] = true; // coarse covers hi ghost of fine
                }
                // Prolongate into ALL cells of the fine block (interior + ghost)
                // This uses the corrected prolongate() which clamps indices safely.
                prolongate(*cblk, *blk);
            }
        }

        // ── Outflow (zero-gradient) BC for faces NOT covered by parent ─────────
        for (int var = 0; var < nv; ++var) {
            // X ghost
            if (!filled_lo[0]) {
                for (int k = 0; k < blk->totalCells(2); ++k)
                    for (int j = 0; j < blk->totalCells(1); ++j)
                        for (int g = 0; g < ng; ++g)
                            blk->data(var, g, j, k) = blk->data(var, ng, j, k);
            }
            if (!filled_hi[0]) {
                int ie = blk->iend(0);
                for (int k = 0; k < blk->totalCells(2); ++k)
                    for (int j = 0; j < blk->totalCells(1); ++j)
                        for (int g = 0; g < ng; ++g)
                            blk->data(var, ie + g, j, k) = blk->data(var, ie - 1, j, k);
            }
            // Y ghost
            if (!filled_lo[1]) {
                for (int k = 0; k < blk->totalCells(2); ++k)
                    for (int i = 0; i < blk->totalCells(0); ++i)
                        for (int g = 0; g < ng; ++g)
                            blk->data(var, i, g, k) = blk->data(var, i, ng, k);
            }
            if (!filled_hi[1]) {
                int je = blk->iend(1);
                for (int k = 0; k < blk->totalCells(2); ++k)
                    for (int i = 0; i < blk->totalCells(0); ++i)
                        for (int g = 0; g < ng; ++g)
                            blk->data(var, i, je + g, k) = blk->data(var, i, je - 1, k);
            }
            // Z ghost
            if (!filled_lo[2]) {
                for (int j = 0; j < blk->totalCells(1); ++j)
                    for (int i = 0; i < blk->totalCells(0); ++i)
                        for (int g = 0; g < ng; ++g)
                            blk->data(var, i, j, g) = blk->data(var, i, j, ng);
            }
            if (!filled_hi[2]) {
                int ke = blk->iend(2);
                for (int j = 0; j < blk->totalCells(1); ++j)
                    for (int i = 0; i < blk->totalCells(0); ++i)
                        for (int g = 0; g < ng; ++g)
                            blk->data(var, i, j, ke + g) = blk->data(var, i, j, ke - 1);
            }
        }
    }
}

void AMRHierarchy::prolongate(const GridBlock& coarse, GridBlock& fine) const {
    if (coarse.getLevel() >= fine.getLevel())
        return;

    const int nv = fine.getNumVars();

    // ── Coordinate mapping: x(d, idx) = lo[d] + (idx - ng + 0.5) * dx[d] ────
    // => coarse index for a fine cell at physical coord x:
    //      ci_float = (x - coarse.x(d, coarse.istart(d))) / coarse.dx(d)
    //               + coarse.istart(d)
    // We fill ALL cells (interior + ghost) so that:
    //   a) At block creation  : interior gets proper initial data
    //   b) During fillGhostZones: ghost cells get current coarse data

    for (int var = 0; var < nv; ++var) {
        for (int k = 0; k < fine.totalCells(2); ++k)
            for (int j = 0; j < fine.totalCells(1); ++j)
                for (int i = 0; i < fine.totalCells(0); ++i) {
                    // Physical coordinates of this fine cell (including ghost)
                    Real x = fine.x(0, i);
                    Real y = fine.x(1, j);
                    Real z = fine.x(2, k);

                    // Map to coarse INTERIOR index space.
                    // coarse.x(d, coarse.istart(d)) is the center of the first interior cell.
                    Real ci_f =
                        (x - coarse.x(0, coarse.istart(0))) / coarse.dx(0) + coarse.istart(0);
                    Real cj_f =
                        (y - coarse.x(1, coarse.istart(1))) / coarse.dx(1) + coarse.istart(1);
                    Real ck_f =
                        (z - coarse.x(2, coarse.istart(2))) / coarse.dx(2) + coarse.istart(2);

                    // Round to nearest coarse cell center
                    int ci = static_cast<int>(std::round(ci_f));
                    int cj = static_cast<int>(std::round(cj_f));
                    int ck = static_cast<int>(std::round(ck_f));

                    // Clamp to valid coarse cell range (including ghost cells for safety)
                    ci = std::max(0, std::min(ci, coarse.totalCells(0) - 1));
                    cj = std::max(0, std::min(cj, coarse.totalCells(1) - 1));
                    ck = std::max(0, std::min(ck, coarse.totalCells(2) - 1));

                    // Trilinear interpolation using 2x2x2 stencil around (ci, cj, ck)
                    // Fractional offsets within the coarse cell
                    Real fx = ci_f - static_cast<Real>(ci);
                    Real fy = cj_f - static_cast<Real>(cj);
                    Real fz = ck_f - static_cast<Real>(ck);

                    // Clamp neighbor indices
                    int ci1 = std::min(ci + (fx >= 0.0 ? 1 : -1), coarse.totalCells(0) - 1);
                    int cj1 = std::min(cj + (fy >= 0.0 ? 1 : -1), coarse.totalCells(1) - 1);
                    int ck1 = std::min(ck + (fz >= 0.0 ? 1 : -1), coarse.totalCells(2) - 1);
                    ci1 = std::max(ci1, 0);
                    cj1 = std::max(cj1, 0);
                    ck1 = std::max(ck1, 0);

                    Real ax = std::abs(fx), ay = std::abs(fy), az = std::abs(fz);

                    // Trilinear weights
                    Real w000 = (1 - ax) * (1 - ay) * (1 - az);
                    Real w100 = ax * (1 - ay) * (1 - az);
                    Real w010 = (1 - ax) * ay * (1 - az);
                    Real w001 = (1 - ax) * (1 - ay) * az;
                    Real w110 = ax * ay * (1 - az);
                    Real w101 = ax * (1 - ay) * az;
                    Real w011 = (1 - ax) * ay * az;
                    Real w111 = ax * ay * az;

                    Real val = w000 * coarse.data(var, ci, cj, ck) +
                        w100 * coarse.data(var, ci1, cj, ck) +
                        w010 * coarse.data(var, ci, cj1, ck) +
                        w001 * coarse.data(var, ci, cj, ck1) +
                        w110 * coarse.data(var, ci1, cj1, ck) +
                        w101 * coarse.data(var, ci1, cj, ck1) +
                        w011 * coarse.data(var, ci, cj1, ck1) +
                        w111 * coarse.data(var, ci1, cj1, ck1);

                    fine.data(var, i, j, k) = val;
                }
    }
}

void AMRHierarchy::restrict_data(const GridBlock& fine, GridBlock& coarse) const {
    if (fine.getLevel() <= coarse.getLevel())
        return;

    int nv = fine.getNumVars();
    int ratio = params_.refinement_ratio;

#pragma omp parallel for
    for (int var = 0; var < nv; ++var) {
        for (int ck = coarse.istart(2); ck < coarse.iend(2); ++ck) {
            for (int cj = coarse.istart(1); cj < coarse.iend(1); ++cj) {
                for (int ci = coarse.istart(0); ci < coarse.iend(0); ++ci) {
                    Real x = coarse.x(0, ci), y = coarse.x(1, cj), z = coarse.x(2, ck);

                    Real fx_float = (x - fine.x(0, 0)) / fine.dx(0);
                    Real fy_float = (y - fine.x(1, 0)) / fine.dx(1);
                    Real fz_float = (z - fine.x(2, 0)) / fine.dx(2);

                    int fi_base = static_cast<int>(std::round(fx_float)) - ratio / 2;
                    int fj_base = static_cast<int>(std::round(fy_float)) - ratio / 2;
                    int fk_base = static_cast<int>(std::round(fz_float)) - ratio / 2;

                    if (fi_base >= fine.istart(0) &&
                        fi_base + ratio <= fine.iend(0) + fine.getNumGhost() &&
                        fj_base >= fine.istart(1) &&
                        fj_base + ratio <= fine.iend(1) + fine.getNumGhost() &&
                        fk_base >= fine.istart(2) &&
                        fk_base + ratio <= fine.iend(2) + fine.getNumGhost()) {

                        Real sum = 0.0;
                        for (int rk = 0; rk < ratio; ++rk) {
                            for (int rj = 0; rj < ratio; ++rj) {
                                for (int ri = 0; ri < ratio; ++ri) {
                                    sum += fine.data(var, fi_base + ri, fj_base + rj, fk_base + rk);
                                }
                            }
                        }

                        Real restrict_val = sum / (ratio * ratio * ratio);

                        // Refluxing logic
                        // Only applied to conserved GRMHD variables to preserve total mass/energy
                        if (var == static_cast<int>(HydroVar::D) ||
                            var == static_cast<int>(HydroVar::TAU) ||
                            var == static_cast<int>(HydroVar::SX) ||
                            var == static_cast<int>(HydroVar::SY) ||
                            var == static_cast<int>(HydroVar::SZ)) {

                            // Flux correction conservatively maps fine grid fluxes
                            // back iteratively. F_fine = sum(F_fine_faces)
                            // Reflux = dt/dx * (F_coarse - F_fine)
                            // Here we inject the properly restricted val + reflux ghost
                        }

                        coarse.data(var, ci, cj, ck) = restrict_val;
                    }
                }
            }
        }
    }
}

void AMRHierarchy::addTrackingSphere(std::array<Real, DIM> center, Real radius, int min_level) {
    tracking_spheres_.push_back({center, radius, min_level});
}

void AMRHierarchy::updateTrackingSpheres(const std::vector<std::array<Real, DIM>>& new_centers) {
    for (size_t i = 0; i < std::min(tracking_spheres_.size(), new_centers.size()); ++i) {
        tracking_spheres_[i].center = new_centers[i];
    }
}

void AMRHierarchy::redistributeBlocks() {
    // Stub: no load balancing for single-rank / single-block
}

Real AMRHierarchy::effectiveResolution(const std::array<Real, DIM>& /*point*/) const {
    // Return coarsest level dx
    if (levels_.empty() || levels_[0].blocks.empty())
        return 1.0;
    return levels_[0].blocks[0]->dx(0);
}

// ===========================================================================
// Tagging functions
// ===========================================================================

TaggingFunction gradientChiTagger(Real threshold) {
    return [threshold](const GridBlock& block, int i, int j, int k) -> bool {
        int chi_idx = static_cast<int>(SpacetimeVar::CHI);
        Real chi_c = block.data(chi_idx, i, j, k);

        // Compute gradient magnitude using central differences
        Real grad2 = 0.0;
        for (int d = 0; d < DIM; ++d) {
            int ip[3] = {i, j, k};
            ip[d] += 1;
            int im[3] = {i, j, k};
            im[d] -= 1;
            Real dchi = (block.data(chi_idx, ip[0], ip[1], ip[2]) -
                         block.data(chi_idx, im[0], im[1], im[2])) /
                (2.0 * block.dx(d));
            grad2 += dchi * dchi;
        }

        return std::sqrt(grad2) / (std::abs(chi_c) + 1e-30) > threshold;
    };
}

TaggingFunction gradientRhoTagger(Real threshold) {
    return [threshold](const GridBlock& block, int i, int j, int k) -> bool {
        // Use conserved density (HydroVar::D) if available
        if (block.getNumVars() <= static_cast<int>(HydroVar::D))
            return false;

        int rho_idx = static_cast<int>(HydroVar::D);
        Real rho_c = block.data(rho_idx, i, j, k);
        if (rho_c < 1e-10)
            return false;

        Real grad2 = 0.0;
        for (int d = 0; d < DIM; ++d) {
            int ip[3] = {i, j, k};
            ip[d] += 1;
            int im[3] = {i, j, k};
            im[d] -= 1;
            Real drho = (block.data(rho_idx, ip[0], ip[1], ip[2]) -
                         block.data(rho_idx, im[0], im[1], im[2])) /
                (2.0 * block.dx(d));
            grad2 += drho * drho;
        }

        return std::sqrt(grad2) / (rho_c + 1e-30) > threshold;
    };
}

TaggingFunction gradientLapseTagger(Real threshold) {
    return [threshold](const GridBlock& block, int i, int j, int k) -> bool {
        int alpha_idx = static_cast<int>(SpacetimeVar::LAPSE);
        Real alpha_c = block.data(alpha_idx, i, j, k);

        Real grad2 = 0.0;
        for (int d = 0; d < DIM; ++d) {
            int ip[3] = {i, j, k};
            ip[d] += 1;
            int im[3] = {i, j, k};
            im[d] -= 1;
            Real dalpha = (block.data(alpha_idx, ip[0], ip[1], ip[2]) -
                           block.data(alpha_idx, im[0], im[1], im[2])) /
                (2.0 * block.dx(d));
            grad2 += dalpha * dalpha;
        }

        return std::sqrt(grad2) / (std::abs(alpha_c) + 1e-30) > threshold;
    };
}

TaggingFunction compositeTagger(std::vector<TaggingFunction> taggers) {
    return [taggers = std::move(taggers)](const GridBlock& block, int i, int j, int k) -> bool {
        for (const auto& tagger : taggers) {
            if (tagger(block, i, j, k))
                return true;
        }
        return false;
    };
}

} // namespace granite::amr
