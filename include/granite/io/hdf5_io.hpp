/**
 * @file granite/io/hdf5_io.hpp
 * @brief HDF5 I/O module — parallel read/write, checkpoint/restart.
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#pragma once

#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"

#include <string>
#include <vector>

namespace granite::io {

struct IOParams {
    std::string output_dir = "output";
    std::string checkpoint_prefix = "checkpoint";
    int output_interval = 10;
    int checkpoint_interval = 1000;
    bool parallel_hdf5 = true;
    int compression_level = 0; ///< 0 = none, 1-9 = gzip level
};

/**
 * @class HDF5Writer
 * @brief Writes grid data and metadata to HDF5 files.
 */
class HDF5Writer {
public:
    explicit HDF5Writer(const IOParams& params);
    ~HDF5Writer();

    /// Write a single grid block to an open file
    void writeBlock(const GridBlock& block,
                    const std::vector<std::string>& var_names,
                    const std::string& filename) const;

    /// Write all blocks at a given time step
    void writeTimestep(const std::vector<const GridBlock*>& blocks,
                       const std::vector<std::string>& var_names,
                       int step,
                       Real time) const;

    /// Write checkpoint (all data needed for restart)
    void writeCheckpoint(const std::vector<const GridBlock*>& blocks,
                         int step,
                         Real time,
                         const SimulationParams& sim_params) const;

    /// Write 1D scalar time-series (e.g., constraints, GW modes)
    void appendTimeSeries(const std::string& filename,
                          const std::string& dataset_name,
                          Real time,
                          Real value) const;

    /// Write 2D spherical data (for Ψ₄ extraction on spheres)
    void writeSphericalData(const std::string& filename,
                            int l_max,
                            Real time,
                            Real r_ext,
                            const std::vector<Real>& data) const;

    const IOParams& params() const { return params_; }

private:
    IOParams params_;
};

/**
 * @class HDF5Reader
 * @brief Reads grid data and metadata from HDF5 files.
 */
class HDF5Reader {
public:
    HDF5Reader() = default;
    ~HDF5Reader() = default;

    /// Read a checkpoint file and reconstruct grid blocks
    void readCheckpoint(const std::string& filename,
                        std::vector<std::unique_ptr<GridBlock>>& blocks,
                        int& step,
                        Real& time,
                        SimulationParams& sim_params) const;

    /// Read a specific dataset from an HDF5 file
    void readDataset(const std::string& filename,
                     const std::string& dataset_name,
                     std::vector<Real>& data,
                     std::vector<int>& shape) const;

    /// List all datasets in an HDF5 file
    std::vector<std::string> listDatasets(const std::string& filename) const;
};

} // namespace granite::io
