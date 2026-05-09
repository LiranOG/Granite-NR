/**
 * @file hlld.cpp
 * @brief HLLD Riemann solver for ideal MHD (Miyoshi & Kusano 2005).
 *
 * Resolves all 7 MHD wave families:
 *   - 2 fast magnetosonic waves (outermost)
 *   - 2 Alfven rotational discontinuities
 *   - 2 slow magnetosonic waves
 *   - 1 entropy (contact) discontinuity
 *
 * The solver structure uses 5 intermediate states:
 *
 *   S_L   S*_L    S_M    S*_R   S_R
 *   U_L | U*_L | U**_L | U**_R | U*_R | U_R
 *       fast_L  Alfven_L contact Alfven_R fast_R
 *
 * References:
 *   - Miyoshi & Kusano (2005) JCP 208, 315-344
 *   - Mignone (2007) JCP 225, 1427-1441
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#include "granite/core/types.hpp"
#include "granite/grmhd/grmhd.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace granite::grmhd {

// ===========================================================================
// Index aliases
// ===========================================================================
namespace {

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

/// Compute 1D MHD flux from primitive state in direction dir
inline void mhdFlux1D(Real rho,
                      Real vn,
                      Real vt1,
                      Real vt2,
                      Real /*p*/,
                      Real /*eps*/,
                      Real Bn,
                      Real Bt1,
                      Real Bt2,
                      Real D,
                      Real Sn,
                      Real St1,
                      Real St2,
                      Real tau,
                      Real ptot,
                      std::array<Real, NUM_HYDRO_VARS>& F) {
    const Real Bv = Bn * vn + Bt1 * vt1 + Bt2 * vt2;
    (void)rho;

    F[iD] = D * vn;
    F[iSX] = Sn * vn + ptot - Bn * Bn;
    F[iSY] = St1 * vn - Bn * Bt1;
    F[iSZ] = St2 * vn - Bn * Bt2;
    F[iTAU] = (tau + ptot) * vn - Bn * Bv;
    F[iBX] = 0.0;
    F[iBY] = Bt1 * vn - Bn * vt1;
    F[iBZ] = Bt2 * vn - Bn * vt2;
}

/// Fast magnetosonic speed for wavespeed estimates.
/// cs2 is the squared adiabatic sound speed, pre-computed by the caller
/// via eos_->soundSpeed() to avoid requiring gamma on the base EOS class.
inline Real fastMagnetosonicSpeed(Real cs2, Real rho, Real B2, Real Bn2) {
    const Real va2 = (rho > 1e-30) ? B2 / rho : 0.0;
    const Real vAn2 = (rho > 1e-30) ? Bn2 / rho : 0.0;
    const Real sum = cs2 + va2;
    const Real disc = std::max(0.0, sum * sum - 4.0 * cs2 * vAn2);
    return std::sqrt(0.5 * (sum + std::sqrt(disc)));
}

} // anonymous namespace

// ===========================================================================
// HLLD Riemann Solver -- Miyoshi & Kusano (2005) JCP 208, 315
// ===========================================================================

void GRMHDEvolution::computeHLLDFlux(const std::array<Real, NUM_HYDRO_VARS>& uL,
                                     const std::array<Real, NUM_HYDRO_VARS>& uR,
                                     const std::array<Real, NUM_PRIMITIVE_VARS>& pL,
                                     const std::array<Real, NUM_PRIMITIVE_VARS>& pR,
                                     Real Bn,
                                     int dir,
                                     std::array<Real, NUM_HYDRO_VARS>& flux) const {
    auto mapVel =
        [dir](const std::array<Real, NUM_PRIMITIVE_VARS>& p, Real& vn, Real& vt1, Real& vt2) {
            if (dir == 0) {
                vn = p[iVX];
                vt1 = p[iVY];
                vt2 = p[iVZ];
            } else if (dir == 1) {
                vn = p[iVY];
                vt1 = p[iVZ];
                vt2 = p[iVX];
            } else {
                vn = p[iVZ];
                vt1 = p[iVX];
                vt2 = p[iVY];
            }
        };
    auto mapB = [dir](const std::array<Real, NUM_PRIMITIVE_VARS>& p, Real& Bt1, Real& Bt2) {
        if (dir == 0) {
            Bt1 = p[iPBY];
            Bt2 = p[iPBZ];
        } else if (dir == 1) {
            Bt1 = p[iPBZ];
            Bt2 = p[iPBX];
        } else {
            Bt1 = p[iPBX];
            Bt2 = p[iPBY];
        }
    };
    auto mapMom = [dir](const std::array<Real, NUM_HYDRO_VARS>& u, Real& Sn, Real& St1, Real& St2) {
        if (dir == 0) {
            Sn = u[iSX];
            St1 = u[iSY];
            St2 = u[iSZ];
        } else if (dir == 1) {
            Sn = u[iSY];
            St1 = u[iSZ];
            St2 = u[iSX];
        } else {
            Sn = u[iSZ];
            St1 = u[iSX];
            St2 = u[iSY];
        }
    };

    Real rhoL = pL[iRHO], pressL = pL[iPRESS], epsL = pL[iEPS];
    Real vnL, vt1L, vt2L, Bt1L, Bt2L, SnL, St1L, St2L;
    mapVel(pL, vnL, vt1L, vt2L);
    mapB(pL, Bt1L, Bt2L);
    mapMom(uL, SnL, St1L, St2L);
    Real BL2 = Bn * Bn + Bt1L * Bt1L + Bt2L * Bt2L;
    Real ptotL = pressL + 0.5 * BL2;

    Real rhoR = pR[iRHO], pressR = pR[iPRESS], epsR = pR[iEPS];
    Real vnR, vt1R, vt2R, Bt1R, Bt2R, SnR, St1R, St2R;
    mapVel(pR, vnR, vt1R, vt2R);
    mapB(pR, Bt1R, Bt2R);
    mapMom(uR, SnR, St1R, St2R);
    Real BR2 = Bn * Bn + Bt1R * Bt1R + Bt2R * Bt2R;
    Real ptotR = pressR + 0.5 * BR2;

    const Real csL = eos_->soundSpeed(rhoL, epsL, pressL);
    const Real csR = eos_->soundSpeed(rhoR, epsR, pressR);
    Real cfL = fastMagnetosonicSpeed(csL * csL, rhoL, BL2, Bn * Bn);
    Real cfR = fastMagnetosonicSpeed(csR * csR, rhoR, BR2, Bn * Bn);

    Real SL = std::min(vnL - cfL, vnR - cfR);
    Real SR = std::max(vnL + cfL, vnR + cfR);

    if (SL >= 0.0) {
        std::array<Real, NUM_HYDRO_VARS> FL;
        mhdFlux1D(rhoL,
                  vnL,
                  vt1L,
                  vt2L,
                  pressL,
                  epsL,
                  Bn,
                  Bt1L,
                  Bt2L,
                  uL[iD],
                  SnL,
                  St1L,
                  St2L,
                  uL[iTAU],
                  ptotL,
                  FL);
        if (dir == 0) {
            flux = FL;
        } else if (dir == 1) {
            flux[iD] = FL[iD];
            flux[iSX] = FL[iSZ];
            flux[iSY] = FL[iSX];
            flux[iSZ] = FL[iSY];
            flux[iTAU] = FL[iTAU];
            flux[iBX] = FL[iBZ];
            flux[iBY] = FL[iBX];
            flux[iBZ] = FL[iBY];
        } else {
            flux[iD] = FL[iD];
            flux[iSX] = FL[iSY];
            flux[iSY] = FL[iSZ];
            flux[iSZ] = FL[iSX];
            flux[iTAU] = FL[iTAU];
            flux[iBX] = FL[iBY];
            flux[iBY] = FL[iBZ];
            flux[iBZ] = FL[iBX];
        }
        return;
    }
    if (SR <= 0.0) {
        std::array<Real, NUM_HYDRO_VARS> FR;
        mhdFlux1D(rhoR,
                  vnR,
                  vt1R,
                  vt2R,
                  pressR,
                  epsR,
                  Bn,
                  Bt1R,
                  Bt2R,
                  uR[iD],
                  SnR,
                  St1R,
                  St2R,
                  uR[iTAU],
                  ptotR,
                  FR);
        if (dir == 0) {
            flux = FR;
        } else if (dir == 1) {
            flux[iD] = FR[iD];
            flux[iSX] = FR[iSZ];
            flux[iSY] = FR[iSX];
            flux[iSZ] = FR[iSY];
            flux[iTAU] = FR[iTAU];
            flux[iBX] = FR[iBZ];
            flux[iBY] = FR[iBX];
            flux[iBZ] = FR[iBY];
        } else {
            flux[iD] = FR[iD];
            flux[iSX] = FR[iSY];
            flux[iSY] = FR[iSZ];
            flux[iSZ] = FR[iSX];
            flux[iTAU] = FR[iTAU];
            flux[iBX] = FR[iBY];
            flux[iBY] = FR[iBZ];
            flux[iBZ] = FR[iBX];
        }
        return;
    }

    Real rhoL_SL = rhoL * (SL - vnL);
    Real rhoR_SR = rhoR * (SR - vnR);
    Real SM_num = ptotL - ptotR + rhoR_SR * vnR - rhoL_SL * vnL;
    Real SM_den = rhoR_SR - rhoL_SL;
    Real SM = (std::abs(SM_den) > 1e-30) ? SM_num / SM_den : 0.5 * (vnL + vnR);

    Real ptot_star = ptotL + rhoL_SL * (SM - vnL);

    Real rhoL_star = rhoL * (SL - vnL) / (SL - SM + 1e-30);
    Real rhoR_star = rhoR * (SR - vnR) / (SR - SM + 1e-30);
    Real vnL_star = SM;
    Real vnR_star = SM;

    Real denom_L = rhoL * (SL - vnL) * (SL - SM) - Bn * Bn;
    Real denom_R = rhoR * (SR - vnR) * (SR - SM) - Bn * Bn;

    Real vt1L_star, vt2L_star, Bt1L_star, Bt2L_star;
    if (std::abs(denom_L) > 1e-30 * (std::abs(rhoL * (SL - vnL) * (SL - SM)) + 1.0)) {
        vt1L_star = vt1L - Bn * Bt1L * (SM - vnL) / denom_L;
        vt2L_star = vt2L - Bn * Bt2L * (SM - vnL) / denom_L;
        Bt1L_star = Bt1L * (rhoL * (SL - vnL) * (SL - vnL) - Bn * Bn) / denom_L;
        Bt2L_star = Bt2L * (rhoL * (SL - vnL) * (SL - vnL) - Bn * Bn) / denom_L;
    } else {
        vt1L_star = vt1L;
        vt2L_star = vt2L;
        Bt1L_star = Bt1L;
        Bt2L_star = Bt2L;
    }

    Real vt1R_star, vt2R_star, Bt1R_star, Bt2R_star;
    if (std::abs(denom_R) > 1e-30 * (std::abs(rhoR * (SR - vnR) * (SR - SM)) + 1.0)) {
        vt1R_star = vt1R - Bn * Bt1R * (SM - vnR) / denom_R;
        vt2R_star = vt2R - Bn * Bt2R * (SM - vnR) / denom_R;
        Bt1R_star = Bt1R * (rhoR * (SR - vnR) * (SR - vnR) - Bn * Bn) / denom_R;
        Bt2R_star = Bt2R * (rhoR * (SR - vnR) * (SR - vnR) - Bn * Bn) / denom_R;
    } else {
        vt1R_star = vt1R;
        vt2R_star = vt2R;
        Bt1R_star = Bt1R;
        Bt2R_star = Bt2R;
    }

    Real BvL_star = Bn * vnL_star + Bt1L_star * vt1L_star + Bt2L_star * vt2L_star;
    Real BvL = Bn * vnL + Bt1L * vt1L + Bt2L * vt2L;
    Real tauL_star =
        ((SL - vnL) * uL[iTAU] - ptotL * vnL + ptot_star * SM + Bn * (BvL - BvL_star)) /
        (SL - SM + 1e-30);

    Real BvR_star = Bn * vnR_star + Bt1R_star * vt1R_star + Bt2R_star * vt2R_star;
    Real BvR = Bn * vnR + Bt1R * vt1R + Bt2R * vt2R;
    Real tauR_star =
        ((SR - vnR) * uR[iTAU] - ptotR * vnR + ptot_star * SM + Bn * (BvR - BvR_star)) /
        (SR - SM + 1e-30);

    Real sqrtRhoL_star = std::sqrt(std::max(rhoL_star, 0.0));
    Real sqrtRhoR_star = std::sqrt(std::max(rhoR_star, 0.0));
    Real signBn = (Bn > 0.0) ? 1.0 : ((Bn < 0.0) ? -1.0 : 0.0);

    Real sqrtSum = sqrtRhoL_star + sqrtRhoR_star;
    Real inv_sqrtSum = (sqrtSum > 1e-30) ? 1.0 / sqrtSum : 0.0;

    Real vt1_dstar =
        (sqrtRhoL_star * vt1L_star + sqrtRhoR_star * vt1R_star + (Bt1R_star - Bt1L_star) * signBn) *
        inv_sqrtSum;
    Real vt2_dstar =
        (sqrtRhoL_star * vt2L_star + sqrtRhoR_star * vt2R_star + (Bt2R_star - Bt2L_star) * signBn) *
        inv_sqrtSum;

    Real Bt1_dstar = (sqrtRhoL_star * Bt1R_star + sqrtRhoR_star * Bt1L_star +
                      sqrtRhoL_star * sqrtRhoR_star * (vt1R_star - vt1L_star) * signBn) *
        inv_sqrtSum;
    Real Bt2_dstar = (sqrtRhoL_star * Bt2R_star + sqrtRhoR_star * Bt2L_star +
                      sqrtRhoL_star * sqrtRhoR_star * (vt2R_star - vt2L_star) * signBn) *
        inv_sqrtSum;

    Real Bv_dstar = Bn * SM + Bt1_dstar * vt1_dstar + Bt2_dstar * vt2_dstar;
    Real tauL_dstar = tauL_star - sqrtRhoL_star * signBn * (BvL_star - Bv_dstar);
    Real tauR_dstar = tauR_star + sqrtRhoR_star * signBn * (BvR_star - Bv_dstar);

    Real SstarL = SM - std::abs(Bn) / (sqrtRhoL_star + 1e-30);
    Real SstarR = SM + std::abs(Bn) / (sqrtRhoR_star + 1e-30);

    std::array<Real, NUM_HYDRO_VARS> FL, FR;
    mhdFlux1D(rhoL,
              vnL,
              vt1L,
              vt2L,
              pressL,
              epsL,
              Bn,
              Bt1L,
              Bt2L,
              uL[iD],
              SnL,
              St1L,
              St2L,
              uL[iTAU],
              ptotL,
              FL);
    mhdFlux1D(rhoR,
              vnR,
              vt1R,
              vt2R,
              pressR,
              epsR,
              Bn,
              Bt1R,
              Bt2R,
              uR[iD],
              SnR,
              St1R,
              St2R,
              uR[iTAU],
              ptotR,
              FR);

    std::array<Real, NUM_HYDRO_VARS> UstarL, UstarR, UdstarL, UdstarR;
    UstarL[iD] = rhoL_star;
    UstarL[iSX] = rhoL_star * vnL_star;
    UstarL[iSY] = rhoL_star * vt1L_star;
    UstarL[iSZ] = rhoL_star * vt2L_star;
    UstarL[iTAU] = tauL_star;
    UstarL[iBX] = Bn;
    UstarL[iBY] = Bt1L_star;
    UstarL[iBZ] = Bt2L_star;

    UstarR[iD] = rhoR_star;
    UstarR[iSX] = rhoR_star * vnR_star;
    UstarR[iSY] = rhoR_star * vt1R_star;
    UstarR[iSZ] = rhoR_star * vt2R_star;
    UstarR[iTAU] = tauR_star;
    UstarR[iBX] = Bn;
    UstarR[iBY] = Bt1R_star;
    UstarR[iBZ] = Bt2R_star;

    UdstarL[iD] = rhoL_star;
    UdstarL[iSX] = rhoL_star * SM;
    UdstarL[iSY] = rhoL_star * vt1_dstar;
    UdstarL[iSZ] = rhoL_star * vt2_dstar;
    UdstarL[iTAU] = tauL_dstar;
    UdstarL[iBX] = Bn;
    UdstarL[iBY] = Bt1_dstar;
    UdstarL[iBZ] = Bt2_dstar;

    UdstarR[iD] = rhoR_star;
    UdstarR[iSX] = rhoR_star * SM;
    UdstarR[iSY] = rhoR_star * vt1_dstar;
    UdstarR[iSZ] = rhoR_star * vt2_dstar;
    UdstarR[iTAU] = tauR_dstar;
    UdstarR[iBX] = Bn;
    UdstarR[iBY] = Bt1_dstar;
    UdstarR[iBZ] = Bt2_dstar;

    std::array<Real, NUM_HYDRO_VARS> F_1d;
    std::array<Real, NUM_HYDRO_VARS> UL_1d, UR_1d;
    UL_1d[iD] = uL[iD];
    UL_1d[iSX] = SnL;
    UL_1d[iSY] = St1L;
    UL_1d[iSZ] = St2L;
    UL_1d[iTAU] = uL[iTAU];
    UL_1d[iBX] = Bn;
    UL_1d[iBY] = Bt1L;
    UL_1d[iBZ] = Bt2L;
    UR_1d[iD] = uR[iD];
    UR_1d[iSX] = SnR;
    UR_1d[iSY] = St1R;
    UR_1d[iSZ] = St2R;
    UR_1d[iTAU] = uR[iTAU];
    UR_1d[iBX] = Bn;
    UR_1d[iBY] = Bt1R;
    UR_1d[iBZ] = Bt2R;

    if (0.0 <= SstarL) {
        for (int n = 0; n < NUM_HYDRO_VARS; ++n)
            F_1d[n] = FL[n] + SL * (UstarL[n] - UL_1d[n]);
    } else if (0.0 <= SM) {
        for (int n = 0; n < NUM_HYDRO_VARS; ++n)
            F_1d[n] = FL[n] + SL * (UstarL[n] - UL_1d[n]) + SstarL * (UdstarL[n] - UstarL[n]);
    } else if (0.0 <= SstarR) {
        for (int n = 0; n < NUM_HYDRO_VARS; ++n)
            F_1d[n] = FR[n] + SR * (UstarR[n] - UR_1d[n]) + SstarR * (UdstarR[n] - UstarR[n]);
    } else {
        for (int n = 0; n < NUM_HYDRO_VARS; ++n)
            F_1d[n] = FR[n] + SR * (UstarR[n] - UR_1d[n]);
    }

    flux[iD] = F_1d[iD];
    flux[iTAU] = F_1d[iTAU];

    if (dir == 0) {
        flux[iSX] = F_1d[iSX];
        flux[iSY] = F_1d[iSY];
        flux[iSZ] = F_1d[iSZ];
        flux[iBX] = F_1d[iBX];
        flux[iBY] = F_1d[iBY];
        flux[iBZ] = F_1d[iBZ];
    } else if (dir == 1) {
        flux[iSX] = F_1d[iSZ];
        flux[iSY] = F_1d[iSX];
        flux[iSZ] = F_1d[iSY];
        flux[iBX] = F_1d[iBZ];
        flux[iBY] = F_1d[iBX];
        flux[iBZ] = F_1d[iBY];
    } else {
        flux[iSX] = F_1d[iSY];
        flux[iSY] = F_1d[iSZ];
        flux[iSZ] = F_1d[iSX];
        flux[iBX] = F_1d[iBY];
        flux[iBY] = F_1d[iBZ];
        flux[iBZ] = F_1d[iBX];
    }

    if (dir == 0)
        flux[iBX] = 0.0;
    if (dir == 1)
        flux[iBY] = 0.0;
    if (dir == 2)
        flux[iBZ] = 0.0;

    (void)epsL;
    (void)epsR;
    (void)vnR_star;
    (void)vnL_star;
}

// ===========================================================================
// Constrained Transport -- Gardiner & Stone (2005) JCP 205, 509
//
// STAGGERED STENCIL REQUIREMENT:
// The CT curl and the divergenceB diagnostic must use the SAME 1-cell
// forward-difference stencil so that div(curl) = 0 holds exactly in
// discrete arithmetic. This is the standard Yee-lattice guarantee.
//
// We adopt:
//   divB[i,j,k] = (Bx[i+1]-Bx[i])/dx + (By[j+1]-By[j])/dy
//               + (Bz[k+1]-Bz[k])/dz
//
// Matching induction update (derived from solenoidality proof):
//   dBx[i,j,k] = dt*( (Ey[i,j,k+1]-Ey[i,j,k])/dz - (Ez[i,j+1,k]-Ez[i,j,k])/dy )
//   dBy[i,j,k] = dt*( (Ez[i+1,j,k]-Ez[i,j,k])/dx - (Ex[i,j,k+1]-Ex[i,j,k])/dz )
//   dBz[i,j,k] = dt*( (Ex[i,j+1,k]-Ex[i,j,k])/dy - (Ey[i+1,j,k]-Ey[i,j,k])/dx )
//
// Proof that D[dB]=0: all mixed second-difference terms cancel pairwise.
// ===========================================================================

void GRMHDEvolution::computeCTUpdate(const GridBlock& spacetime_grid,
                                     const GridBlock& prim_grid,
                                     GridBlock& hydro_grid,
                                     Real dt) const {
    (void)spacetime_grid;

    const int is = hydro_grid.istart();
    const int ie0 = hydro_grid.iend(0);
    const int ie1 = hydro_grid.iend(1);
    const int ie2 = hydro_grid.iend(2);
    const Real inv_dx = 1.0 / hydro_grid.dx(0);
    const Real inv_dy = 1.0 / hydro_grid.dx(1);
    const Real inv_dz = 1.0 / hydro_grid.dx(2);

    constexpr int iVXp = static_cast<int>(PrimitiveVar::VX);
    constexpr int iVYp = static_cast<int>(PrimitiveVar::VY);
    constexpr int iVZp = static_cast<int>(PrimitiveVar::VZ);
    constexpr int iBXh = static_cast<int>(HydroVar::BX);
    constexpr int iBYh = static_cast<int>(HydroVar::BY);
    constexpr int iBZh = static_cast<int>(HydroVar::BZ);

    const int tot0 = hydro_grid.totalCells(0);
    const int tot1 = hydro_grid.totalCells(1);
    const int ncells = tot0 * tot1 * hydro_grid.totalCells(2);

    std::vector<Real> dBx(static_cast<size_t>(ncells), 0.0);
    std::vector<Real> dBy(static_cast<size_t>(ncells), 0.0);
    std::vector<Real> dBz(static_cast<size_t>(ncells), 0.0);

    auto idx = [tot0, tot1](int i, int j, int k) -> size_t {
        return static_cast<size_t>(i + tot0 * (j + tot1 * k));
    };

    // Cell-centered EMFs: E = -(v x B)
    // Ex = -(vy*Bz - vz*By), Ey = -(vz*Bx - vx*Bz), Ez = -(vx*By - vy*Bx)
    auto emfX = [&](int i, int j, int k) -> Real {
        const Real vy = prim_grid.data(iVYp, i, j, k);
        const Real vz = prim_grid.data(iVZp, i, j, k);
        const Real By = hydro_grid.data(iBYh, i, j, k);
        const Real Bz = hydro_grid.data(iBZh, i, j, k);
        return -(vy * Bz - vz * By);
    };
    auto emfY = [&](int i, int j, int k) -> Real {
        const Real vz = prim_grid.data(iVZp, i, j, k);
        const Real vx = prim_grid.data(iVXp, i, j, k);
        const Real Bz = hydro_grid.data(iBZh, i, j, k);
        const Real Bx = hydro_grid.data(iBXh, i, j, k);
        return -(vz * Bx - vx * Bz);
    };
    auto emfZ = [&](int i, int j, int k) -> Real {
        const Real vx = prim_grid.data(iVXp, i, j, k);
        const Real vy = prim_grid.data(iVYp, i, j, k);
        const Real Bx = hydro_grid.data(iBXh, i, j, k);
        const Real By = hydro_grid.data(iBYh, i, j, k);
        return -(vx * By - vy * Bx);
    };

    // Phase 1: compute ALL increments from the ORIGINAL B-field.
    // Forward differences -- matches divergenceB stencil exactly.
    for (int k = is; k < ie2; ++k)
        for (int j = is; j < ie1; ++j)
            for (int i = is; i < ie0; ++i) {
                const size_t flat = idx(i, j, k);

                dBx[flat] = dt *
                    ((emfY(i, j, k + 1) - emfY(i, j, k)) * inv_dz -
                     (emfZ(i, j + 1, k) - emfZ(i, j, k)) * inv_dy);

                dBy[flat] = dt *
                    ((emfZ(i + 1, j, k) - emfZ(i, j, k)) * inv_dx -
                     (emfX(i, j, k + 1) - emfX(i, j, k)) * inv_dz);

                dBz[flat] = dt *
                    ((emfX(i, j + 1, k) - emfX(i, j, k)) * inv_dy -
                     (emfY(i + 1, j, k) - emfY(i, j, k)) * inv_dx);
            }

    // Phase 2: apply all increments simultaneously.
    for (int k = is; k < ie2; ++k)
        for (int j = is; j < ie1; ++j)
            for (int i = is; i < ie0; ++i) {
                const size_t flat = idx(i, j, k);
                hydro_grid.data(iBXh, i, j, k) += dBx[flat];
                hydro_grid.data(iBYh, i, j, k) += dBy[flat];
                hydro_grid.data(iBZh, i, j, k) += dBz[flat];
            }
}

// ===========================================================================
// Divergence diagnostic: max |div B|
//
// Uses 1-cell FORWARD differences -- the SAME stencil as computeCTUpdate.
// This makes the discrete div(curl) = 0 identity hold exactly, so CT
// preserves this diagnostic to machine precision (~1e-15).
//
//   divB[i,j,k] = (Bx[i+1]-Bx[i])/dx + (By[j+1]-By[j])/dy
//               + (Bz[k+1]-Bz[k])/dz
// ===========================================================================

Real GRMHDEvolution::divergenceB(const GridBlock& hydro_grid) {
    constexpr int iBXh = static_cast<int>(HydroVar::BX);
    constexpr int iBYh = static_cast<int>(HydroVar::BY);
    constexpr int iBZh = static_cast<int>(HydroVar::BZ);

    const int is = hydro_grid.istart();
    const int ie0 = hydro_grid.iend(0);
    const int ie1 = hydro_grid.iend(1);
    const int ie2 = hydro_grid.iend(2);
    const Real dxI = 1.0 / hydro_grid.dx(0);
    const Real dyI = 1.0 / hydro_grid.dx(1);
    const Real dzI = 1.0 / hydro_grid.dx(2);

    Real maxDiv = 0.0;

    for (int k = is; k < ie2; ++k)
        for (int j = is; j < ie1; ++j)
            for (int i = is; i < ie0; ++i) {
                // 1-cell forward difference -- dual to CT curl stencil.
                Real dBx_dx =
                    (hydro_grid.data(iBXh, i + 1, j, k) - hydro_grid.data(iBXh, i, j, k)) * dxI;
                Real dBy_dy =
                    (hydro_grid.data(iBYh, i, j + 1, k) - hydro_grid.data(iBYh, i, j, k)) * dyI;
                Real dBz_dz =
                    (hydro_grid.data(iBZh, i, j, k + 1) - hydro_grid.data(iBZh, i, j, k)) * dzI;

                Real divB = std::abs(dBx_dx + dBy_dy + dBz_dz);
                maxDiv = std::max(maxDiv, divB);
            }

    return maxDiv;
}

} // namespace granite::grmhd
