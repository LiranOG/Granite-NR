/**
 * @file neutrino.cpp
 * @brief Neutrino transport — leakage + M1 hybrid scheme.
 *
 * Implements the leakage approximation for three neutrino species:
 *   ν_e  (electron neutrino)
 *   ν̄_e (electron antineutrino)
 *   ν_x  (heavy-lepton neutrinos: μ, τ combined)
 *
 * === Leakage Scheme ===
 * Production rates from nuclear reactions:
 *   - Pair capture (β processes): e⁻ + p → n + ν_e  (URCA)
 *   - Pair annihilation:          e⁺ + e⁻ → ν + ν̄
 *   - Plasmon decay:              γ_pl → ν + ν̄
 *
 * Diffusion time-scale and optical depth:
 *   τ(ν_e) = ∫ κ_ν ρ dr  (along radial ray)
 *
 * Effective cooling rate (Ruffert et al. 1996):
 *   Q_cool = Q_free * min(1, 1/τ)
 *   Q_heat = Q_cool * (1 - e^{-τ}) / τ  (re-absorbed fraction)
 *
 * === Electron Fraction ===
 * dY_e/dt computed from net β-process rates.
 *
 * References:
 *   - Ruffert, Janka, Schäfer (1996) A&A 311, 532
 *   - Rosswog & Liebendörfer (2003) MNRAS 342, 673
 *   - Perego et al. (2016) ApJS 223, 22
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#include "granite/neutrino/neutrino.hpp"

#include <algorithm>
#include <cmath>

namespace granite::neutrino {

// ===========================================================================
// Internal helpers
// ===========================================================================
namespace {

/// Primitive variable indices
constexpr int iRHO = 0;
constexpr int iVX = 1;
constexpr int iVY = 2;
constexpr int iVZ = 3;
constexpr int iPRESS = 4;
constexpr int iEPS = 8;
constexpr int iTEMP = 9;
constexpr int iYE = 10;

/// Spacetime variable
constexpr int iCHI = 0;

// ---------------------------------------------------------------------------
// Physical constants for neutrino reactions
// ---------------------------------------------------------------------------
constexpr Real G_F = 1.1664e-5; ///< Fermi coupling [GeV^{-2}] — in CGS via MeV
constexpr Real M_NBAR =
    939.565 * 1.60218e-6; ///< Nucleon mass energy [erg] = m_n c² (939.565 MeV × MeV_to_erg)

/// 1 MeV in erg
constexpr Real MEV_TO_ERG = 1.60218e-6;

/// Mean neutrino energy at temperature T
/// ε_ν ≈ 3.15 k_B T  (Fermi-Dirac average for μ=0)
Real meanNeutrinoEnergy(Real T_K) {
    return 3.15 * constants::K_BOLTZ * T_K;
}

/**
 * @brief URCA electron-capture rate (per unit volume).
 *
 * Q_e-cap = (G_F cos θ_C)²/(π) * g_V² * m_n^4 c^8 * ρ Y_p * ε_ν^5 / (ℏ c)^6
 *
 * Simplified analytic approximation (Ruffert et al. 1996, eq. A.5):
 *   Q_URCA ≈ 9.2×10²⁰ * ρ_{10} * X_p * (T_MeV)^6 / (1 + exp(-(μ_e-Q)/T))
 *           [erg / cm³ / s]
 *
 * where ρ_{10} = ρ / 10^{10} g/cm³, X_p = proton fraction,
 * T_MeV = k_B T / MeV, Q = mn - mp = 1.29 MeV.
 */
Real urcaCaptureRate(Real rho, Real Ye, Real T_K) {
    const Real rho10 = rho / 1.0e10;
    const Real Xp = Ye; // proton fraction ≈ Y_e
    const Real T_MeV = constants::K_BOLTZ * T_K / MEV_TO_ERG;
    if (T_MeV < 1.0e-6)
        return 0.0;
    const Real T6 = T_MeV * T_MeV * T_MeV * T_MeV * T_MeV * T_MeV;

    // Ruffert approximation: Q ≈ 9.2e20 ρ₁₀ Xp T_MeV^6 [erg/cm³/s]
    return 9.2e20 * rho10 * Xp * T6;
}

/**
 * @brief Pair-process emission rate (e⁺e⁻ → νν̄).
 *
 * Approximation (Itoh et al. 1989):
 *   Q_pair ≈ 9.6×10²¹ * T_MeV^9  [erg/cm³/s]  (for η_e ≈ 0)
 */
Real pairEmissionRate(Real T_K) {
    const Real T_MeV = constants::K_BOLTZ * T_K / MEV_TO_ERG;
    if (T_MeV < 1.0e-6)
        return 0.0;
    const Real T9 = std::pow(T_MeV, 9.0);
    return 9.6e21 * T9;
}

/**
 * @brief Neutrino absorption opacity κ_ν (cm²/g) per species.
 *
 * Dominant process: ν_e + n → p + e⁻
 * κ_abs ≈ 3.6×10⁻¹⁶ (ε_ν / MeV)² cm² / nucleon = κ₀ ε_ν² [cm²/g × MeV⁻²]
 *
 * With κ₀ ≈ 0.06 cm²/g/MeV² (Rosswog & Liebendörfer 2003)
 */
Real absorptionOpacity(Real T_K, int species) {
    const Real kappa0 = 0.06; // cm²/g/MeV²
    const Real eps_MeV = meanNeutrinoEnergy(T_K) / MEV_TO_ERG;
    const Real kappa = kappa0 * eps_MeV * eps_MeV;

    // Heavy lepton opacity is ~10x smaller (no charged-current)
    return (species == static_cast<int>(NeutrinoSpecies::NU_X)) ? 0.1 * kappa : kappa;
}

/**
 * @brief Neutrino scattering opacity κ_s (cm²/g).
 *
 * Dominant: neutral current on nucleons.
 * κ_sc ≈ 0.0625 cm²/g/MeV² * ε_ν²  (roughly equal to κ_abs)
 */
Real scatteringOpacity(Real T_K) {
    const Real kappa0 = 0.0625;
    const Real eps_MeV = meanNeutrinoEnergy(T_K) / MEV_TO_ERG;
    return kappa0 * eps_MeV * eps_MeV;
}

/**
 * @brief Naive radial optical depth estimate via column density.
 *
 * τ ≈ κ_tot * ρ * R_eff   where R_eff is estimated from local density:
 *   R_eff ~ (ρ / |∇ρ|)  ≈ density scale height
 *
 * For the leakage scheme, an exact ray integration is ideal but expensive.
 * We use the local approximation here (valid when density falls steeply).
 *
 * A more accurate version would integrate along radial rays — deferred to
 * computeOpticalDepth() which does ray tracing.
 */
Real localOpticalDepth(Real rho, Real T_K, int species, Real dx) {
    const Real kappa = absorptionOpacity(T_K, species) + scatteringOpacity(T_K);
    // Optical depth over one grid cell (minimum estimate)
    return kappa * rho * dx;
}

} // anonymous namespace

// ===========================================================================
// NeutrinoTransport
// ===========================================================================

NeutrinoTransport::NeutrinoTransport(const NeutrinoParams& params) : params_(params) {}

// ---------------------------------------------------------------------------
// computeLeakageRates: cooling and heating rates per species
// ---------------------------------------------------------------------------
void NeutrinoTransport::computeLeakageRates(
    const GridBlock& spacetime,
    const GridBlock& hydro_prim,
    std::vector<std::array<Real, NUM_NU_SPECIES>>& Q_cool,
    std::vector<std::array<Real, NUM_NU_SPECIES>>& Q_heat) const {
    const int tot = static_cast<int>(spacetime.totalSize());
    Q_cool.assign(tot, {0.0, 0.0, 0.0});
    Q_heat.assign(tot, {0.0, 0.0, 0.0});

    const int is = hydro_prim.istart();
    const int ie0 = hydro_prim.iend(0);
    const int ie1 = hydro_prim.iend(1);
    const int ie2 = hydro_prim.iend(2);
    const int nx = hydro_prim.totalCells(0);
    const int ny = hydro_prim.totalCells(1);
    const Real dx = hydro_prim.dx(0);

    for (int k = is; k < ie2; ++k)
        for (int j = is; j < ie1; ++j)
            for (int i = is; i < ie0; ++i) {

                const int flat = i + nx * (j + ny * k);

                const Real rho = std::max(hydro_prim.data(iRHO, i, j, k), 1.0e-30);
                const Real T = std::max(hydro_prim.data(iTEMP, i, j, k), 1.0e4); // K
                const Real Ye = std::clamp(hydro_prim.data(iYE, i, j, k), 0.01, 0.60);

                // ---- Free-space emission rates ----
                const Real Q_urca = urcaCaptureRate(rho, Ye, T); // ν_e dominant
                const Real Q_pair = pairEmissionRate(T);         // all species

                // Free-space rates per species [erg/cm³/s]
                const std::array<Real, NUM_NU_SPECIES> Q_free = {
                    Q_urca + Q_pair / 3.0, // ν_e
                    Q_pair / 3.0,          // ν̄_e  (no urca for antiν leakage here)
                    Q_pair / 3.0           // ν_x
                };

                // ---- Optical depth (local approximation) ----
                for (int s = 0; s < NUM_NU_SPECIES; ++s) {
                    const Real tau = localOpticalDepth(rho, T, s, dx);

                    // Effective cooling rate: leakage factor = 1 / (1 + τ²)
                    // (Ruffert 1996 prescription; smooth transition free→diffusion)
                    const Real f_leak = 1.0 / (1.0 + tau * tau);

                    Q_cool[flat][s] = Q_free[s] * f_leak;

                    // Heating rate: fraction re-absorbed in optically thick regions
                    // Q_heat ~ Q_cool * (1 - exp(-τ)) approximated as Q_cool * τ/(1+τ)
                    const Real f_heat = tau / (1.0 + tau);
                    Q_heat[flat][s] = Q_cool[flat][s] * f_heat;
                }

                (void)spacetime;
            }
}

// ---------------------------------------------------------------------------
// computeOpticalDepth: ray-integrate τ from star centre outward
// ---------------------------------------------------------------------------
void NeutrinoTransport::computeOpticalDepth(
    const GridBlock& spacetime,
    const GridBlock& hydro_prim,
    std::vector<std::array<Real, NUM_NU_SPECIES>>& tau) const {
    // Integrate optical depth along radial rays from the grid centre.
    // For each cell, τ(r) = ∫_r^∞ κ_tot ρ dr'
    // We accumulate outward-in (high-r to low-r) along each axis for
    // a Cartesian grid. A proper spherical ray trace is more accurate
    // but is O(N⁴); this axis-aligned integration is O(N³).

    const int is = hydro_prim.istart();
    const int ie0 = hydro_prim.iend(0);
    const int ie1 = hydro_prim.iend(1);
    const int ie2 = hydro_prim.iend(2);
    const int nx = hydro_prim.totalCells(0);
    const int ny = hydro_prim.totalCells(1);
    const Real dx = hydro_prim.dx(0);

    const int tot = static_cast<int>(hydro_prim.totalSize());
    tau.assign(tot, {0.0, 0.0, 0.0});

    // Integrate along z-axis rays (from top to bottom)
    for (int j = is; j < ie1; ++j)
        for (int i = is; i < ie0; ++i) {
            std::array<Real, NUM_NU_SPECIES> tau_running = {0.0, 0.0, 0.0};
            // From the outer boundary inward
            for (int k = ie2 - 1; k >= is; --k) {
                const int flat = i + nx * (j + ny * k);
                const Real rho = std::max(hydro_prim.data(iRHO, i, j, k), 1.0e-30);
                const Real T = std::max(hydro_prim.data(iTEMP, i, j, k), 1.0e4);

                for (int s = 0; s < NUM_NU_SPECIES; ++s) {
                    const Real kappa = absorptionOpacity(T, s) + scatteringOpacity(T);
                    tau_running[s] += kappa * rho * dx;
                    // Take mean of x, y, z ray contributions (simplified)
                    tau[flat][s] = tau_running[s];
                }
            }
        }

    (void)spacetime;
}

// ---------------------------------------------------------------------------
// computeRHS: M1 neutrino moment evolution (if enabled)
// ---------------------------------------------------------------------------
void NeutrinoTransport::computeRHS(const GridBlock& spacetime,
                                   const GridBlock& /*hydro_prim*/,
                                   const GridBlock& nu_data,
                                   GridBlock& rhs) const {
    // When use_m1_transport is enabled, evolve neutrino energy density
    // and flux in the M1 approximation (identical structure to M1 photon
    // transport but with neutrino opacities).
    //
    // For the leakage-only mode, the RHS contribution of neutrinos to
    // the spacetime/hydro system is handled via the cooling rates Q_cool
    // and does NOT require evolving separate neutrino grid variables.
    // So the RHS of nu_data is zero (source subtracted from hydro separately).

    if (!params_.use_m1_transport) {
        // Zero out all RHS (leakage handled in applyImplicitSources)
        const int is = rhs.istart();
        const int ie0 = rhs.iend(0);
        const int ie1 = rhs.iend(1);
        const int ie2 = rhs.iend(2);
        for (int k = is; k < ie2; ++k)
            for (int j = is; j < ie1; ++j)
                for (int i = is; i < ie0; ++i)
                    for (int v = 0; v < rhs.getNumVars(); ++v)
                        rhs.data(v, i, j, k) = 0.0;
        return;
    }

    // M1 evolution (mirrors photon M1 — see m1.cpp for details).
    // Uses 4th-order central FD for spatial derivatives of moments.
    const int is = nu_data.istart();
    const int ie0 = nu_data.iend(0);
    const int ie1 = nu_data.iend(1);
    const int ie2 = nu_data.iend(2);

    // Energy index: 0 = E_nu, 1/2/3 = F_nu^{x/y/z}
    // (per species, combined into single grid for simplicity)
    const Real alpha_mean = 1.0; // flat space default
    (void)alpha_mean;

    for (int k = is; k < ie2; ++k)
        for (int j = is; j < ie1; ++j)
            for (int i = is; i < ie0; ++i) {
                // 4th-order FD derivative helper
                auto D = [&](int var, int dir) -> Real {
                    const Real h = nu_data.dx(dir);
                    auto f = [&](int di, int dj, int dk) {
                        return nu_data.data(var, i + di, j + dj, k + dk);
                    };
                    if (dir == 0)
                        return (-f(2, 0, 0) + 8 * f(1, 0, 0) - 8 * f(-1, 0, 0) + f(-2, 0, 0)) /
                            (12.0 * h);
                    if (dir == 1)
                        return (-f(0, 2, 0) + 8 * f(0, 1, 0) - 8 * f(0, -1, 0) + f(0, -2, 0)) /
                            (12.0 * h);
                    return (-f(0, 0, 2) + 8 * f(0, 0, 1) - 8 * f(0, 0, -1) + f(0, 0, -2)) /
                        (12.0 * h);
                };

                const Real lapse = spacetime.data(18, i, j, k); // iLAPSE
                // Simple advection: ∂_t E_nu = -α ∇·F_nu
                if (rhs.getNumVars() >= 4) {
                    const Real divF = D(1, 0) + D(2, 1) + D(3, 2); // divF_nu
                    rhs.data(0, i, j, k) = -lapse * divF;          // E_nu
                    // Flux: ∂_t F^i_nu = -α ∂_j P^{ij} (Eddington ~1/3 E_nu)
                    rhs.data(1, i, j, k) = -lapse * D(0, 0) / 3.0;
                    rhs.data(2, i, j, k) = -lapse * D(0, 1) / 3.0;
                    rhs.data(3, i, j, k) = -lapse * D(0, 2) / 3.0;
                }
            }
}

// ---------------------------------------------------------------------------
// updateElectronFraction: dY_e/dt from weak reactions
// ---------------------------------------------------------------------------
void NeutrinoTransport::updateElectronFraction(
    const GridBlock& hydro_prim,
    const std::vector<std::array<Real, NUM_NU_SPECIES>>& Q_cool,
    const std::vector<std::array<Real, NUM_NU_SPECIES>>& Q_heat,
    std::vector<Real>& dYe_dt) const {
    // Net electron fraction change from β processes:
    //   dY_e/dt = -Γ_{ec}/(m_B) + Γ_{pc}/(m_B)
    //
    // where Γ_{ec} = e⁻ capture rate, Γ_{pc} = positron capture rate.
    //
    // Simplified from cooling rates:
    //   ν_e  emission → removes a Y_e (e⁻ + p → n + ν_e)
    //   ν̄_e emission → adds a Y_e    (e⁺ + n → p + ν̄_e)
    //
    // dY_e/dt ≈ (Q_heat[ν̄_e] - Q_heat[ν_e] - Q_cool[ν_e] + Q_cool[ν̄_e])
    //           / (m_B c² ρ)  [per baryon, per second]
    //
    // Here m_B = 939.6 MeV/c² in CGS.

    const int is = hydro_prim.istart();
    const int ie0 = hydro_prim.iend(0);
    const int ie1 = hydro_prim.iend(1);
    const int ie2 = hydro_prim.iend(2);
    const int nx = hydro_prim.totalCells(0);
    const int ny = hydro_prim.totalCells(1);

    constexpr Real m_B_erg = 939.565e6 * constants::K_BOLTZ; // ~ baryon rest energy [erg]

    const int tot = static_cast<int>(hydro_prim.totalSize());
    dYe_dt.assign(tot, 0.0);

    for (int k = is; k < ie2; ++k)
        for (int j = is; j < ie1; ++j)
            for (int i = is; i < ie0; ++i) {

                const int flat = i + nx * (j + ny * k);
                const Real rho = std::max(hydro_prim.data(iRHO, i, j, k), 1.0e-30);

                // Rate per unit volume → rate per baryon: divide by (ρ / m_B)
                const Real n_B = rho / (m_B_erg / (constants::C_CGS * constants::C_CGS));

                // ν_e leaves → Y_e decreases; ν̄_e leaves → Y_e increases
                constexpr int iNuE = static_cast<int>(NeutrinoSpecies::NU_E);
                constexpr int iNuEbar = static_cast<int>(NeutrinoSpecies::NU_E_BAR);

                const Real dNe = -Q_cool[flat][iNuE] / m_B_erg;     // ν_e emission
                const Real dNeb = +Q_cool[flat][iNuEbar] / m_B_erg; // ν̄_e emission
                const Real dHe = +Q_heat[flat][iNuE] / m_B_erg;     // ν_e absorption
                const Real dHeb = -Q_heat[flat][iNuEbar] / m_B_erg; // ν̄_e absorption

                dYe_dt[flat] = (n_B > 1.0e-30) ? (dNe + dNeb + dHe + dHeb) / n_B : 0.0;
            }
}

} // namespace granite::neutrino
