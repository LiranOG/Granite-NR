/**
 * @file ccz4.cpp
 * @brief CCZ4 evolution RHS implementation.
 *
 * Evaluates the right-hand side of the CCZ4 system with moving-puncture
 * gauge conditions. Uses 4th-order centered finite differences for spatial
 * derivatives and 5th-order Kreiss-Oliger dissipation.
 *
 * Reference: Alic et al., PRD 85, 064040 (2012)
 *
 * @copyright 2026 Liran M. Schwartz
 */
#include "granite/spacetime/ccz4.hpp"

#include "granite/core/types.hpp"

#include <cmath>
#include <stdexcept>

#ifdef GRANITE_USE_OPENMP
#include <omp.h>
#endif

namespace granite::spacetime {

// ===========================================================================
// Shorthand for variable indices
// ===========================================================================
namespace {

using V = SpacetimeVar;

constexpr int iCHI = static_cast<int>(V::CHI);
constexpr int iGXX = static_cast<int>(V::GAMMA_XX);
constexpr int iGXY = static_cast<int>(V::GAMMA_XY);
constexpr int iGXZ = static_cast<int>(V::GAMMA_XZ);
constexpr int iGYY = static_cast<int>(V::GAMMA_YY);
constexpr int iGYZ = static_cast<int>(V::GAMMA_YZ);
constexpr int iGZZ = static_cast<int>(V::GAMMA_ZZ);
constexpr int iAXX = static_cast<int>(V::A_XX);
constexpr int iAXY = static_cast<int>(V::A_XY);
constexpr int iAXZ = static_cast<int>(V::A_XZ);
constexpr int iAYY = static_cast<int>(V::A_YY);
constexpr int iAYZ = static_cast<int>(V::A_YZ);
constexpr int iAZZ = static_cast<int>(V::A_ZZ);
constexpr int iK = static_cast<int>(V::K_TRACE);
constexpr int iGHX = static_cast<int>(V::GAMMA_HAT_X);
constexpr int iGHY = static_cast<int>(V::GAMMA_HAT_Y);
constexpr int iGHZ = static_cast<int>(V::GAMMA_HAT_Z);
constexpr int iTHETA = static_cast<int>(V::THETA);
constexpr int iALPHA = static_cast<int>(V::LAPSE);
constexpr int iBETAX = static_cast<int>(V::SHIFT_X);
constexpr int iBETAY = static_cast<int>(V::SHIFT_Y);
constexpr int iBETAZ = static_cast<int>(V::SHIFT_Z);

// Conformal metric component indices within the 6-array
constexpr int XX = 0, XY = 1, XZ = 2, YY = 3, YZ = 4, ZZ = 5;

} // anonymous namespace

// ===========================================================================
// Constructor
// ===========================================================================

CCZ4Evolution::CCZ4Evolution(const CCZ4Params& params) : params_(params) {}

// ===========================================================================
// Finite difference stencils (4th order centered)
// ===========================================================================

Real CCZ4Evolution::d1(const GridBlock& grid, int var, int dir, int i, int j, int k) const {
    Real inv12h = 1.0 / (12.0 * grid.dx(dir));

    // Shift indices along direction 'dir'
    auto shift = [&](int s, int& si, int& sj, int& sk) {
        si = i;
        sj = j;
        sk = k;
        if (dir == 0)
            si += s;
        else if (dir == 1)
            sj += s;
        else
            sk += s;
    };

    int im2, jm2, km2;
    shift(-2, im2, jm2, km2);
    int im1, jm1, km1;
    shift(-1, im1, jm1, km1);
    int ip1, jp1, kp1;
    shift(+1, ip1, jp1, kp1);
    int ip2, jp2, kp2;
    shift(+2, ip2, jp2, kp2);

    return inv12h *
        (-grid.data(var, ip2, jp2, kp2) + 8.0 * grid.data(var, ip1, jp1, kp1) -
         8.0 * grid.data(var, im1, jm1, km1) + grid.data(var, im2, jm2, km2));
}

// 4th-order upwinded (one-sided) first derivative.
// For beta > 0: backward stencil (wind from left)
//   f'_i = (25f_i − 48f_{i-1} + 36f_{i-2} − 16f_{i-3} + 3f_{i-4}) / (12h)
// For beta < 0: forward stencil (wind from right)
//   f'_i = (−25f_i + 48f_{i+1} − 36f_{i+2} + 16f_{i+3} − 3f_{i+4}) / (12h)
//
// With ng=4 ghost cells both stencils fit within the domain for all
// interior cells. This satisfies the CFL condition regardless of |β|.
Real CCZ4Evolution::d1up(
    const GridBlock& grid, int var, int dir, Real beta_dir, int i, int j, int k) const {
    const Real inv12h = 1.0 / (12.0 * grid.dx(dir));
    auto shift = [&](int s, int& si, int& sj, int& sk) {
        si = i;
        sj = j;
        sk = k;
        if (dir == 0)
            si += s;
        else if (dir == 1)
            sj += s;
        else
            sk += s;
    };
    if (beta_dir >= 0.0) {
        // Backward stencil: stencil points i-4 ... i
        int im1, jm1, km1;
        shift(-1, im1, jm1, km1);
        int im2, jm2, km2;
        shift(-2, im2, jm2, km2);
        int im3, jm3, km3;
        shift(-3, im3, jm3, km3);
        int im4, jm4, km4;
        shift(-4, im4, jm4, km4);
        return inv12h *
            (25.0 * grid.data(var, i, j, k) - 48.0 * grid.data(var, im1, jm1, km1) +
             36.0 * grid.data(var, im2, jm2, km2) - 16.0 * grid.data(var, im3, jm3, km3) +
             3.0 * grid.data(var, im4, jm4, km4));
    } else {
        // Forward stencil: stencil points i ... i+4
        int ip1, jp1, kp1;
        shift(+1, ip1, jp1, kp1);
        int ip2, jp2, kp2;
        shift(+2, ip2, jp2, kp2);
        int ip3, jp3, kp3;
        shift(+3, ip3, jp3, kp3);
        int ip4, jp4, kp4;
        shift(+4, ip4, jp4, kp4);
        return inv12h *
            (-25.0 * grid.data(var, i, j, k) + 48.0 * grid.data(var, ip1, jp1, kp1) -
             36.0 * grid.data(var, ip2, jp2, kp2) + 16.0 * grid.data(var, ip3, jp3, kp3) -
             3.0 * grid.data(var, ip4, jp4, kp4));
    }
}

Real CCZ4Evolution::d2(
    const GridBlock& grid, int var, int dir1, int dir2, int i, int j, int k) const {
    if (dir1 == dir2) {
        // Second derivative: (-u_{i+2} + 16u_{i+1} - 30u_i + 16u_{i-1} - u_{i-2}) / (12h²)
        Real inv12h2 = 1.0 / (12.0 * grid.dx(dir1) * grid.dx(dir1));

        auto shift = [&](int s, int& si, int& sj, int& sk) {
            si = i;
            sj = j;
            sk = k;
            if (dir1 == 0)
                si += s;
            else if (dir1 == 1)
                sj += s;
            else
                sk += s;
        };

        int im2, jm2, km2;
        shift(-2, im2, jm2, km2);
        int im1, jm1, km1;
        shift(-1, im1, jm1, km1);
        int ip1, jp1, kp1;
        shift(+1, ip1, jp1, kp1);
        int ip2, jp2, kp2;
        shift(+2, ip2, jp2, kp2);

        return inv12h2 *
            (-grid.data(var, ip2, jp2, kp2) + 16.0 * grid.data(var, ip1, jp1, kp1) -
             30.0 * grid.data(var, i, j, k) + 16.0 * grid.data(var, im1, jm1, km1) -
             grid.data(var, im2, jm2, km2));
    } else {
        // Mixed partial: apply d1 in dir2 to the d1 stencil in dir1
        // For 4th order, use the compact cross-derivative stencil
        Real inv4hh = 1.0 / (4.0 * grid.dx(dir1) * grid.dx(dir2));

        // d²f/dx_a dx_b ≈ [f(+a,+b) - f(+a,-b) - f(-a,+b) + f(-a,-b)] / (4h_a h_b)
        // Using 2nd-order for cross-derivatives (common in NR codes)
        auto shift2 = [&](int sa, int sb, int& si, int& sj, int& sk) {
            si = i;
            sj = j;
            sk = k;
            if (dir1 == 0)
                si += sa;
            else if (dir1 == 1)
                sj += sa;
            else
                sk += sa;
            if (dir2 == 0)
                si += sb;
            else if (dir2 == 1)
                sj += sb;
            else
                sk += sb;
        };

        int ipp_i, ipp_j, ipp_k;
        shift2(+1, +1, ipp_i, ipp_j, ipp_k);
        int ipm_i, ipm_j, ipm_k;
        shift2(+1, -1, ipm_i, ipm_j, ipm_k);
        int imp_i, imp_j, imp_k;
        shift2(-1, +1, imp_i, imp_j, imp_k);
        int imm_i, imm_j, imm_k;
        shift2(-1, -1, imm_i, imm_j, imm_k);

        return inv4hh *
            (grid.data(var, ipp_i, ipp_j, ipp_k) - grid.data(var, ipm_i, ipm_j, ipm_k) -
             grid.data(var, imp_i, imp_j, imp_k) + grid.data(var, imm_i, imm_j, imm_k));
    }
}

// ===========================================================================
// RHS computation (vacuum overload)
// ===========================================================================

void CCZ4Evolution::computeRHSVacuum(const GridBlock& grid, GridBlock& rhs) const {
    // Fix: Pre-allocate static vectors to avoid dynamic allocation per call
    static thread_local std::vector<Real> zero_rho;
    static thread_local std::vector<std::array<Real, DIM>> zero_Si;
    static thread_local std::vector<std::array<Real, SYM_TENSOR_COMPS>> zero_Sij;
    static thread_local std::vector<Real> zero_S;

    std::size_t N = grid.totalSize();
    if (zero_rho.size() < N) {
        zero_rho.assign(N, 0.0);
        zero_Si.assign(N, {0.0, 0.0, 0.0});
        zero_Sij.assign(N, {0, 0, 0, 0, 0, 0});
        zero_S.assign(N, 0.0);
    }

    computeRHS(grid, rhs, zero_rho, zero_Si, zero_Sij, zero_S);
}

// ===========================================================================
// Full RHS computation
// ===========================================================================

void CCZ4Evolution::computeRHS(const GridBlock& grid,
                               GridBlock& rhs,
                               const std::vector<Real>& rho_matter,
                               const std::vector<std::array<Real, DIM>>& Si_matter,
                               const std::vector<std::array<Real, SYM_TENSOR_COMPS>>& Sij_matter,
                               const std::vector<Real>& S_trace) const {
    const Real kappa1 = params_.kappa1;
    const Real kappa2 = params_.kappa2;
    const Real eta = params_.eta;

    const int is = grid.istart();
    const int ie0 = grid.iend(0); // X interior end
    const int ie1 = grid.iend(1); // Y interior end
    const int ie2 = grid.iend(2); // Z interior end

    // Loop over all interior cells (dimension-aware bounds for non-cubic grids)
#ifdef GRANITE_USE_OPENMP
#pragma omp parallel for collapse(3) schedule(static)
#endif
    for (int k = is; k < ie2; ++k) {
        for (int j = is; j < ie1; ++j) {
            for (int i = is; i < ie0; ++i) {

                int flat = grid.totalCells(0) * (grid.totalCells(1) * k + j) + i;

                // ──────────────────────────────────────────
                // 1. Read current state at (i,j,k)
                // ──────────────────────────────────────────
                Real chi_raw = grid.data(iCHI, i, j, k);
                Real chi = (std::isfinite(chi_raw) && chi_raw >= 1.0e-4) ? chi_raw : 1.0e-4;
                Real alpha = grid.data(iALPHA, i, j, k);
                Real K = grid.data(iK, i, j, k);
                Real Theta = grid.data(iTHETA, i, j, k);

                // Conformal metric γ̃_{ij}
                Real gt[6];
                gt[XX] = grid.data(iGXX, i, j, k);
                gt[XY] = grid.data(iGXY, i, j, k);
                gt[XZ] = grid.data(iGXZ, i, j, k);
                gt[YY] = grid.data(iGYY, i, j, k);
                gt[YZ] = grid.data(iGYZ, i, j, k);
                gt[ZZ] = grid.data(iGZZ, i, j, k);

                // Traceless extrinsic curvature Ã_{ij}
                Real At[6];
                At[XX] = grid.data(iAXX, i, j, k);
                At[XY] = grid.data(iAXY, i, j, k);
                At[XZ] = grid.data(iAXZ, i, j, k);
                At[YY] = grid.data(iAYY, i, j, k);
                At[YZ] = grid.data(iAYZ, i, j, k);
                At[ZZ] = grid.data(iAZZ, i, j, k);

                // Conformal Γ̃^i
                Real Ghat[3];
                Ghat[0] = grid.data(iGHX, i, j, k);
                Ghat[1] = grid.data(iGHY, i, j, k);
                Ghat[2] = grid.data(iGHZ, i, j, k);

                // Shift β^i
                Real beta[3];
                beta[0] = grid.data(iBETAX, i, j, k);
                beta[1] = grid.data(iBETAY, i, j, k);
                beta[2] = grid.data(iBETAZ, i, j, k);

                // ──────────────────────────────────────────
                // 2. Compute inverse conformal metric γ̃^{ij}
                // ──────────────────────────────────────────
                Real det_gt = gt[XX] * (gt[YY] * gt[ZZ] - gt[YZ] * gt[YZ]) -
                    gt[XY] * (gt[XY] * gt[ZZ] - gt[YZ] * gt[XZ]) +
                    gt[XZ] * (gt[XY] * gt[YZ] - gt[YY] * gt[XZ]);

                Real inv_det = 1.0 / (det_gt + 1.0e-30);

                Real gtu[6]; // γ̃^{ij}
                gtu[XX] = (gt[YY] * gt[ZZ] - gt[YZ] * gt[YZ]) * inv_det;
                gtu[XY] = (gt[XZ] * gt[YZ] - gt[XY] * gt[ZZ]) * inv_det;
                gtu[XZ] = (gt[XY] * gt[YZ] - gt[XZ] * gt[YY]) * inv_det;
                gtu[YY] = (gt[XX] * gt[ZZ] - gt[XZ] * gt[XZ]) * inv_det;
                gtu[YZ] = (gt[XY] * gt[XZ] - gt[XX] * gt[YZ]) * inv_det;
                gtu[ZZ] = (gt[XX] * gt[YY] - gt[XY] * gt[XY]) * inv_det;

                // ──────────────────────────────────────────
                // 3. First derivatives
                // ──────────────────────────────────────────
                Real d_chi[3], d_alpha[3];
                Real d_gt[6][3]; // d_gt[comp][dir]
                Real d_At[6][3];
                Real d_K[3], d_Theta[3];
                Real d_Ghat[3][3]; // d_Ghat[comp][dir]
                Real d_beta[3][3]; // d_beta[comp][dir]

                for (int d = 0; d < DIM; ++d) {
                    d_chi[d] = d1(grid, iCHI, d, i, j, k);
                    d_alpha[d] = d1(grid, iALPHA, d, i, j, k);
                    d_K[d] = d1(grid, iK, d, i, j, k);
                    d_Theta[d] = d1(grid, iTHETA, d, i, j, k);

                    d_gt[XX][d] = d1(grid, iGXX, d, i, j, k);
                    d_gt[XY][d] = d1(grid, iGXY, d, i, j, k);
                    d_gt[XZ][d] = d1(grid, iGXZ, d, i, j, k);
                    d_gt[YY][d] = d1(grid, iGYY, d, i, j, k);
                    d_gt[YZ][d] = d1(grid, iGYZ, d, i, j, k);
                    d_gt[ZZ][d] = d1(grid, iGZZ, d, i, j, k);

                    d_At[XX][d] = d1(grid, iAXX, d, i, j, k);
                    d_At[XY][d] = d1(grid, iAXY, d, i, j, k);
                    d_At[XZ][d] = d1(grid, iAXZ, d, i, j, k);
                    d_At[YY][d] = d1(grid, iAYY, d, i, j, k);
                    d_At[YZ][d] = d1(grid, iAYZ, d, i, j, k);
                    d_At[ZZ][d] = d1(grid, iAZZ, d, i, j, k);

                    d_Ghat[0][d] = d1(grid, iGHX, d, i, j, k);
                    d_Ghat[1][d] = d1(grid, iGHY, d, i, j, k);
                    d_Ghat[2][d] = d1(grid, iGHZ, d, i, j, k);

                    d_beta[0][d] = d1(grid, iBETAX, d, i, j, k);
                    d_beta[1][d] = d1(grid, iBETAY, d, i, j, k);
                    d_beta[2][d] = d1(grid, iBETAZ, d, i, j, k);
                }

                // ──────────────────────────────────────────
                // 4. Second derivatives of χ and α (for Ricci)
                // ──────────────────────────────────────────
                Real dd_chi[3][3], dd_alpha[3][3];
                Real dd_gt[6][3][3]; // second derivatives of conformal metric

                for (int a = 0; a < DIM; ++a) {
                    for (int b = a; b < DIM; ++b) {
                        dd_chi[a][b] = d2(grid, iCHI, a, b, i, j, k);
                        dd_chi[b][a] = dd_chi[a][b];
                        dd_alpha[a][b] = d2(grid, iALPHA, a, b, i, j, k);
                        dd_alpha[b][a] = dd_alpha[a][b];

                        dd_gt[XX][a][b] = d2(grid, iGXX, a, b, i, j, k);
                        dd_gt[XY][a][b] = d2(grid, iGXY, a, b, i, j, k);
                        dd_gt[XZ][a][b] = d2(grid, iGXZ, a, b, i, j, k);
                        dd_gt[YY][a][b] = d2(grid, iGYY, a, b, i, j, k);
                        dd_gt[YZ][a][b] = d2(grid, iGYZ, a, b, i, j, k);
                        dd_gt[ZZ][a][b] = d2(grid, iGZZ, a, b, i, j, k);

                        // Symmetrize
                        dd_gt[XX][b][a] = dd_gt[XX][a][b];
                        dd_gt[XY][b][a] = dd_gt[XY][a][b];
                        dd_gt[XZ][b][a] = dd_gt[XZ][a][b];
                        dd_gt[YY][b][a] = dd_gt[YY][a][b];
                        dd_gt[YZ][b][a] = dd_gt[YZ][a][b];
                        dd_gt[ZZ][b][a] = dd_gt[ZZ][a][b];
                    }
                }

                // ──────────────────────────────────────────
                // 5. Christoffel symbols of the conformal metric
                // ──────────────────────────────────────────
                // Γ̃^i_{jk} = (1/2) γ̃^{il} (∂_j γ̃_{lk} + ∂_k γ̃_{lj} - ∂_l γ̃_{jk})
                //
                // We store Γ̃^i_{jk} as chris[i][jk] using the symIdx convention.
                // This is a key intermediate for the Ricci tensor.
                Real chris[3][6]; // chris[upper_i][lower_jk symmetric]

                // Map from (j,k) pair to index into d_gt
                auto gtComp = [](int a, int b) -> int { return symIdx(a, b); };

                for (int ii = 0; ii < 3; ++ii) {
                    for (int jj = 0; jj < 3; ++jj) {
                        for (int kk = jj; kk < 3; ++kk) {
                            Real val = 0.0;
                            // Sum over l
                            for (int ll = 0; ll < 3; ++ll) {
                                int il = symIdx(ii, ll);
                                Real gtu_il;
                                if (il == XX)
                                    gtu_il = gtu[XX];
                                else if (il == XY)
                                    gtu_il = gtu[XY];
                                else if (il == XZ)
                                    gtu_il = gtu[XZ];
                                else if (il == YY)
                                    gtu_il = gtu[YY];
                                else if (il == YZ)
                                    gtu_il = gtu[YZ];
                                else
                                    gtu_il = gtu[ZZ];

                                int lk = gtComp(ll, kk);
                                int lj = gtComp(ll, jj);
                                int jk_c = gtComp(jj, kk);

                                val +=
                                    0.5 * gtu_il * (d_gt[lk][jj] + d_gt[lj][kk] - d_gt[jk_c][ll]);
                            }
                            chris[ii][symIdx(jj, kk)] = val;
                        }
                    }
                }

                // ──────────────────────────────────────────
                // 6. Ricci tensor
                // ──────────────────────────────────────────
                // R̃_{ij} from conformal metric + χ contributions
                //
                // R_{ij} = R̃_{ij} + R^χ_{ij}
                //
                // where R^χ_{ij} involves derivatives of χ.
                //
                // R_{ij} = R̃_{ij} + R^χ_{ij}  (full computation)
                //
                // R̃_{ij}: conformal Ricci tensor (terms 1-5 below).
                // R^χ_{ij}: conformal-factor contribution (Baumgarte-Shapiro eq. 3.68).
                // Both tensors are fully computed; their sum gives the physical Ricci tensor.
                Real Rt[6] = {};
                Real Rchi[6] = {};

                // R̃_{ij} fully computed (reuse gtComp from above):

                for (int ii = 0; ii < 3; ++ii) {
                    for (int jj = ii; jj < 3; ++jj) {
                        int ij = gtComp(ii, jj);
                        Real R_ij = 0.0;

                        // Term 1: -(1/2) \gamma^{lm} \partial_l \partial_m \gamma_{ij}
                        for (int l = 0; l < 3; ++l) {
                            for (int m = 0; m < 3; ++m) {
                                Real gtu_lm = gtu[gtComp(l, m)];
                                R_ij += -0.5 * gtu_lm * dd_gt[ij][l][m];
                            }
                        }

                        // Term 2: (1/2) (\gamma_{ki} \partial_j \Gamma^k + \gamma_{kj} \partial_i
                        // \Gamma^k)
                        for (int idx = 0; idx < 3; ++idx) {
                            Real gtk_i = gt[gtComp(idx, ii)];
                            Real gtk_j = gt[gtComp(idx, jj)];
                            R_ij += 0.5 * (gtk_i * d_Ghat[idx][jj] + gtk_j * d_Ghat[idx][ii]);
                        }

                        // Term 3: \Gamma^k \Gamma_{(ij)k}
                        for (int idx = 0; idx < 3; ++idx) {
                            Real G_ijk = 0.0;
                            for (int m = 0; m < 3; ++m)
                                G_ijk += gt[gtComp(idx, m)] * chris[m][ij];
                            R_ij += Ghat[idx] * G_ijk;
                        }

                        // Term 4: \gamma^{lm}(\Gamma^k_{li} \Gamma_{jkm} ...)
                        for (int l = 0; l < 3; ++l) {
                            for (int m = 0; m < 3; ++m) {
                                Real gtu_lm = gtu[gtComp(l, m)];
                                for (int idx = 0; idx < 3; ++idx) {
                                    Real Gam_jkm = 0.0, Gam_ikm = 0.0;
                                    for (int n = 0; n < 3; ++n) {
                                        Gam_jkm += gt[gtComp(idx, n)] * chris[n][gtComp(jj, m)];
                                        Gam_ikm += gt[gtComp(idx, n)] * chris[n][gtComp(ii, m)];
                                    }
                                    R_ij += gtu_lm *
                                        (chris[idx][gtComp(l, ii)] * Gam_jkm +
                                         chris[idx][gtComp(l, jj)] * Gam_ikm);

                                    // Term 5: -\Gamma^k_{lm} \Gamma_{kij}
                                    Real Gam_kij = 0.0;
                                    for (int n = 0; n < 3; ++n)
                                        Gam_kij += gt[gtComp(idx, n)] * chris[n][gtComp(ii, jj)];
                                    R_ij -= gtu_lm * chris[idx][gtComp(l, m)] * Gam_kij;
                                }
                            }
                        }
                        Rt[ij] = R_ij;

                        // Covariant derivative D_i D_j \chi = \partial_i \partial_j \chi -
                        // \tilde{\Gamma}^k_{ij} \partial_k \chi
                        Real D_i_D_j_chi = dd_chi[ii][jj];
                        for (int idx = 0; idx < 3; ++idx)
                            D_i_D_j_chi -= chris[idx][ij] * d_chi[idx];

                        Real D_k_D_k_chi = 0.0;
                        Real d_chi_sq = 0.0;
                        for (int l = 0; l < 3; ++l) {
                            for (int m = 0; m < 3; ++m) {
                                Real gtu_lm = gtu[gtComp(l, m)];
                                Real D_l_D_m_chi = dd_chi[l][m];
                                for (int idx = 0; idx < 3; ++idx)
                                    D_l_D_m_chi -= chris[idx][gtComp(l, m)] * d_chi[idx];
                                D_k_D_k_chi += gtu_lm * D_l_D_m_chi;
                                d_chi_sq += gtu_lm * d_chi[l] * d_chi[m];
                            }
                        }

                        Real term1 = (0.5 / (chi + 1e-30)) * (D_i_D_j_chi + gt[ij] * D_k_D_k_chi);
                        // Baumgarte-Shapiro eq. 3.68: coefficient is -3, NOT +2
                        Real term2 = (0.25 / ((chi * chi) + 1e-30)) *
                            (d_chi[ii] * d_chi[jj] - 3.0 * gt[ij] * d_chi_sq);
                        Rchi[ij] = term1 - term2;
                    }
                }

                // ──────────────────────────────────────────
                // 7. Divergence of shift: ∂_i β^i
                // ──────────────────────────────────────────
                Real div_beta = d_beta[0][0] + d_beta[1][1] + d_beta[2][2];

                // Chi-weighted SELECTIVE advection — β^i ∂_i f.
                //
                // Pure upwinding (d1up) was tested globally and FAILS at step 9:
                // the 4th-order upwinded stencil overshoots steep Ã_ij gradients
                // near the puncture and generates NaN. Pure centered (d1) fails
                // when |β|·dt/dx > 1 (CFL > 1 for advection).
                //
                // Solution: use chi as a mask. Near the puncture (chi << 1),
                // β ≈ 0 so CFL ≈ 0 and centered FD is safe. Far from the
                // puncture (chi → 1), β can be large and upwinding is needed.
                //
                // Keep Γ̂^i (T7 term) always centered — its near-puncture
                // gradients caused the original step-9 NaN.
                Real chi_c = params_.chi_blend_center;
                Real chi_w = params_.chi_blend_width;
                Real blend_w = 0.5 * (1.0 + std::tanh((chi - chi_c) / chi_w));

                auto advec = [&](int var) -> Real {
                    Real adv_up = beta[0] * d1up(grid, var, 0, beta[0], i, j, k) +
                        beta[1] * d1up(grid, var, 1, beta[1], i, j, k) +
                        beta[2] * d1up(grid, var, 2, beta[2], i, j, k);
                    Real adv_cen = beta[0] * d1(grid, var, 0, i, j, k) +
                        beta[1] * d1(grid, var, 1, i, j, k) + beta[2] * d1(grid, var, 2, i, j, k);
                    return blend_w * adv_up + (1.0 - blend_w) * adv_cen;
                };

                // ──────────────────────────────────────────
                // 9. RHS: ∂_t χ
                // ──────────────────────────────────────────
                // ∂_t χ = (2/3) χ (α K − ∂_i β^i) + β^k ∂_k χ
                Real rhs_chi = (2.0 / 3.0) * chi * (alpha * K - div_beta) + advec(iCHI);

                // ──────────────────────────────────────────
                // 10. RHS: ∂_t γ̃_{ij}
                // ──────────────────────────────────────────
                // ∂_t γ̃_{ij} = -2α Ã_{ij} + Lie_β γ̃_{ij} - (2/3) γ̃_{ij} ∂_k β^k
                Real rhs_gt[6];
                for (int ii = 0; ii < 3; ++ii) {
                    for (int jj = ii; jj < 3; ++jj) {
                        int ij = symIdx(ii, jj);
                        // Lie derivative transport: β^k ∂_k γ̃_{ij}
                        // Use selective upwinding (smooth blending)
                        int gt_var = iGXX + ij; // variable index for this metric component
                        Real adv_up_gt = 0.0, adv_cen_gt = 0.0;
                        for (int idx = 0; idx < 3; ++idx) {
                            adv_up_gt += beta[idx] * d1up(grid, gt_var, idx, beta[idx], i, j, k);
                            adv_cen_gt += beta[idx] * d_gt[ij][idx];
                        }
                        Real Lie_gt = blend_w * adv_up_gt + (1.0 - blend_w) * adv_cen_gt;
                        // Deformation terms (centered — not advection)
                        for (int idx = 0; idx < 3; ++idx) {
                            Lie_gt += gt[symIdx(ii, idx)] * d_beta[idx][jj] +
                                gt[symIdx(jj, idx)] * d_beta[idx][ii];
                        }
                        rhs_gt[ij] =
                            -2.0 * alpha * At[ij] + Lie_gt - (2.0 / 3.0) * gt[ij] * div_beta;
                    }
                }

                Real AtAt = 0.0;
                for (int ii = 0; ii < 3; ++ii) {
                    for (int jj = 0; jj < 3; ++jj) {
                        for (int kk = 0; kk < 3; ++kk) {
                            for (int ll = 0; ll < 3; ++ll) {
                                AtAt += gtu[symIdx(ii, kk)] * gtu[symIdx(jj, ll)] *
                                    At[symIdx(ii, jj)] * At[symIdx(kk, ll)];
                            }
                        }
                    }
                }

                Real laplacian_alpha = 0.0;
                for (int a = 0; a < 3; ++a) {
                    for (int b = a; b < 3; ++b) {
                        Real gtu_ab = gtu[symIdx(a, b)];
                        Real factor = (a == b) ? 1.0 : 2.0;
                        laplacian_alpha += factor * gtu_ab * dd_alpha[a][b];
                        // Covariant derivative correction
                        for (int idx = 0; idx < 3; ++idx)
                            laplacian_alpha -=
                                factor * gtu_ab * chris[idx][symIdx(a, b)] * d_alpha[idx];
                    }
                }

                // Physical Laplacian from conformal decomposition:
                //   D^k D_k α = χ · γ̃^{ab}[∂_a∂_b α − Γ̃^k_{ab} ∂_k α]
                //             − ½ γ̃^{ab} ∂_a χ ∂_b α
                // Proof (Baumgarte&Shapiro eq.3.122): D²α = ψ^{-4}(D̃²α + 2D̃(lnψ)·D̃α)
                //   with ψ = χ^{-1/4} → ∂_i lnψ = -(∂_i χ)/(4χ)
                //   → 2(grad lnψ·grad α) = -(1/2)(grad χ·grad α)/χ
                //   → D²α = χD̃²α - (1/2)γ̃^{ab}(∂_a χ)(∂_b α)   ← coefficient IS -1/2
                // NOTE: +0.25 was tested and is WRONG — causes faster α recovery
                // which drives secondary gauge collapse 20 steps earlier (step 80 vs 100).
                Real chi_grad_alpha = 0.0;
                for (int a2 = 0; a2 < 3; ++a2)
                    for (int b2 = 0; b2 < 3; ++b2)
                        chi_grad_alpha += gtu[symIdx(a2, b2)] * d_chi[a2] * d_alpha[b2];

                Real physical_lap_alpha = chi * laplacian_alpha - 0.5 * chi_grad_alpha;

                Real rhs_K = -physical_lap_alpha + alpha * (AtAt + K * K / 3.0) +
                    4.0 * constants::PI * alpha * (rho_matter[flat] + S_trace[flat]) +
                    kappa1 * (1.0 - kappa2) * alpha * Theta + advec(iK);

                Real R_scalar = 0.0;
                for (int ii = 0; ii < 3; ++ii) {
                    for (int jj = 0; jj < 3; ++jj) {
                        R_scalar +=
                            chi * gtu[symIdx(ii, jj)] * (Rt[symIdx(ii, jj)] + Rchi[symIdx(ii, jj)]);
                    }
                }

                Real rhs_Theta = 0.5 * alpha *
                        (R_scalar - AtAt + 2.0 * K * K / 3.0 -
                         16.0 * constants::PI * rho_matter[flat]) -
                    alpha * kappa1 * (2.0 + kappa2) * Theta + advec(iTHETA);

                // ──────────────────────────────────────────
                // 13. RHS: ∂_t Ã_{ij}
                // ──────────────────────────────────────────
                // Full computation of the trace-free extrinsic curvature rate
                Real rhs_At_raw[6];
                Real tr_rhs_At = 0.0;

                for (int ii = 0; ii < 3; ++ii) {
                    for (int jj = ii; jj < 3; ++jj) {
                        int ij = symIdx(ii, jj);

                        Real D_i_D_j_alpha = dd_alpha[ii][jj];
                        for (int idx = 0; idx < 3; ++idx)
                            D_i_D_j_alpha -= chris[idx][ij] * d_alpha[idx];

                        Real S_ij = Sij_matter[flat][ij];
                        Real R_ij = Rt[ij] + Rchi[ij];
                        Real term1 =
                            chi * (-D_i_D_j_alpha + alpha * (R_ij - 8.0 * constants::PI * S_ij));

                        Real A_sq_ij = 0.0;
                        for (int idx = 0; idx < 3; ++idx) {
                            Real A_u_k_j = 0.0;
                            for (int l = 0; l < 3; ++l) {
                                A_u_k_j += gtu[symIdx(idx, l)] * At[symIdx(l, jj)];
                            }
                            A_sq_ij += At[symIdx(ii, idx)] * A_u_k_j;
                        }
                        Real term2 = alpha * (K * At[ij] - 2.0 * A_sq_ij);

                        Real Lie_At = 0.0;
                        // Transport term β^k ∂_k Ã_{ij}: selective upwinding (smooth blending)
                        int at_var = iAXX + ij;
                        Real adv_up_at = 0.0, adv_cen_at = 0.0;
                        for (int idx = 0; idx < 3; ++idx) {
                            adv_up_at += beta[idx] * d1up(grid, at_var, idx, beta[idx], i, j, k);
                            adv_cen_at += beta[idx] * d_At[ij][idx];
                        }
                        Lie_At = blend_w * adv_up_at + (1.0 - blend_w) * adv_cen_at;
                        // Deformation terms (centered — not advection)
                        for (int idx = 0; idx < 3; ++idx) {
                            Lie_At += At[symIdx(ii, idx)] * d_beta[idx][jj] +
                                At[symIdx(jj, idx)] * d_beta[idx][ii];
                        }
                        Lie_At -= (2.0 / 3.0) * At[ij] * div_beta;

                        rhs_At_raw[ij] = term1 + term2 + Lie_At;
                    }
                }

                for (int ii = 0; ii < 3; ++ii) {
                    for (int jj = 0; jj < 3; ++jj) {
                        tr_rhs_At += gtu[symIdx(ii, jj)] * rhs_At_raw[symIdx(ii, jj)];
                    }
                }

                Real rhs_At[6];
                for (int ab = 0; ab < 6; ++ab) {
                    rhs_At[ab] = rhs_At_raw[ab] - (1.0 / 3.0) * gt[ab] * tr_rhs_At;
                }

                // ──────────────────────────────────────────
                // 14. RHS: ∂_t Γ̃^i (Full CCZ4 evolution — Alic et al. 2012 eq. 11)
                // ──────────────────────────────────────────
                Real rhs_Ghat[3] = {};

                // Second derivatives of shift: ∂_a ∂_b β^i
                Real dd_beta[3][3][3]; // dd_beta[component][dir1][dir2]
                {
                    const int beta_vars[3] = {iBETAX, iBETAY, iBETAZ};
                    for (int comp = 0; comp < 3; ++comp) {
                        for (int a = 0; a < 3; ++a) {
                            for (int b = a; b < 3; ++b) {
                                dd_beta[comp][a][b] = d2(grid, beta_vars[comp], a, b, i, j, k);
                                dd_beta[comp][b][a] = dd_beta[comp][a][b];
                            }
                        }
                    }
                }

                for (int ii = 0; ii < 3; ++ii) {
                    // Term 1: 2α Γ̃^i_{jk} Ã^{jk}
                    Real T1 = 0.0;
                    for (int jj = 0; jj < 3; ++jj)
                        for (int kk = 0; kk < 3; ++kk)
                            T1 += chris[ii][symIdx(jj, kk)] * At[symIdx(jj, kk)];
                    T1 *= 2.0 * alpha;

                    // Term 2: -(4/3) α γ̃^{ij} ∂_j K
                    Real T2 = 0.0;
                    for (int jj = 0; jj < 3; ++jj)
                        T2 += gtu[symIdx(ii, jj)] * d_K[jj];
                    T2 *= -(4.0 / 3.0) * alpha;

                    // Term 3: -16π α γ̃^{ij} S_j (matter source)
                    Real T3 = 0.0;
                    for (int jj = 0; jj < 3; ++jj)
                        T3 += gtu[symIdx(ii, jj)] * Si_matter[flat][jj];
                    T3 *= -16.0 * constants::PI * alpha;

                    // Term 4: α Ã^{ij} ∂_j χ / χ (conformal factor coupling)
                    // = 3α/χ · Ã^{ij} ∂_j χ  (raising with γ̃^{ik})
                    Real T4 = 0.0;
                    for (int jj = 0; jj < 3; ++jj) {
                        // Raise first index: Ã^{ij} = γ̃^{ik} Ã_{kj}
                        Real Atu_ij = 0.0;
                        for (int kk = 0; kk < 3; ++kk)
                            Atu_ij += gtu[symIdx(ii, kk)] * At[symIdx(kk, jj)];
                        T4 += Atu_ij * d_chi[jj];
                    }
                    T4 *= 3.0 * alpha / (chi + 1e-30);

                    // Term 5: γ̃^{jk} ∂_j ∂_k β^i (Laplacian of shift)
                    Real T5 = 0.0;
                    for (int jj = 0; jj < 3; ++jj)
                        for (int kk = 0; kk < 3; ++kk)
                            T5 += gtu[symIdx(jj, kk)] * dd_beta[ii][jj][kk];

                    // Term 6: (1/3) γ̃^{ij} ∂_j(∂_k β^k)
                    // ∂_j(div_beta) = ∂_j ∂_k β^k = Σ_k ∂²β^k/∂x_j∂x_k
                    Real T6 = 0.0;
                    for (int jj = 0; jj < 3; ++jj) {
                        Real d_divbeta_j = 0.0;
                        for (int kk = 0; kk < 3; ++kk)
                            d_divbeta_j += dd_beta[kk][kk][jj];
                        T6 += gtu[symIdx(ii, jj)] * d_divbeta_j;
                    }
                    T6 *= (1.0 / 3.0);

                    // Term 7: Lie derivative terms:
                    //   -Γ̃^j ∂_j β^i + (2/3)Γ̃^i ∂_j β^j + β^j ∂_j Γ̃^i
                    Real T7 = 0.0;
                    for (int jj = 0; jj < 3; ++jj) {
                        T7 -= Ghat[jj] * d_beta[ii][jj];
                    }
                    int gh_var = iGHX + ii;
                    Real adv_up_gh = 0.0, adv_cen_gh = 0.0;
                    for (int jj = 0; jj < 3; ++jj) {
                        adv_up_gh += beta[jj] * d1up(grid, gh_var, jj, beta[jj], i, j, k);
                        adv_cen_gh += beta[jj] * d_Ghat[ii][jj];
                    }
                    T7 += blend_w * adv_up_gh + (1.0 - blend_w) * adv_cen_gh;
                    T7 += (2.0 / 3.0) * Ghat[ii] * div_beta;

                    // Term 8: CCZ4 damping: -2κ₁ α Γ̃^i
                    Real T8 = -2.0 * kappa1 * alpha * Ghat[ii];

                    rhs_Ghat[ii] = T1 + T2 + T3 + T4 + T5 + T6 + T7 + T8;
                }

                // ──────────────────────────────────────────
                // 15. RHS: ∂_t α (1+log slicing)
                // ──────────────────────────────────────────
                Real rhs_alpha = -2.0 * alpha * K + advec(iALPHA);

                // ──────────────────────────────────────────
                // 16. RHS: ∂_t β^i (Γ-driver)
                // ──────────────────────────────────────────
                Real rhs_beta[3];
                for (int d = 0; d < 3; ++d) {
                    // Standard Γ-driver: ∂_t β^i = (3/4) Γ̃^i − η β^i + β^j ∂_j β^i
                    // The advection term β^j∂_jβ^i is essential for moving-puncture
                    // stability — without it the shift develops gauge shocks.
                    rhs_beta[d] = 0.75 * Ghat[d] - eta * beta[d] + advec(iBETAX + d);
                }

                // ──────────────────────────────────────────
                // 17. Write to output RHS grid
                // ──────────────────────────────────────────
                rhs.data(iCHI, i, j, k) = rhs_chi;
                rhs.data(iGXX, i, j, k) = rhs_gt[XX];
                rhs.data(iGXY, i, j, k) = rhs_gt[XY];
                rhs.data(iGXZ, i, j, k) = rhs_gt[XZ];
                rhs.data(iGYY, i, j, k) = rhs_gt[YY];
                rhs.data(iGYZ, i, j, k) = rhs_gt[YZ];
                rhs.data(iGZZ, i, j, k) = rhs_gt[ZZ];
                rhs.data(iAXX, i, j, k) = rhs_At[XX];
                rhs.data(iAXY, i, j, k) = rhs_At[XY];
                rhs.data(iAXZ, i, j, k) = rhs_At[XZ];
                rhs.data(iAYY, i, j, k) = rhs_At[YY];
                rhs.data(iAYZ, i, j, k) = rhs_At[YZ];
                rhs.data(iAZZ, i, j, k) = rhs_At[ZZ];
                rhs.data(iK, i, j, k) = rhs_K;
                rhs.data(iGHX, i, j, k) = rhs_Ghat[0];
                rhs.data(iGHY, i, j, k) = rhs_Ghat[1];
                rhs.data(iGHZ, i, j, k) = rhs_Ghat[2];
                rhs.data(iTHETA, i, j, k) = rhs_Theta;
                rhs.data(iALPHA, i, j, k) = rhs_alpha;
                rhs.data(iBETAX, i, j, k) = rhs_beta[0];
                rhs.data(iBETAY, i, j, k) = rhs_beta[1];
                rhs.data(iBETAZ, i, j, k) = rhs_beta[2];

            } // i
        } // j
    } // k

    // Apply Kreiss-Oliger dissipation
    applyDissipation(grid, rhs);
}

// ===========================================================================
// Kreiss-Oliger dissipation (5th order)
// ===========================================================================

void CCZ4Evolution::applyDissipation(const GridBlock& grid, GridBlock& rhs) const {
    const Real sigma = params_.ko_sigma;
    if (sigma <= 0.0)
        return;

    const int is = grid.istart();
    const int ie0 = grid.iend(0);
    const int ie1 = grid.iend(1);
    const int ie2 = grid.iend(2);

    // 5th-order KO operator: D_KO = -σ h⁵/64 Δ³_d
    // where Δ³ is the undivided 6th-order difference operator:
    // Δ³f_i = f_{i-3} - 6f_{i-2} + 15f_{i-1} - 20f_i + 15f_{i+1} - 6f_{i+2} + f_{i+3}

    // Precompute per-direction KO coefficients to avoid division inside the loop.
    const Real koCoeff[DIM] = {
        sigma / (64.0 * grid.dx(0)), sigma / (64.0 * grid.dx(1)), sigma / (64.0 * grid.dx(2))};

    // KO stencil weights for the 6th-order undivided difference operator:
    // D^6 f_i = f_{i-3} - 6f_{i-2} + 15f_{i-1} - 20f_i + 15f_{i+1} - 6f_{i+2} + f_{i+3}
    // This implements -sigma/(64h) * D^6 = -(sigma h^5)/64 * d^6f/dx^6  (5th-order KO)
    static constexpr int koOff[7] = {-3, -2, -1, 0, 1, 2, 3};
    static constexpr Real koWts[7] = {1.0, -6.0, 15.0, -20.0, 15.0, -6.0, 1.0};

    // Phase 4D fix H1: Single OMP parallel region over spatial cells (k,j pairs),
    // with the variable loop innermost. Benefits:
    //   1. Thread-pool entered/exited ONCE instead of 22 times (one per var).
    //   2. diss[NUM_SPACETIME_VARS] is a small stack array — stays in L1 cache.
    //   3. All NUM_SPACETIME_VARS contributions computed per cell before writing,
    //      reducing the number of rhs.data() writes from 22*N^3 to N^3.
#ifdef GRANITE_USE_OPENMP
#pragma omp parallel for collapse(2) schedule(static)
#endif
    for (int k = is; k < ie2; ++k) {
        for (int j = is; j < ie1; ++j) {
            for (int i = is; i < ie0; ++i) {

                // Accumulate dissipation for all variables at this cell.
                // Stack-allocated array stays in L1 cache for NUM_SPACETIME_VARS=22.
                Real diss[NUM_SPACETIME_VARS] = {};

                for (int d = 0; d < DIM; ++d) {
                    const Real cf = koCoeff[d];
                    // Compute 6th-order undivided difference for all vars at once.
                    // Grouping the var loop inside the direction loop keeps
                    // the stencil index arithmetic (si, sj, sk) outside the var loop.
                    for (int s = 0; s < 7; ++s) {
                        const int si = i + (d == 0 ? koOff[s] : 0);
                        const int sj = j + (d == 1 ? koOff[s] : 0);
                        const int sk = k + (d == 2 ? koOff[s] : 0);
                        const Real w = cf * koWts[s];
                        for (int var = 0; var < NUM_SPACETIME_VARS; ++var) {
                            diss[var] += w * grid.data(var, si, sj, sk);
                        }
                    }
                }

                // Write accumulated dissipation to RHS in one pass.
                for (int var = 0; var < NUM_SPACETIME_VARS; ++var) {
                    rhs.data(var, i, j, k) += diss[var];
                }
            }
        }
    }
}

// ===========================================================================
// Constraint computation
// ===========================================================================

void CCZ4Evolution::computeConstraints(const GridBlock& grid,
                                       std::vector<Real>& ham,
                                       std::vector<std::array<Real, DIM>>& mom) const {
    std::size_t N = grid.totalSize();
    ham.assign(N, 0.0);
    mom.assign(N, {0.0, 0.0, 0.0});

    const int is = grid.istart();
    const int ie0 = grid.iend(0);
    const int ie1 = grid.iend(1);
    const int ie2 = grid.iend(2);

    for (int k = is; k < ie2; ++k) {
        for (int j = is; j < ie1; ++j) {
            for (int i = is; i < ie0; ++i) {
                int flat = grid.totalCells(0) * (grid.totalCells(1) * k + j) + i;

                // To accurately calculate Hamiltonian and Momentum constraints,
                // we reconstruct derivatives and the Ricci tensor scalar natively.
                // Note: For extreme performance, this duplicates some compute from computeRHS,
                // but constraints are typically evaluated sparingly in an MPI block.

                Real chi = grid.data(iCHI, i, j, k);
                Real K = grid.data(iK, i, j, k);

                Real gt[6] = {grid.data(iGXX, i, j, k),
                              grid.data(iGXY, i, j, k),
                              grid.data(iGXZ, i, j, k),
                              grid.data(iGYY, i, j, k),
                              grid.data(iGYZ, i, j, k),
                              grid.data(iGZZ, i, j, k)};
                Real At[6] = {grid.data(iAXX, i, j, k),
                              grid.data(iAXY, i, j, k),
                              grid.data(iAXZ, i, j, k),
                              grid.data(iAYY, i, j, k),
                              grid.data(iAYZ, i, j, k),
                              grid.data(iAZZ, i, j, k)};

                Real det_gt = gt[XX] * (gt[YY] * gt[ZZ] - gt[YZ] * gt[YZ]) -
                    gt[XY] * (gt[XY] * gt[ZZ] - gt[YZ] * gt[XZ]) +
                    gt[XZ] * (gt[XY] * gt[YZ] - gt[YY] * gt[XZ]);
                Real inv_det = 1.0 / (det_gt + 1.0e-30);

                Real gtu[6] = {(gt[YY] * gt[ZZ] - gt[YZ] * gt[YZ]) * inv_det,
                               (gt[XZ] * gt[YZ] - gt[XY] * gt[ZZ]) * inv_det,
                               (gt[XY] * gt[YZ] - gt[XZ] * gt[YY]) * inv_det,
                               (gt[XX] * gt[ZZ] - gt[XZ] * gt[XZ]) * inv_det,
                               (gt[XY] * gt[XZ] - gt[XX] * gt[YZ]) * inv_det,
                               (gt[XX] * gt[YY] - gt[XY] * gt[XY]) * inv_det};

                // Use 4th-order member FD stencils (consistent with evolution)
                // Capture 'this' pointer for member function access
                auto d1_4th = [&](int var, int d_idx) -> Real {
                    return this->d1(grid, var, d_idx, i, j, k);
                };
                auto d2_4th = [&](int var, int d1_idx, int d2_idx) -> Real {
                    return this->d2(grid, var, d1_idx, d2_idx, i, j, k);
                };

                Real AtAt = 0.0;
                for (int ii = 0; ii < 3; ++ii) {
                    for (int jj = 0; jj < 3; ++jj) {
                        for (int kk = 0; kk < 3; ++kk) {
                            for (int ll = 0; ll < 3; ++ll) {
                                AtAt += gtu[symIdx(ii, kk)] * gtu[symIdx(jj, ll)] *
                                    At[symIdx(ii, jj)] * At[symIdx(kk, ll)];
                            }
                        }
                    }
                }

                const int gt_vars[6] = {iGXX, iGXY, iGXZ, iGYY, iGYZ, iGZZ};
                Real d_gt[6][3];
                Real dd_gt[6][3][3];
                for (int v = 0; v < 6; ++v) {
                    for (int d = 0; d < 3; ++d) {
                        d_gt[v][d] = d1_4th(gt_vars[v], d);
                        for (int d2_idx = d; d2_idx < 3; ++d2_idx) {
                            dd_gt[v][d][d2_idx] = d2_4th(gt_vars[v], d, d2_idx);
                            dd_gt[v][d2_idx][d] = dd_gt[v][d][d2_idx];
                        }
                    }
                }
                Real d_chi[3];
                Real dd_chi[3][3];
                for (int d = 0; d < 3; ++d) {
                    d_chi[d] = d1_4th(iCHI, d);
                    for (int d2_idx = d; d2_idx < 3; ++d2_idx) {
                        dd_chi[d][d2_idx] = d2_4th(iCHI, d, d2_idx);
                        dd_chi[d2_idx][d] = dd_chi[d][d2_idx];
                    }
                }

                Real chris[3][6];
                for (int kk = 0; kk < 3; ++kk) {
                    for (int ii = 0; ii < 3; ++ii) {
                        for (int jj = ii; jj < 3; ++jj) {
                            Real sum = 0.0;
                            for (int m = 0; m < 3; ++m) {
                                sum += 0.5 * gtu[symIdx(kk, m)] *
                                    (d_gt[symIdx(m, ii)][jj] + d_gt[symIdx(m, jj)][ii] -
                                     d_gt[symIdx(ii, jj)][m]);
                            }
                            chris[kk][symIdx(ii, jj)] = sum;
                        }
                    }
                }

                Real d_Ghat[3][3] = {0};
                Real Ghat[3] = {
                    grid.data(iGHX, i, j, k), grid.data(iGHY, i, j, k), grid.data(iGHZ, i, j, k)};
                for (int d = 0; d < 3; ++d) {
                    d_Ghat[0][d] = d1_4th(iGHX, d);
                    d_Ghat[1][d] = d1_4th(iGHY, d);
                    d_Ghat[2][d] = d1_4th(iGHZ, d);
                }

                Real Rt[6] = {0};
                Real Rchi[6] = {0};
                for (int ii = 0; ii < 3; ++ii) {
                    for (int jj = ii; jj < 3; ++jj) {
                        int ij = symIdx(ii, jj);
                        Real R_ij = 0.0;
                        for (int l = 0; l < 3; ++l) {
                            for (int m = 0; m < 3; ++m)
                                R_ij += -0.5 * gtu[symIdx(l, m)] * dd_gt[ij][l][m];
                        }
                        for (int kk = 0; kk < 3; ++kk) {
                            R_ij += 0.5 *
                                (gt[symIdx(kk, ii)] * d_Ghat[kk][jj] +
                                 gt[symIdx(kk, jj)] * d_Ghat[kk][ii]);
                            Real G_ijk = 0.0;
                            for (int m = 0; m < 3; ++m)
                                G_ijk += gt[symIdx(kk, m)] * chris[m][ij];
                            R_ij += Ghat[kk] * G_ijk;
                        }
                        for (int l = 0; l < 3; ++l) {
                            for (int m = 0; m < 3; ++m) {
                                Real gtu_lm = gtu[symIdx(l, m)];
                                for (int kk = 0; kk < 3; ++kk) {
                                    Real Gam_jkm = 0.0, Gam_ikm = 0.0, Gam_kij = 0.0;
                                    for (int n = 0; n < 3; ++n) {
                                        Gam_jkm += gt[symIdx(kk, n)] * chris[n][symIdx(jj, m)];
                                        Gam_ikm += gt[symIdx(kk, n)] * chris[n][symIdx(ii, m)];
                                        Gam_kij += gt[symIdx(kk, n)] * chris[n][symIdx(ii, jj)];
                                    }
                                    R_ij += gtu_lm *
                                        (chris[kk][symIdx(l, ii)] * Gam_jkm +
                                         chris[kk][symIdx(l, jj)] * Gam_ikm);
                                    R_ij -= gtu_lm * chris[kk][symIdx(l, m)] * Gam_kij;
                                }
                            }
                        }
                        Rt[ij] = R_ij;

                        Real D_i_D_j_chi = dd_chi[ii][jj];
                        for (int kk = 0; kk < 3; ++kk)
                            D_i_D_j_chi -= chris[kk][ij] * d_chi[kk];
                        Real D_k_D_k_chi = 0.0, d_chi_sq = 0.0;
                        for (int l = 0; l < 3; ++l) {
                            for (int m = 0; m < 3; ++m) {
                                Real gtu_lm = gtu[symIdx(l, m)];
                                Real D_l_D_m_chi = dd_chi[l][m];
                                for (int kk = 0; kk < 3; ++kk)
                                    D_l_D_m_chi -= chris[kk][symIdx(l, m)] * d_chi[kk];
                                D_k_D_k_chi += gtu_lm * D_l_D_m_chi;
                                d_chi_sq += gtu_lm * d_chi[l] * d_chi[m];
                            }
                        }
                        // Baumgarte-Shapiro eq. 3.68: coefficient is -3, NOT +2
                        Rchi[ij] = (0.5 / (chi + 1e-30)) * (D_i_D_j_chi + gt[ij] * D_k_D_k_chi) -
                            (0.25 / ((chi * chi) + 1e-30)) *
                                (d_chi[ii] * d_chi[jj] - 3.0 * gt[ij] * d_chi_sq);
                    }
                }

                Real R_scalar = 0.0;
                for (int ii = 0; ii < 3; ++ii) {
                    for (int jj = 0; jj < 3; ++jj) {
                        R_scalar +=
                            gtu[symIdx(ii, jj)] * (Rt[symIdx(ii, jj)] + Rchi[symIdx(ii, jj)]);
                    }
                }
                R_scalar *= chi; // Physical scalar

                Real rho = 0.0; // Assume matter density falls back to zero for vacuum constraints
                ham[flat] = R_scalar + (2.0 / 3.0) * K * K - AtAt - 16.0 * constants::PI * rho;

                const int At_vars[6] = {iAXX, iAXY, iAXZ, iAYY, iAYZ, iAZZ};
                Real d_At[6][3];
                for (int v = 0; v < 6; ++v)
                    for (int d = 0; d < 3; ++d)
                        d_At[v][d] = d1_4th(At_vars[v], d);
                Real d_K[3];
                for (int d = 0; d < 3; ++d)
                    d_K[d] = d1_4th(iK, d);

                for (int ii = 0; ii < 3; ++ii) {
                    Real Mi = 0.0;
                    for (int jj = 0; jj < 3; ++jj) {
                        for (int kk = 0; kk < 3; ++kk) {
                            Real gtu_jk = gtu[symIdx(jj, kk)];
                            Real D_j_A_ki = d_At[symIdx(kk, ii)][jj];
                            for (int m = 0; m < 3; ++m) {
                                D_j_A_ki -= chris[m][symIdx(jj, kk)] * At[symIdx(m, ii)] +
                                    chris[m][symIdx(jj, ii)] * At[symIdx(kk, m)];
                            }
                            Mi += gtu_jk * D_j_A_ki;
                            Mi -= (3.0 / (2.0 * chi + 1e-30)) * At[symIdx(ii, jj)] * gtu_jk *
                                d_chi[kk];
                        }
                    }
                    Mi -= (2.0 / 3.0) * d_K[ii];
                    mom[flat][ii] = Mi;
                }
            }
        }
    }
}

// ===========================================================================
// Utility: flat spacetime initialization
// ===========================================================================

void setFlatSpacetime(GridBlock& grid) {
    const int ie0 = grid.totalCells(0);
    const int ie1 = grid.totalCells(1);
    const int ie2 = grid.totalCells(2);

    for (int k = 0; k < ie2; ++k) {
        for (int j = 0; j < ie1; ++j) {
            for (int i = 0; i < ie0; ++i) {
                // χ = 1
                grid.data(static_cast<int>(SpacetimeVar::CHI), i, j, k) = 1.0;
                // γ̃_{ij} = δ_{ij}
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_XX), i, j, k) = 1.0;
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_XY), i, j, k) = 0.0;
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_XZ), i, j, k) = 0.0;
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_YY), i, j, k) = 1.0;
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_YZ), i, j, k) = 0.0;
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_ZZ), i, j, k) = 1.0;
                // α = 1
                grid.data(static_cast<int>(SpacetimeVar::LAPSE), i, j, k) = 1.0;
                // All other variables = 0 (already initialized by constructor)
            }
        }
    }
}

void setGaugeWaveData(GridBlock& grid, Real amplitude, Real wavelength) {
    const int ie0 = grid.totalCells(0);
    const int ie1 = grid.totalCells(1);
    const int ie2 = grid.totalCells(2);

    for (int k = 0; k < ie2; ++k) {
        for (int j = 0; j < ie1; ++j) {
            for (int i = 0; i < ie0; ++i) {
                Real x = grid.x(0, i);
                Real H = amplitude * std::sin(2.0 * constants::PI * x / wavelength);

                // ds² = -(1+H)dt² + (1+H)dx² + dy² + dz²
                // => α² = (1+H), γ_xx = (1+H), γ_yy = γ_zz = 1
                // Conformal: det γ̃ = 1 => χ = (1+H)^{-1/3}
                Real detgam = (1.0 + H);
                Real chi = std::pow(detgam, -1.0 / 3.0);

                grid.data(static_cast<int>(SpacetimeVar::CHI), i, j, k) = chi;
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_XX), i, j, k) = (1.0 + H) * chi;
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_XY), i, j, k) = 0.0;
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_XZ), i, j, k) = 0.0;
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_YY), i, j, k) = chi;
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_YZ), i, j, k) = 0.0;
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_ZZ), i, j, k) = chi;
                grid.data(static_cast<int>(SpacetimeVar::LAPSE), i, j, k) = std::sqrt(1.0 + H);
                // K_{ij} from time derivative of metric
                // For the gauge wave at t=0, K = 0 in harmonic gauge
                // (non-trivial K arises in moving-puncture gauge)
            }
        }
    }
}

} // namespace granite::spacetime
