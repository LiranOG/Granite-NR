/**
 * @file granite/grmhd/grmhd.hpp
 * @brief GRMHD evolution module — public interface.
 *
 * General-relativistic magnetohydrodynamics in the Valencia formulation.
 * Provides flux computation, primitive recovery, and the full RHS for the
 * conserved GRMHD variables on a curved dynamical spacetime.
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#pragma once

#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"

#include <functional>

namespace granite::grmhd {

// ===========================================================================
// Equation of State interface
// ===========================================================================

/**
 * @class EquationOfState
 * @brief Abstract interface for the equation of state.
 *
 * Provides a unified interface for both simple analytic EOS (IdealGasEOS)
 * and full nuclear microphysics tables (TabulatedEOS). The 3-parameter form
 * (rho, T, Ye) is the canonical for nuclear EOS; the 2-parameter form
 * (rho, eps) is the fallback for analytic EOS where T is irrelevant.
 *
 * All energy units are in c=1 geometric units (erg/g expressed as c^-2).
 * Temperatures are in MeV. Chemical potentials are in MeV.
 */
class EquationOfState {
public:
    virtual ~EquationOfState() = default;

    // -----------------------------------------------------------------------
    // Primary 2-parameter interface (rho, eps) -- mandatory for all EOS
    // -----------------------------------------------------------------------

    /// Pressure p(rho, eps) [c=1 units]. For tabulated EOS, eps is inverted
    /// to T first via Newton-Raphson, then p(rho,T,Ye) is returned.
    virtual Real pressure(Real rho, Real eps) const = 0;

    /// Adiabatic sound speed cs = sqrt(dp/drho|_s) in units of c.
    /// Must satisfy 0 < cs < 1 (causality). For tabulated EOS, p is provided
    /// to avoid double inversion.
    virtual Real soundSpeed(Real rho, Real eps, Real p) const = 0;

    /// Specific enthalpy h = 1 + eps + p/rho (dimensionless, c=1)
    Real enthalpy(Real rho, Real eps, Real p) const { return 1.0 + eps + p / (rho + 1e-30); }

    // -----------------------------------------------------------------------
    // Extended 3-parameter interface (rho, T, Ye) -- tabulated EOS
    // Default implementations provide safe stubs for analytic EOS.
    // -----------------------------------------------------------------------

    /// Temperature T [MeV] given (rho, eps, Ye). For analytic EOS returns 0.
    /// For tabulated EOS performs Newton-Raphson inversion of eps_table(rho,T,Ye).
    virtual Real temperature(Real rho, Real eps, Real Ye) const {
        (void)rho;
        (void)eps;
        (void)Ye;
        return 0.0;
    }

    /// Specific entropy per baryon s [k_B/baryon] given (rho, T, Ye).
    /// For analytic ideal gas: s = (1/(gamma-1)) * ln(p/rho^gamma) + const.
    virtual Real entropy(Real rho, Real T, Real Ye) const {
        (void)rho;
        (void)T;
        (void)Ye;
        return 0.0;
    }

    /// Squared sound speed cs^2 from (rho, T, Ye). Avoids double inversion
    /// for tabulated EOS. Default: delegate to soundSpeed(rho, eps, p).
    virtual Real cs2FromRhoTYe(Real rho, Real T, Real Ye) const {
        const Real eps = epsFromRhoTYe(rho, T, Ye);
        const Real p = pressureFromRhoTYe(rho, T, Ye);
        const Real cs = soundSpeed(rho, eps, p);
        return cs * cs;
    }

    /// Pressure from (rho, T, Ye). Default: invert to eps and call pressure().
    virtual Real pressureFromRhoTYe(Real rho, Real T, Real Ye) const {
        (void)T;
        (void)Ye;
        // For analytic EOS, p = (gamma-1)*rho*eps = pressure(rho, eps)
        // This default works only if eps is known -- subclasses should override.
        return pressure(rho, 0.0); // placeholder, TabulatedEOS overrides
    }

    /// Specific internal energy from (rho, T, Ye). Default returns 0.
    virtual Real epsFromRhoTYe(Real rho, Real T, Real Ye) const {
        (void)rho;
        (void)T;
        (void)Ye;
        return 0.0;
    }

    /// Electron chemical potential mu_e [MeV] from (rho, T, Ye).
    virtual Real muElectron(Real rho, Real T, Real Ye) const {
        (void)rho;
        (void)T;
        (void)Ye;
        return 0.0;
    }

    /// Neutron chemical potential mu_n [MeV] from (rho, T, Ye).
    virtual Real muNeutron(Real rho, Real T, Real Ye) const {
        (void)rho;
        (void)T;
        (void)Ye;
        return 0.0;
    }

    /// Proton chemical potential mu_p [MeV] from (rho, T, Ye).
    virtual Real muProton(Real rho, Real T, Real Ye) const {
        (void)rho;
        (void)T;
        (void)Ye;
        return 0.0;
    }

    /// Beta-equilibrium electron fraction Y_e such that mu_e + mu_p - mu_n = 0.
    /// Default: 0.5 (symmetric nuclear matter approximation).
    virtual Real betaEquilibriumYe(Real rho, Real T) const {
        (void)rho;
        (void)T;
        return 0.5;
    }

    /// Whether this EOS uses T and Ye as independent variables.
    /// Drives the con2prim pathway selection.
    virtual bool isTabulated() const { return false; }

    /// Adiabatic index for atmosphere fallback.
    virtual Real gamma() const { return 5.0 / 3.0; }
};

/**
 * @class IdealGasEOS
 * @brief Ideal gas (polytropic) EOS: p = (gamma-1)*rho*eps.
 *
 * Sound speed: cs^2 = gamma*p / (rho*h)  where h = 1 + eps + p/rho.
 * This is the relativistic ideal gas. isTabulated() = false so the full
 * 3-parameter tabulated pathway is bypassed.
 */
class IdealGasEOS : public EquationOfState {
public:
    explicit IdealGasEOS(Real gamma = 5.0 / 3.0) : gamma_(gamma) {}

    Real pressure(Real rho, Real eps) const override { return (gamma_ - 1.0) * rho * eps; }

    Real soundSpeed(Real rho, Real eps, Real p) const override {
        Real h = enthalpy(rho, eps, p);
        return std::sqrt(gamma_ * p / (rho * h + 1e-30));
    }

    Real pressureFromRhoTYe(Real rho, Real T, Real Ye) const override {
        // For ideal gas: eps = (3/2) * T_cgs / (mu * m_p * c^2)
        // Here T is never physically meaningful -- this path should not be called.
        (void)T;
        (void)Ye;
        return pressure(rho, 0.0);
    }

    Real gamma() const { return gamma_; }

private:
    Real gamma_;
};

// ===========================================================================
// GR Metric at a cell face — public interface between spacetime and matter
// ===========================================================================

/**
 * @struct GRMetric3
 * @brief 3+1 metric quantities at a cell face (interface between CCZ4 and GRMHD).
 *
 * Contains the physical (not conformal) spatial metric gamma_ij, its inverse,
 * the square root of its determinant, the lapse alpha, shift beta^i, and the
 * trace of extrinsic curvature K. These are derived from the CCZ4 conformal
 * variables (chi, gamma_tilde_ij, K, alpha, beta^i) by readMetric().
 *
 * Phase 4D: Promoted from the anonymous-namespace internal 'Metric3' to a
 * public struct to enable the GR-aware HLLE solver interface (fix for audit
 * bug C3) and direct construction in unit tests (GRMHDGRTest suite).
 */
struct GRMetric3 {
    Real gxx, gxy, gxz, gyy, gyz, gzz; ///< Physical lower metric gamma_ij = gamma_tilde_ij / chi
    Real igxx, igxy, igxz, igyy, igyz, igzz; ///< Inverse physical metric gamma^{ij}
    Real sqrtg;                              ///< sqrt(det(gamma_ij))
    Real alpha;                              ///< Lapse alpha
    Real betax, betay, betaz;                ///< Shift beta^i
    Real K;                                  ///< Trace of extrinsic curvature K = gamma^{ij} K_{ij}

    /// Construct a flat (Minkowski) metric in Cartesian coordinates.
    /// Useful for special-relativistic tests and as a unit-test baseline.
    /// In flat spacetime: alpha=1, beta=0, gamma_ij=delta_ij, sqrtg=1.
    static GRMetric3 flat() {
        GRMetric3 g;
        g.gxx = g.gyy = g.gzz = 1.0;
        g.gxy = g.gxz = g.gyz = 0.0;
        g.igxx = g.igyy = g.igzz = 1.0;
        g.igxy = g.igxz = g.igyz = 0.0;
        g.sqrtg = 1.0;
        g.alpha = 1.0;
        g.betax = g.betay = g.betaz = 0.0;
        g.K = 0.0;
        return g;
    }
};

// ===========================================================================
// Riemann Solver selection
// ===========================================================================

enum class RiemannSolver { HLLE, HLLD, LLF };
enum class Reconstruction { MP5, PPM, WENO5, PLM };

// ===========================================================================
// GRMHD Parameters
// ===========================================================================

struct GRMHDParams {
    RiemannSolver riemann_solver = RiemannSolver::HLLE;
    Reconstruction reconstruction = Reconstruction::MP5;
    Real atm_density = 1.0e-12;            ///< Atmosphere density floor
    Real atm_pressure = 1.0e-16;           ///< Atmosphere pressure floor
    Real lorentz_max = 100.0;              ///< Maximum Lorentz factor
    int con2prim_maxiter = 30;             ///< Max iterations for conservative-to-primitive
    Real con2prim_tol = 1.0e-10;           ///< Convergence tolerance
    bool use_constrained_transport = true; ///< Enforce ∇·B = 0 via CT
    Real excision_chi_thresh = 1.0e-4;     ///< Threshold for puncture excision mask
    Real sigma_max = 100.0;                ///< Maximum magnetization (b^2 / 2rho)
};

// ===========================================================================
// GRMHD Evolution class
// ===========================================================================

/**
 * @class GRMHDEvolution
 * @brief Computes the RHS of the GRMHD conservation equations.
 *
 * The conserved variables (D, Sⱼ, τ, Bⁱ) are evolved using finite-volume
 * HRSC methods with the metric provided by the spacetime module.
 */
class GRMHDEvolution {
public:
    GRMHDEvolution(const GRMHDParams& params, std::shared_ptr<EquationOfState> eos);
    ~GRMHDEvolution() = default;

    /**
     * @brief Compute the RHS for the conserved GRMHD variables.
     *
     * @param spacetime_grid  Grid with spacetime metric (22 vars)
     * @param hydro_grid      Grid with conserved hydro variables (8 vars)
     * @param prim_grid       Grid with primitive variables (11 vars)
     * @param rhs             Output: ∂_t U for each conserved variable
     */
    void computeRHS(const GridBlock& spacetime_grid,
                    const GridBlock& hydro_grid,
                    const GridBlock& prim_grid,
                    GridBlock& rhs) const;

    /**
     * @brief Recover primitive variables from conserved variables.
     *
     * Uses the Palenzuela et al. (2015) algorithm with failsafe atmosphere.
     *
     * @param spacetime_grid  Grid with spacetime metric
     * @param hydro_grid      Grid with conserved variables (input)
     * @param prim_grid       Grid with primitive variables (output)
     * @return Number of failed cells (where atmosphere was applied)
     */
    int conservedToPrimitive(const GridBlock& spacetime_grid,
                             const GridBlock& hydro_grid,
                             GridBlock& prim_grid) const;

    /**
     * @brief Compute the GR-aware HLLE numerical flux at a cell interface.
     *
     * Phase 4D fix for audit bug C3: the metric 'g' is now the actual curved-
     * spacetime metric at the interface (arithmetic average of the two adjacent
     * cell-centre metrics), replacing the previous hard-coded flat dummy.
     *
     * Flux scaling:  F -> alpha * sqrt(gamma) * F  (Valencia formulation)
     * Wavespeeds:    lambda_pm = alpha*(v_d +/- c_f)/(1 +/- v_d*c_f) - beta_d
     *
     * For flat spacetime (alpha=1, beta=0, sqrtg=1) the result is identical
     * to the previous implementation, preserving all existing test results.
     *
     * @param uL   Conserved variables, left state
     * @param uR   Conserved variables, right state
     * @param pL   Primitive variables, left state
     * @param pR   Primitive variables, right state
     * @param g    Spacetime metric at the cell interface (GRMetric3)
     * @param dir  Sweep direction (0=x, 1=y, 2=z)
     * @param flux Output HLLE flux vector
     */
    void computeHLLEFlux(const std::array<Real, NUM_HYDRO_VARS>& uL,
                         const std::array<Real, NUM_HYDRO_VARS>& uR,
                         const std::array<Real, NUM_PRIMITIVE_VARS>& pL,
                         const std::array<Real, NUM_PRIMITIVE_VARS>& pR,
                         const GRMetric3& g,
                         int dir,
                         std::array<Real, NUM_HYDRO_VARS>& flux) const;

    /**
     * @brief Compute HLLD numerical flux at a cell interface.
     *
     * Full Miyoshi & Kusano (2005) solver resolving all 7 MHD wave families.
     */
    void computeHLLDFlux(const std::array<Real, NUM_HYDRO_VARS>& uL,
                         const std::array<Real, NUM_HYDRO_VARS>& uR,
                         const std::array<Real, NUM_PRIMITIVE_VARS>& pL,
                         const std::array<Real, NUM_PRIMITIVE_VARS>& pR,
                         Real Bn,
                         int dir,
                         std::array<Real, NUM_HYDRO_VARS>& flux) const;

    /**
     * @brief Compute matter source terms for the spacetime (energy-momentum).
     */
    void computeMatterSources(const GridBlock& spacetime_grid,
                              const GridBlock& prim_grid,
                              std::vector<Real>& rho,
                              std::vector<std::array<Real, DIM>>& Si,
                              std::vector<std::array<Real, SYM_TENSOR_COMPS>>& Sij,
                              std::vector<Real>& S_trace) const;

    /**
     * @brief Constrained Transport update for divergence-free magnetic field.
     *
     * Uses the upwind CT algorithm (Gardiner & Stone 2005) to compute
     * edge-centered EMFs from face-centered HLLD fluxes and update B^i
     * in a manner that preserves ∇·B = 0 to machine precision.
     *
     * @param spacetime_grid Metric data
     * @param prim_grid      Primitive variables
     * @param hydro_grid     Conserved variables (B^i updated in place)
     * @param dt             Time step size
     */
    void computeCTUpdate(const GridBlock& spacetime_grid,
                         const GridBlock& prim_grid,
                         GridBlock& hydro_grid,
                         Real dt) const;

    /**
     * @brief Compute max |∇·B| over the grid (diagnostic).
     *
     * Returns machine-zero (~1e-15) if CT is working correctly.
     */
    static Real divergenceB(const GridBlock& hydro_grid);

    /// Access internal parameters
    const GRMHDParams& params() const { return params_; }
    const EquationOfState& eos() const { return *eos_; }

private:
    GRMHDParams params_;
    std::shared_ptr<EquationOfState> eos_;
};

} // namespace granite::grmhd
