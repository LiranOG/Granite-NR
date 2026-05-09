/**
 * @file granite/postprocess/postprocess.hpp
 * @brief Post-processing suite — GW extraction, EM diagnostics, recoil.
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#pragma once

#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"

#include <complex>
#include <map>
#include <vector>

namespace granite::postprocess {

// ===========================================================================
// Gravitational Wave Extraction
// ===========================================================================

struct GWExtractionParams {
    std::vector<Real> extraction_radii = {100.0, 200.0, 300.0, 500.0};
    int l_max = 4;     ///< Maximum ℓ for spherical harmonic decomposition
    int n_theta = 100; ///< Angular resolution on extraction sphere
    int n_phi = 200;
};

/**
 * @class Psi4Extractor
 * @brief Extracts Ψ₄ on coordinate spheres and decomposes into modes.
 */
class Psi4Extractor {
public:
    explicit Psi4Extractor(const GWExtractionParams& params);

    /// Extract Ψ₄ at all extraction radii for one time step
    void extract(const GridBlock& spacetime, Real time);

    /// Get the (ℓ,m) mode of Ψ₄ at a given extraction radius
    std::complex<Real> getMode(int l, int m, Real r_ext) const;

    /// Integrate Ψ₄ to get strain h₊ - ih×
    std::complex<Real> computeStrain(int l, int m, Real r_ext) const;

    /// Compute total radiated GW energy
    Real computeRadiatedEnergy(Real r_ext) const;

    /// Compute total radiated GW linear momentum (for recoil)
    std::array<Real, DIM> computeRadiatedMomentum(Real r_ext) const;

    /// Compute GW energy spectrum dE/df
    std::vector<std::pair<Real, Real>> computeEnergySpectrum(int l, int m, Real r_ext) const;

    /// Richardson extrapolation to r → ∞
    std::complex<Real> extrapolateToInfinity(int l, int m) const;

    /// Get the time series of a mode
    const std::vector<std::pair<Real, std::complex<Real>>>&
    getTimeSeries(int l, int m, Real r_ext) const;

private:
    GWExtractionParams params_;

    // Storage: time_series_[r_ext][(l,m)] = vector<(time, Ψ₄^{lm})>
    using ModeKey = std::pair<int, int>;
    std::map<Real, std::map<ModeKey, std::vector<std::pair<Real, std::complex<Real>>>>> data_;
};

// ===========================================================================
// Electromagnetic Diagnostics
// ===========================================================================

/**
 * @class EMDiagnostics
 * @brief Computes electromagnetic observables from GRMHD + radiation data.
 */
class EMDiagnostics {
public:
    /// Compute bolometric luminosity L = ∫ κ_a(aT⁴ - E_r) dV
    Real computeBolometricLuminosity(const GridBlock& spacetime,
                                     const GridBlock& hydro_prim,
                                     const GridBlock& radiation) const;

    /// Compute Blandford-Znajek jet power
    /// P_jet ≈ (Φ_BH / 4π)² (Ω_H r_+)² / c
    Real computeJetPower(Real bh_spin, Real bh_mass, Real magnetic_flux_horizon) const;

    /// Compute accretion rate Ṁ through a surface at radius r
    Real computeAccretionRate(const GridBlock& spacetime,
                              const GridBlock& hydro_prim,
                              Real radius) const;

    /// Compute Eddington luminosity
    static Real eddingtonLuminosity(Real mass_msun);
};

// ===========================================================================
// Recoil & Remnant Characterization
// ===========================================================================

/**
 * @class RemnantAnalyzer
 * @brief Computes final BH properties from simulation data.
 */
class RemnantAnalyzer {
public:
    /// Estimate recoil velocity from radiated GW momentum
    std::array<Real, DIM> computeRecoilVelocity(const Psi4Extractor& gw_extractor) const;

    /// Compute final mass from horizon area
    Real computeFinalMass(Real horizon_area) const;

    /// Compute final spin from isolated-horizon diagnostics
    Real computeFinalSpin(Real horizon_area,
                          Real equatorial_circumference,
                          Real polar_circumference) const;
};

} // namespace granite::postprocess
