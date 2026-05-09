/**
 * @file granite/spacetime/ccz4.hpp
 * @brief CCZ4 spacetime evolution — public interface.
 *
 * Provides the right-hand side (RHS) evaluation for the CCZ4 formulation
 * of the Einstein field equations with constraint damping, plus the
 * 1+log lapse and Γ-driver shift gauge conditions.
 *
 * Reference: Alic et al., PRD 85, 064040 (2012)
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#pragma once

#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"

namespace granite::spacetime {

/**
 * @struct CCZ4Params
 * @brief Parameters controlling the CCZ4 evolution.
 */
struct CCZ4Params {
    Real kappa1 = 0.02;              ///< Constraint damping parameter κ₁
    Real kappa2 = 0.0;               ///< Constraint damping parameter κ₂
    Real eta = 2.0;                  ///< Γ-driver shift damping η (typically 1/M_ADM)
    Real ko_sigma = 0.1;             ///< Kreiss-Oliger dissipation coefficient σ ∈ [0,1)
    int fd_order = 4;                ///< Finite-difference order (2, 4, 6, or 8)
    bool use_moving_puncture = true; ///< Use χ = e^{-4φ} instead of φ
    Real chi_blend_center = 0.05;    ///< Center of the sigmoid blending chi threshold
    Real chi_blend_width = 0.02;     ///< Width of the sigmoid blending
};

/**
 * @class CCZ4Evolution
 * @brief Computes the RHS of the CCZ4 evolution equations.
 *
 * The evolved variables are stored in a GridBlock using the SpacetimeVar
 * enumeration: {χ, γ̃ᵢⱼ(6), Ãᵢⱼ(6), K, Γ̃ⁱ(3), Θ, α, βⁱ(3)} = 22 total.
 *
 * Usage:
 * @code
 *   CCZ4Evolution ccz4(params);
 *   ccz4.computeRHS(grid, rhs_grid, matter_source);
 * @endcode
 */
class CCZ4Evolution {
public:
    explicit CCZ4Evolution(const CCZ4Params& params);
    ~CCZ4Evolution() = default;

    /**
     * @brief Compute the RHS of the CCZ4 evolution system.
     *
     * Evaluates ∂_t u = L(u) for all 22 spacetime variables at every
     * interior cell of the grid block. Ghost zones must be filled before
     * calling this function.
     *
     * @param grid         Input grid block with current spacetime data
     * @param rhs          Output grid block; on return, rhs.data(var,i,j,k)
     *                     contains ∂_t u_var at cell (i,j,k)
     * @param rho_matter   Energy density source ρ(i,j,k)
     * @param Si_matter    Momentum density source Sᵢ(i,j,k) [3 components]
     * @param Sij_matter   Stress source Sᵢⱼ(i,j,k) [6 components, symmetric]
     * @param S_trace      Trace of stress S(i,j,k)
     */
    void computeRHS(const GridBlock& grid,
                    GridBlock& rhs,
                    const std::vector<Real>& rho_matter,
                    const std::vector<std::array<Real, DIM>>& Si_matter,
                    const std::vector<std::array<Real, SYM_TENSOR_COMPS>>& Sij_matter,
                    const std::vector<Real>& S_trace) const;

    /**
     * @brief Compute the RHS for vacuum evolution (no matter sources).
     *
     * Convenience overload that sets all matter sources to zero.
     */
    void computeRHSVacuum(const GridBlock& grid, GridBlock& rhs) const;

    /**
     * @brief Compute constraint violations: Hamiltonian H and momentum Mⁱ.
     *
     * @param grid         Grid block with spacetime data
     * @param ham          Output: Hamiltonian constraint H(i,j,k)
     * @param mom          Output: Momentum constraints Mⁱ(i,j,k)
     */
    void computeConstraints(const GridBlock& grid,
                            std::vector<Real>& ham,
                            std::vector<std::array<Real, DIM>>& mom) const;

    /**
     * @brief Apply Kreiss-Oliger dissipation to the RHS.
     *
     * Fifth-order dissipation operator: D_KO = -σ·h⁵/64 · Σ_d ∂_d⁶
     */
    void applyDissipation(const GridBlock& grid, GridBlock& rhs) const;

    /// Access internal parameters
    const CCZ4Params& params() const { return params_; }

private:
    CCZ4Params params_;

    // Internal helpers for FD stencils
    Real d1(const GridBlock& grid, int var, int d, int i, int j, int k) const;
    Real d1up(const GridBlock& grid, int var, int d, Real beta_d, int i, int j, int k) const;
    Real d2(const GridBlock& grid, int var, int d1, int d2, int i, int j, int k) const;
};

// ===========================================================================
// Standalone utility functions
// ===========================================================================

/**
 * @brief Set flat Minkowski initial data on a grid block.
 *
 * Sets γ̃ᵢⱼ = δᵢⱼ, χ = 1, α = 1, and all other variables to zero.
 */
void setFlatSpacetime(GridBlock& grid);

/**
 * @brief Set gauge-wave initial data (Apples-with-Apples test).
 *
 * Sinusoidal perturbation propagating in the x-direction:
 *   ds² = -(1+A sin(2πx/L)) dt² + (1+A sin(2πx/L)) dx² + dy² + dz²
 *
 * @param grid      Grid block to initialize
 * @param amplitude Perturbation amplitude A (typically 0.01–0.1)
 * @param wavelength Wavelength L
 */
void setGaugeWaveData(GridBlock& grid, Real amplitude, Real wavelength);

} // namespace granite::spacetime
