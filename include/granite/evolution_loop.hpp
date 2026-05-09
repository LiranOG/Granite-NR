// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Liran M. Schwartz

/**
 * @file granite/evolution_loop.hpp
 * @brief RK3 time-integration loop and diagnostic/IO triggering.
 *
 * The SSP-RK3 time integrator (TimeIntegrator) is an internal
 * implementation detail defined only in evolution_loop.cpp.
 */
#pragma once

#include "granite/simulation_setup.hpp"

namespace granite {

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
