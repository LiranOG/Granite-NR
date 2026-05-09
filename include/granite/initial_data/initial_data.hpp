/**
 * @file granite/initial_data/initial_data.hpp
 * @brief Initial data construction — multi-BH punctures and stellar matter.
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#pragma once

#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"

#include <vector>

namespace granite::initial_data {

// ===========================================================================
// Black hole descriptors
// ===========================================================================

/**
 * @struct BlackHoleParams
 * @brief Parameters describing a single black hole.
 */
struct BlackHoleParams {
    Real mass;                      ///< Bare mass parameter m
    std::array<Real, DIM> position; ///< Coordinate position
    std::array<Real, DIM> momentum; ///< Linear momentum P^i (Bowen-York)
    std::array<Real, DIM> spin;     ///< Angular momentum S^i (Bowen-York)
};

// ===========================================================================
// Star descriptors
// ===========================================================================

enum class StellarModel { POLYTROPE, TOV, MESA_TABLE };

/**
 * @struct StarParams
 * @brief Parameters describing a central star.
 */
struct StarParams {
    Real mass;                                        ///< Gravitational mass M_*
    Real radius;                                      ///< Stellar radius R_*
    std::array<Real, DIM> position = {0.0, 0.0, 0.0}; ///< Center position
    StellarModel model = StellarModel::POLYTROPE;
    Real polytropic_gamma = 4.0 / 3.0; ///< Γ for polytrope
    Real polytropic_K = 1.0;           ///< K for polytrope
    Real central_density = 1.0;        ///< ρ_c [g/cm³] (for TOV / polytrope)
    Real B_field_seed = 1.0e4;         ///< Seed B-field [G]
    Real temperature = 1.0e8;          ///< Initial temperature [K]
    Real Ye = 0.5;                     ///< Electron fraction
};

// ===========================================================================
// Initial data generators
// ===========================================================================

/**
 * @class InitialData
 * @brief Base interface for all initial data generators.
 */
class InitialData {
public:
    virtual ~InitialData() = default;
};

/**
 * @class BrillLindquist
 * @brief Analytic Brill-Lindquist initial data (N BHs, no momenta).
 *
 * Conformal factor: ψ = 1 + Σ_A m_A / (2|r - r_A|)
 * Extrinsic curvature: K_ij = 0
 *
 * This is the simplest multi-BH initial data; valid for BHs at rest
 * with no spin.
 */
class BrillLindquist {
public:
    explicit BrillLindquist(const std::vector<BlackHoleParams>& bhs);

    /// Set the conformal metric and lapse on the grid
    void apply(GridBlock& grid) const;

    /// Evaluate the conformal factor ψ at a point
    Real conformalFactor(Real x, Real y, Real z) const;

    /// Compute the ADM mass
    Real admMass() const;

private:
    std::vector<BlackHoleParams> bhs_;
};

/**
 * @class BowenYorkPuncture
 * @brief Bowen-York puncture data with momenta and spins.
 *
 * Solves the Hamiltonian constraint for the regular correction u(r):
 *   Δ̃u + β(r)(ψ_BL + u)^{-7} K̃^{ij} K̃_{ij} = 0
 *
 * Requires an elliptic solver (spectral or multigrid).
 */
class BowenYorkPuncture {
public:
    explicit BowenYorkPuncture(const std::vector<BlackHoleParams>& bhs);

    /// Solve the Hamiltonian constraint and set data on the grid
    void solve(GridBlock& grid) const;

    /// Set the Bowen-York extrinsic curvature from momenta and spins
    void setBowenYorkExtrinsicCurvature(GridBlock& grid) const;

    /// Compute the ADM mass and angular momentum
    Real admMass() const;
    std::array<Real, DIM> admAngularMomentum() const;

private:
    std::vector<BlackHoleParams> bhs_;
};

/**
 * @struct TwoPuncturesParams
 * @brief Parameters for the TwoPunctures spectral solver spectral ID.
 */
struct TwoPuncturesParams {
    std::array<Real, DIM> par_m_plus = {{0.5, 0.0, 0.0}};
    std::array<Real, DIM> par_m_minus = {{0.5, 0.0, 0.0}};
    std::array<Real, DIM> par_b = {{1.0, 0.0, 0.0}};
    std::array<Real, DIM> par_P_plus = {{0.0, 0.0, 0.0}};
    std::array<Real, DIM> par_P_minus = {{0.0, 0.0, 0.0}};
    std::array<Real, DIM> par_S_plus = {{0.0, 0.0, 0.0}};
    std::array<Real, DIM> par_S_minus = {{0.0, 0.0, 0.0}};

    int npoints_A = 30;
    int npoints_B = 30;
    int npoints_phi = 16;
};

/**
 * @class TwoPuncturesBBH
 * @brief Wrapper for resolving the Hamiltonian constraint using spectral TwoPunctures.
 */
class TwoPuncturesBBH : public InitialData {
public:
    explicit TwoPuncturesBBH(const TwoPuncturesParams& params);

    /// Generate TwoPunctures Initial Data and map it onto the grid
    void generate(GridBlock& grid) const;

private:
    TwoPuncturesParams params_;
};

/**
 * @class SuperposedKerrSchild
 * @brief Superposed Kerr-Schild initial data for spinning BHs.
 *
 * g_μν = η_μν + Σ_A H_A ℓ^A_μ ℓ^A_ν + δg^corr_μν
 */
class SuperposedKerrSchild {
public:
    explicit SuperposedKerrSchild(const std::vector<BlackHoleParams>& bhs);

    /// Set superposed Kerr-Schild metric on the grid
    void apply(GridBlock& grid) const;

    /// Compute Kerr-Schild scalar H for a single BH
    static Real kerrSchildH(Real mass, Real spin_a, Real x, Real y, Real z);

private:
    std::vector<BlackHoleParams> bhs_;
};

/**
 * @class StellarInitialData
 * @brief Constructs hydrostatic stellar profiles.
 */
class StellarInitialData {
public:
    explicit StellarInitialData(const std::vector<StarParams>& stars);

    /// Build stellar profiles and set hydro data on the grid
    void apply(GridBlock& spacetime_grid, GridBlock& hydro_grid) const;

    /// Solve the Lane-Emden / TOV equation for a single star
    struct StellarProfile {
        std::vector<Real> r;     ///< Radial coordinate
        std::vector<Real> rho;   ///< Density profile
        std::vector<Real> press; ///< Pressure profile
        std::vector<Real> eps;   ///< Internal energy profile
        std::vector<Real> mass;  ///< Enclosed mass m(r)
    };

    StellarProfile solvePolytrope(const StarParams& star) const;
    StellarProfile solveTOV(const StarParams& star) const;

    /// Add a seed magnetic field (dipolar) to a stellar profile
    void addMagneticField(GridBlock& hydro_grid, const StarParams& star) const;

private:
    std::vector<StarParams> stars_;
};

// ===========================================================================
// Convenience: set up the full N=5 benchmark scenario
// ===========================================================================

/**
 * @brief Create the benchmark configuration: 5 SMBHs + 2 stars.
 *
 * Places 5 equal-mass BHs at pentagon vertices (R₀ = 1 pc),
 * and 2 ultra-massive stars (M* = 4300 M☉) at the origin.
 *
 * @param Mbh     Black hole mass [M☉] (default 1e8)
 * @param Mstar   Star mass [M☉] (default 4300)
 * @param R0      Pentagon radius [pc] (default 1.0)
 * @param N       Number of BHs (default 5)
 */
void setupBenchmarkScenario(GridBlock& spacetime_grid,
                            GridBlock& hydro_grid,
                            Real Mbh = 1.0e8,
                            Real Mstar = 4300.0,
                            Real R0 = 1.0,
                            int N = 5);

} // namespace granite::initial_data
