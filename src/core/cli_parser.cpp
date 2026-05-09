// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Liran M. Schwartz

/**
 * @file cli_parser.cpp
 * @brief CLI argument parsing, YAML parameter ingestion, and banner printing.
 *
 * Extracted from the monolithic src/main.cpp to isolate all configuration
 * concerns into a single translation unit.
 */
#include "granite/cli_parser.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <yaml-cpp/yaml.h>

namespace granite {

void printBanner() {
    std::cout << "================================================================\n";
    std::cout << " GRANITE v" << "0.6.7.2" << "\n";
    std::cout << " General-Relativistic Adaptive N-body Integrated Tool\n";
    std::cout << " for Extreme Astrophysics\n";
    std::cout << "================================================================\n\n";
}

ParsedConfig parseConfig(int argc, char* argv[]) {
    ParsedConfig cfg;

    if (argc > 1) {
        std::string param_file = argv[1];
        std::cout << "Loading parameter file: " << param_file << "\n";
        try {
            YAML::Node config = YAML::LoadFile(param_file);
            if (config["grid"]) {
                auto grid = config["grid"];
                if (grid["ncells"]) {
                    cfg.params.ncells[0] = grid["ncells"][0].as<int>();
                    cfg.params.ncells[1] = grid["ncells"][1].as<int>();
                    cfg.params.ncells[2] = grid["ncells"][2].as<int>();
                }
                if (grid["domain_lo"]) {
                    cfg.params.domain_lo[0] = grid["domain_lo"][0].as<Real>();
                    cfg.params.domain_lo[1] = grid["domain_lo"][1].as<Real>();
                    cfg.params.domain_lo[2] = grid["domain_lo"][2].as<Real>();
                }
                if (grid["domain_hi"]) {
                    cfg.params.domain_hi[0] = grid["domain_hi"][0].as<Real>();
                    cfg.params.domain_hi[1] = grid["domain_hi"][1].as<Real>();
                    cfg.params.domain_hi[2] = grid["domain_hi"][2].as<Real>();
                }
                if (grid["ghost_cells"])
                    cfg.params.ghost_cells = grid["ghost_cells"].as<int>();
            }
            if (config["time"]) {
                auto time = config["time"];
                if (time["cfl"])
                    cfg.params.cfl = time["cfl"].as<Real>();
                if (time["t_final"])
                    cfg.params.t_final = time["t_final"].as<Real>();
                if (time["max_steps"])
                    cfg.params.max_steps = time["max_steps"].as<int>();
            }
            if (config["ccz4"]) {
                auto ccz4 = config["ccz4"];
                if (ccz4["kappa1"])
                    cfg.params.kappa1 = ccz4["kappa1"].as<Real>();
                if (ccz4["kappa2"])
                    cfg.params.kappa2 = ccz4["kappa2"].as<Real>();
                if (ccz4["eta"])
                    cfg.params.eta = ccz4["eta"].as<Real>();
                if (ccz4["ko_sigma"])
                    cfg.params.ko_sigma = ccz4["ko_sigma"].as<Real>();
            }
            if (config["io"]) {
                auto io = config["io"];
                if (io["output_dir"])
                    cfg.params.output_dir = io["output_dir"].as<std::string>();
                if (io["output_interval"])
                    cfg.params.output_interval = io["output_interval"].as<int>();
                if (io["checkpoint_interval"])
                    cfg.params.checkpoint_interval = io["checkpoint_interval"].as<int>();
            }
            // --- Parse initial data type ---
            if (config["initial_data"]) {
                auto id = config["initial_data"];
                if (id["type"])
                    cfg.initial_data_type = id["type"].as<std::string>();
                if (id["amplitude"])
                    cfg.gauge_wave_amplitude = id["amplitude"].as<Real>();
                if (id["wavelength"])
                    cfg.gauge_wave_wavelength = id["wavelength"].as<Real>();
            }
            // --- Parse black holes ---
            if (config["black_holes"] && config["black_holes"].IsSequence()) {
                for (auto bh_node : config["black_holes"]) {
                    initial_data::BlackHoleParams bh;
                    bh.mass = bh_node["mass"] ? bh_node["mass"].as<Real>() : 1.0;
                    if (bh_node["position"]) {
                        bh.position = {bh_node["position"][0].as<Real>(),
                                       bh_node["position"][1].as<Real>(),
                                       bh_node["position"][2].as<Real>()};
                    } else {
                        bh.position = {0.0, 0.0, 0.0};
                    }
                    if (bh_node["momentum"]) {
                        bh.momentum = {bh_node["momentum"][0].as<Real>(),
                                       bh_node["momentum"][1].as<Real>(),
                                       bh_node["momentum"][2].as<Real>()};
                    } else {
                        bh.momentum = {0.0, 0.0, 0.0};
                    }
                    if (bh_node["spin"]) {
                        bh.spin = {bh_node["spin"][0].as<Real>(),
                                   bh_node["spin"][1].as<Real>(),
                                   bh_node["spin"][2].as<Real>()};
                    } else {
                        bh.spin = {0.0, 0.0, 0.0};
                    }
                    cfg.bh_params.push_back(bh);
                }
            }
        } catch (const YAML::Exception& e) {
            std::cerr << "YAML parsing error: " << e.what() << "\n";
            throw std::runtime_error(std::string("YAML parsing error: ") + e.what());
        }
    }

    return cfg;
}

} // namespace granite
