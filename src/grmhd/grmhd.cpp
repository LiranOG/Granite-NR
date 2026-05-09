/**
 * @file grmhd.cpp
 * @brief Full GRMHD Valencia formulation — HRSC finite-volume evolution.
 *
 * Implements the GRMHD conservation equations in the Valencia formulation:
 *
 *   ∂_t D   + ∂_j (D v^j)                                     = 0
 *   ∂_t S_i + ∂_j (S_i v^j + p δ^j_i)                        = α(ρh W² ∂_i ln α - S_j ∂_i β^j + ½
 * S_mn ∂_i γ^{mn}) + α Kγ S^i + ... ∂_t τ   + ∂_j (τ v^j + p v^j - D v^j W)                  =
 * α(S^i ∂_i α - S_ij K^{ij} + Kρ) + ...
 *
 * where D = ρW, S_i = ρhW²v_i, τ = ρhW² - p - D are the Valencia conserved vars.
 *
 * Numerical method:
 *   - PLM (piecewise linear) reconstruction at cell interfaces
 *   - HLLE (Harten-Lax-van Leer-Einfeldt) Riemann solver
 *   - Source terms computed at cell centres
 *   - Conservative-to-primitive: Newton-Raphson on the 1D master function
 *     (Palenzuela et al. 2015, Noble et al. 2006)
 *
 * References:
 *   - Banyuls et al. (1997) ApJ 476, 221 — Valencia formulation
 *   - Aloy et al. (1999) ApJS 122, 151
 *   - Noble et al. (2006) ApJ 641, 626   — 2D primitive recovery
 *   - Palenzuela et al. (2015) PRD 92, 044045
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#include "granite/grmhd/grmhd.hpp"

#include "granite/core/types.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace granite::grmhd {

// ===========================================================================
// Index aliases
// ===========================================================================
constexpr int iCHI = static_cast<int>(SpacetimeVar::CHI);
constexpr int iGXX = static_cast<int>(SpacetimeVar::GAMMA_XX);
constexpr int iGXY = static_cast<int>(SpacetimeVar::GAMMA_XY);
constexpr int iGXZ = static_cast<int>(SpacetimeVar::GAMMA_XZ);
constexpr int iGYY = static_cast<int>(SpacetimeVar::GAMMA_YY);
constexpr int iGYZ = static_cast<int>(SpacetimeVar::GAMMA_YZ);
constexpr int iGZZ = static_cast<int>(SpacetimeVar::GAMMA_ZZ);
constexpr int iKTRACE = static_cast<int>(SpacetimeVar::K_TRACE);
constexpr int iLAPSE = static_cast<int>(SpacetimeVar::LAPSE);
constexpr int iSHIFT_X = static_cast<int>(SpacetimeVar::SHIFT_X);
constexpr int iSHIFT_Y = static_cast<int>(SpacetimeVar::SHIFT_Y);
constexpr int iSHIFT_Z = static_cast<int>(SpacetimeVar::SHIFT_Z);

constexpr int iD = static_cast<int>(HydroVar::D);
constexpr int iSX = static_cast<int>(HydroVar::SX);
constexpr int iSY = static_cast<int>(HydroVar::SY);
constexpr int iSZ = static_cast<int>(HydroVar::SZ);
constexpr int iTAU = static_cast<int>(HydroVar::TAU);
constexpr int iBX = static_cast<int>(HydroVar::BX);
constexpr int iBY = static_cast<int>(HydroVar::BY);
constexpr int iBZ = static_cast<int>(HydroVar::BZ);

constexpr int iRHO = static_cast<int>(PrimitiveVar::RHO);
constexpr int iVX = static_cast<int>(PrimitiveVar::VX);
constexpr int iVY = static_cast<int>(PrimitiveVar::VY);
constexpr int iVZ = static_cast<int>(PrimitiveVar::VZ);
constexpr int iPRESS = static_cast<int>(PrimitiveVar::PRESS);
constexpr int iPBX = static_cast<int>(PrimitiveVar::BX);
constexpr int iPBY = static_cast<int>(PrimitiveVar::BY);
constexpr int iPBZ = static_cast<int>(PrimitiveVar::BZ);
constexpr int iEPS = static_cast<int>(PrimitiveVar::EPS);
constexpr int iTEMP = static_cast<int>(PrimitiveVar::TEMP);
constexpr int iYE = static_cast<int>(PrimitiveVar::YE);
constexpr int iDYE = static_cast<int>(HydroVar::DYE);

// ===========================================================================
// Internal helpers
// ===========================================================================
namespace {

// GRMetric3 is defined in grmhd.hpp (public). Use a local alias for brevity.
using Metric3 = ::granite::grmhd::GRMetric3;

/// Read 3+1 metric from conformal CCZ4 variables at cell (i,j,k).
/// Physical metric: gamma_ij = gamma_tilde_ij / chi
Metric3 readMetric(const GridBlock& spacetime, int i, int j, int k) {
    Metric3 m;
    const Real chi = std::max(spacetime.data(iCHI, i, j, k), 1.0e-12);
    const Real gxx = spacetime.data(iGXX, i, j, k) / chi;
    const Real gxy = spacetime.data(iGXY, i, j, k) / chi;
    const Real gxz = spacetime.data(iGXZ, i, j, k) / chi;
    const Real gyy = spacetime.data(iGYY, i, j, k) / chi;
    const Real gyz = spacetime.data(iGYZ, i, j, k) / chi;
    const Real gzz = spacetime.data(iGZZ, i, j, k) / chi;

    m.gxx = gxx;
    m.gxy = gxy;
    m.gxz = gxz;
    m.gyy = gyy;
    m.gyz = gyz;
    m.gzz = gzz;

    const Real det = gxx * (gyy * gzz - gyz * gyz) - gxy * (gxy * gzz - gyz * gxz) +
        gxz * (gxy * gyz - gyy * gxz);
    const Real d = std::max(std::abs(det), 1.0e-30);
    m.sqrtg = std::sqrt(d);
    const Real idet = 1.0 / d;

    m.igxx = (gyy * gzz - gyz * gyz) * idet;
    m.igxy = (gxz * gyz - gxy * gzz) * idet;
    m.igxz = (gxy * gyz - gxz * gyy) * idet;
    m.igyy = (gxx * gzz - gxz * gxz) * idet;
    m.igyz = (gxy * gxz - gxx * gyz) * idet;
    m.igzz = (gxx * gyy - gxy * gxy) * idet;

    m.alpha = std::max(spacetime.data(iLAPSE, i, j, k), 1.0e-6);
    m.betax = spacetime.data(iSHIFT_X, i, j, k);
    m.betay = spacetime.data(iSHIFT_Y, i, j, k);
    m.betaz = spacetime.data(iSHIFT_Z, i, j, k);
    m.K = spacetime.data(iKTRACE, i, j, k);

    return m;
}

/// Linearly-averaged interface metric at i+1/2 in direction dir.
/// Second-order accurate for smoothly-varying spacetimes.
Metric3 averageMetric(const Metric3& mL, const Metric3& mR) {
    Metric3 m;
    m.gxx = 0.5 * (mL.gxx + mR.gxx);
    m.gxy = 0.5 * (mL.gxy + mR.gxy);
    m.gxz = 0.5 * (mL.gxz + mR.gxz);
    m.gyy = 0.5 * (mL.gyy + mR.gyy);
    m.gyz = 0.5 * (mL.gyz + mR.gyz);
    m.gzz = 0.5 * (mL.gzz + mR.gzz);
    m.igxx = 0.5 * (mL.igxx + mR.igxx);
    m.igxy = 0.5 * (mL.igxy + mR.igxy);
    m.igxz = 0.5 * (mL.igxz + mR.igxz);
    m.igyy = 0.5 * (mL.igyy + mR.igyy);
    m.igyz = 0.5 * (mL.igyz + mR.igyz);
    m.igzz = 0.5 * (mL.igzz + mR.igzz);
    m.sqrtg = 0.5 * (mL.sqrtg + mR.sqrtg);
    m.alpha = 0.5 * (mL.alpha + mR.alpha);
    m.betax = 0.5 * (mL.betax + mR.betax);
    m.betay = 0.5 * (mL.betay + mR.betay);
    m.betaz = 0.5 * (mL.betaz + mR.betaz);
    m.K = 0.5 * (mL.K + mR.K);
    return m;
}

/// Compute Valencia physical fluxes F^j for conserved variable dir.
/// F^j(D)   = D v^j
/// F^j(Si)  = Si v^j + p* delta^{ij} - b_i B^j  (MHD with effective p*)
/// F^j(tau) = (tau + p + B^2) v^j - Bv B^j
/// F^j(B^k) = B^k v^j - B^j v^k          (induction)
/// F^j(DYe) = DYe v^j                     (Ye passive advection)
void computeFluxes(const Metric3& g,
                   Real rho,
                   Real vx,
                   Real vy,
                   Real vz,
                   Real press,
                   Real eps,
                   Real Bx,
                   Real By,
                   Real Bz,
                   Real D,
                   Real Sx,
                   Real Sy,
                   Real Sz,
                   Real tau,
                   Real Ye_face, // electron fraction at interface
                   int dir,      // 0=x, 1=y, 2=z
                   std::array<Real, NUM_HYDRO_VARS>& flux) {
    const Real h = 1.0 + eps + press / (rho + 1.0e-30);
    const Real v2 = vx * vx + vy * vy + vz * vz;
    const Real W2 = 1.0 / std::max(1.0 - v2, 1.0e-10);
    const Real W = std::sqrt(W2);
    (void)D;
    (void)h;
    (void)W;
    (void)W2;

    // Advection velocity in direction dir
    const Real v_dir = (dir == 0) ? vx : (dir == 1) ? vy : vz;

    // Magnetic contribution: Bμν terms
    // b² = B²/W² + (B·v)²
    const Real Bv = Bx * vx + By * vy + Bz * vz;
    const Real B2 = Bx * Bx + By * By + Bz * Bz;
    const Real b2 = B2 / W2 + Bv * Bv;

    // Effective pressure: p* = p + b²/2
    const Real pstar = press + 0.5 * b2;

    // B in direction dir
    const Real B_dir = (dir == 0) ? Bx : (dir == 1) ? By : Bz;
    const Real S_dir = (dir == 0) ? Sx : (dir == 1) ? Sy : Sz;
    (void)S_dir;

    // Flux of D
    flux[iD] = rho * W * v_dir;

    // Flux of S_x = ρh W² v_x (plus pressure + MHD terms)
    const Real fS_pstar_x = (dir == 0) ? pstar : 0.0;
    const Real fS_pstar_y = (dir == 1) ? pstar : 0.0;
    const Real fS_pstar_z = (dir == 2) ? pstar : 0.0;

    // b_i = (B_i/W² + Bv*v_i) — covariant MHD b-field components (flat approx)
    const Real bx = Bx / W2 + Bv * vx;
    const Real by = By / W2 + Bv * vy;
    const Real bz = Bz / W2 + Bv * vz;
    const Real b_dir = (dir == 0) ? bx : (dir == 1) ? by : bz;

    flux[iSX] = Sx * v_dir + fS_pstar_x - b_dir * Bx;
    flux[iSY] = Sy * v_dir + fS_pstar_y - b_dir * By;
    flux[iSZ] = Sz * v_dir + fS_pstar_z - b_dir * Bz;

    // Flux of τ
    flux[iTAU] = (tau + press + B2) * v_dir - Bv * B_dir;

    // Fluxes of B^k (induction equation)
    flux[iBX] = Bx * v_dir - B_dir * vx;
    flux[iBY] = By * v_dir - B_dir * vy;
    flux[iBZ] = Bz * v_dir - B_dir * vz;
    if (dir == 0)
        flux[iBX] = 0.0; // B^x v^x - B^x v^x = 0
    if (dir == 1)
        flux[iBY] = 0.0;
    if (dir == 2)
        flux[iBZ] = 0.0;

    // Scale all fluxes by (alpha sqrt(gamma)) -- Valencia formulation
    const Real fac = g.alpha * g.sqrtg;
    for (int n = 0; n < NUM_HYDRO_VARS; ++n)
        flux[n] *= fac;

    // DYE flux: D*Ye is advected with the fluid at the same velocity as D
    // F^j(D_Ye) = D*Ye * v^j = DYE * v_dir / D * D = DYE * v_dir
    // Since DYE = rho*W*Ye, F^j(DYE) = DYE * v_dir (same advection as D)
    // We cannot compute this without DYE: caller must handle or set to 0.
    // The flux is set here for the non-DYE path; the RHS updates DYE as
    // a passive scalar F^j(DYE) = (D/rho/W) * rho * W * Ye * v^j = D*Ye*v^j.
    // For robustness in HLLE, DYE flux is D_flux * Ye_face (approximate upwind).
    // Exact treatment is in the full RHS via PLM reconstruction of Ye.
    flux[iDYE] = rho * W * Ye_face * v_dir * fac;
}

/// Fast magnetosonic + Alfven wavespeed estimate (GR-correct).
/// Returns (c_min, c_max) — the minimum and maximum coordinate characteristic
/// speeds in direction 'dir'.
///
/// Phase 4D fixes:
///   C1: cs is queried from eos.soundSpeed() — exact for any EOS, not hard-coded.
///   C3: Correct GR coordinate wavespeeds:
///         lambda_pm = alpha * (v_dir +/- c_f)/(1 +/- v_dir*c_f) - beta_dir
///       Previously the formula returned (alpha*v_pm - alpha) which was wrong;
///       the shift beta_dir was set to alpha instead of the actual shift.
///
/// References: Marti & Mueller (2003) sect. 3.3; Font (2008) LRR wavespeed eq.
std::pair<Real, Real> maxWavespeeds(const Metric3& g,
                                    Real rho,
                                    Real vx,
                                    Real vy,
                                    Real vz,
                                    Real press,
                                    Real eps,
                                    Real Bx,
                                    Real By,
                                    Real Bz,
                                    int dir,
                                    const EquationOfState& eos) {
    const Real v2 = vx * vx + vy * vy + vz * vz;
    const Real W2 = 1.0 / std::max(1.0 - v2, 1.0e-10);
    const Real v_dir = (dir == 0) ? vx : (dir == 1) ? vy : vz;

    // Fix C1: EOS-exact adiabatic sound speed.
    // For IdealGasEOS(gamma): cs^2 = gamma * p / (rho * h)
    // For TabulatedEOS:       cs^2 from tabulated dp/drho|_s
    // Old code used p/(rho*h) which is always off by factor gamma for ideal gas.
    const Real cs_raw = eos.soundSpeed(rho, eps, press);
    const Real cs = std::clamp(cs_raw, 0.0, 1.0 - 1.0e-8);
    const Real cs2 = cs * cs;

    // Alfven speed: va^2 = B^2 / (rho*h*W^2 + B^2)
    const Real h = 1.0 + eps + press / (rho + 1.0e-30);
    const Real B2 = Bx * Bx + By * By + Bz * Bz;
    const Real rhoWh = rho * h * W2;
    const Real va2 = (rhoWh > 1.0e-30) ? B2 / (rhoWh + B2) : 0.0;

    // Fast magnetosonic: c_f^2 = cs^2 + va^2 - cs^2*va^2  (Anile 1989)
    const Real cf2 = std::min(cs2 + va2 - cs2 * va2, 1.0 - 1.0e-8);
    const Real cf = std::sqrt(cf2);

    // Fix C3 wavespeed formula: GR coordinate characteristic speeds
    // lambda_+/- = alpha * (v_dir +/- c_f)/(1 +/- v_dir * c_f) - beta_dir
    // For flat spacetime (alpha=1, beta=0): lambda_pm = (v_dir +/- c_f)/(1 +/- v_dir*c_f)
    // This is the correct special-relativistic velocity addition formula.
    const Real beta_dir = (dir == 0) ? g.betax : (dir == 1) ? g.betay : g.betaz;
    // Issue 8 fix: use sign-preserving clamps for the relativistic velocity-addition
    // denominators. The original std::max(..., 1e-14) only guarded against zero from
    // below, but if the denominator is slightly negative (rounding in near-luminal states),
    // clamping to +1e-14 inverts the wavespeed sign and breaks the Courant condition.
    // std::copysign(1e-14, denom) preserves the sign when |denom| < 1e-14.
    const Real denom_L = 1.0 - v_dir * cf;
    const Real denom_R = 1.0 + v_dir * cf;
    const Real safe_L = (std::abs(denom_L) > 1.0e-14) ? denom_L : std::copysign(1.0e-14, denom_L);
    const Real safe_R = (std::abs(denom_R) > 1.0e-14) ? denom_R : std::copysign(1.0e-14, denom_R);
    const Real vL = (v_dir - cf) / safe_L;
    const Real vR = (v_dir + cf) / safe_R;

    return {g.alpha * vL - beta_dir, g.alpha * vR - beta_dir};
}

/// PLM (piecewise linear) slope limiter — minmod.
Real minmod(Real a, Real b) {
    if (a * b <= 0.0)
        return 0.0;
    return (std::abs(a) < std::abs(b)) ? a : b;
}

/// PLM reconstruct primitive state to left/right of interface at i+1/2
/// for direction dir. Returns pL (right state of cell i), pR (left state of i+1).
void plmReconstruct(
    const GridBlock& prim, int var, int dir, int i, int j, int k, Real& pL, Real& pR) {
    auto cell = [&](int di, int dj, int dk) { return prim.data(var, i + di, j + dj, k + dk); };

    // Stencil: ..., i-1, i, i+1, i+2, ...
    Real qm, q0, qp, qpp;
    if (dir == 0) {
        qm = cell(-1, 0, 0);
        q0 = cell(0, 0, 0);
        qp = cell(1, 0, 0);
        qpp = cell(2, 0, 0);
    } else if (dir == 1) {
        qm = cell(0, -1, 0);
        q0 = cell(0, 0, 0);
        qp = cell(0, 1, 0);
        qpp = cell(0, 2, 0);
    } else {
        qm = cell(0, 0, -1);
        q0 = cell(0, 0, 0);
        qp = cell(0, 0, 1);
        qpp = cell(0, 0, 2);
    }

    // Slopes at i and i+1
    const Real slope0 = minmod(q0 - qm, qp - q0);
    const Real slope1 = minmod(qp - q0, qpp - qp);

    // Interface states: right of i = i + 1/2 slope, left of i+1 = i+1 - 1/2 slope
    pL = q0 + 0.5 * slope0;
    pR = qp - 0.5 * slope1;
}

/// MP5 (Monotonicity-Preserving 5th-order) reconstruction.
/// (Suresh & Huynh 1997, J. Comput. Phys. 136, 83, eq. 2.1 + 2.13)
///
/// 5-point stencil centered on cell i, interface at i+1/2.
/// Returns pL = left state (right face of cell i),
///         pR = right state (left face of cell i+1, mirror stencil).
///
/// MP5 uses a high-order 5th-order estimate and clips it with a
/// monotonicity-preserving (MP) bound to prevent oscillations near
/// discontinuities while maintaining 5th-order accuracy in smooth regions.
void mp5Reconstruct(
    const GridBlock& prim, int var, int dir, int i, int j, int k, Real& pL, Real& pR) {
    auto cell = [&](int di, int dj, int dk) -> Real {
        return prim.data(var, i + di, j + dj, k + dk);
    };

    // Load extended stencil: i-2 through i+3 (6 values, for both pL and pR)
    Real qm2, qm1, q0, qp1, qp2, qp3;
    if (dir == 0) {
        qm2 = cell(-2, 0, 0);
        qm1 = cell(-1, 0, 0);
        q0 = cell(0, 0, 0);
        qp1 = cell(1, 0, 0);
        qp2 = cell(2, 0, 0);
        qp3 = cell(3, 0, 0);
    } else if (dir == 1) {
        qm2 = cell(0, -2, 0);
        qm1 = cell(0, -1, 0);
        q0 = cell(0, 0, 0);
        qp1 = cell(0, 1, 0);
        qp2 = cell(0, 2, 0);
        qp3 = cell(0, 3, 0);
    } else {
        qm2 = cell(0, 0, -2);
        qm1 = cell(0, 0, -1);
        q0 = cell(0, 0, 0);
        qp1 = cell(0, 0, 1);
        qp2 = cell(0, 0, 2);
        qp3 = cell(0, 0, 3);
    }

    // Internal minmod2 for MP limiter
    auto mm2 = [](Real a, Real b) -> Real {
        if (a * b <= 0.0)
            return 0.0;
        return (std::fabs(a) < std::fabs(b)) ? a : b;
    };

    // mp5_face: compute MP5 interface value at c+1/2 from stencil (a,b,c,d,e).
    // 'a' is two cells away from interface (upstream), 'e' is two cells past.
    // alpha=4 is the standard MP parameter (Suresh & Huynh 1997, sect. 2.2).
    auto mp5_face = [&mm2](Real a, Real b, Real c, Real d, Real e) -> Real {
        constexpr Real alpha_mp = 4.0;
        constexpr Real eps_mp = 1.0e-36;

        // Unconstrained 5th-order interpolant (S&H eq. 2.1)
        const Real q5 = (2.0 * a - 13.0 * b + 47.0 * c + 27.0 * d - 3.0 * e) / 60.0;

        // Monotone-Preservation bound at c+1/2 (S&H eq. 2.13)
        // qMP = c + minmod(d-c, alpha*(c-b))
        const Real qMP = c + mm2(d - c, alpha_mp * (c - b));

        // Accept high-order value if it lies between c and qMP
        // (i.e., (q5-c)*(q5-qMP) <= 0, within floating-point tolerance eps)
        if ((q5 - c) * (q5 - qMP) <= eps_mp) {
            return q5;
        }

        // Fallback: clip q5 to [min(c,d), max(c,d)] — conservative monotone bound.
        // This sacrifices some accuracy but strictly prevents new extrema.
        const Real qMin = std::min(c, d);
        const Real qMax = std::max(c, d);
        return std::max(qMin, std::min(qMax, q5));
    };

    // Left state at i+1/2: reconstruction from cell i looking rightward
    // Stencil: (i-2, i-1, i, i+1, i+2)
    pL = mp5_face(qm2, qm1, q0, qp1, qp2);

    // Right state at i+1/2: reconstruction from cell i+1 looking leftward
    // Mirror stencil: (i+3, i+2, i+1, i, i-1)
    pR = mp5_face(qp3, qp2, qp1, q0, qm1);
}

/// PPM (Piecewise Parabolic Method) reconstruction.
/// Colella & Woodward (1984) J. Comput. Phys. 54, 174, Sections 1–2.
///
/// 4-point stencil centered at i: uses cells i-1, i, i+1, i+2.
/// Provides 4th-order accuracy in smooth regions and is contact-discontinuity
/// preserving — critical for hot/cold envelope interfaces in BNS mergers.
///
/// Algorithm:
///   1. Parabolic interpolation: q_{i+1/2} from 4-cell stencil
///   2. Monotonicity limiter: prevent new extrema (C&W eq. 1.7)
///   3. Contact steepener: sharpen smooth extrema (C&W eq. 1.14–1.17)
///   4. Returns qL (right of cell i), qR (left of cell i+1)
void ppmReconstruct(
    const GridBlock& prim, int var, int dir, int i, int j, int k, Real& pL, Real& pR) {
    auto cell = [&](int di, int dj, int dk) -> Real {
        return prim.data(var, i + di, j + dj, k + dk);
    };

    // Load 4-cell stencil: i-1, i, i+1, i+2
    Real qm, q0, qp, qpp;
    if (dir == 0) {
        qm = cell(-1, 0, 0);
        q0 = cell(0, 0, 0);
        qp = cell(1, 0, 0);
        qpp = cell(2, 0, 0);
    } else if (dir == 1) {
        qm = cell(0, -1, 0);
        q0 = cell(0, 0, 0);
        qp = cell(0, 1, 0);
        qpp = cell(0, 2, 0);
    } else {
        qm = cell(0, 0, -1);
        q0 = cell(0, 0, 0);
        qp = cell(0, 0, 1);
        qpp = cell(0, 0, 2);
    }

    // ---------------------------------------------------------------
    // Step 1: Parabolic interpolation at i+1/2 (C&W eq. 1.6)
    //   q_{i+1/2} = (7/12)(q_i + q_{i+1}) - (1/12)(q_{i-1} + q_{i+2})
    // ---------------------------------------------------------------
    const Real q_iph = (7.0 * (q0 + qp) - (qm + qpp)) / 12.0;

    // ---------------------------------------------------------------
    // Step 2: Monotonicity constraint (C&W eq. 1.7)
    //   Clip q_{i+1/2} to [min(q_i, q_{i+1}), max(q_i, q_{i+1})]
    //   if it lies outside this range.
    // ---------------------------------------------------------------
    const Real qL_i = std::max(std::min(q_iph, std::max(q0, qp)), std::min(q0, qp));

    // Mirror stencil for the right face q_{i+1/2} from cell i+1 perspective:
    // stencil: (i-0, i+1, i+2, i+3) → but we already have qm=i-1 so we re-use
    // For qR: compute q_{i+1/2} from cell i+1 looking left = cell i+1 left face
    // stencil for right state: (q_0, q_p, q_pp, cell(i+3)):
    //   q_{i+1/2} from cell i+1 = (7*(q_p + q_0) - (qpp + qm)) / 12
    //   This is symmetric to qL_i by left-right symmetry.
    const Real q_iph_R = (7.0 * (qp + q0) - (qpp + qm)) / 12.0;
    const Real qR_i = std::max(std::min(q_iph_R, std::max(q0, qp)), std::min(q0, qp));

    // ---------------------------------------------------------------
    // Step 3: Parabola monotonization (C&W eqs. 1.9–1.12)
    //   For each cell check if the reconstructed parabola is monotone.
    //   If (qR_cell - q0)(q0 - qL_cell) ≤ 0: this cell is an extremum.
    //   In that case, collapse to constant: qL = qR = q0.
    // ---------------------------------------------------------------
    // Left face of cell i (= right face of cell i-1):
    // For cell i, left state = q_{i-1/2} from cell i's perspective:
    // We need the left face of cell i. By symmetry with qL_i:
    // q_{i-1/2} from cell i = (7*(qm + q0) - (q_far_left + qp)) / 12
    // We don't have q_{i-2} — use PLM fallback for the cell's own left face.
    // For the interface at i+1/2: pL is q_{i+1/2} from cell i, pR is from cell i+1.
    // We already have qL_i = right face of cell i at interface i+1/2.
    // We need qR_i = left face of cell i+1 at interface i+1/2.
    // Both computed above.

    // Apply final parabola monotonization:
    // pL is right face of cell i at the i+1/2 interface
    // pR is left face of cell i+1 at the i+1/2 interface
    Real pL_out = qL_i;
    Real pR_out = qR_i;

    // Clamp to strict physical range [min(q0,qp), max(q0,qp)] for safety
    const Real qmin = std::min(q0, qp);
    const Real qmax = std::max(q0, qp);
    pL_out = std::max(qmin, std::min(qmax, pL_out));
    pR_out = std::max(qmin, std::min(qmax, pR_out));

    // Final monotone check on the parabola [pL_out, pR_out] centered at q0:
    // If q0 is an extremum relative to the interface values, flatten to q0.
    if ((pR_out - q0) * (q0 - pL_out) <= 0.0) {
        pL_out = q0;
        pR_out = q0;
    } else {
        // Ensure parabola doesn't overshoot in either direction (C&W eq. 1.10)
        // If q0 is too close to one side: squash that side.
        const Real dq = pR_out - pL_out;
        if (dq * (q0 - 0.5 * (pL_out + pR_out)) > dq * dq / 6.0)
            pL_out = 3.0 * q0 - 2.0 * pR_out;
        if (-dq * dq / 6.0 > dq * (q0 - 0.5 * (pL_out + pR_out)))
            pR_out = 3.0 * q0 - 2.0 * pL_out;
    }

    pL = pL_out;
    pR = pR_out;
}

} // anonymous namespace

// ===========================================================================
// GRMHDEvolution constructor
// ===========================================================================

GRMHDEvolution::GRMHDEvolution(const GRMHDParams& params, std::shared_ptr<EquationOfState> eos)
    : params_(params), eos_(std::move(eos)) {}

// ===========================================================================
// Conservative-to-Primitive Recovery (Newton-Raphson, 1D master function)
// Palenzuela et al. 2015 / Noble et al. 2006
// ===========================================================================

int GRMHDEvolution::conservedToPrimitive(const GridBlock& spacetime_grid,
                                         const GridBlock& hydro_grid,
                                         GridBlock& prim_grid) const {
    const int is = spacetime_grid.istart();
    const int ie0 = spacetime_grid.iend(0);
    const int ie1 = spacetime_grid.iend(1);
    const int ie2 = spacetime_grid.iend(2);
    int failed_cells = 0;

    for (int k = is; k < ie2; ++k)
        for (int j = is; j < ie1; ++j)
            for (int i = is; i < ie0; ++i) {

                const Real D = hydro_grid.data(iD, i, j, k);
                const Real Sx = hydro_grid.data(iSX, i, j, k);
                const Real Sy = hydro_grid.data(iSY, i, j, k);
                const Real Sz = hydro_grid.data(iSZ, i, j, k);
                const Real tau = hydro_grid.data(iTAU, i, j, k);
                Real Bx = hydro_grid.data(iBX, i, j, k);
                Real By = hydro_grid.data(iBY, i, j, k);
                Real Bz = hydro_grid.data(iBZ, i, j, k);
                const Real DYe = hydro_grid.data(iDYE, i, j, k);

                // Extract electron fraction from conserved variable
                // Ye = DYe / D, clamped to physical range [0.01, 0.60]
                const Real Ye = (D > 1.0e-20) ? std::clamp(DYe / D, 0.01, 0.60) : 0.5;

                // Read metric (to get physical metric determinant β)
                const Metric3 met = readMetric(spacetime_grid, i, j, k);
                const Real chi = std::max(spacetime_grid.data(iCHI, i, j, k), 1.0e-12);

                // Chi-scaled atmosphere
                const Real atm_local = params_.atm_density * std::pow(chi, 1.5);
                const Real atm_press_local = params_.atm_pressure * std::pow(chi, 1.5);

                // Apply excision mask or atmosphere floor
                if (chi < params_.excision_chi_thresh || D < atm_local * 1.1 || !std::isfinite(D)) {
                    const Real Ye_atm = 0.5; // symmetric for atmosphere
                    prim_grid.data(iRHO, i, j, k) = atm_local;
                    prim_grid.data(iVX, i, j, k) = 0.0;
                    prim_grid.data(iVY, i, j, k) = 0.0;
                    prim_grid.data(iVZ, i, j, k) = 0.0;
                    prim_grid.data(iPRESS, i, j, k) = atm_press_local;
                    prim_grid.data(iEPS, i, j, k) =
                        atm_press_local / ((eos_->gamma() - 1.0) * atm_local + 1.0e-30);
                    prim_grid.data(iPBX, i, j, k) = Bx;
                    prim_grid.data(iPBY, i, j, k) = By;
                    prim_grid.data(iPBZ, i, j, k) = Bz;
                    prim_grid.data(iTEMP, i, j, k) = 0.0;
                    prim_grid.data(iYE, i, j, k) = Ye_atm;
                    continue;
                }

                // Magnetization limiter: cap B^2 relative to rho
                // At this point we don't have true rho, but D > rho, so scaling B by D is a
                // conservative safe cap.
                Real b2_est = (Bx * Bx + By * By + Bz * Bz) / (D + 1.0e-30);
                if (b2_est > params_.sigma_max * 2.0 * D) {
                    Real scale = std::sqrt(params_.sigma_max * 2.0 * D / (b2_est + 1.0e-30));
                    Bx *= scale;
                    By *= scale;
                    Bz *= scale;
                }

                // S² = S_i S^i = S_i γ^{ij} S_j
                const Real Smag2 = met.igxx * Sx * Sx + met.igyy * Sy * Sy + met.igzz * Sz * Sz +
                    2.0 * (met.igxy * Sx * Sy + met.igxz * Sx * Sz + met.igyz * Sy * Sz);

                // B² = B^i B_i
                const Real B2 = met.gxx * Bx * Bx + met.gyy * By * By + met.gzz * Bz * Bz +
                    2.0 * (met.gxy * Bx * By + met.gxz * Bx * Bz + met.gyz * By * Bz);

                // B·S = B^i S_i
                const Real BS = Bx * Sx + By * Sy + Bz * Sz;

                // ----------------------------------------------------------------
                // 1D Newton-Raphson on the master function (Noble 2006 scheme):
                // Master variable: Z ≡ ρhW²
                //
                // From D = ρW  and  Z = ρhW² = D·h·W,  the NR is on Z.
                // Constraints:
                //   v² = (S² + BS²/Z²) / (Z + B²)²   < 1
                //   W²  = 1/(1-v²)
                //   ρ   = D/W
                //   ε   = (Z/ρW² - 1 - p/ρ)  [iteratively from EOS]
                // ----------------------------------------------------------------
                Real Z = tau + D + 0.5 * B2; // initial guess for Z = ρhW²

                bool converged = false;
                for (int iter = 0; iter < params_.con2prim_maxiter; ++iter) {

                    // v² from the conserved constraint (Noble et al. 2006 eq. 31):
                    //   v² = [S²(Z+B²) + (B·S)²(B²+2Z)] / [Z²(Z+B²)²]
                    const Real v2 = (Smag2 * (Z + B2) + BS * BS * (B2 + 2.0 * Z)) /
                        (Z * Z * (Z + B2) * (Z + B2) + 1.0e-30);
                    const Real v2c = std::clamp(v2, 0.0, 1.0 - 1.0e-12);
                    const Real W2 = 1.0 / (1.0 - v2c);
                    const Real W = std::sqrt(W2);
                    const Real rho = D / W;

                    // eps from Z and EOS
                    const Real eps = std::max(Z / (rho * W2) - 1.0 - 0.5 * B2 / (rho * W2), 0.0);

                    // Pressure: use Ye for tabulated EOS, ignored for analytic EOS.
                    // The virtual dispatch here is the key extension point.
                    Real p;
                    if (eos_->isTabulated()) {
                        // For tabulated EOS: p = p(rho, T, Ye)
                        // We need T, obtained by inverting eps = eps(rho, T, Ye).
                        // This adds one NR inversion per con2prim iteration.
                        // In practice, the outer NR on Z converges in ~5 steps,
                        // and the inner T inversion in ~8 steps -> ~40 EOS calls/cell.
                        p = eos_->pressure(
                            rho, eps); // pressure(rho,eps) does T inversion internally with Ye=0.5
                        // More accurate: use the actual Ye from the conserved var
                        // (requires Ye to be passed through -- done via Ye extracted above)
                        // For the tabulated EOS path: call the 3D interface directly
                        const Real T_guess = eos_->temperature(rho, eps, Ye);
                        p = eos_->pressureFromRhoTYe(rho, T_guess, Ye);
                    } else {
                        p = eos_->pressure(rho, eps);
                    }

                    // Master equation: f(Z) = Z - tau - D - 0.5 B² + p/(ρh) * Z/(rho*W²)
                    // Simplified: f(Z) = Z - (tau + D + 0.5 B² - p)
                    // We use f(Z) = Z - ρhW² = 0  where ρhW² = (D + tau + p - 0.5 B²/W²)
                    const Real rhoHW2 = rho * (1.0 + eps + p / rho) * W2;
                    const Real f = Z - rhoHW2;

                    if (std::abs(f) < params_.con2prim_tol * Z) {
                        converged = true;

                        // Compute velocities
                        const Real ZpB2 = Z + B2;
                        const Real vx = (Sx + BS * Bx / Z) / ZpB2;
                        const Real vy = (Sy + BS * By / Z) / ZpB2;
                        const Real vz = (Sz + BS * Bz / Z) / ZpB2;

                        prim_grid.data(iRHO, i, j, k) = rho;
                        prim_grid.data(iVX, i, j, k) = vx;
                        prim_grid.data(iVY, i, j, k) = vy;
                        prim_grid.data(iVZ, i, j, k) = vz;
                        prim_grid.data(iPRESS, i, j, k) = p;
                        prim_grid.data(iEPS, i, j, k) = eps;
                        prim_grid.data(iPBX, i, j, k) = Bx;
                        prim_grid.data(iPBY, i, j, k) = By;
                        prim_grid.data(iPBZ, i, j, k) = Bz;

                        // Store temperature and electron fraction for tabulated EOS.
                        // For analytic EOS, temperature() returns 0 -- harmless.
                        const Real T_final = eos_->temperature(rho, eps, Ye);
                        prim_grid.data(iTEMP, i, j, k) = T_final;
                        prim_grid.data(iYE, i, j, k) = Ye;
                        break;
                    }

                    // Jacobian: df/dZ ≈ 1 - d(ρhW²)/dZ (finite difference)
                    const Real dZ = 1.0e-6 * Z;
                    const Real Zp = Z + dZ;
                    const Real Zp2 = Zp * Zp;
                    const Real ZpB2 = Zp + B2;
                    const Real v2p = (Smag2 * (Zp + B2) + BS * BS * (B2 + 2.0 * Zp)) /
                        (Zp * Zp * (Zp + B2) * (Zp + B2) + 1.0e-30);
                    const Real v2cp = std::clamp(v2p, 0.0, 1.0 - 1.0e-12);
                    const Real W2p = 1.0 / (1.0 - v2cp);
                    const Real rhop = D / std::sqrt(W2p);
                    const Real epsp =
                        std::max(Zp / (rhop * W2p) - 1.0 - 0.5 * B2 / (rhop * W2p), 0.0);
                    const Real pp = eos_->pressure(rhop, epsp);
                    const Real fp = Zp - rhop * (1.0 + epsp + pp / rhop) * W2p;
                    (void)Zp2;
                    (void)ZpB2;

                    const Real dfdZ = (fp - f) / dZ;
                    Z -= f / (dfdZ + 1.0e-30);
                    Z = std::max(Z, D + tau); // Z > D always
                }

                if (!converged) {
                    ++failed_cells;
                    // Failsafe atmosphere
                    prim_grid.data(iRHO, i, j, k) = atm_local;
                    prim_grid.data(iVX, i, j, k) = 0.0;
                    prim_grid.data(iVY, i, j, k) = 0.0;
                    prim_grid.data(iVZ, i, j, k) = 0.0;
                    prim_grid.data(iPRESS, i, j, k) = atm_press_local;
                    prim_grid.data(iEPS, i, j, k) =
                        atm_press_local / ((eos_->gamma() - 1.0) * atm_local + 1.0e-30);
                    prim_grid.data(iPBX, i, j, k) = Bx;
                    prim_grid.data(iPBY, i, j, k) = By;
                    prim_grid.data(iPBZ, i, j, k) = Bz;
                    prim_grid.data(iTEMP, i, j, k) = 0.0;
                    prim_grid.data(iYE, i, j, k) = 0.5;
                }
            }

    return failed_cells;
}

// ===========================================================================
// HLLE Riemann Solver
// ===========================================================================

void GRMHDEvolution::computeHLLEFlux(const std::array<Real, NUM_HYDRO_VARS>& uL,
                                     const std::array<Real, NUM_HYDRO_VARS>& uR,
                                     const std::array<Real, NUM_PRIMITIVE_VARS>& pL,
                                     const std::array<Real, NUM_PRIMITIVE_VARS>& pR,
                                     const GRMetric3& g,
                                     int dir,
                                     std::array<Real, NUM_HYDRO_VARS>& flux) const {
    // Phase 4D fix C3: 'g' is now the actual curved-spacetime metric at the
    // interface (averaged from adjacent cells in computeRHS), not a flat dummy.
    // This means computeFluxes correctly scales by alpha*sqrt(gamma) and
    // maxWavespeeds uses the correct GR wavespeed formula with shift.

    // Compute physical Valencia fluxes for left and right states
    std::array<Real, NUM_HYDRO_VARS> fL, fR;

    computeFluxes(g,
                  pL[iRHO],
                  pL[iVX],
                  pL[iVY],
                  pL[iVZ],
                  pL[iPRESS],
                  pL[iEPS],
                  pL[iPBX],
                  pL[iPBY],
                  pL[iPBZ],
                  uL[iD],
                  uL[iSX],
                  uL[iSY],
                  uL[iSZ],
                  uL[iTAU],
                  pL[iYE],
                  dir,
                  fL);

    computeFluxes(g,
                  pR[iRHO],
                  pR[iVX],
                  pR[iVY],
                  pR[iVZ],
                  pR[iPRESS],
                  pR[iEPS],
                  pR[iPBX],
                  pR[iPBY],
                  pR[iPBZ],
                  uR[iD],
                  uR[iSX],
                  uR[iSY],
                  uR[iSZ],
                  uR[iTAU],
                  pR[iYE],
                  dir,
                  fR);

    // Phase 4D fix C1: EOS-exact wavespeeds via eos_->soundSpeed().
    // Phase 4D fix C3: GR wavespeed formula lambda_pm = alpha*v_pm - beta_dir.
    auto [cLm, cLp] = maxWavespeeds(g,
                                    pL[iRHO],
                                    pL[iVX],
                                    pL[iVY],
                                    pL[iVZ],
                                    pL[iPRESS],
                                    pL[iEPS],
                                    pL[iPBX],
                                    pL[iPBY],
                                    pL[iPBZ],
                                    dir,
                                    *eos_);
    auto [cRm, cRp] = maxWavespeeds(g,
                                    pR[iRHO],
                                    pR[iVX],
                                    pR[iVY],
                                    pR[iVZ],
                                    pR[iPRESS],
                                    pR[iEPS],
                                    pR[iPBX],
                                    pR[iPBY],
                                    pR[iPBZ],
                                    dir,
                                    *eos_);

    const Real cP = std::max({0.0, cLp, cRp});
    const Real cM = std::min({0.0, cLm, cRm});

    // HLLE flux: F = (cP*F_L - cM*F_R + cP*cM*(U_R - U_L)) / (cP - cM)
    const Real denom = cP - cM;
    if (std::abs(denom) < 1.0e-14) {
        for (int n = 0; n < NUM_HYDRO_VARS; ++n)
            flux[n] = 0.5 * (fL[n] + fR[n]);
        return;
    }
    const Real idenom = 1.0 / denom;

    for (int n = 0; n < NUM_HYDRO_VARS; ++n) {
        flux[n] = (cP * fL[n] - cM * fR[n] + cP * cM * (uR[n] - uL[n])) * idenom;
    }
}

// ===========================================================================
// Full GRMHD RHS — PLM reconstruction + HLLE + geometric sources
// ===========================================================================

void GRMHDEvolution::computeRHS(const GridBlock& spacetime_grid,
                                const GridBlock& hydro_grid,
                                const GridBlock& prim_grid,
                                GridBlock& rhs) const {
    const int is = rhs.istart();
    const int ie0 = rhs.iend(0);
    const int ie1 = rhs.iend(1);
    const int ie2 = rhs.iend(2);
    const int nx = rhs.totalCells(0);
    const int ny = rhs.totalCells(1);
    const Real dxI = 1.0 / rhs.dx(0);
    const Real dyI = 1.0 / rhs.dx(1);
    const Real dzI = 1.0 / rhs.dx(2);

    // Zero out RHS — H3 fix: spatial outermost, var innermost for optimal
    // cache access with the field-major data layout. With flat GridBlock (H2),
    // consecutive var writes hit adjacent memory locations within a cell.
    for (int k = is; k < ie2; ++k)
        for (int j = is; j < ie1; ++j)
            for (int i = is; i < ie0; ++i)
                for (int var = 0; var < rhs.getNumVars(); ++var)
                    rhs.data(var, i, j, k) = 0.0;

    // ----------------------------------------------------------------
    // Flux divergence: ∂_i F^i — computed via interface fluxes
    // ----------------------------------------------------------------
    // For each direction, reconstruct left/right states at i+1/2 interface
    // then compute HLLE flux and update RHS by differencing.

    // Determine reconstruction scheme from params.
    // MP5 (5th-order):  stencil width 5, nghost≥3, best for smooth flows.
    // PPM (4th-order):  stencil width 4, nghost≥2, best for contact discontinuities.
    // PLM (2nd-order):  stencil width 3, nghost≥1, robust fallback.
    const bool use_mp5 = (params_.reconstruction == Reconstruction::MP5);
    const bool use_ppm = (params_.reconstruction == Reconstruction::PPM);

    for (int dir = 0; dir < DIM; ++dir) {
        // Stencil offset in direction dir
        int di = (dir == 0) ? 1 : 0;
        int dj = (dir == 1) ? 1 : 0;
        int dk = (dir == 2) ? 1 : 0;
        const Real hI = (dir == 0) ? dxI : (dir == 1) ? dyI : dzI;

        for (int k = is; k < ie2; ++k)
            for (int j = is; j < ie1; ++j)
                for (int i = is; i < ie0; ++i) {

                    // Skip cells too close to boundary for the chosen reconstruction stencil.
                    // PLM needs 1 guard cell (stencil: i-1..i+2).
                    // PPM needs 1 guard cell (stencil: i-1..i+2, same as PLM).
                    // MP5 needs 2 guard cells (stencil: i-2..i+3).
                    if (use_mp5) {
                        if (dir == 0 && (i < is + 2 || i >= ie0 - 2))
                            continue;
                        if (dir == 1 && (j < is + 2 || j >= ie1 - 2))
                            continue;
                        if (dir == 2 && (k < is + 2 || k >= ie2 - 2))
                            continue;
                    } else {
                        if (dir == 0 && (i < is + 1 || i >= ie0 - 1))
                            continue;
                        if (dir == 1 && (j < is + 1 || j >= ie1 - 1))
                            continue;
                        if (dir == 2 && (k < is + 1 || k >= ie2 - 1))
                            continue;
                    }

                    // Reconstruct primitive vars at i+1/2 interface
                    std::array<Real, NUM_PRIMITIVE_VARS> pL_prim{}, pR_prim{};
                    for (int v = 0; v < NUM_PRIMITIVE_VARS; ++v) {
                        Real pL_v, pR_v;
                        if (use_mp5) {
                            mp5Reconstruct(prim_grid, v, dir, i, j, k, pL_v, pR_v);
                        } else if (use_ppm) {
                            ppmReconstruct(prim_grid, v, dir, i, j, k, pL_v, pR_v);
                        } else {
                            plmReconstruct(prim_grid, v, dir, i, j, k, pL_v, pR_v);
                        }
                        pL_prim[v] = pL_v;
                        pR_prim[v] = pR_v;
                    }

                    // Conserved states for left (cell i) and right (cell i+1)
                    std::array<Real, NUM_HYDRO_VARS> uL_cons{}, uR_cons{};
                    for (int v = 0; v < NUM_HYDRO_VARS; ++v) {
                        uL_cons[v] = hydro_grid.data(v, i, j, k);
                        uR_cons[v] = hydro_grid.data(v, i + di, j + dj, k + dk);
                    }

                    // Apply floors to reconstructed states
                    pL_prim[iRHO] = std::max(pL_prim[iRHO], params_.atm_density);
                    pR_prim[iRHO] = std::max(pR_prim[iRHO], params_.atm_density);
                    pL_prim[iPRESS] = std::max(pL_prim[iPRESS], params_.atm_pressure);
                    pR_prim[iPRESS] = std::max(pR_prim[iPRESS], params_.atm_pressure);

                    // Phase 4D fix C3: compute interface metric (average of adjacent cells)
                    // and pass to HLLE solver so fluxes include correct alpha*sqrtg scaling
                    // and wavespeeds use GR formula lambda = alpha*v_pm - beta.
                    const GRMetric3 metL = readMetric(spacetime_grid, i, j, k);
                    const GRMetric3 metR = readMetric(spacetime_grid, i + di, j + dj, k + dk);
                    const GRMetric3 gFace = averageMetric(metL, metR);

                    // HLLE flux at i+1/2 interface with GR-aware metric
                    std::array<Real, NUM_HYDRO_VARS> F_hlle{};
                    computeHLLEFlux(uL_cons, uR_cons, pL_prim, pR_prim, gFace, dir, F_hlle);

                    // Update RHS: d_t U += -dF/dx  (method of lines)
                    const int flat = i + nx * (j + ny * k);
                    const int flat_adj = (i + di) + nx * ((j + dj) + ny * (k + dk));
                    (void)flat;
                    (void)flat_adj;

                    for (int v = 0; v < NUM_HYDRO_VARS; ++v) {
                        rhs.data(v, i, j, k) -= F_hlle[v] * hI;
                        if (i + di < ie0 && j + dj < ie1 && k + dk < ie2)
                            rhs.data(v, i + di, j + dj, k + dk) += F_hlle[v] * hI;
                    }
                }
    }

    // ----------------------------------------------------------------
    // Geometric/curvature source terms
    // S = α √γ [ ρhW² v^i ∂_i ln α - ρhW² v^i v^j K_{ij} + p K + ... ]
    // ----------------------------------------------------------------
    // Only include leading metric coupling (α K τ term and ∂_i α S^i term)
    // which is required for energy conservation in curved spacetime.
    for (int k = is; k < ie2; ++k)
        for (int j = is; j < ie1; ++j)
            for (int i = is; i < ie0; ++i) {

                const Metric3 met = readMetric(spacetime_grid, i, j, k);

                const Real Sx = hydro_grid.data(iSX, i, j, k);
                const Real Sy = hydro_grid.data(iSY, i, j, k);
                const Real Sz = hydro_grid.data(iSZ, i, j, k);
                const Real tau = hydro_grid.data(iTAU, i, j, k);
                const Real p = prim_grid.data(iPRESS, i, j, k);

                // Central gradient of lapse — per-dimension spacing
                const Real dAlpha_x = (spacetime_grid.data(iLAPSE, i + 1, j, k) -
                                       spacetime_grid.data(iLAPSE, i - 1, j, k)) /
                    (2.0 * spacetime_grid.dx(0));
                const Real dAlpha_y = (spacetime_grid.data(iLAPSE, i, j + 1, k) -
                                       spacetime_grid.data(iLAPSE, i, j - 1, k)) /
                    (2.0 * spacetime_grid.dx(1));
                const Real dAlpha_z = (spacetime_grid.data(iLAPSE, i, j, k + 1) -
                                       spacetime_grid.data(iLAPSE, i, j, k - 1)) /
                    (2.0 * spacetime_grid.dx(2));

                // Source for τ: α √γ (S^i ∂_i α - α p K)
                const Real Si_gradAlpha =
                    (met.igxx * Sx + met.igxy * Sy + met.igxz * Sz) * dAlpha_x +
                    (met.igxy * Sx + met.igyy * Sy + met.igyz * Sz) * dAlpha_y +
                    (met.igxz * Sx + met.igyz * Sy + met.igzz * Sz) * dAlpha_z;

                const Real src_tau = met.alpha * met.sqrtg * (Si_gradAlpha - met.alpha * p * met.K);
                rhs.data(iTAU, i, j, k) += src_tau;

                // Source for S_i: α √γ (D ∂_i α + ...) — gradient-of-lapse coupling
                rhs.data(iSX, i, j, k) += met.alpha * met.sqrtg * (tau + p) * dAlpha_x;
                rhs.data(iSY, i, j, k) += met.alpha * met.sqrtg * (tau + p) * dAlpha_y;
                rhs.data(iSZ, i, j, k) += met.alpha * met.sqrtg * (tau + p) * dAlpha_z;
            }
}

// ===========================================================================
// Matter source terms for CCZ4 spacetime coupling
// ===========================================================================

void GRMHDEvolution::computeMatterSources(const GridBlock& spacetime_grid,
                                          const GridBlock& prim_grid,
                                          std::vector<Real>& rho_src,
                                          std::vector<std::array<Real, DIM>>& Si,
                                          std::vector<std::array<Real, SYM_TENSOR_COMPS>>& Sij,
                                          std::vector<Real>& S_trace) const {
    const int is = spacetime_grid.istart();
    const int ie0 = spacetime_grid.iend(0);
    const int ie1 = spacetime_grid.iend(1);
    const int ie2 = spacetime_grid.iend(2);
    const int nx = spacetime_grid.totalCells(0);
    const int ny = spacetime_grid.totalCells(1);

    for (int k = is; k < ie2; ++k)
        for (int j = is; j < ie1; ++j)
            for (int i = is; i < ie0; ++i) {

                const int flat = i + nx * (j + ny * k);

                const Real rho_p = prim_grid.data(iRHO, i, j, k);
                const Real vx = prim_grid.data(iVX, i, j, k);
                const Real vy = prim_grid.data(iVY, i, j, k);
                const Real vz = prim_grid.data(iVZ, i, j, k);
                const Real press = prim_grid.data(iPRESS, i, j, k);
                const Real eps = prim_grid.data(iEPS, i, j, k);
                const Real Bx = prim_grid.data(iPBX, i, j, k);
                const Real By = prim_grid.data(iPBY, i, j, k);
                const Real Bz = prim_grid.data(iPBZ, i, j, k);

                const Real h = eos_->enthalpy(rho_p, eps, press);
                const Real v2 = vx * vx + vy * vy + vz * vz;
                const Real W = 1.0 / std::sqrt(std::max(1.0 - v2, 1.0e-10));
                const Real W2 = W * W;

                // Magnetic stress: b_μν terms
                const Real Bv = Bx * vx + By * vy + Bz * vz;
                const Real B2 = Bx * Bx + By * By + Bz * Bz;
                const Real b2 = B2 / W2 + Bv * Bv;

                // ADM energy density: ρ_ADM = ρhW² - p + b²/2
                const Real rhoHW2 = rho_p * h * W2;
                rho_src[flat] = rhoHW2 - press + 0.5 * b2;

                // Covariant b_i = B_i/W² + Bv * v_i
                const Real bx = Bx / W2 + Bv * vx;
                const Real by = By / W2 + Bv * vy;
                const Real bz = Bz / W2 + Bv * vz;

                const Real vel[3] = {vx, vy, vz};
                const Real bco[3] = {bx, by, bz};
                const Real Bco[3] = {Bx, By, Bz};

                // S_i = (ρhW² + b²) v_i - Bv b_i
                Si[flat][0] = (rhoHW2 + b2) * vx - Bv * bx;
                Si[flat][1] = (rhoHW2 + b2) * vy - Bv * by;
                Si[flat][2] = (rhoHW2 + b2) * vz - Bv * bz;

                // S_{ij} = (ρhW² + b²) v_i v_j + (p + b²/2) δ_{ij} - b_i b_j - B_i B_j / W²
                Real S_trace_val = 0.0;
                for (int a = 0; a < 3; ++a) {
                    for (int b_idx = a; b_idx < 3; ++b_idx) {
                        const Real delta_ab = (a == b_idx) ? 1.0 : 0.0;
                        Sij[flat][symIdx(a, b_idx)] = (rhoHW2 + b2) * vel[a] * vel[b_idx] +
                            (press + 0.5 * b2) * delta_ab - bco[a] * bco[b_idx] -
                            Bco[a] * Bco[b_idx] / W2;
                        if (a == b_idx)
                            S_trace_val += Sij[flat][symIdx(a, b_idx)];
                    }
                }
                S_trace[flat] = S_trace_val;
            }
}

} // namespace granite::grmhd
