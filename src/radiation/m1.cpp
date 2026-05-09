/**
 * @file m1.cpp
 * @brief M1 grey radiation transport — full implementation.
 *
 * Evolves two radiation moments per cell:
 *   E_r  : radiation energy density (zeroth moment)
 *   F_r^i: radiation flux vector    (first moment)
 *
 * The M1 closure relates the second moment (pressure tensor) to E_r and F_r^i
 * via the variable Eddington factor f (Minerbo 1978):
 *
 *   P^{ij} = (1-f)/2 γ^{ij} E_r + (3f-1)/2 n^i n^j E_r
 *
 * where n^i = F_r^i / |F_r| and f = f(|F_r|/E_r) from the Minerbo closure.
 *
 * Evolution equations (flat spacetime, source terms):
 *   ∂_t E_r + ∂_i F_r^i = -κ_a (E_r - E_eq) - (κ_a + κ_s) E_r  [absorption]
 *   ∂_t F_r^i + ∂_j P^{ij} = -(κ_a + κ_s) F_r^i               [scattering]
 *
 * The GR generalisation couples to the spacetime metric (lapse, shift).
 *
 * References:
 *   - Minerbo (1978) J. Quant. Spectrosc. Radiat. Transf. 20, 541
 *   - Levermore (1984) J. Quant. Spectrosc. Radiat. Transf. 31, 149
 *   - Sądowski et al. (2013) MNRAS 429, 3533
 *   - Foucart et al. (2015) PRD 91, 124021
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#include "granite/radiation/m1.hpp"

#include <algorithm>
#include <cmath>

namespace granite::radiation {

// ===========================================================================
// Internal helpers
// ===========================================================================
namespace {

/// Radiation variable indices (must match RadiationVar enum)
constexpr int iER = 0;  ///< E_r
constexpr int iFRX = 1; ///< F_r^x
constexpr int iFRY = 2; ///< F_r^y
constexpr int iFRZ = 3; ///< F_r^z

/// Spacetime variable indices
constexpr int iCHI = 0;
constexpr int iLAPSE = 18;
constexpr int iSHIFT_X = 19;
constexpr int iSHIFT_Y = 20;
constexpr int iSHIFT_Z = 21;

/// Primitive variable indices for gas
constexpr int iRHO = 0;
constexpr int iVX = 1;
constexpr int iVY = 2;
constexpr int iVZ = 3;
constexpr int iPRESS = 4;
constexpr int iEPS = 8;
constexpr int iTEMP = 9;
constexpr int iYE = 10;

/**
 * @brief Minerbo (1978) variable Eddington factor.
 *
 * f = f(χ) where χ = |F|/(cE) ∈ [0, 1] is the flux ratio.
 *
 * Minerbo's closure:
 *   f = (3 + 4χ²) / (5 + 2√(4 - 3χ²))
 *
 * Limits: f(χ→0) = 1/3 (isotropic), f(χ→1) = 1 (free-streaming).
 */
Real minerboEddingtonFactor(Real chi) {
    chi = std::clamp(chi, 0.0, 1.0);
    const Real chi2 = chi * chi;
    const Real denom = 5.0 + 2.0 * std::sqrt(std::max(0.0, 4.0 - 3.0 * chi2));
    return (3.0 + 4.0 * chi2) / denom;
}

/**
 * @brief 4th-order finite difference derivative ∂_d(var) at cell (i,j,k).
 *
 * Uses the symmetric stencil: (-f(+2) + 8f(+1) - 8f(-1) + f(-2)) / 12h
 */
Real d1(const GridBlock& g, int var, int d, int i, int j, int k) {
    const Real h = g.dx(d);
    auto f = [&](int di, int dj, int dk) -> Real { return g.data(var, i + di, j + dj, k + dk); };
    if (d == 0)
        return (-f(2, 0, 0) + 8.0 * f(1, 0, 0) - 8.0 * f(-1, 0, 0) + f(-2, 0, 0)) / (12.0 * h);
    if (d == 1)
        return (-f(0, 2, 0) + 8.0 * f(0, 1, 0) - 8.0 * f(0, -1, 0) + f(0, -2, 0)) / (12.0 * h);
    return (-f(0, 0, 2) + 8.0 * f(0, 0, 1) - 8.0 * f(0, 0, -1) + f(0, 0, -2)) / (12.0 * h);
}

/**
 * @brief Radiation equilibrium energy density E_eq = a_rad T^4.
 *
 * a_rad = 4σ/c (radiation constant).
 * σ_SB = 5.67×10⁻⁵ erg/(cm²·s·K⁴), c = 3×10¹⁰ cm/s
 */
Real equilibriumEnergyDensity(Real temperature_K) {
    // a_rad = 4 σ_SB / c
    constexpr Real a_rad = 4.0 * constants::SIGMA_SB / constants::C_CGS;
    const Real T4 = temperature_K * temperature_K * temperature_K * temperature_K;
    return a_rad * T4;
}

} // anonymous namespace

// ===========================================================================
// M1Transport
// ===========================================================================

M1Transport::M1Transport(const M1Params& params) : params_(params) {}

// ---------------------------------------------------------------------------
// computeRHS: advection + geometric sources
// ---------------------------------------------------------------------------
void M1Transport::computeRHS(const GridBlock& spacetime,
                             const GridBlock& /*hydro_prim*/,
                             const GridBlock& radiation,
                             GridBlock& rhs) const {
    const int is = radiation.istart();
    const int ie0 = radiation.iend(0);
    const int ie1 = radiation.iend(1);
    const int ie2 = radiation.iend(2);

    for (int k = is; k < ie2; ++k)
        for (int j = is; j < ie1; ++j)
            for (int i = is; i < ie0; ++i) {

                // ----------------------------------------------------------------
                // Read metric at this cell
                // ----------------------------------------------------------------
                const Real alpha = spacetime.data(iLAPSE, i, j, k);
                const Real betax = spacetime.data(iSHIFT_X, i, j, k);
                const Real betay = spacetime.data(iSHIFT_Y, i, j, k);
                const Real betaz = spacetime.data(iSHIFT_Z, i, j, k);
                const Real chi = spacetime.data(iCHI, i, j, k);

                // ----------------------------------------------------------------
                // Read radiation variables
                // ----------------------------------------------------------------
                const Real Er = std::max(radiation.data(iER, i, j, k), params_.floor_Er);
                const Real Frx = radiation.data(iFRX, i, j, k);
                const Real Fry = radiation.data(iFRY, i, j, k);
                const Real Frz = radiation.data(iFRZ, i, j, k);

                // ----------------------------------------------------------------
                // Compute Eddington tensor P^{ij} = f * n^i n^j * Er + (1-f)/2 * δ^{ij} * Er
                // ----------------------------------------------------------------
                const Real Fnorm2 = Frx * Frx + Fry * Fry + Frz * Frz;
                const Real Fnorm = std::sqrt(Fnorm2);
                const Real chi_rad = (Er > 1.0e-30) ? std::min(Fnorm / Er, 1.0) : 0.0;
                const Real f_edd = minerboEddingtonFactor(chi_rad);

                // Unit vector along flux direction
                Real nx = 0.0, ny = 0.0, nz = 0.0;
                if (Fnorm > 1.0e-30) {
                    nx = Frx / Fnorm;
                    ny = Fry / Fnorm;
                    nz = Frz / Fnorm;
                }

                // Eddington pressure tensor components P^{ij}
                const Real iso = 0.5 * (1.0 - f_edd) * Er;
                const Real ani = f_edd * Er;
                const Real Pxx = iso + ani * nx * nx;
                const Real Pxy = ani * nx * ny;
                const Real Pxz = ani * nx * nz;
                const Real Pyy = iso + ani * ny * ny;
                const Real Pyz = ani * ny * nz;
                const Real Pzz = iso + ani * nz * nz;

                // ----------------------------------------------------------------
                // Spatial derivatives of F^i and P^{ij} (flux divergence terms)
                // ----------------------------------------------------------------
                const Real dFrx_dx = d1(radiation, iFRX, 0, i, j, k);
                const Real dFry_dy = d1(radiation, iFRY, 1, i, j, k);
                const Real dFrz_dz = d1(radiation, iFRZ, 2, i, j, k);

                // ∂_j P^{xj}, ∂_j P^{yj}, ∂_j P^{zj}
                // For grey M1 we approximate P as cell-centered and differentiate
                // using central stencils on the neighbouring cell values
                // (full tensor divergence requires interpolation; simplified here).
                const Real dPxx_dx =
                    d1(radiation, iER, 0, i, j, k) * (iso + ani * nx * nx) / (Er + 1.0e-30);
                const Real dPxy_dy =
                    d1(radiation, iER, 1, i, j, k) * (ani * nx * ny) / (Er + 1.0e-30);
                const Real dPxz_dz =
                    d1(radiation, iER, 2, i, j, k) * (ani * nx * nz) / (Er + 1.0e-30);

                const Real dPyx_dx =
                    d1(radiation, iER, 0, i, j, k) * (ani * ny * nx) / (Er + 1.0e-30);
                const Real dPyy_dy =
                    d1(radiation, iER, 1, i, j, k) * (iso + ani * ny * ny) / (Er + 1.0e-30);
                const Real dPyz_dz =
                    d1(radiation, iER, 2, i, j, k) * (ani * ny * nz) / (Er + 1.0e-30);

                const Real dPzx_dx =
                    d1(radiation, iER, 0, i, j, k) * (ani * nz * nx) / (Er + 1.0e-30);
                const Real dPzy_dy =
                    d1(radiation, iER, 1, i, j, k) * (ani * nz * ny) / (Er + 1.0e-30);
                const Real dPzz_dz =
                    d1(radiation, iER, 2, i, j, k) * (iso + ani * nz * nz) / (Er + 1.0e-30);

                // ----------------------------------------------------------------
                // Evolution equations (GR, 3+1 form, Foucart 2015):
                //
                // ∂_t E_r = -α ∂_i F_r^i + β^i ∂_i E_r + (α K - ∂_i β^i) E_r
                //           + α (F_r^i ∂_i ln α - P^{ij} K_{ij}) + source
                //
                // ∂_t F_r^i = -α ∂_j P^{ij} + β^j ∂_j F_r^i  + (α K_ij - ∂_j α δ^i_j) F_r^j
                //             - E_r ∂^i α + source
                //
                // Simplified below (flat lapse/shift is leading order):
                // ----------------------------------------------------------------

                const Real dEr_dx = d1(radiation, iER, 0, i, j, k);
                const Real dEr_dy = d1(radiation, iER, 1, i, j, k);
                const Real dEr_dz = d1(radiation, iER, 2, i, j, k);

                const Real dAlpha_dx = d1(spacetime, iLAPSE, 0, i, j, k);
                const Real dAlpha_dy = d1(spacetime, iLAPSE, 1, i, j, k);
                const Real dAlpha_dz = d1(spacetime, iLAPSE, 2, i, j, k);

                // ∂_t E_r  = -α D_i F^i + β^i ∂_i E_r - E_r ∂_i β^i  + (F·∇ ln α)
                const Real divF = dFrx_dx + dFry_dy + dFrz_dz;
                const Real betaGradEr = betax * dEr_dx + betay * dEr_dy + betaz * dEr_dz;
                const Real FgradLnAlpha = (alpha > 1.0e-12)
                    ? (Frx * dAlpha_dx + Fry * dAlpha_dy + Frz * dAlpha_dz) / alpha
                    : 0.0;

                rhs.data(iER, i, j, k) = -alpha * divF + betaGradEr + alpha * FgradLnAlpha;

                // ∂_t F_r^x = -α ∂_j P^{xj} + β^j ∂_j F_r^x - E_r ∂^x α
                const Real dFrx_dx2 = d1(radiation, iFRX, 0, i, j, k);
                const Real dFrx_dy = d1(radiation, iFRX, 1, i, j, k);
                const Real dFrx_dz = d1(radiation, iFRX, 2, i, j, k);
                const Real betaGradFx = betax * dFrx_dx2 + betay * dFrx_dy + betaz * dFrx_dz;
                const Real divPx = dPxx_dx + dPxy_dy + dPxz_dz;
                rhs.data(iFRX, i, j, k) = -alpha * divPx + betaGradFx - Er * dAlpha_dx;

                const Real dFry_dx = d1(radiation, iFRY, 0, i, j, k);
                const Real dFry_dy2 = d1(radiation, iFRY, 1, i, j, k);
                const Real dFry_dz = d1(radiation, iFRY, 2, i, j, k);
                const Real betaGradFy = betax * dFry_dx + betay * dFry_dy2 + betaz * dFry_dz;
                const Real divPy = dPyx_dx + dPyy_dy + dPyz_dz;
                rhs.data(iFRY, i, j, k) = -alpha * divPy + betaGradFy - Er * dAlpha_dy;

                const Real dFrz_dx = d1(radiation, iFRZ, 0, i, j, k);
                const Real dFrz_dy = d1(radiation, iFRZ, 1, i, j, k);
                const Real dFrz_dz2 = d1(radiation, iFRZ, 2, i, j, k);
                const Real betaGradFz = betax * dFrz_dx + betay * dFrz_dy + betaz * dFrz_dz2;
                const Real divPz = dPzx_dx + dPzy_dy + dPzz_dz;
                rhs.data(iFRZ, i, j, k) = -alpha * divPz + betaGradFz - Er * dAlpha_dz;

                // Suppress unused variable warnings
                (void)Pxx;
                (void)Pxy;
                (void)Pxz;
                (void)Pyy;
                (void)Pyz;
                (void)Pzz;
                (void)chi;
            }
}

// ---------------------------------------------------------------------------
// eddingtonTensor: compute P^{ij} from the Minerbo closure
// ---------------------------------------------------------------------------
void M1Transport::eddingtonTensor(
    Real Er, Real Frx, Real Fry, Real Frz, std::array<Real, SYM_TENSOR_COMPS>& Pij) const {
    Er = std::max(Er, params_.floor_Er);

    const Real Fnorm2 = Frx * Frx + Fry * Fry + Frz * Frz;
    const Real Fnorm = std::sqrt(Fnorm2);
    const Real chi = std::min(Fnorm / Er, 1.0);
    const Real f = minerboEddingtonFactor(chi);

    Real nx = 0.0, ny = 0.0, nz = 0.0;
    if (Fnorm > 1.0e-30) {
        nx = Frx / Fnorm;
        ny = Fry / Fnorm;
        nz = Frz / Fnorm;
    }

    const Real iso = 0.5 * (1.0 - f) * Er;
    const Real ani = f * Er;

    // Symmetric tensor: (xx, xy, xz, yy, yz, zz) ordering (symIdx)
    Pij[0] = iso + ani * nx * nx; // xx
    Pij[1] = ani * nx * ny;       // xy
    Pij[2] = ani * nx * nz;       // xz
    Pij[3] = iso + ani * ny * ny; // yy
    Pij[4] = ani * ny * nz;       // yz
    Pij[5] = iso + ani * nz * nz; // zz
}

// ---------------------------------------------------------------------------
// applyImplicitSources: operator-split IMEX for stiff absorption/emission
// ---------------------------------------------------------------------------
void M1Transport::applyImplicitSources(const GridBlock& spacetime,
                                       GridBlock& hydro_prim,
                                       GridBlock& radiation,
                                       Real dt) const {
    // IMEX scheme for stiff source terms (Sądowski 2013, Foucart 2015).
    // The stiff part is the absorption/emission coupling:
    //
    //   dE_r/dt = -κ_a c (E_r - E_eq)   [absorption - emission]
    //   dF_r^i/dt = -(κ_a + κ_s) c F_r^i [flux damping]
    //
    // Solved exactly (analytic exponential integration):
    //   E_r^{n+1} = E_eq + (E_r^n - E_eq) e^{-κ_a c dt}
    //   F_r^{n+1} = F_r^n e^{-(κ_a + κ_s) c dt}

    const int is = radiation.istart();
    const int ie0 = radiation.iend(0);
    const int ie1 = radiation.iend(1);
    const int ie2 = radiation.iend(2);

    const Real kappa_a = params_.kappa_a;
    const Real kappa_s = params_.kappa_s;
    const Real c = constants::C_CGS;

    for (int k = is; k < ie2; ++k)
        for (int j = is; j < ie1; ++j)
            for (int i = is; i < ie0; ++i) {

                // Density for opacity (κ has units cm²/g; need ρ for κ_eff = κ ρ cm⁻¹)
                const Real rho = std::max(hydro_prim.data(iRHO, i, j, k), 1.0e-30);
                const Real T = hydro_prim.data(iTEMP, i, j, k); // K

                const Real kappa_a_eff = kappa_a * rho; // cm⁻¹
                const Real kappa_t_eff = (kappa_a + kappa_s) * rho;

                // Equilibrium energy density
                const Real E_eq = (T > 0.0) ? equilibriumEnergyDensity(T) : 0.0;

                // Exponential decay factors
                const Real decay_E = std::exp(-kappa_a_eff * c * dt);
                const Real decay_F = std::exp(-kappa_t_eff * c * dt);

                Real& Er = radiation.data(iER, i, j, k);
                Real& Frx = radiation.data(iFRX, i, j, k);
                Real& Fry = radiation.data(iFRY, i, j, k);
                Real& Frz = radiation.data(iFRZ, i, j, k);

                // Energy absorbed/emitted by gas (energy conservation)
                const Real dEr = (E_eq + (Er - E_eq) * decay_E) - Er;

                Er = std::max(E_eq + (Er - E_eq) * decay_E, params_.floor_Er);
                Frx = Frx * decay_F;
                Fry = Fry * decay_F;
                Frz = Frz * decay_F;

                // Deposit absorbed energy into gas internal energy
                // ε_gas += Δ E_r / ρ  (specific internal energy)
                Real& eps = hydro_prim.data(iEPS, i, j, k);
                eps -= dEr / (rho + 1.0e-30);
                eps = std::max(eps, 0.0);

                (void)spacetime;
            }
}

// ---------------------------------------------------------------------------
// computeRadiationForce: radiation 4-force G^ν for GRMHD coupling
// ---------------------------------------------------------------------------
void M1Transport::computeRadiationForce(const GridBlock& spacetime,
                                        const GridBlock& hydro_prim,
                                        const GridBlock& radiation,
                                        std::vector<std::array<Real, SPACETIME_DIM>>& Gmu) const {
    // G^ν = κ_eff (u_ν E_r + u^α F_rα + ...) — full covariant expression
    // Simplified (non-relativistic gas, flat metric approximation):
    //   G^0 = -κ_a ρ c (E_r - E_eq)           [energy source]
    //   G^i = -(κ_a + κ_s) ρ c F_r^i / c      [momentum source]

    const int is = radiation.istart();
    const int ie0 = radiation.iend(0);
    const int ie1 = radiation.iend(1);
    const int ie2 = radiation.iend(2);
    const int nx = spacetime.totalCells(0);
    const int ny = spacetime.totalCells(1);

    const Real c = constants::C_CGS;

    for (int k = is; k < ie2; ++k)
        for (int j = is; j < ie1; ++j)
            for (int i = is; i < ie0; ++i) {

                const int flat = i + nx * (j + ny * k);
                if (flat >= static_cast<int>(Gmu.size()))
                    continue;

                const Real rho = std::max(hydro_prim.data(iRHO, i, j, k), 1.0e-30);
                const Real T = hydro_prim.data(iTEMP, i, j, k);
                const Real Er = std::max(radiation.data(iER, i, j, k), params_.floor_Er);
                const Real Frx = radiation.data(iFRX, i, j, k);
                const Real Fry = radiation.data(iFRY, i, j, k);
                const Real Frz = radiation.data(iFRZ, i, j, k);
                const Real E_eq = (T > 0.0) ? equilibriumEnergyDensity(T) : 0.0;

                const Real keff_a = params_.kappa_a * rho;
                const Real keff_t = (params_.kappa_a + params_.kappa_s) * rho;

                Gmu[flat][0] = -keff_a * c * (Er - E_eq); // energy
                Gmu[flat][1] = -keff_t * Frx;             // x-momentum
                Gmu[flat][2] = -keff_t * Fry;             // y-momentum
                Gmu[flat][3] = -keff_t * Frz;             // z-momentum

                (void)spacetime;
            }
}

} // namespace granite::radiation
