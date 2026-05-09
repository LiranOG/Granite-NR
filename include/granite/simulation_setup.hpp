// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Liran M. Schwartz

/**
 * @file granite/simulation_setup.hpp
 * @brief Simulation component instantiation and initial data application.
 */
#pragma once

#include "granite/amr/amr.hpp"
#include "granite/cli_parser.hpp"
#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"
#include "granite/grmhd/grmhd.hpp"
#include "granite/io/hdf5_io.hpp"
#include "granite/spacetime/ccz4.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

namespace granite {

/**
 * @brief Per-block storage bundle for the RK3 integrator.
 *
 * Owns the hydro, primitive, RHS, and stage grids for one AMR block.
 * The spacetime grid (st) is a non-owning pointer into the AMR hierarchy.
 */
struct BlockBundle {
    int id = -1;
    GridBlock* st;
    std::unique_ptr<GridBlock> hydro;
    std::unique_ptr<GridBlock> prim;
    std::unique_ptr<GridBlock> st_rhs;
    std::unique_ptr<GridBlock> hydro_rhs;
    std::unique_ptr<GridBlock> st_stage;
    std::unique_ptr<GridBlock> hydro_stage;

    // Pre-allocated scratch buffers (avoid per-RK3-step heap allocation)
    std::vector<Real> rho_scratch;
    std::vector<std::array<Real, DIM>> Si_scratch;
    std::vector<std::array<Real, SYM_TENSOR_COMPS>> Sij_scratch;
    std::vector<Real> S_scratch;
    bool scratch_allocated = false;

    // Rule-of-Five: explicitly default move ops to prevent raw-pointer (st)
    // corruption; delete copy ops since unique_ptr members are non-copyable.
    BlockBundle() = default;
    ~BlockBundle() = default;
    BlockBundle(BlockBundle&&) noexcept = default;
    BlockBundle& operator=(BlockBundle&&) noexcept = default;
    BlockBundle(const BlockBundle&) = delete;
    BlockBundle& operator=(const BlockBundle&) = delete;

    void allocateScratch(std::size_t total_size);
    void clearScratch();
};

/**
 * @brief Owns all long-lived simulation objects.
 *
 * Created by setupSimulation(), consumed by runEvolutionLoop().
 * Encapsulates the AMR hierarchy, physics modules, I/O writer,
 * and the block-bundle bookkeeping that was previously scattered
 * across main() local variables and lambdas.
 */
struct SimulationContext {
    // Physics modules
    spacetime::CCZ4Evolution ccz4;
    grmhd::GRMHDEvolution grmhd;

    // AMR hierarchy
    amr::AMRHierarchy hierarchy;

    // I/O
    io::HDF5Writer writer;

    // Block management
    std::vector<BlockBundle> active_bundles;
    std::unordered_map<int, size_t> id_to_index;

    // Time-stepping state
    Real dt;
    Real t = 0.0;
    int step = 0;
    int initial_ncells;

    // Config (kept for I/O and diagnostics)
    SimulationParams params;

    // Constructor — takes ownership of pre-built components
    SimulationContext(spacetime::CCZ4Evolution&& ccz4_in,
                      grmhd::GRMHDEvolution&& grmhd_in,
                      amr::AMRHierarchy&& hierarchy_in,
                      io::HDF5Writer&& writer_in,
                      Real dt_in,
                      int initial_ncells_in,
                      const SimulationParams& params_in)
        : ccz4(std::move(ccz4_in)),
          grmhd(std::move(grmhd_in)),
          hierarchy(std::move(hierarchy_in)),
          writer(std::move(writer_in)),
          dt(dt_in),
          initial_ncells(initial_ncells_in),
          params(params_in) {}

    /// Synchronize active_bundles with the current AMR hierarchy state.
    void syncBlocks();
};

/**
 * @brief Construct and wire all simulation components from a ParsedConfig.
 *
 * Handles: micro-offset, grid creation, AMR init, EOS selection,
 * CCZ4/GRMHD instantiation, initial data dispatch, and tracking spheres.
 *
 * @param config  The parsed CLI + YAML configuration.
 * @return std::unique_ptr<SimulationContext>  Ready-to-evolve context.
 */
std::unique_ptr<SimulationContext> setupSimulation(ParsedConfig& config);

} // namespace granite
