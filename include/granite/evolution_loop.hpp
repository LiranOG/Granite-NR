// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Liran M. Schwartz

/**
 * @file granite/evolution_loop.hpp
 * @brief RK3 time-integration loop and diagnostic/IO triggering.
 */
#pragma once

#include "granite/simulation_setup.hpp"

namespace granite {

/**
 * @brief SSP-RK3 time integrator for the method-of-lines approach.
 *
 * u^(1) = u^n + Δt L(u^n)
 * u^(2) = (3/4)u^n + (1/4)u^(1) + (1/4)Δt L(u^(1))
 * u^{n+1} = (1/3)u^n + (2/3)u^(2) + (2/3)Δt L(u^(2))
 */
class TimeIntegrator {
public:
    static void sspRK3Step(std::vector<BlockBundle*>& bundles,
                           std::vector<BlockBundle>& active_bundles,
                           const std::unordered_map<int, size_t>& id_to_index,
                           const spacetime::CCZ4Evolution& ccz4,
                           const grmhd::GRMHDEvolution& grmhd,
                           Real dt);
};

/**
 * @brief Run the main evolution loop until t_final or max_steps is reached.
 *
 * Drives the AMR subcycling, diagnostic output, checkpointing, and
 * NaN monitoring. This is the top-level time-marching entry point.
 *
 * @param ctx  Mutable simulation context (time/step state is updated in place).
 */
void runEvolutionLoop(SimulationContext& ctx);

} // namespace granite
