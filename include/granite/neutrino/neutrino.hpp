/**
 * @file granite/neutrino/neutrino.hpp
 * @brief Neutrino transport module — leakage + M1 hybrid.
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#pragma once

#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"

namespace granite::neutrino {

enum class NeutrinoSpecies { NU_E, NU_E_BAR, NU_X, NUM_SPECIES };
constexpr int NUM_NU_SPECIES = 3;

struct NeutrinoParams {
    bool use_leakage = true;
    bool use_m1_transport = false;             ///< Full M1 (more expensive)
    Real optical_depth_transition = 2.0 / 3.0; ///< τ at which leakage → M1
};

/**
 * @class NeutrinoTransport
 * @brief Hybrid leakage + M1 scheme for 3 neutrino species.
 */
class NeutrinoTransport {
public:
    explicit NeutrinoTransport(const NeutrinoParams& params);
    ~NeutrinoTransport() = default;

    /// Compute neutrino cooling/heating rates Q_ν
    void computeLeakageRates(const GridBlock& spacetime,
                             const GridBlock& hydro_prim,
                             std::vector<std::array<Real, NUM_NU_SPECIES>>& Q_cool,
                             std::vector<std::array<Real, NUM_NU_SPECIES>>& Q_heat) const;

    /// Evolve neutrino M1 moments (if enabled)
    void computeRHS(const GridBlock& spacetime,
                    const GridBlock& hydro_prim,
                    const GridBlock& nu_data,
                    GridBlock& rhs) const;

    /// Compute optical depth along radial rays
    void computeOpticalDepth(const GridBlock& spacetime,
                             const GridBlock& hydro_prim,
                             std::vector<std::array<Real, NUM_NU_SPECIES>>& tau) const;

    /// Update electron fraction Y_e from neutrino reactions
    void updateElectronFraction(const GridBlock& hydro_prim,
                                const std::vector<std::array<Real, NUM_NU_SPECIES>>& Q_cool,
                                const std::vector<std::array<Real, NUM_NU_SPECIES>>& Q_heat,
                                std::vector<Real>& dYe_dt) const;

    const NeutrinoParams& params() const { return params_; }

private:
    NeutrinoParams params_;
};

} // namespace granite::neutrino
