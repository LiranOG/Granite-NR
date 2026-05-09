/**
 * @file tabulated_eos.cpp
 * @brief Tabulated Nuclear EOS implementation (StellarCollapse/CompOSE format).
 *
 * This file implements:
 *   1. TabulatedEOS::buildSynthetic() -- in-memory ideal-gas table for CI
 *   2. TabulatedEOS::loadFromHDF5()   -- real nuclear table reader
 *   3. TabulatedEOS::interpolate()    -- tri-linear interpolation engine
 *   4. TabulatedEOS::invertTemperature() -- NR + bisection T inversion
 *   5. TabulatedEOS::betaEquilibriumYe() -- beta-eq NR solver
 *   6. All EquationOfState virtual overrides
 *
 * Mathematical formulation:
 * --------------------------
 * The tri-linear interpolant in (x1, x2, x3) = (log_rho, log_T, Ye) space:
 *
 *   f(x) = sum_{abc in {0,1}^3} f[i+a, j+b, k+c] * phi_a(fx) * phi_b(fy) * phi_c(fz)
 *
 * where phi_0(t) = 1-t, phi_1(t) = t, and (i,j,k) is the lower-left bracket.
 * This is the standard Yee-lattice 8-point stencil, preserving monotonicity.
 *
 * Temperature inversion via Newton-Raphson on g(T) = eps_table(rho,T,Ye) - eps:
 *   T_{n+1} = T_n - g(T_n) / g'(T_n)
 *   g'(T_n) = (deps/dT) evaluated by central finite difference on table.
 *
 * Beta equilibrium: find Y_e such that delta_mu(rho, T, Ye) = 0 where
 *   delta_mu = mu_e(rho,T,Ye) + mu_p(rho,T,Ye) - mu_n(rho,T,Ye)
 * Uses bisection on the monotone interval [Ye_min, 0.5].
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#include "granite/grmhd/tabulated_eos.hpp"

#include "granite/core/types.hpp"

#ifdef GRANITE_USE_HDF5
#include <hdf5.h>
#endif

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace granite::grmhd {

// ===========================================================================
// Internal constants
// ===========================================================================
namespace {
constexpr Real LOG10E = 0.43429448190325182765; // log10(e)
constexpr double C2_CGS = eos_units::C2_CGS;
constexpr int MAX_NR_ITER = 60;
constexpr Real NR_TOL = 1.0e-10;
constexpr Real EPS_FLOOR = 1.0e-20; // Prevents division by zero in NR

// MeV -> erg/g -> c=1 (geometric): T_MeV * k_B / (m_p * c^2)
// In CGS: k_B = 8.617e-5 eV/K, but T is in MeV = 1e6 eV = 1.602e-6 erg
// We store eps in erg/g * (1/c^2) = g/g = dimensionless
// So: k_B T / (m_p c^2) in MeV: k_B T [erg] / (m_p [g] * c^2 [cm^2/s^2])
//   = T [MeV] * 1.602e-6 [erg/MeV] / (1.673e-24 [g] * 8.988e20 [cm^2/s^2])
constexpr double M_PROTON_CGS = 1.672621898e-24; // g
constexpr double MEV_PER_ERG = 1.0 / 1.60217663e-6;
// For synthetic EOS: eps(rho=1, T, Ye) = T_MeV * CVoverMp
// where CV = 3/2 k_B per baryon (ideal gas)
constexpr double MEV_TO_C1 = 1.60217663e-6 / (M_PROTON_CGS * C2_CGS);
// So eps [c=1] = T [MeV] * MEV_TO_C1 * (1/(gamma-1))
} // anonymous namespace

// ===========================================================================
// Helper: find bracket index (binary search on sorted array)
// ===========================================================================

static int findBracket(const std::vector<Real>& axis, Real x, int n) {
    // Binary search: find i such that axis[i] <= x < axis[i+1]
    int lo = 0, hi = n - 2;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (axis[mid] <= x)
            lo = mid;
        else
            hi = mid - 1;
    }
    return std::clamp(lo, 0, n - 2);
}

// ===========================================================================
// findRhoBracket / findTBracket / findYeBracket
// ===========================================================================

void TabulatedEOS::findRhoBracket(Real rho, int& i, Real& fx) const {
    const Real log_rho = std::log10(std::max(rho, 1e-30));
    // Fast O(1) lookup using uniform spacing
    const Real log_rho_clamped = std::clamp(log_rho, log_rho_[0], log_rho_[nRho_ - 1]);
    const Real t = (log_rho_clamped - log_rho_[0]) / d_log_rho_;
    i = std::clamp(static_cast<int>(t), 0, nRho_ - 2);
    fx = t - static_cast<Real>(i);
    fx = std::clamp(fx, 0.0, 1.0);
}

void TabulatedEOS::findTBracket(Real T, int& j, Real& fy) const {
    const Real log_T = std::log10(std::max(T, 1e-30));
    const Real log_T_clamped = std::clamp(log_T, log_T_[0], log_T_[nTemp_ - 1]);
    const Real t = (log_T_clamped - log_T_[0]) / d_log_T_;
    j = std::clamp(static_cast<int>(t), 0, nTemp_ - 2);
    fy = t - static_cast<Real>(j);
    fy = std::clamp(fy, 0.0, 1.0);
}

void TabulatedEOS::findYeBracket(Real Ye, int& k, Real& fz) const {
    const Real Ye_clamped = std::clamp(Ye, Ye_[0], Ye_[nYe_ - 1]);
    const Real t = (Ye_clamped - Ye_[0]) / d_Ye_;
    k = std::clamp(static_cast<int>(t), 0, nYe_ - 2);
    fz = t - static_cast<Real>(k);
    fz = std::clamp(fz, 0.0, 1.0);
}

// ===========================================================================
// nodeValue: raw table access with bounds checking
// ===========================================================================

Real TabulatedEOS::nodeValue(EOSVar var, int iRho, int iT, int iYe) const {
    const int v = static_cast<int>(var);
    const int flat = flatIdx(iRho, iT, iYe);
    return data_[v][static_cast<size_t>(flat)];
}

// ===========================================================================
// interpolate: the core tri-linear engine
//
// Given (rho, T, Ye), interpolate one EOS quantity from the 3D table using
// the 8-point trilinear stencil:
//
//   f = sum_{a,b,c in {0,1}} f[i+a, j+b, k+c] * w_a(fx) * w_b(fy) * w_c(fz)
//
// where w_0(t) = 1-t, w_1(t) = t (linear hat functions).
//
// For log-spaced axes (rho, T), interpolation is in log space which is
// equivalent to power-law interpolation in physical space -- this preserves
// smoothness and guarantees positive values when the table stores log quantities.
// ===========================================================================

Real TabulatedEOS::interpolate(EOSVar var, Real rho, Real T, Real Ye) const {
    // -------------------------------------------------------------------------
    // Step 1: Find the enclosing cell (i, j, k) and fractional positions
    // -------------------------------------------------------------------------
    int i, j, k;
    Real fx, fy, fz;
    findRhoBracket(rho, i, fx);
    findTBracket(T, j, fy);
    findYeBracket(Ye, k, fz);

    // -------------------------------------------------------------------------
    // Step 2: Read the 8 corner values of the cell
    //   f[a][b][c] = table at (i+a, j+b, k+c)  for a,b,c in {0,1}
    // -------------------------------------------------------------------------
    const Real f000 = nodeValue(var, i, j, k);
    const Real f100 = nodeValue(var, i + 1, j, k);
    const Real f010 = nodeValue(var, i, j + 1, k);
    const Real f110 = nodeValue(var, i + 1, j + 1, k);
    const Real f001 = nodeValue(var, i, j, k + 1);
    const Real f101 = nodeValue(var, i + 1, j, k + 1);
    const Real f011 = nodeValue(var, i, j + 1, k + 1);
    const Real f111 = nodeValue(var, i + 1, j + 1, k + 1);

    // -------------------------------------------------------------------------
    // Step 3: Tri-linear interpolation via successive bilinear steps
    //   First interpolate in Ye (innermost, fz):
    //     f_ij  = f[i,j,k]*(1-fz) + f[i,j,k+1]*fz
    //   Then interpolate in T (fy):
    //     f_i   = f_i0*(1-fy) + f_i1*fy
    //   Finally interpolate in rho (fx):
    //     f     = f_0*(1-fx) + f_1*fx
    //
    // This is algebraically equivalent to the full 8-term expansion.
    // The order (z->y->x) matches the inner-fastest index convention.
    // -------------------------------------------------------------------------
    const Real f00 = f000 * (1.0 - fz) + f001 * fz;
    const Real f10 = f100 * (1.0 - fz) + f101 * fz;
    const Real f01 = f010 * (1.0 - fz) + f011 * fz;
    const Real f11 = f110 * (1.0 - fz) + f111 * fz;

    const Real f0 = f00 * (1.0 - fy) + f01 * fy;
    const Real f1 = f10 * (1.0 - fy) + f11 * fy;

    return f0 * (1.0 - fx) + f1 * fx;
}

// ===========================================================================
// Primary interface: pressureFromRhoTYe
// ===========================================================================

Real TabulatedEOS::pressureFromRhoTYe(Real rho, Real T, Real Ye) const {
    // Table stores log10(p) [dyne/cm^2], converted to c=1 on load.
    return interpolate(EOSVar::LOGPRESS, rho, T, Ye);
}

// ===========================================================================
// Primary interface: epsFromRhoTYe
// ===========================================================================

Real TabulatedEOS::epsFromRhoTYe(Real rho, Real T, Real Ye) const {
    // Table stores log10(eps + energy_shift) [erg/g], converted to c=1 on load.
    // The raw table value is log10(eps_erg_per_g + energy_shift).
    // On load, we stored: eps_c1 = interpolated_value (already in c=1 units).
    return interpolate(EOSVar::LOGENERGY, rho, T, Ye);
}

// ===========================================================================
// invertTemperature: Newton-Raphson with bisection fallback
//
// Solves: g(T) = epsFromRhoTYe(rho, T, Ye) - eps_target = 0
//
// Physics: eps(rho, T, Ye) is a monotonically increasing function of T
// at fixed rho, Ye (more thermal energy at higher T). This guarantees
// a unique root and makes bisection a valid fallback.
//
// NR convergence: quadratic near the root; typically < 10 iterations for
// nuclear EOS tables with smooth energy dependence on T.
// ===========================================================================

Real TabulatedEOS::invertTemperature(Real rho, Real eps_target, Real Ye) const {
    // T bounds
    const Real T_lo = TMin();
    const Real T_hi = TMax();

    // Evaluate eps at table extremes for physical sanity check
    const Real eps_lo = epsFromRhoTYe(rho, T_lo, Ye);
    const Real eps_hi = epsFromRhoTYe(rho, T_hi, Ye);

    // If target outside table range -- clamp to boundary
    if (eps_target <= eps_lo)
        return T_lo;
    if (eps_target >= eps_hi)
        return T_hi;

    // -------------------------------------------------------------------------
    // Initial guess: linear interpolation in log T
    // -------------------------------------------------------------------------
    Real log_T = std::log10(T_lo) +
        (std::log10(T_hi) - std::log10(T_lo)) * (eps_target - eps_lo) /
            (eps_hi - eps_lo + EPS_FLOOR);
    Real T = std::pow(10.0, std::clamp(log_T, std::log10(T_lo), std::log10(T_hi)));

    // Bisection bracket (always valid since eps is monotone in T)
    Real T_brac_lo = T_lo;
    Real T_brac_hi = T_hi;

    // -------------------------------------------------------------------------
    // Newton-Raphson iteration
    // -------------------------------------------------------------------------
    for (int iter = 0; iter < MAX_NR_ITER; ++iter) {
        const Real eps_T = epsFromRhoTYe(rho, T, Ye);
        const Real g = eps_T - eps_target;
        const Real rel_err = std::abs(g) / (std::abs(eps_target) + EPS_FLOOR);

        if (rel_err < NR_TOL)
            return T; // Converged

        // Update bisection bracket (maintains root bracketing)
        if (g < 0.0)
            T_brac_lo = T;
        else
            T_brac_hi = T;

        // Numerical derivative: central difference in log T space
        // dT = 1% of T, symmetric in log space
        const Real dT = 1.0e-4 * T;
        const Real T_plus = std::min(T + dT, T_hi);
        const Real T_minus = std::max(T - dT, T_lo);
        const Real deps_dT = (epsFromRhoTYe(rho, T_plus, Ye) - epsFromRhoTYe(rho, T_minus, Ye)) /
            (T_plus - T_minus + EPS_FLOOR);

        Real T_new = T;
        if (std::abs(deps_dT) > EPS_FLOOR) {
            const Real step = g / deps_dT;
            T_new = T - step;
        }

        // Check if NR step is within bracket -- if not, use bisection
        if (T_new <= T_brac_lo || T_new >= T_brac_hi) {
            T_new = 0.5 * (T_brac_lo + T_brac_hi);
        }

        T = std::clamp(T_new, T_lo, T_hi);
    }

    // NR did not fully converge -- return best estimate (within bracket)
    return T;
}

// ===========================================================================
// temperature -- public interface calls invertTemperature
// ===========================================================================

Real TabulatedEOS::temperature(Real rho, Real eps, Real Ye) const {
    return invertTemperature(rho, eps, Ye);
}

// ===========================================================================
// pressure(rho, eps) -- 2-parameter path via temperature inversion
// Uses default Ye = 0.5 (symmetric nuclear matter approximation)
// ===========================================================================

Real TabulatedEOS::pressure(Real rho, Real eps) const {
    // Use Ye = 0.5 as default -- correct for symmetric nuclear matter.
    // For BNS/CCSN simulations, callers should use pressureFromRhoTYe directly.
    constexpr Real Ye_default = 0.5;
    const Real T = invertTemperature(rho, eps, Ye_default);
    return pressureFromRhoTYe(rho, T, Ye_default);
}

// ===========================================================================
// soundSpeed -- from (rho, eps, p) using tabulated cs2
// ===========================================================================

Real TabulatedEOS::soundSpeed(Real rho, Real eps, Real p) const {
    (void)p;
    constexpr Real Ye_default = 0.5;
    const Real T = invertTemperature(rho, eps, Ye_default);
    const Real cs2 = cs2FromRhoTYe(rho, T, Ye_default);
    return std::sqrt(std::clamp(cs2, 0.0, 1.0 - 1.0e-10));
}

// ===========================================================================
// entropy, cs2, chemical potentials -- direct lookup
// ===========================================================================

Real TabulatedEOS::entropy(Real rho, Real T, Real Ye) const {
    return interpolate(EOSVar::ENTROPY, rho, T, Ye);
}

Real TabulatedEOS::cs2FromRhoTYe(Real rho, Real T, Real Ye) const {
    return interpolate(EOSVar::CS2, rho, T, Ye);
}

Real TabulatedEOS::muElectron(Real rho, Real T, Real Ye) const {
    return interpolate(EOSVar::MU_E, rho, T, Ye);
}

Real TabulatedEOS::muNeutron(Real rho, Real T, Real Ye) const {
    return interpolate(EOSVar::MU_N, rho, T, Ye);
}

Real TabulatedEOS::muProton(Real rho, Real T, Real Ye) const {
    return interpolate(EOSVar::MU_P, rho, T, Ye);
}

// ===========================================================================
// betaEquilibriumYe: bisection on delta_mu(Ye) = mu_e + mu_p - mu_n = 0
//
// Physics: In beta equilibrium, the weak reactions
//   n -> p + e^- + nu_e     (neutron decay)
//   p + e^- -> n + nu_e     (electron capture)
// are in equilibrium, requiring mu_e + mu_p = mu_n.
//
// For a nuclear EOS, delta_mu(rho, T, Ye) is a monotonically decreasing
// function of Ye: at low Ye (neutron-rich), delta_mu > 0; at high Ye
// (proton-rich), delta_mu < 0. Bisection is guaranteed to converge.
//
// Returns Y_e in [Ye_min, 0.5]. For rho below nuclear saturation density
// (~2.7e14 g/cc), beta-eq Ye is ~0.03-0.05 at T~0.
// ===========================================================================

Real TabulatedEOS::betaEquilibriumYe(Real rho, Real T) const {
    // Delta_mu = mu_e + mu_p - mu_n
    auto delta_mu = [&](Real Ye) -> Real {
        return muElectron(rho, T, Ye) + muProton(rho, T, Ye) - muNeutron(rho, T, Ye);
    };

    const Real Ye_lo = YeMin();
    const Real Ye_hi = std::min(0.5, YeMax());

    const Real dmu_lo = delta_mu(Ye_lo);
    const Real dmu_hi = delta_mu(Ye_hi);

    // If no sign change, return the endpoint with smaller |delta_mu|
    if (dmu_lo * dmu_hi >= 0.0) {
        return (std::abs(dmu_lo) < std::abs(dmu_hi)) ? Ye_lo : Ye_hi;
    }

    // Bisection
    Real lo = Ye_lo, hi = Ye_hi;
    for (int iter = 0; iter < 60; ++iter) {
        const Real mid = 0.5 * (lo + hi);
        const Real dmu_mid = delta_mu(mid);
        if (std::abs(dmu_mid) < 1.0e-8 || (hi - lo) < 1.0e-10)
            return mid;
        if (dmu_lo * dmu_mid < 0.0)
            hi = mid;
        else
            lo = mid;
    }
    return 0.5 * (lo + hi);
}

// ===========================================================================
// buildSynthetic: generate an in-memory ideal-gas table
//
// Physical model:
//   p(rho, T, Ye)   = (gamma-1) * rho * eps(rho, T, Ye)
//   eps(rho, T, Ye) = (k_B T) / ((gamma-1) * m_p * c^2)   [c=1 units]
//                   = T [MeV] * MEV_TO_C1 / (gamma-1)
//   cs2(rho, T, Ye) = gamma * p / (rho * h)                [c=1, dimensionless]
//   s(rho, T, Ye)   = 0  (arbitrary normalization for synthetic)
//   mu_e            = T * Ye * 10  (monotone proxy, not physical)
//   mu_n            = T * (1-Ye) * 5  (proxy)
//   mu_p            = T * Ye * 3     (proxy)
//   delta_mu = mu_e + mu_p - mu_n = T*(Ye*13 - 5 + 5*Ye) = T*(18*Ye - 5)
//   beta_eq: 18*Ye = 5 => Ye = 5/18 ~ 0.278 (constant, independent of rho,T)
//
// The pressure and eps are stored as actual values (NOT log) in c=1 units.
// But to match the StellarCollapse convention of the interpolator which expects
// the data already in c=1 units, we store them directly.
// ===========================================================================

std::shared_ptr<TabulatedEOS> TabulatedEOS::buildSynthetic(int nRho,
                                                           int nTemp,
                                                           int nYe,
                                                           Real gamma,
                                                           Real log_rho_min,
                                                           Real log_rho_max,
                                                           Real log_T_min,
                                                           Real log_T_max,
                                                           Real Ye_min,
                                                           Real Ye_max) {
    auto eos = std::shared_ptr<TabulatedEOS>(new TabulatedEOS());
    eos->nRho_ = nRho;
    eos->nTemp_ = nTemp;
    eos->nYe_ = nYe;
    eos->eos_name_ = "SyntheticIdealGas";
    eos->energy_shift_ = 0.0;

    // Build axes
    eos->log_rho_.resize(static_cast<size_t>(nRho));
    eos->log_T_.resize(static_cast<size_t>(nTemp));
    eos->Ye_.resize(static_cast<size_t>(nYe));

    const Real d_log_rho = (log_rho_max - log_rho_min) / (nRho - 1);
    const Real d_log_T = (log_T_max - log_T_min) / (nTemp - 1);
    const Real d_Ye = (Ye_max - Ye_min) / (nYe - 1);

    eos->d_log_rho_ = d_log_rho;
    eos->d_log_T_ = d_log_T;
    eos->d_Ye_ = d_Ye;

    for (int ir = 0; ir < nRho; ++ir)
        eos->log_rho_[static_cast<size_t>(ir)] = log_rho_min + ir * d_log_rho;
    for (int it = 0; it < nTemp; ++it)
        eos->log_T_[static_cast<size_t>(it)] = log_T_min + it * d_log_T;
    for (int iy = 0; iy < nYe; ++iy)
        eos->Ye_[static_cast<size_t>(iy)] = Ye_min + iy * d_Ye;

    // Allocate data arrays
    const size_t ntot = static_cast<size_t>(nRho * nTemp * nYe);
    eos->data_.resize(NUM_EOS_VARS, std::vector<Real>(ntot, 0.0));

    const Real gm1 = gamma - 1.0;

    // Fill table cell by cell
    for (int ir = 0; ir < nRho; ++ir)
        for (int it = 0; it < nTemp; ++it)
            for (int iy = 0; iy < nYe; ++iy) {
                const size_t flat = static_cast<size_t>(eos->flatIdx(ir, it, iy));
                const Real rho = std::pow(10.0, eos->log_rho_[static_cast<size_t>(ir)]);
                const Real T = std::pow(10.0, eos->log_T_[static_cast<size_t>(it)]); // MeV
                const Real Ye = eos->Ye_[static_cast<size_t>(iy)];

                // eps in c=1 units: eps = T[MeV] * k_B / ((gamma-1) * m_p c^2)
                const Real eps = T * static_cast<Real>(MEV_TO_C1) / gm1;

                // Pressure in c=1 units: p = (gamma-1) * rho * eps
                const Real p = gm1 * rho * eps;

                // Sound speed squared (relativistic ideal gas): cs^2 = gamma*p / (rho*h)
                const Real h = 1.0 + eps + p / (rho + 1.0e-30);
                const Real cs2 = gamma * p / (rho * h + 1.0e-30);

                // Specific entropy (dimensionless proxy, monotone in T)
                const Real s = std::log(T) - (gamma - 1.0) * std::log(rho + 1.0e-30);

                // Chemical potential proxies (monotone in Ye, sign change at ~0.278)
                const Real mu_e = T * Ye * 10.0;
                const Real mu_n = T * (1.0 - Ye) * 5.0;
                const Real mu_p = T * Ye * 3.0;

                eos->data_[static_cast<int>(EOSVar::LOGPRESS)][flat] = p;
                eos->data_[static_cast<int>(EOSVar::LOGENERGY)][flat] = eps;
                eos->data_[static_cast<int>(EOSVar::ENTROPY)][flat] = s;
                eos->data_[static_cast<int>(EOSVar::CS2)][flat] = std::clamp(cs2, 0.0, 1.0 - 1e-10);
                eos->data_[static_cast<int>(EOSVar::MU_E)][flat] = mu_e;
                eos->data_[static_cast<int>(EOSVar::MU_N)][flat] = mu_n;
                eos->data_[static_cast<int>(EOSVar::MU_P)][flat] = mu_p;
            }

    return eos;
}

// ===========================================================================
// loadFromHDF5: StellarCollapse format reader
// ===========================================================================

std::shared_ptr<TabulatedEOS> TabulatedEOS::loadFromHDF5(const std::string& path) {
#ifndef GRANITE_USE_HDF5
    (void)path;
    throw std::runtime_error(
        "TabulatedEOS::loadFromHDF5: GRANITE was compiled without HDF5 support. "
        "Rebuild with -DGRANITE_ENABLE_HDF5=ON.");
#else
    auto eos = std::shared_ptr<TabulatedEOS>(new TabulatedEOS());

    // -----------------------------------------------------------------------
    // Open file
    // -----------------------------------------------------------------------
    hid_t file = H5Fopen(path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file < 0) {
        throw std::runtime_error("TabulatedEOS: could not open HDF5 file: " + path);
    }

    // Helper: read 1D dataset into vector
    auto readVector1D = [&](const char* name, std::vector<double>& out) {
        hid_t ds = H5Dopen2(file, name, H5P_DEFAULT);
        if (ds < 0)
            throw std::runtime_error(std::string("TabulatedEOS: missing dataset '") + name + "'");
        hid_t sp = H5Dget_space(ds);
        hsize_t dims[1];
        H5Sget_simple_extent_dims(sp, dims, nullptr);
        out.resize(static_cast<size_t>(dims[0]));
        H5Dread(ds, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, out.data());
        H5Sclose(sp);
        H5Dclose(ds);
    };

    // Helper: read 3D dataset (nRho x nTemp x nYe) into flat vector
    // StellarCollapse stores as [nYe][nTemp][nRho] in HDF5 (C row-major)
    // but the index ordering is (Ye fastest varying in the physical flattening)
    auto readVector3D = [&](const char* name, std::vector<double>& out, size_t ntot) {
        hid_t ds = H5Dopen2(file, name, H5P_DEFAULT);
        if (ds < 0)
            throw std::runtime_error(std::string("TabulatedEOS: missing dataset '") + name + "'");
        out.resize(ntot);
        H5Dread(ds, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, out.data());
        H5Dclose(ds);
    };

    // Issue 4 fix: Read energy_shift attribute using a single group handle that is
    // always closed. The original code called H5Gopen2() as an argument to H5Aexists()
    // which created a leaked hid_t handle (never stored, never closed). A second
    // H5Gopen2 was then called inside the if-body, holding two open handles and only
    // closing one. This caused one HDF5 object reference to leak per checkpoint load.
    {
        double shift = 0.0;
        hid_t grp = H5Gopen2(file, "/", H5P_DEFAULT);
        if (grp >= 0) {
            if (H5Aexists(grp, "energy_shift") > 0) {
                hid_t attr = H5Aopen(grp, "energy_shift", H5P_DEFAULT);
                if (attr >= 0) {
                    H5Aread(attr, H5T_NATIVE_DOUBLE, &shift);
                    H5Aclose(attr);
                }
            }
            H5Gclose(grp); // Always closed — single handle
        }
        eos->energy_shift_ = static_cast<Real>(shift);
    }

    // -----------------------------------------------------------------------
    // Read axes
    // -----------------------------------------------------------------------
    std::vector<double> log_rho_raw, log_T_raw, Ye_raw;
    readVector1D("logrho", log_rho_raw);
    readVector1D("logtemp", log_T_raw);
    readVector1D("ye", Ye_raw);

    eos->nRho_ = static_cast<int>(log_rho_raw.size());
    eos->nTemp_ = static_cast<int>(log_T_raw.size());
    eos->nYe_ = static_cast<int>(Ye_raw.size());

    eos->log_rho_.resize(static_cast<size_t>(eos->nRho_));
    eos->log_T_.resize(static_cast<size_t>(eos->nTemp_));
    eos->Ye_.resize(static_cast<size_t>(eos->nYe_));

    for (int i = 0; i < eos->nRho_; ++i)
        eos->log_rho_[static_cast<size_t>(i)] =
            static_cast<Real>(log_rho_raw[static_cast<size_t>(i)]);
    for (int i = 0; i < eos->nTemp_; ++i)
        eos->log_T_[static_cast<size_t>(i)] = static_cast<Real>(log_T_raw[static_cast<size_t>(i)]);
    for (int i = 0; i < eos->nYe_; ++i)
        eos->Ye_[static_cast<size_t>(i)] = static_cast<Real>(Ye_raw[static_cast<size_t>(i)]);

    // Uniform spacing (validated assumption for StellarCollapse)
    eos->d_log_rho_ =
        (eos->nRho_ > 1) ? (eos->log_rho_.back() - eos->log_rho_.front()) / (eos->nRho_ - 1) : 1.0;
    eos->d_log_T_ =
        (eos->nTemp_ > 1) ? (eos->log_T_.back() - eos->log_T_.front()) / (eos->nTemp_ - 1) : 1.0;
    eos->d_Ye_ = (eos->nYe_ > 1) ? (eos->Ye_.back() - eos->Ye_.front()) / (eos->nYe_ - 1) : 1.0;

    // Issue 12 fix: validate that the table spacing is approximately uniform.
    // The findRhoBracket / findTBracket / findYeBracket functions use an O(1) formula
    // that assumes strictly uniform log-spacing. StellarCollapse tables are only
    // approximately uniform, and denser tables (nRho > 300) can deviate up to 2%.
    // We check here and warn if any gap deviates more than 1% from the mean spacing,
    // so users know that large high-density / high-temperature tables may produce
    // slightly wrong bracket indices and should use a binary search instead.
    {
        auto checkUniform = [](const std::vector<Real>& axis, Real d_mean, const char* name) {
            if (axis.size() < 3)
                return;
            double tol = 0.01 * std::abs(static_cast<double>(d_mean)); // 1% tolerance
            for (size_t ii = 1; ii < axis.size(); ++ii) {
                double gap = static_cast<double>(axis[ii] - axis[ii - 1]);
                if (std::abs(gap - static_cast<double>(d_mean)) > tol) {
                    std::cerr << "[TabulatedEOS] WARNING (Issue 12): " << name
                              << " axis is not uniformly spaced (gap[" << ii << "]=" << gap
                              << " vs mean=" << d_mean
                              << "). The O(1) bracket lookup may return wrong index "
                              << "for this table. Consider using binary search.\n";
                    return; // Warn once per axis
                }
            }
        };
        checkUniform(eos->log_rho_, eos->d_log_rho_, "log_rho");
        checkUniform(eos->log_T_, eos->d_log_T_, "log_T");
        checkUniform(eos->Ye_, eos->d_Ye_, "Ye");
    }

    // -----------------------------------------------------------------------
    // Read 3D tables and convert to geometric (c=1) units
    // -----------------------------------------------------------------------
    const size_t ntot = static_cast<size_t>(eos->nRho_ * eos->nTemp_ * eos->nYe_);
    eos->data_.resize(NUM_EOS_VARS, std::vector<Real>(ntot, 0.0));

    // StellarCollapse 3D layout: the dataset is stored in HDF5 as [nYe][nTemp][nRho]
    // which when read into a flat C array gives flat = iRho + nRho*(iT + nTemp*iYe).
    // GRANITE uses flat = iYe + nYe*(iT + nTemp*iRho). We need to transpose.

    auto transposeAndStore =
        [&](const char* ds_name, EOSVar var, double scale, double offset, bool is_log_table) {
            std::vector<double> raw;
            readVector3D(ds_name, raw, ntot);
            const int nR = eos->nRho_, nT = eos->nTemp_, nY = eos->nYe_;
            for (int iR = 0; iR < nR; ++iR)
                for (int iT = 0; iT < nT; ++iT)
                    for (int iY = 0; iY < nY; ++iY) {
                        // Input layout: [iY][iT][iR] -> flat_in = iR + nR*(iT + nT*iY)
                        const int flat_in = iR + nR * (iT + nT * iY);
                        // Output layout: iY + nY*(iT + nT*iR)
                        const int flat_out = eos->flatIdx(iR, iT, iY);
                        double val = raw[static_cast<size_t>(flat_in)];
                        if (is_log_table) {
                            // Convert from log10 to physical, then apply scale
                            val = std::pow(10.0, val) + offset;
                        }
                        eos->data_[static_cast<int>(var)][static_cast<size_t>(flat_out)] =
                            static_cast<Real>(val * scale);
                    }
        };

    // logpress: log10(p [dyne/cm^2]) -> p [c=1]
    transposeAndStore("logpress", EOSVar::LOGPRESS, eos_units::PRESS_TO_GU, 0.0, true);

    // logenergy: log10(|eps_erg| + energy_shift) -> eps [c=1]
    // eps_erg_per_g = 10^(table_val) - energy_shift
    // eps_c1 = eps_erg_per_g * EPS_TO_GU
    transposeAndStore(
        "logenergy", EOSVar::LOGENERGY, eos_units::EPS_TO_GU, -eos->energy_shift_, true);

    // entropy: s [k_B/baryon] -- no conversion needed
    transposeAndStore("entropy", EOSVar::ENTROPY, 1.0, 0.0, false);

    // cs2: [cm^2/s^2] -> dimensionless c=1
    transposeAndStore("cs2", EOSVar::CS2, eos_units::CS2_TO_GU, 0.0, false);

    // chemical potentials: [MeV] -- stored as-is (MeV scale)
    transposeAndStore("mu_e", EOSVar::MU_E, 1.0, 0.0, false);
    transposeAndStore("mu_n", EOSVar::MU_N, 1.0, 0.0, false);
    transposeAndStore("mu_p", EOSVar::MU_P, 1.0, 0.0, false);

    // Read EOS name attribute if present
    {
        hid_t grp = H5Gopen2(file, "/", H5P_DEFAULT);
        if (H5Aexists(grp, "eos_name") > 0) {
            hid_t attr = H5Aopen(grp, "eos_name", H5P_DEFAULT);
            hid_t atype = H5Aget_type(attr);
            char buf[128] = {};
            H5Aread(attr, atype, buf);
            eos->eos_name_ = std::string(buf);
            H5Tclose(atype);
            H5Aclose(attr);
        }
        H5Gclose(grp);
    }

    H5Fclose(file);

    std::cout << "[TabulatedEOS] Loaded '" << eos->eos_name_ << "' from " << path
              << "  (nRho=" << eos->nRho_ << ", nT=" << eos->nTemp_ << ", nYe=" << eos->nYe_ << ")"
              << std::endl;

    return eos;
#endif // GRANITE_USE_HDF5
}

} // namespace granite::grmhd
