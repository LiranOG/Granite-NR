/**
 * @file hdf5_io.cpp
 * @brief HDF5 parallel I/O implementation.
 *
 * @copyright 2026 Liran M. Schwartz
 */
#include "granite/io/hdf5_io.hpp"

#ifdef GRANITE_USE_HDF5
#include <hdf5.h>
#ifdef GRANITE_USE_MPI
#include <mpi.h>
#endif
#endif

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace granite::io {

// ===========================================================================
// HDF5Writer
// ===========================================================================

HDF5Writer::HDF5Writer(const IOParams& params) : params_(params) {
    // Create output directory if it doesn't exist
    std::filesystem::create_directories(params_.output_dir);
}

HDF5Writer::~HDF5Writer() = default;

void HDF5Writer::writeBlock(const GridBlock& block,
                            const std::vector<std::string>& var_names,
                            const std::string& filename) const {
#ifdef GRANITE_USE_HDF5
    // Open file (create if new, append if existing)
    hid_t plist_id = H5Pcreate(H5P_FILE_ACCESS);
#ifdef GRANITE_USE_MPI
#if defined(H5_HAVE_PARALLEL)
    if (params_.parallel_hdf5) {
        H5Pset_fapl_mpio(plist_id, MPI_COMM_WORLD, MPI_INFO_NULL);
    }
#endif // H5_HAVE_PARALLEL
#endif // GRANITE_USE_MPI

    hid_t file_id = H5Fcreate(filename.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, plist_id);
    H5Pclose(plist_id);

    if (file_id < 0) {
        std::cerr << "ERROR: Failed to create HDF5 file: " << filename << "\n";
        return;
    }

    // Write grid metadata as attributes
    std::string level_name = "level_" + std::to_string(block.getLevel());
    std::string block_name = "block_" + std::to_string(block.getId());

    // HDF5 cannot create nested groups in one call — create parent first.
    hid_t level_group_id =
        H5Gcreate2(file_id, level_name.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (level_group_id < 0) {
        // Level group may already exist in an append scenario — try opening it.
        level_group_id = H5Gopen2(file_id, level_name.c_str(), H5P_DEFAULT);
    }
    if (level_group_id < 0) {
        std::cerr << "ERROR: Failed to create/open HDF5 group: " << level_name << "\n";
        H5Fclose(file_id);
        return;
    }

    hid_t group_id =
        H5Gcreate2(level_group_id, block_name.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    // Cell counts (interior)
    {
        hsize_t dim = 3;
        hid_t space = H5Screate_simple(1, &dim, nullptr);
        hid_t attr =
            H5Acreate2(group_id, "ncells", H5T_NATIVE_INT, space, H5P_DEFAULT, H5P_DEFAULT);
        int nc[3] = {block.numCells()[0], block.numCells()[1], block.numCells()[2]};
        H5Awrite(attr, H5T_NATIVE_INT, nc);
        H5Aclose(attr);
        H5Sclose(space);
    }

    // Cell spacing
    {
        hsize_t dim = 3;
        hid_t space = H5Screate_simple(1, &dim, nullptr);
        hid_t attr = H5Acreate2(
            group_id, "cell_spacing", H5T_NATIVE_DOUBLE, space, H5P_DEFAULT, H5P_DEFAULT);
        double dx[3] = {block.dx(0), block.dx(1), block.dx(2)};
        H5Awrite(attr, H5T_NATIVE_DOUBLE, dx);
        H5Aclose(attr);
        H5Sclose(space);
    }

    // Write each variable as a 3D dataset (interior cells only)
    int nvars = std::min(static_cast<int>(var_names.size()), block.getNumVars());
    for (int v = 0; v < nvars; ++v) {
        hsize_t dims[3] = {static_cast<hsize_t>(block.numCells()[2]),
                           static_cast<hsize_t>(block.numCells()[1]),
                           static_cast<hsize_t>(block.numCells()[0])};
        hid_t dataspace = H5Screate_simple(3, dims, nullptr);

        // Optional compression
        hid_t dcpl = H5Pcreate(H5P_DATASET_CREATE);
        if (params_.compression_level > 0) {
            H5Pset_deflate(dcpl, params_.compression_level);
            hsize_t chunk[3] = {std::min(dims[0], static_cast<hsize_t>(32)),
                                std::min(dims[1], static_cast<hsize_t>(32)),
                                std::min(dims[2], static_cast<hsize_t>(32))};
            H5Pset_chunk(dcpl, 3, chunk);
        }

        hid_t dataset = H5Dcreate2(group_id,
                                   var_names[v].c_str(),
                                   H5T_NATIVE_DOUBLE,
                                   dataspace,
                                   H5P_DEFAULT,
                                   dcpl,
                                   H5P_DEFAULT);

        // Copy interior data to contiguous buffer
        std::size_t total = dims[0] * dims[1] * dims[2];
        std::vector<double> buffer(total);

        int is_x = block.istart(0);
        int is_y = block.istart(1);
        int is_z = block.istart(2);
        for (hsize_t kk = 0; kk < dims[0]; ++kk)
            for (hsize_t jj = 0; jj < dims[1]; ++jj)
                for (hsize_t ii = 0; ii < dims[2]; ++ii)
                    buffer[kk * dims[1] * dims[2] + jj * dims[2] + ii] =
                        block.data(v, is_x + ii, is_y + jj, is_z + kk);

        H5Dwrite(dataset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer.data());

        H5Dclose(dataset);
        H5Pclose(dcpl);
        H5Sclose(dataspace);
    }

    H5Gclose(group_id);
    H5Gclose(level_group_id);
    H5Fclose(file_id);
#else
    std::cerr << "WARNING: HDF5 not available. No data written.\n";
#endif
}

void HDF5Writer::writeTimestep(const std::vector<const GridBlock*>& blocks,
                               const std::vector<std::string>& var_names,
                               int step,
                               Real time) const {
    std::ostringstream ss;
    ss << params_.output_dir << "/output_" << std::setfill('0') << std::setw(6) << step << ".h5";

    for (const auto* block : blocks) {
        writeBlock(*block, var_names, ss.str());
    }
}

void HDF5Writer::writeCheckpoint(const std::vector<const GridBlock*>& blocks,
                                 int step,
                                 Real time,
                                 const SimulationParams& sim_params) const {
#ifdef GRANITE_USE_HDF5
    std::ostringstream ss;
    ss << params_.output_dir << "/" << params_.checkpoint_prefix << "_" << std::setfill('0')
       << std::setw(6) << step << ".h5";

    std::cout << "Writing checkpoint: " << ss.str() << " (t=" << time << ")\n";

    hid_t file_id = H5Fcreate(ss.str().c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (file_id < 0) {
        std::cerr << "Failed to create checkpoint file.\n";
        return;
    }

    // Write parameters as attributes onto the root group
    hid_t root = H5Gopen2(file_id, "/", H5P_DEFAULT);

    auto write_attr_real = [root](const char* name, Real val) {
        hid_t space = H5Screate(H5S_SCALAR);
        hid_t attr = H5Acreate2(root, name, H5T_NATIVE_DOUBLE, space, H5P_DEFAULT, H5P_DEFAULT);
        H5Awrite(attr, H5T_NATIVE_DOUBLE, &val);
        H5Aclose(attr);
        H5Sclose(space);
    };
    auto write_attr_int = [root](const char* name, int val) {
        hid_t space = H5Screate(H5S_SCALAR);
        hid_t attr = H5Acreate2(root, name, H5T_NATIVE_INT, space, H5P_DEFAULT, H5P_DEFAULT);
        H5Awrite(attr, H5T_NATIVE_INT, &val);
        H5Aclose(attr);
        H5Sclose(space);
    };

    write_attr_real("time", time);
    write_attr_int("step", step);
    write_attr_int("num_blocks", static_cast<int>(blocks.size()));

    for (size_t b = 0; b < blocks.size(); ++b) {
        const auto* block = blocks[b];
        std::string gname = "block_" + std::to_string(b);
        hid_t group = H5Gcreate2(root, gname.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

        hsize_t total_size = static_cast<hsize_t>(block->totalSize());
        hsize_t dims[1] = {total_size};
        hid_t space = H5Screate_simple(1, dims, nullptr);

        for (int v = 0; v < block->getNumVars(); ++v) {
            std::string dname = "var_" + std::to_string(v);
            hid_t dataset = H5Dcreate2(group,
                                       dname.c_str(),
                                       H5T_NATIVE_DOUBLE,
                                       space,
                                       H5P_DEFAULT,
                                       H5P_DEFAULT,
                                       H5P_DEFAULT);
            H5Dwrite(dataset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, block->rawData(v));
            H5Dclose(dataset);
        }
        H5Sclose(space);
        H5Gclose(group);
    }

    H5Gclose(root);
    H5Fclose(file_id);
#endif
}

void HDF5Writer::appendTimeSeries(const std::string& filename,
                                  const std::string& dataset_name,
                                  Real time,
                                  Real value) const {
#ifdef GRANITE_USE_HDF5
    // Append to extendable HDF5 dataset or simple CSV fallback
    // Note: Due to limitations in simple HDF5 API for extending matrices easily without High Level
    // HL PT API, we use a simple chunk structure to write to an extendible dataset.

    hid_t file;
    // Suppress HDF5 errors since H5Fopen fails if file doesn't exist.
    H5E_auto2_t old_func;
    void* old_client_data;
    H5Eget_auto2(H5E_DEFAULT, &old_func, &old_client_data);
    H5Eset_auto2(H5E_DEFAULT, NULL, NULL);

    file = H5Fopen(filename.c_str(), H5F_ACC_RDWR, H5P_DEFAULT);

    H5Eset_auto2(H5E_DEFAULT, old_func, old_client_data);

    if (file < 0) {
        file = H5Fcreate(filename.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    }

    hid_t dataset;
    // Suppress errors around H5Lexists — a missing path legitimately triggers
    // HDF5 diagnostics even when we explicitly want to check existence.
    H5Eget_auto2(H5E_DEFAULT, &old_func, &old_client_data);
    H5Eset_auto2(H5E_DEFAULT, NULL, NULL);
    htri_t dset_exists = H5Lexists(file, dataset_name.c_str(), H5P_DEFAULT);
    H5Eset_auto2(H5E_DEFAULT, old_func, old_client_data);

    if (dset_exists > 0) {
        dataset = H5Dopen2(file, dataset_name.c_str(), H5P_DEFAULT);
        hid_t space = H5Dget_space(dataset);
        hsize_t dims[2];
        H5Sget_simple_extent_dims(space, dims, nullptr);
        H5Sclose(space);

        dims[0] += 1; // extend row
        H5Dset_extent(dataset, dims);

        space = H5Dget_space(dataset);
        hsize_t start[2] = {dims[0] - 1, 0};
        hsize_t count[2] = {1, 2};
        H5Sselect_hyperslab(space, H5S_SELECT_SET, start, nullptr, count, nullptr);

        hid_t memspace = H5Screate_simple(2, count, nullptr);
        Real data[2] = {time, value};
        H5Dwrite(dataset, H5T_NATIVE_DOUBLE, memspace, space, H5P_DEFAULT, data);

        H5Sclose(memspace);
        H5Sclose(space);
    } else {
        // Create any intermediate groups in the dataset path (e.g. "constraints" in
        // "constraints/hamiltonian_l2")
        {
            std::string path = dataset_name;
            std::string::size_type pos = 0;
            while ((pos = path.find('/', pos)) != std::string::npos) {
                std::string group_path = path.substr(0, pos);
                if (!group_path.empty()) {
                    H5E_auto2_t ef;
                    void* ec;
                    H5Eget_auto2(H5E_DEFAULT, &ef, &ec);
                    H5Eset_auto2(H5E_DEFAULT, NULL, NULL);
                    hid_t grp = H5Gopen2(file, group_path.c_str(), H5P_DEFAULT);
                    H5Eset_auto2(H5E_DEFAULT, ef, ec);
                    if (grp < 0) {
                        grp = H5Gcreate2(
                            file, group_path.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
                    }
                    if (grp >= 0)
                        H5Gclose(grp);
                }
                ++pos;
            }
        }

        hsize_t dims[2] = {1, 2};
        hsize_t maxdims[2] = {H5S_UNLIMITED, 2};
        hid_t space = H5Screate_simple(2, dims, maxdims);

        hid_t prop = H5Pcreate(H5P_DATASET_CREATE);
        hsize_t chunk[2] = {1000, 2};
        H5Pset_chunk(prop, 2, chunk);

        dataset = H5Dcreate2(
            file, dataset_name.c_str(), H5T_NATIVE_DOUBLE, space, H5P_DEFAULT, prop, H5P_DEFAULT);

        Real data[2] = {time, value};
        H5Dwrite(dataset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);

        H5Pclose(prop);
        H5Sclose(space);
    }

    H5Dclose(dataset);
    H5Fclose(file);
#endif
}

void HDF5Writer::writeSphericalData(const std::string& filename,
                                    int l_max,
                                    Real time,
                                    Real r_ext,
                                    const std::vector<Real>& data) const {
#ifdef GRANITE_USE_HDF5
    hid_t file_id;
    // Suppress errors if file doesn't exist
    H5E_auto2_t old_func;
    void* old_client_data;
    H5Eget_auto2(H5E_DEFAULT, &old_func, &old_client_data);
    H5Eset_auto2(H5E_DEFAULT, NULL, NULL);

    file_id = H5Fopen(filename.c_str(), H5F_ACC_RDWR, H5P_DEFAULT);
    H5Eset_auto2(H5E_DEFAULT, old_func, old_client_data);

    if (file_id < 0) {
        file_id = H5Fcreate(filename.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    }
    if (file_id < 0)
        return;

    std::string grp_name = "/SphericalData_" + std::to_string(static_cast<int>(r_ext));
    hid_t grp_id;
    if (H5Lexists(file_id, grp_name.c_str(), H5P_DEFAULT) > 0) {
        grp_id = H5Gopen2(file_id, grp_name.c_str(), H5P_DEFAULT);
    } else {
        grp_id = H5Gcreate2(file_id, grp_name.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    }

    std::string dataset_name = "time_" + std::to_string(time);
    hsize_t dims[1] = {static_cast<hsize_t>(data.size())};
    hid_t space = H5Screate_simple(1, dims, nullptr);
    hid_t dataset = H5Dcreate2(grp_id,
                               dataset_name.c_str(),
                               H5T_NATIVE_DOUBLE,
                               space,
                               H5P_DEFAULT,
                               H5P_DEFAULT,
                               H5P_DEFAULT);

    if (dataset >= 0) {
        H5Dwrite(dataset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, data.data());
        H5Dclose(dataset);
    }
    H5Sclose(space);
    H5Gclose(grp_id);
    H5Fclose(file_id);
#endif
}

// ===========================================================================
// HDF5Reader
// ===========================================================================

void HDF5Reader::readCheckpoint(const std::string& filename,
                                std::vector<std::unique_ptr<GridBlock>>& blocks,
                                int& step,
                                Real& time,
                                SimulationParams& sim_params) const {
#ifdef GRANITE_USE_HDF5
    hid_t file_id = H5Fopen(filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file_id < 0) {
        throw std::runtime_error("Failed to open checkpoint: " + filename);
    }

    // Read global attributes
    hid_t root = H5Gopen2(file_id, "/", H5P_DEFAULT);
    auto read_attr_real = [root](const char* name) -> Real {
        Real val = 0.0;
        hid_t attr = H5Aopen(root, name, H5P_DEFAULT);
        if (attr >= 0) {
            H5Aread(attr, H5T_NATIVE_DOUBLE, &val);
            H5Aclose(attr);
        }
        return val;
    };
    auto read_attr_int = [root](const char* name) -> int {
        int val = 0;
        hid_t attr = H5Aopen(root, name, H5P_DEFAULT);
        if (attr >= 0) {
            H5Aread(attr, H5T_NATIVE_INT, &val);
            H5Aclose(attr);
        }
        return val;
    };

    time = read_attr_real("time");
    step = read_attr_int("step");
    int num_blocks = read_attr_int("num_blocks");

    blocks.clear();
    for (int b = 0; b < num_blocks; ++b) {
        // Reconstruct grid block from simulation params
        // Assuming block 0 is level 0, id 0 for now (AMR is generic)
        blocks.push_back(std::make_unique<GridBlock>(b,
                                                     0,
                                                     sim_params.ncells,
                                                     sim_params.domain_lo,
                                                     sim_params.domain_hi,
                                                     sim_params.ghost_cells,
                                                     NUM_SPACETIME_VARS));

        std::string gname = "block_" + std::to_string(b);
        hid_t group = H5Gopen2(root, gname.c_str(), H5P_DEFAULT);
        if (group < 0)
            continue;

        for (int v = 0; v < blocks.back()->getNumVars(); ++v) {
            std::string dname = "var_" + std::to_string(v);
            hid_t dataset = H5Dopen2(group, dname.c_str(), H5P_DEFAULT);
            if (dataset >= 0) {
                H5Dread(dataset,
                        H5T_NATIVE_DOUBLE,
                        H5S_ALL,
                        H5S_ALL,
                        H5P_DEFAULT,
                        blocks.back()->rawData(v));
                H5Dclose(dataset);
            }
        }
        H5Gclose(group);
    }

    H5Gclose(root);

    H5Fclose(file_id);
#else
    throw std::runtime_error("HDF5 not available for checkpoint reading.");
#endif
}

void HDF5Reader::readDataset(const std::string& filename,
                             const std::string& dataset_name,
                             std::vector<Real>& data,
                             std::vector<int>& shape) const {
#ifdef GRANITE_USE_HDF5
    hid_t file_id = H5Fopen(filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
    hid_t dataset = H5Dopen2(file_id, dataset_name.c_str(), H5P_DEFAULT);
    hid_t dataspace = H5Dget_space(dataset);

    int ndims = H5Sget_simple_extent_ndims(dataspace);
    std::vector<hsize_t> dims(ndims);
    H5Sget_simple_extent_dims(dataspace, dims.data(), nullptr);

    shape.resize(ndims);
    std::size_t total = 1;
    for (int i = 0; i < ndims; ++i) {
        shape[i] = static_cast<int>(dims[i]);
        total *= dims[i];
    }

    data.resize(total);
    H5Dread(dataset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, data.data());

    H5Sclose(dataspace);
    H5Dclose(dataset);
    H5Fclose(file_id);
#endif
}

std::vector<std::string> HDF5Reader::listDatasets(const std::string& filename) const {
    std::vector<std::string> names;
#ifdef GRANITE_USE_HDF5
    hid_t file_id = H5Fopen(filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file_id >= 0) {
        auto iter_func = [](hid_t loc_id,
                            const char* name,
                            const H5L_info_t* info,
                            void* operator_data) -> herr_t {
            if (info->type == H5L_TYPE_HARD) {
                H5O_info_t obj_info;
                // Cross-version HDF5 compatibility guard:
                //   HDF5 >= 1.12 (incl. v2.x / vcpkg): 5-argument form required;
                //   the 4th arg selects which fields to populate (H5O_INFO_ALL = all).
                //   HDF5 <= 1.10 (Linux CI / system packages): 4-argument form only;
                //   there is no 'fields' parameter — all info is always populated.
#if (H5_VERS_MAJOR > 1) || (H5_VERS_MAJOR == 1 && H5_VERS_MINOR >= 12)
                H5Oget_info_by_name(loc_id, name, &obj_info, H5O_INFO_ALL, H5P_DEFAULT);
#else
                H5Oget_info_by_name(loc_id, name, &obj_info, H5P_DEFAULT);
#endif
                if (obj_info.type == H5O_TYPE_DATASET) {
                    auto* names = static_cast<std::vector<std::string>*>(operator_data);
                    names->push_back(name);
                }
            }
            return 0;
        };
        H5Literate(file_id, H5_INDEX_NAME, H5_ITER_INC, NULL, iter_func, &names);
        H5Fclose(file_id);
    }
#endif
    return names;
}

} // namespace granite::io
