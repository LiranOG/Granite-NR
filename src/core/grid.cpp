/**
 * @file grid.cpp
 * @brief GridBlock implementation — field-major AMR block storage with
 *        boundary packing/unpacking for MPI ghost-zone exchange.
 */
#include "granite/core/grid.hpp"

#include <stdexcept>
#include <string>

namespace granite {

GridBlock::GridBlock(int id,
                     int level,
                     std::array<int, DIM> ncells,
                     std::array<Real, DIM> lo,
                     std::array<Real, DIM> hi,
                     int nghost,
                     int num_vars)
    : id_(id), level_(level), nghost_(nghost), num_vars_(num_vars), ncells_(ncells), lo_(lo),
      hi_(hi) {
    for (int d = 0; d < DIM; ++d) {
        dx_[d] = (hi_[d] - lo_[d]) / static_cast<Real>(ncells_[d]);
    }

    // H2 fix (Phase 5): single contiguous allocation of size num_vars × total_cells.
    // stride_ is the number of cells per variable slab (including ghosts).
    // data_[var * stride_ + flat(i,j,k)] — field-major, one malloc.
    stride_ = totalSize();
    data_.assign(static_cast<std::size_t>(num_vars_) * stride_, 0.0);
}

void GridBlock::packBoundary(int face, std::vector<Real>& buffer) const {
    buffer.clear();

    // For each face we pack exactly nghost_ slabs along the normal direction,
    // and ALL cells (including ghosts) along the two transverse directions so
    // that corner/edge ghost zones are communicated correctly.
    int nx = totalCells(0);
    int ny = totalCells(1);
    int nz = totalCells(2);
    int ng = nghost_;

    // Default: full slab extents
    int is = 0, ie = nx;
    int js = 0, je = ny;
    int ks = 0, ke = nz;

    // Restrict to the nghost interior slabs adjacent to the requested face
    switch (face) {
    case 0:
        is = ng;
        ie = 2 * ng;
        break; // X-low  interior face
    case 1:
        is = nx - 2 * ng;
        ie = nx - ng;
        break; // X-high interior face
    case 2:
        js = ng;
        je = 2 * ng;
        break; // Y-low  interior face
    case 3:
        js = ny - 2 * ng;
        je = ny - ng;
        break; // Y-high interior face
    case 4:
        ks = ng;
        ke = 2 * ng;
        break; // Z-low  interior face
    case 5:
        ks = nz - 2 * ng;
        ke = nz - ng;
        break; // Z-high interior face
    default:
        break;
    }

    buffer.reserve(static_cast<std::size_t>(num_vars_) * static_cast<std::size_t>(ie - is) *
                   static_cast<std::size_t>(je - js) * static_cast<std::size_t>(ke - ks));

    for (int v = 0; v < num_vars_; ++v)
        for (int k = ks; k < ke; ++k)
            for (int j = js; j < je; ++j)
                for (int i = is; i < ie; ++i)
                    buffer.push_back(data(v, i, j, k));
}

void GridBlock::unpackBoundary(int face, const std::vector<Real>& buffer) {
    int nx = totalCells(0);
    int ny = totalCells(1);
    int nz = totalCells(2);
    int ng = nghost_;

    // Default: full slab extents
    int is = 0, ie = nx;
    int js = 0, je = ny;
    int ks = 0, ke = nz;

    // Place incoming data into the ghost zone on the requested face
    switch (face) {
    case 0:
        is = 0;
        ie = ng;
        break; // X-low  ghost zone
    case 1:
        is = nx - ng;
        ie = nx;
        break; // X-high ghost zone
    case 2:
        js = 0;
        je = ng;
        break; // Y-low  ghost zone
    case 3:
        js = ny - ng;
        je = ny;
        break; // Y-high ghost zone
    case 4:
        ks = 0;
        ke = ng;
        break; // Z-low  ghost zone
    case 5:
        ks = nz - ng;
        ke = nz;
        break; // Z-high ghost zone
    default:
        break;
    }

    // Validate buffer size to catch mismatches early
    std::size_t expected = static_cast<std::size_t>(num_vars_) * static_cast<std::size_t>(ie - is) *
        static_cast<std::size_t>(je - js) * static_cast<std::size_t>(ke - ks);
    if (buffer.size() != expected) {
        // This is a programming error: sizes must match
        throw std::runtime_error("GridBlock::unpackBoundary: buffer size mismatch. "
                                 "Expected " +
                                 std::to_string(expected) + " got " +
                                 std::to_string(buffer.size()));
    }

    std::size_t idx = 0;
    for (int v = 0; v < num_vars_; ++v)
        for (int k = ks; k < ke; ++k)
            for (int j = js; j < je; ++j)
                for (int i = is; i < ie; ++i)
                    data(v, i, j, k) = buffer[idx++];
}

} // namespace granite
