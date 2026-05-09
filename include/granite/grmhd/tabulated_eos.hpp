/**
 * @file granite/grmhd/tabulated_eos.hpp
 * @brief Tabulated Nuclear Equation of State -- StellarCollapse/CompOSE format.
 *
 * Implements a full 3-parameter nuclear microphysics EOS table:
 *   (rho [g/cc], T [MeV], Ye [-]) --> {p, eps, s, cs2, mu_e, mu_n, mu_p}
 *
 * Table format: StellarCollapse HDF5 (Hempel, O'Connor, Roberts, Fischer...):
 *   /logrho    [nRho]              log10(rho) [g/cc]
 *   /logtemp   [nTemp]             log10(T)   [MeV]
 *   /ye        [nYe]               Y_e        [-]
 *   /logpress  [nRho*nTemp*nYe]   log10(p)   [dyne/cm^2]
 *   /logenergy [nRho*nTemp*nYe]   log10(|eps|+energy_shift) [erg/g]
 *   /entropy   [nRho*nTemp*nYe]   s          [k_B/baryon]
 *   /cs2       [nRho*nTemp*nYe]   cs^2       [cm^2/s^2]
 *   /mu_e      [nRho*nTemp*nYe]   mu_e       [MeV]
 *   /mu_n      [nRho*nTemp*nYe]   mu_n       [MeV]
 *   /mu_p      [nRho*nTemp*nYe]   mu_p       [MeV]
 *
 * Index ordering (C row-major, matching StellarCollapse convention):
 *   flat = iYe + nYe * (iT + nT * iRho)
 *
 * All interpolations use tri-linear (8-point) stencil in
 * (log10_rho, log10_T, Ye) space. Temperature inversion (eps -> T) uses
 * Newton-Raphson with bisection fallback.
 *
 * References:
 *   O'Connor & Ott (2010) CQG 27, 114103
 *   Hempel & Schaffner-Bielich (2010) NPA 837, 210
 *   Steiner, Hempel, Fischer (2013) ApJ 774, 17 (SFHo/SFHx)
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#pragma once

#include "granite/core/types.hpp"
#include "granite/grmhd/grmhd.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace granite::grmhd {

// ===========================================================================
// Enum for table quantities -- used to index into the 3D data arrays
// ===========================================================================
enum class EOSVar : int {
    LOGPRESS = 0, ///< log10(p [dyne/cm^2])
    LOGENERGY,    ///< log10(eps + energy_shift [erg/g])
    ENTROPY,      ///< s [k_B/baryon]
    CS2,          ///< cs^2 [cm^2/s^2] (divided by c^2 on load -> dimensionless)
    MU_E,         ///< mu_e [MeV]
    MU_N,         ///< mu_n [MeV]
    MU_P,         ///< mu_p [MeV]
    NUM_VARS
};
constexpr int NUM_EOS_VARS = static_cast<int>(EOSVar::NUM_VARS);

// ===========================================================================
// Unit conversion factors (CGS <-> geometric c=1)
// ===========================================================================
namespace eos_units {
// 1 MeV = 1e6 eV; energy in erg/g -> c=1 units: divide by c^2 [cm^2/s^2]
constexpr double C_CGS = 2.99792458e10;           // cm/s
constexpr double C2_CGS = C_CGS * C_CGS;          // cm^2/s^2
constexpr double PRESS_TO_GU = 1.0 / C2_CGS;      // dyne/cm^2 -> code units (c=1, rho in g/cc)
constexpr double EPS_TO_GU = 1.0 / C2_CGS;        // erg/g -> code units
constexpr double MEV_TO_ERG = 1.60217663e-6;      // MeV -> erg
constexpr double MEV_TO_GU = MEV_TO_ERG / C2_CGS; // MeV -> c=1 (g/cc normalization not needed)
// cs2 in table: [cm^2/s^2]. Convert to c=1: divide by C2_CGS.
constexpr double CS2_TO_GU = 1.0 / C2_CGS;
} // namespace eos_units

// ===========================================================================
// TabulatedEOS
// ===========================================================================

/**
 * @class TabulatedEOS
 * @brief Nuclear microphysics EOS with tri-linear interpolation.
 *
 * Thread-safe after construction (read-only). Supports both HDF5 table
 * loading (with GRANITE_USE_HDF5) and synthetic in-memory table generation
 * (for testing without HDF5).
 *
 * The 3 independent variables and their spacing:
 *   log10(rho): uniformly spaced in log space, nRho points
 *   log10(T):   uniformly spaced in log space, nTemp points
 *   Ye:         uniformly spaced linearly,      nYe points
 *
 * All derived quantities are stored at table nodes and interpolated.
 * No extrapolation: queries outside [min,max] are clamped to boundary.
 */
class TabulatedEOS : public EquationOfState {
public:
    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    /**
     * @brief Load from StellarCollapse HDF5 file.
     * @throws std::runtime_error if HDF5 is not enabled or file not found.
     */
    static std::shared_ptr<TabulatedEOS> loadFromHDF5(const std::string& path);

    /**
     * @brief Build a synthetic ideal-gas-equivalent table for testing.
     *
     * Fills the table analytically using: p = (gamma-1)*rho*eps,
     * cs^2 = gamma*p/(rho*h), s = 0 (entropy ignored for ideal gas).
     * This allows the full interpolation machinery to be tested without
     * a real nuclear table.
     *
     * @param nRho   Number of density points  (default 60)
     * @param nTemp  Number of temperature points (default 40)
     * @param nYe    Number of Y_e points (default 20)
     * @param gamma  Adiabatic index (default 5/3)
     * @param log_rho_min  log10(rho_min [g/cc]) (default 3.0 = 1e3 g/cc)
     * @param log_rho_max  log10(rho_max [g/cc]) (default 15.0 = 1e15 g/cc)
     * @param log_T_min    log10(T_min [MeV])     (default -2.0 = 0.01 MeV)
     * @param log_T_max    log10(T_max [MeV])     (default 2.5 = ~316 MeV)
     * @param Ye_min  Minimum Y_e (default 0.01)
     * @param Ye_max  Maximum Y_e (default 0.60)
     */
    static std::shared_ptr<TabulatedEOS> buildSynthetic(int nRho = 60,
                                                        int nTemp = 40,
                                                        int nYe = 20,
                                                        Real gamma = 5.0 / 3.0,
                                                        Real log_rho_min = 3.0,
                                                        Real log_rho_max = 15.0,
                                                        Real log_T_min = -2.0,
                                                        Real log_T_max = 2.5,
                                                        Real Ye_min = 0.01,
                                                        Real Ye_max = 0.60);

    // -----------------------------------------------------------------------
    // Primary EquationOfState interface (rho, eps) -- 2-parameter path
    // -----------------------------------------------------------------------

    /**
     * @brief Pressure p(rho, eps) by temperature inversion then table lookup.
     *
     * Algorithm:
     *   1. T = invertTemperature(rho, eps, Ye_default=0.5) via Newton-Raphson
     *   2. p = pressureFromRhoTYe(rho, T, 0.5)
     *
     * This path is used by the existing con2prim which does not yet carry Ye.
     */
    Real pressure(Real rho, Real eps) const override;
    Real soundSpeed(Real rho, Real eps, Real p) const override;

    // -----------------------------------------------------------------------
    // Extended 3-parameter interface (rho, T, Ye)
    // -----------------------------------------------------------------------

    Real temperature(Real rho, Real eps, Real Ye) const override;
    Real entropy(Real rho, Real T, Real Ye) const override;
    Real pressureFromRhoTYe(Real rho, Real T, Real Ye) const override;
    Real epsFromRhoTYe(Real rho, Real T, Real Ye) const override;
    Real cs2FromRhoTYe(Real rho, Real T, Real Ye) const override;
    Real muElectron(Real rho, Real T, Real Ye) const override;
    Real muNeutron(Real rho, Real T, Real Ye) const override;
    Real muProton(Real rho, Real T, Real Ye) const override;
    Real betaEquilibriumYe(Real rho, Real T) const override;
    bool isTabulated() const override { return true; }

    // -----------------------------------------------------------------------
    // Table metadata accessors
    // -----------------------------------------------------------------------

    Real rhoMin() const { return std::pow(10.0, log_rho_[0]); }
    Real rhoMax() const { return std::pow(10.0, log_rho_[nRho_ - 1]); }
    Real TMin() const { return std::pow(10.0, log_T_[0]); }
    Real TMax() const { return std::pow(10.0, log_T_[nTemp_ - 1]); }
    Real YeMin() const { return Ye_[0]; }
    Real YeMax() const { return Ye_[nYe_ - 1]; }
    int nRho() const { return nRho_; }
    int nTemp() const { return nTemp_; }
    int nYe() const { return nYe_; }
    const std::string& eosName() const { return eos_name_; }
    Real energyShift() const { return energy_shift_; }

    // -----------------------------------------------------------------------
    // Low-level interpolation (public for testing)
    // -----------------------------------------------------------------------

    /**
     * @brief Full tri-linear interpolation of one table quantity.
     *
     * Inputs are clamped to the table bounds. Returns the quantity in
     * geometric (c=1) units -- conversion is applied on table load.
     *
     * @param var   Which EOS quantity to return
     * @param rho   Rest-mass density [g/cc]
     * @param T     Temperature [MeV]
     * @param Ye    Electron fraction [-]
     */
    Real interpolate(EOSVar var, Real rho, Real T, Real Ye) const;

    /**
     * @brief Newton-Raphson temperature inversion: find T such that
     *        epsFromRhoTYe(rho, T, Ye) = eps_target.
     *
     * Algorithm:
     *   - Initial bracket: [T_min, T_max] from table extremes
     *   - NR step: T_{n+1} = T_n - (eps(T_n) - eps_target) / (deps/dT)(T_n)
     *     where deps/dT = (eps(T+dT) - eps(T-dT)) / (2*dT), dT = 1e-4*T
     *   - If NR step diverges (|delta_T| > T/2), fall back to bisection
     *   - Convergence: |eps(T) - eps_target| / (|eps_target| + eps_floor) < 1e-10
     *   - Maximum iterations: 60 (NR is quadratically convergent -> usually < 10)
     *
     * @param rho        Rest-mass density [g/cc]
     * @param eps_target Specific internal energy [c=1 units]
     * @param Ye         Electron fraction [-]
     * @return Temperature [MeV] or T_min/T_max if inversion fails (fallsafe)
     */
    Real invertTemperature(Real rho, Real eps_target, Real Ye) const;

private:
    // -----------------------------------------------------------------------
    // Private constructor -- use factory methods
    // -----------------------------------------------------------------------
    TabulatedEOS() = default;

    // -----------------------------------------------------------------------
    // Table grid axes
    // -----------------------------------------------------------------------
    int nRho_ = 0;
    int nTemp_ = 0;
    int nYe_ = 0;

    std::vector<Real> log_rho_; // log10(rho [g/cc]),  size nRho
    std::vector<Real> log_T_;   // log10(T [MeV]),     size nTemp
    std::vector<Real> Ye_;      // Y_e [-],            size nYe

    // Grid spacings (uniform in log/linear space)
    Real d_log_rho_ = 0.0;
    Real d_log_T_ = 0.0;
    Real d_Ye_ = 0.0;

    // -----------------------------------------------------------------------
    // Table data: [NUM_EOS_VARS] arrays of size (nRho * nTemp * nYe)
    // Index: flat = iYe + nYe * (iT + nTemp * iRho)
    // All values stored in c=1 geometric units after CGS conversion on load.
    // -----------------------------------------------------------------------
    std::vector<std::vector<Real>> data_; // data_[var][flat_idx]

    // Energy shift: eps_table = log10(eps_erg_per_g + energy_shift) [erg/g].
    // On load, we store eps in c=1 units: eps_c1 = eps_erg/c^2.
    Real energy_shift_ = 0.0;

    std::string eos_name_ = "unknown";

    // -----------------------------------------------------------------------
    // Private helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Compute flat index from (iRho, iT, iYe).
     * Convention: iYe varies fastest (row-major with Ye innermost).
     */
    inline int flatIdx(int iRho, int iT, int iYe) const {
        return iYe + nYe_ * (iT + nTemp_ * iRho);
    }

    /**
     * @brief Find table bracket and fractional position for log10(rho).
     * Returns i = table index such that log_rho_[i] <= log10(rho) < log_rho_[i+1].
     * Sets fx = fractional position in [0, 1].
     */
    void findRhoBracket(Real rho, int& i, Real& fx) const;
    void findTBracket(Real T, int& j, Real& fy) const;
    void findYeBracket(Real Ye, int& k, Real& fz) const;

    // Internal pressure/eps queries from (iRho, iT, iYe) node values
    Real nodeValue(EOSVar var, int iRho, int iT, int iYe) const;
};

} // namespace granite::grmhd
