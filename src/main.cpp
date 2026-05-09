// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Liran M. Schwartz

/**
 * @file main.cpp
 * @brief GRANITE main entry point.
 *
 * Minimal wrapper: parses CLI, sets up the simulation, runs the
 * evolution loop. All heavy lifting is in cli_parser, simulation_setup,
 * and evolution_loop translation units.
 */
#include "granite/cli_parser.hpp"
#include "granite/evolution_loop.hpp"
#include "granite/simulation_setup.hpp"

#include <iostream>

#ifdef GRANITE_USE_PETSC
#include <petscsys.h>
#endif

#ifdef GRANITE_USE_MPI
#include <mpi.h>
#endif

int main(int argc, char* argv[]) {
    // Force line-buffered stdout so output appears immediately even when
    // piped through `tee run.log` or redirected.
    std::cout << std::unitbuf;

#ifdef GRANITE_USE_PETSC
    PetscInitialize(&argc, &argv, nullptr, nullptr);
#endif

#ifdef GRANITE_USE_MPI
    MPI_Init(&argc, &argv);
#endif

    using namespace granite;

    printBanner();

    // 1. Parse CLI arguments and YAML parameter file
    ParsedConfig config = parseConfig(argc, argv);

    // 2. Construct all simulation components and apply initial data
    auto ctx = setupSimulation(config);

    // 3. Run the evolution loop
    runEvolutionLoop(*ctx);

#ifdef GRANITE_USE_MPI
    MPI_Finalize();
#endif

#ifdef GRANITE_USE_PETSC
    PetscFinalize();
#endif

    return 0;
}
