// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Liran M. Schwartz

/**
 * @file simulation_setup.cpp
 * @brief Component instantiation, initial data dispatch, and AMR wiring.
 *
 * Extracted from the monolithic src/main.cpp to isolate all simulation
 * construction logic into a single translation unit.
 */
#include "granite/simulation_setup.hpp"

#include "granite/initial_data/initial_data.hpp"
#include "granite/spacetime/ccz4.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <unordered_map>

namespace granite {

// ─── BlockBundle ─────────────────────────────────────────────────────────

void BlockBundle::allocateScratch(std::size_t total_size) {
    if (!scratch_allocated) {
        rho_scratch.resize(total_size, 0.0);
        Si_scratch.resize(total_size, {0.0, 0.0, 0.0});
        Sij_scratch.resize(total_size);
        S_scratch.resize(total_size, 0.0);
        scratch_allocated = true;
    }
}

void BlockBundle::clearScratch() {
    std::fill(rho_scratch.begin(), rho_scratch.end(), 0.0);
    for (auto& s : Si_scratch)
        s = {0.0, 0.0, 0.0};
    for (auto& s : Sij_scratch)
        s = {0, 0, 0, 0, 0, 0};
    std::fill(S_scratch.begin(), S_scratch.end(), 0.0);
}

// ─── SimulationContext::syncBlocks ───────────────────────────────────────

void SimulationContext::syncBlocks() {
    auto active_blocks = hierarchy.getAllBlocks();

    // Build a fast lookup of currently active block IDs → block pointers
    std::unordered_map<int, GridBlock*> active_map;
    active_map.reserve(active_blocks.size());
    for (auto* b : active_blocks)
        active_map[b->getId()] = b;

    // Remove stale bundles whose IDs no longer appear in the hierarchy.
    // O(N) scan with O(1) lookup per bundle — replaces previous O(N log N) std::set.
    active_bundles.erase(std::remove_if(active_bundles.begin(),
                                        active_bundles.end(),
                                        [&](const BlockBundle& bundle) {
                                            return active_map.find(bundle.id) == active_map.end();
                                        }),
                         active_bundles.end());

    // Rebuild id_to_index for the surviving bundles (needed for O(1) "exists?" check below)
    id_to_index.clear();
    for (size_t idx = 0; idx < active_bundles.size(); ++idx)
        id_to_index[active_bundles[idx].id] = idx;

    // Add missing bundles and sync ST pointers — O(1) per block via id_to_index
    for (auto* b : active_blocks) {
        auto it = id_to_index.find(b->getId());
        if (it == id_to_index.end()) {
            // New block — allocate a full bundle
            BlockBundle bundle;
            bundle.id = b->getId();
            bundle.st = b;
            // Use actual block dimensions — child AMR blocks may differ from root params.ncells
            const auto nc = b->numCells();
            bundle.hydro = std::make_unique<GridBlock>(b->getId(),
                                                       b->getLevel(),
                                                       nc,
                                                       b->lowerCorner(),
                                                       b->upperCorner(),
                                                       b->getNumGhost(),
                                                       NUM_HYDRO_VARS);
            bundle.prim = std::make_unique<GridBlock>(b->getId(),
                                                      b->getLevel(),
                                                      nc,
                                                      b->lowerCorner(),
                                                      b->upperCorner(),
                                                      b->getNumGhost(),
                                                      NUM_PRIMITIVE_VARS);
            bundle.st_rhs = std::make_unique<GridBlock>(b->getId(),
                                                        b->getLevel(),
                                                        nc,
                                                        b->lowerCorner(),
                                                        b->upperCorner(),
                                                        b->getNumGhost(),
                                                        NUM_SPACETIME_VARS);
            bundle.hydro_rhs = std::make_unique<GridBlock>(b->getId(),
                                                           b->getLevel(),
                                                           nc,
                                                           b->lowerCorner(),
                                                           b->upperCorner(),
                                                           b->getNumGhost(),
                                                           NUM_HYDRO_VARS);
            bundle.st_stage = std::make_unique<GridBlock>(b->getId(),
                                                          b->getLevel(),
                                                          nc,
                                                          b->lowerCorner(),
                                                          b->upperCorner(),
                                                          b->getNumGhost(),
                                                          NUM_SPACETIME_VARS);
            bundle.hydro_stage = std::make_unique<GridBlock>(b->getId(),
                                                             b->getLevel(),
                                                             nc,
                                                             b->lowerCorner(),
                                                             b->upperCorner(),
                                                             b->getNumGhost(),
                                                             NUM_HYDRO_VARS);
            id_to_index[bundle.id] = active_bundles.size();
            active_bundles.push_back(std::move(bundle));
        } else {
            // Existing block — sync the ST pointer
            active_bundles[it->second].st = b;
        }
    }

    // Allocate scratch buffers (one-time per bundle)
    for (auto& bundle : active_bundles) {
        if (bundle.st) {
            bundle.allocateScratch(bundle.st->totalSize());
        }
    }
}

// ─── setupSimulation ─────────────────────────────────────────────────────

std::unique_ptr<SimulationContext> setupSimulation(ParsedConfig& config) {
    auto& params = config.params;
    auto& bh_params = config.bh_params;
    const auto& initial_data_type = config.initial_data_type;

    // --- MICRO_OFFSET: phase-shift the grid away from integer/zero coordinates ---
    {
        constexpr Real MICRO_OFFSET = 1.3621415e-6;
        for (int d = 0; d < 3; ++d) {
            params.domain_lo[d] += MICRO_OFFSET;
            params.domain_hi[d] += MICRO_OFFSET;
        }
    }

    // --- Create GRMHD system ---
    grmhd::GRMHDParams grmhd_params;
    auto eos = std::make_shared<grmhd::IdealGasEOS>(5.0 / 3.0);
    grmhd::GRMHDEvolution grmhd_evo(grmhd_params, eos);

    // --- Create AMR hierarchy ---
    amr::AMRParams amr_params;
    amr_params.regrid_interval = params.regrid_interval;
    amr::AMRHierarchy hierarchy(amr_params, params);
    std::cout << "Initializing AMR Hierarchy...\n";
    hierarchy.initialize(amr::gradientChiTagger(params.refine_threshold));

    // --- CCZ4 evolution ---
    spacetime::CCZ4Params ccz4_params;
    ccz4_params.kappa1 = params.kappa1;
    ccz4_params.kappa2 = params.kappa2;
    ccz4_params.eta = params.eta;
    ccz4_params.ko_sigma = params.ko_sigma;
    spacetime::CCZ4Evolution ccz4_evo(ccz4_params);

    // --- I/O Writer ---
    io::IOParams io_params;
    io_params.output_dir = params.output_dir;
    io_params.output_interval = params.output_interval;
    io_params.checkpoint_interval = params.checkpoint_interval;
    io::HDF5Writer writer(io_params);

    // --- Compute time step ---
    auto initial_blocks = hierarchy.getAllBlocks();
    GridBlock* base_block = initial_blocks[0];
    Real dx_min = std::min({base_block->dx(0), base_block->dx(1), base_block->dx(2)});
    Real dt = params.cfl * dx_min;

    int initial_ncells = params.ncells[0];

    // Build context
    auto ctx = std::make_unique<SimulationContext>(std::move(ccz4_evo),
                                                   std::move(grmhd_evo),
                                                   std::move(hierarchy),
                                                   std::move(writer),
                                                   dt,
                                                   initial_ncells,
                                                   params);

    // --- Set initial data ---
    std::cout << "Setting initial data: type='" << initial_data_type << "'\n";
    ctx->syncBlocks();

    // All scenarios: set GRMHD atmosphere on hydro grid
    auto setAtmosphere = [](GridBlock& hy) {
        for (int v = 0; v < hy.getNumVars(); ++v)
            for (int k = 0; k < hy.totalCells(2); ++k)
                for (int j = 0; j < hy.totalCells(1); ++j)
                    for (int i = 0; i < hy.totalCells(0); ++i)
                        hy.data(v, i, j, k) = (v == 0 || v == 4) ? 1.0e-12 : 0.0;
    };

    if (initial_data_type == "brill_lindquist") {
        if (bh_params.empty()) {
            initial_data::BlackHoleParams default_bh;
            default_bh.mass = 1.0;
            default_bh.position = {0.0, 0.0, 0.0};
            default_bh.momentum = {0.0, 0.0, 0.0};
            default_bh.spin = {0.0, 0.0, 0.0};
            bh_params.push_back(default_bh);
        }
        initial_data::BrillLindquist bl_id(bh_params);
        std::cout << "  Brill-Lindquist: " << bh_params.size()
                  << " BH(s), ADM mass = " << bl_id.admMass() << " M_sun\n";
        for (auto& bundle : ctx->active_bundles) {
            bl_id.apply(*(bundle.st));
            setAtmosphere(*(bundle.hydro));
            ctx->grmhd.conservedToPrimitive(*(bundle.st), *(bundle.hydro), *(bundle.prim));
        }

    } else if (initial_data_type == "bowen_york") {
        if (bh_params.empty()) {
            initial_data::BlackHoleParams default_bh;
            default_bh.mass = 1.0;
            default_bh.position = {0.0, 0.0, 0.0};
            default_bh.momentum = {0.0, 0.0, 0.0};
            default_bh.spin = {0.0, 0.0, 0.0};
            bh_params.push_back(default_bh);
        }
        initial_data::BowenYorkPuncture by_id(bh_params);
        std::cout << "  Bowen-York: " << bh_params.size()
                  << " BH(s), ADM mass = " << by_id.admMass() << " M_sun\n";
        for (auto& bundle : ctx->active_bundles) {
            by_id.solve(*(bundle.st));
            setAtmosphere(*(bundle.hydro));
            ctx->grmhd.conservedToPrimitive(*(bundle.st), *(bundle.hydro), *(bundle.prim));
        }

    } else if (initial_data_type == "two_punctures") {
        if (bh_params.size() < 2) {
            initial_data::BlackHoleParams p1, p2;
            p1.mass = 0.5;
            p1.position = {1.0, 0.0, 0.0};
            p2.mass = 0.5;
            p2.position = {-1.0, 0.0, 0.0};
            p1.momentum = {0.0, 0.1, 0.0};
            p2.momentum = {0.0, -0.1, 0.0};
            bh_params = {p1, p2};
        }
        initial_data::TwoPuncturesParams tp_params;
        tp_params.par_m_plus[0] = bh_params[0].mass;
        tp_params.par_m_minus[0] = bh_params[1].mass;
        tp_params.par_P_plus = bh_params[0].momentum;
        tp_params.par_P_minus = bh_params[1].momentum;
        tp_params.par_S_plus = bh_params[0].spin;
        tp_params.par_S_minus = bh_params[1].spin;
        tp_params.par_b[0] = std::abs(bh_params[0].position[0] - bh_params[1].position[0]) / 2.0;
        initial_data::TwoPuncturesBBH tp_id(tp_params);
        std::cout << "  Two-Punctures: " << bh_params.size()
                  << " BH(s), generating conformal spectral data...\n";
        for (auto& bundle : ctx->active_bundles) {
            tp_id.generate(*(bundle.st));
            setAtmosphere(*(bundle.hydro));
            ctx->grmhd.conservedToPrimitive(*(bundle.st), *(bundle.hydro), *(bundle.prim));
        }

    } else if (initial_data_type == "gauge_wave") {
        std::cout << "  Gauge wave: A=" << config.gauge_wave_amplitude
                  << ", L=" << config.gauge_wave_wavelength << "\n";
        for (auto& bundle : ctx->active_bundles) {
            spacetime::setFlatSpacetime(*(bundle.st));
            GridBlock& st = *(bundle.st);
            for (int k = 0; k < st.totalCells(2); ++k)
                for (int j = 0; j < st.totalCells(1); ++j)
                    for (int i = 0; i < st.totalCells(0); ++i) {
                        Real x = st.x(0, i);
                        Real lapse = 1.0 +
                            config.gauge_wave_amplitude *
                                std::sin(2.0 * M_PI * x / config.gauge_wave_wavelength);
                        st.data(static_cast<int>(SpacetimeVar::LAPSE), i, j, k) = lapse;
                    }
            setAtmosphere(*(bundle.hydro));
            ctx->grmhd.conservedToPrimitive(*(bundle.st), *(bundle.hydro), *(bundle.prim));
        }

    } else {
        if (initial_data_type != "flat") {
            std::cerr << "Warning: unknown initial_data type '" << initial_data_type
                      << "', falling back to 'flat'.\n";
        }
        std::cout << "  Flat Minkowski spacetime + GRMHD atmosphere\n";
        for (auto& bundle : ctx->active_bundles) {
            spacetime::setFlatSpacetime(*(bundle.st));
            setAtmosphere(*(bundle.hydro));
            ctx->grmhd.conservedToPrimitive(*(bundle.st), *(bundle.hydro), *(bundle.prim));
        }
    }

    std::cout << "Grid: " << params.ncells[0] << " x " << params.ncells[1] << " x "
              << params.ncells[2] << "\n";
    std::cout << "dx = " << dx_min << ", dt = " << dt << "\n";
    std::cout << "t_final = " << params.t_final << "\n\n";

    // --- Inject level-0 dt into AMR hierarchy ---
    ctx->hierarchy.setLevelDt(0, dt);

    // --- Register BBH tracking spheres ---
    if (initial_data_type == "two_punctures" || initial_data_type == "brill_lindquist" ||
        initial_data_type == "bowen_york") {
        for (const auto& bh : bh_params) {
            std::array<Real, DIM> center = bh.position;
            Real sphere_radius = std::max(2.0 * bh.mass, 0.5);
            int min_ref_level = std::min(amr_params.max_levels - 1, 3);
            ctx->hierarchy.addTrackingSphere(center, sphere_radius, min_ref_level);
            std::cout << "  [AMR] Tracking sphere @ (" << center[0] << "," << center[1] << ","
                      << center[2] << ") R=" << sphere_radius << " min_level=" << min_ref_level
                      << "\n";
        }
        ctx->hierarchy.regrid(0, amr::gradientChiTagger(params.refine_threshold));
        ctx->syncBlocks();
        ctx->hierarchy.fillGhostZones(0);
    }

    return ctx;
}

} // namespace granite
