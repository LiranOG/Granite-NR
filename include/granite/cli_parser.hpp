// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Liran M. Schwartz

/**
 * @file granite/cli_parser.hpp
 * @brief CLI argument parsing and YAML parameter ingestion.
 */
#pragma once

#include "granite/core/types.hpp"
#include "granite/initial_data/initial_data.hpp"

#include <string>
#include <vector>

namespace granite {

/**
 * @brief Holds the result of parsing CLI arguments and the YAML parameter file.
 *
 * Encapsulates all state extracted from command-line arguments and the YAML
 * config so that main() does not need to know about YAML internals.
 */
struct ParsedConfig {
    SimulationParams params;
    std::string initial_data_type = "flat";
    Real gauge_wave_amplitude = 0.1;
    Real gauge_wave_wavelength = 1.0;
    std::vector<initial_data::BlackHoleParams> bh_params;
};

/**
 * @brief Print the GRANITE startup banner to stdout.
 */
void printBanner();

/**
 * @brief Parse command-line arguments and load the YAML parameter file.
 *
 * @param argc  Argument count from main().
 * @param argv  Argument vector from main().
 * @return ParsedConfig  Fully populated configuration struct.
 *
 * On YAML parse failure, prints an error to stderr and calls std::exit(1).
 */
ParsedConfig parseConfig(int argc, char* argv[]);

} // namespace granite
