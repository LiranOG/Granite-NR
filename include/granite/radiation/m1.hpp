/**
 * @file granite/radiation/m1.hpp
 * @brief M1 radiation transport module — public interface.
 *
 * Frequency-integrated, grey M1 closure scheme for photon radiation,
 * coupled to the GRMHD gas via absorption, scattering, and emission.
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#pragma once

#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"

namespace granite::radiation {

struct M1Params {
    Real kappa_a = 0.4; ///< Absorption opacity [cm²/g] (default: Thomson)
    Real kappa_s = 0.2; ///< Scattering opacity [cm²/g]
    bool use_tabulated_opacities = false;
    bool use_imex = true;    ///< Use IMEX for stiff source terms
    Real floor_Er = 1.0e-20; ///< Radiation energy density floor
};

/**
 * @class M1Transport
 * @brief Evolves radiation energy density E_r and flux F_r^i using M1 closure.
 */
class M1Transport {
public:
    explicit M1Transport(const M1Params& params);
    ~M1Transport() = default;

    /// Compute RHS for radiation variables (E_r, F_r^i)
    void computeRHS(const GridBlock& spacetime,
                    const GridBlock& hydro_prim,
                    const GridBlock& radiation,
                    GridBlock& rhs) const;

    /// Compute the variable Eddington tensor (Minerbo/Levermore-Pomraning)
    void eddingtonTensor(
        Real Er, Real Frx, Real Fry, Real Frz, std::array<Real, SYM_TENSOR_COMPS>& Pij) const;

    /// Apply implicit radiation-matter coupling source terms (IMEX)
    void applyImplicitSources(const GridBlock& spacetime,
                              GridBlock& hydro_prim,
                              GridBlock& radiation,
                              Real dt) const;

    /// Compute radiation 4-force G^ν for GRMHD coupling
    void computeRadiationForce(const GridBlock& spacetime,
                               const GridBlock& hydro_prim,
                               const GridBlock& radiation,
                               std::vector<std::array<Real, SPACETIME_DIM>>& Gmu) const;

    const M1Params& params() const { return params_; }

private:
    M1Params params_;
};

} // namespace granite::radiation
