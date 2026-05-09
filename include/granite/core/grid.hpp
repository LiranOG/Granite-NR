/**
 * @file granite/core/grid.hpp
 * @brief Uniform Cartesian grid block — the fundamental data container.
 *
 * A GridBlock represents a single rectangular patch of the AMR hierarchy.
 * It stores all evolved field variables on a uniform Cartesian grid with
 * ghost zones, and provides indexing, coordinate queries, and finite-
 * difference stencil access.
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#pragma once

#include "granite/core/types.hpp"

#include <cassert>
#include <memory>
#include <string>
#include <vector>

namespace granite {

/**
 * @class GridBlock
 * @brief A rectangular patch storing field data on a uniform Cartesian grid.
 *
 * Memory layout: flat contiguous storage with field-major ordering.
 * All variables are packed into a single allocation of size num_vars × total_cells:
 *
 *   data_[var * stride_ + flat(i,j,k)]
 *
 * where stride_ = totalCells(0) × totalCells(1) × totalCells(2).
 *
 * This layout (H2 fix from Phase 5) eliminates the per-variable heap fragmentation
 * of the previous vector<vector<Real>> design (N_vars separate allocations).
 * With a single contiguous block:
 *   - Allocator overhead: 1 allocation instead of num_vars
 *   - KO accumulator loop (iterating var ∈ [0,22] per cell): all 22 values
 *     for a given cell are now stride_ words apart — predictable for the
 *     hardware prefetcher when stride_ fits in L2 cache.
 *   - rawData(var) returns a pointer directly into the flat buffer.
 */
class GridBlock {
public:
    /**
     * @brief Construct a grid block.
     * @param id         Unique block identifier
     * @param level      AMR refinement level (0 = coarsest)
     * @param ncells     Number of interior cells per dimension
     * @param lo         Physical coordinates of lower-left corner
     * @param hi         Physical coordinates of upper-right corner
     * @param nghost     Number of ghost zones
     * @param num_vars   Total number of field variables
     */
    GridBlock(int id,
              int level,
              std::array<int, DIM> ncells,
              std::array<Real, DIM> lo,
              std::array<Real, DIM> hi,
              int nghost,
              int num_vars);

    ~GridBlock() = default;

    // --- Accessors ---

    int getId() const { return id_; }
    int getLevel() const { return level_; }
    int getNumGhost() const { return nghost_; }
    int getNumVars() const { return num_vars_; }

    int getRank() const { return rank_; }
    void setRank(int r) { rank_ = r; }

    int getNeighborId(int dir) const { return neighbors_[dir]; }
    void setNeighborId(int dir, int id) { neighbors_[dir] = id; }

    int getNeighborRank(int dir) const { return neighbor_ranks_[dir]; }
    void setNeighborRank(int dir, int r) { neighbor_ranks_[dir] = r; }

    void packBoundary(int face, std::vector<Real>& buffer) const;
    void unpackBoundary(int face, const std::vector<Real>& buffer);

    /// Total cells (interior + ghost) in dimension d
    int totalCells(int d) const { return ncells_[d] + 2 * nghost_; }

    /// Cell spacing in dimension d
    Real dx(int d) const { return dx_[d]; }

    /// Physical coordinate of cell center (i,j,k), including ghost zones
    Real x(int d, int idx) const {
        return lo_[d] + (static_cast<Real>(idx) - nghost_ + 0.5) * dx_[d];
    }

    /// Interior cell range: [istart(d), iend(d)) in dimension d.
    int istart(int d = 0) const {
        (void)d;
        return nghost_;
    }
    int iend(int d) const { return nghost_ + ncells_[d]; }

    int iend0() const { return nghost_ + ncells_[0]; }
    int iend1() const { return nghost_ + ncells_[1]; }
    int iend2() const { return nghost_ + ncells_[2]; }

    /// Access field data: data(var, i, j, k).
    /// Flat address = var * stride_ + i + nx*(j + ny*k)
    Real& data(int var, int i, int j, int k) {
        return data_[static_cast<std::size_t>(var) * stride_ + flatIndex(i, j, k)];
    }
    const Real& data(int var, int i, int j, int k) const {
        return data_[static_cast<std::size_t>(var) * stride_ + flatIndex(i, j, k)];
    }

    /// Raw pointer to contiguous data for variable var.
    /// Useful for MPI Send/Recv and BLAS calls.
    Real* rawData(int var) { return data_.data() + static_cast<std::ptrdiff_t>(var) * stride_; }
    const Real* rawData(int var) const {
        return data_.data() + static_cast<std::ptrdiff_t>(var) * stride_;
    }

    /// Raw pointer to the entire flat buffer (for bulk MPI transfers).
    Real* flatBuffer() { return data_.data(); }
    const Real* flatBuffer() const { return data_.data(); }

    /// Total number of cells in the block (including ghost)
    std::size_t totalSize() const {
        return static_cast<std::size_t>(totalCells(0)) * totalCells(1) * totalCells(2);
    }

    // --- Grid metadata ---

    const std::array<int, DIM>& numCells() const { return ncells_; }
    const std::array<Real, DIM>& lowerCorner() const { return lo_; }
    const std::array<Real, DIM>& upperCorner() const { return hi_; }
    const std::array<Real, DIM>& cellSpacing() const { return dx_; }

private:
    int id_;
    int level_;
    int nghost_;
    int num_vars_;
    int rank_ = 0;
    int neighbors_[6] = {-1, -1, -1, -1, -1, -1};
    int neighbor_ranks_[6] = {-1, -1, -1, -1, -1, -1};
    std::array<int, DIM> ncells_;
    std::array<Real, DIM> lo_, hi_, dx_;

    /// Stride between variable planes: stride_ = totalCells(0)*totalCells(1)*totalCells(2)
    std::size_t stride_ = 0;

    /// Flat contiguous field storage: data_[var * stride_ + flat(i,j,k)].
    /// Single allocation of size num_vars_ × stride_.
    std::vector<Real> data_;

    int flatIndex(int i, int j, int k) const {
        int nx = totalCells(0);
        int ny = totalCells(1);
        return i + nx * (j + ny * k);
    }
};

} // namespace granite
