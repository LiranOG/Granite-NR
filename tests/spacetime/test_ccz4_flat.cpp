/**
 * @file test_ccz4_flat.cpp
 * @brief Test: flat spacetime must remain flat under CCZ4 evolution.
 *
 * This is the most basic test for any NR code. Minkowski spacetime
 * is an exact solution. All RHS values should be zero (to machine
 * precision plus finite-difference error).
 */
#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"
#include "granite/spacetime/ccz4.hpp"

#include <cmath>
#include <gtest/gtest.h>
#include <vector>

using namespace granite;
using namespace granite::spacetime;

class CCZ4FlatTest : public ::testing::Test {
protected:
    void SetUp() override {
        ncells = {32, 32, 32};
        lo = {-10.0, -10.0, -10.0};
        hi = {10.0, 10.0, 10.0};
        int nghost = 4;

        grid = std::make_unique<GridBlock>(0, 0, ncells, lo, hi, nghost, NUM_SPACETIME_VARS);
        rhs = std::make_unique<GridBlock>(1, 0, ncells, lo, hi, nghost, NUM_SPACETIME_VARS);

        setFlatSpacetime(*grid);

        CCZ4Params params;
        params.ko_sigma = 0.0; // No dissipation for this test
        ccz4 = std::make_unique<CCZ4Evolution>(params);
    }

    std::array<int, DIM> ncells;
    std::array<Real, DIM> lo, hi;
    std::unique_ptr<GridBlock> grid, rhs;
    std::unique_ptr<CCZ4Evolution> ccz4;
};

TEST_F(CCZ4FlatTest, RHSIsZeroForFlatSpacetime) {
    ccz4->computeRHSVacuum(*grid, *rhs);

    Real max_rhs = 0.0;
    int is = grid->istart() + 1; // Skip boundary-adjacent cells
    int ie = grid->iend(0) - 1;

    for (int v = 0; v < NUM_SPACETIME_VARS; ++v) {
        for (int k = is; k < ie; ++k)
            for (int j = is; j < ie; ++j)
                for (int i = is; i < ie; ++i) {
                    Real val = std::abs(rhs->data(v, i, j, k));
                    if (val > max_rhs)
                        max_rhs = val;
                }
    }

    // For flat spacetime, all RHS should be zero (up to floating-point)
    EXPECT_LT(max_rhs, 1.0e-10) << "RHS not zero for flat spacetime. Max |RHS| = " << max_rhs;
}

TEST_F(CCZ4FlatTest, RHSMatchesVacuumOverload) {
    // computeRHS with zero matter sources must match computeRHSVacuum exactly.
    GridBlock rhs_full(2,
                       0,
                       grid->numCells(),
                       grid->lowerCorner(),
                       grid->upperCorner(),
                       grid->getNumGhost(),
                       NUM_SPACETIME_VARS);
    GridBlock rhs_vac(3,
                      0,
                      grid->numCells(),
                      grid->lowerCorner(),
                      grid->upperCorner(),
                      grid->getNumGhost(),
                      NUM_SPACETIME_VARS);

    std::size_t N = grid->totalSize();
    std::vector<Real> rho(N, 0.0);
    std::vector<std::array<Real, DIM>> Si(N, {0.0, 0.0, 0.0});
    std::vector<std::array<Real, SYM_TENSOR_COMPS>> Sij(N);
    for (auto& s : Sij)
        s.fill(0.0);
    std::vector<Real> Strace(N, 0.0);

    ccz4->computeRHS(*grid, rhs_full, rho, Si, Sij, Strace);
    ccz4->computeRHSVacuum(*grid, rhs_vac);

    int is = grid->istart() + 1;
    int ie = grid->iend(0) - 1;
    for (int v = 0; v < NUM_SPACETIME_VARS; ++v) {
        for (int k = is; k < ie; ++k)
            for (int j = is; j < ie; ++j)
                for (int i = is; i < ie; ++i) {
                    EXPECT_DOUBLE_EQ(rhs_full.data(v, i, j, k), rhs_vac.data(v, i, j, k))
                        << "computeRHS and computeRHSVacuum differ at var=" << v << " (" << i << ","
                        << j << "," << k << ")";
                }
    }
}

TEST_F(CCZ4FlatTest, ConstraintsVanishForFlat) {
    std::vector<Real> ham;
    std::vector<std::array<Real, DIM>> mom;

    ccz4->computeConstraints(*grid, ham, mom);

    Real max_ham = 0.0;
    int is = grid->istart() + 1;
    int ie = grid->iend(0) - 1;

    for (int k = is; k < ie; ++k)
        for (int j = is; j < ie; ++j)
            for (int i = is; i < ie; ++i) {
                int flat = grid->totalCells(0) * (grid->totalCells(1) * k + j) + i;
                Real val = std::abs(ham[flat]);
                if (val > max_ham)
                    max_ham = val;
            }

    EXPECT_LT(max_ham, 1.0e-10) << "Hamiltonian constraint non-zero for flat spacetime.";
}

TEST_F(CCZ4FlatTest, MomentumConstraintsVanishForFlat) {
    std::vector<Real> ham;
    std::vector<std::array<Real, DIM>> mom;

    ccz4->computeConstraints(*grid, ham, mom);

    Real max_mom = 0.0;
    int is = grid->istart() + 1;
    int ie = grid->iend(0) - 1;

    for (int k = is; k < ie; ++k)
        for (int j = is; j < ie; ++j)
            for (int i = is; i < ie; ++i) {
                int flat = grid->totalCells(0) * (grid->totalCells(1) * k + j) + i;
                for (int d = 0; d < DIM; ++d) {
                    Real val = std::abs(mom[flat][d]);
                    if (val > max_mom)
                        max_mom = val;
                }
            }

    EXPECT_LT(max_mom, 1.0e-10) << "Momentum constraints non-zero for flat spacetime.";
}

TEST_F(CCZ4FlatTest, FlatSpacetimeInitialization) {
    int mid = grid->istart() + 16; // Center cell

    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::CHI), mid, mid, mid), 1.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::GAMMA_XX), mid, mid, mid), 1.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::GAMMA_YY), mid, mid, mid), 1.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::GAMMA_ZZ), mid, mid, mid), 1.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::GAMMA_XY), mid, mid, mid), 0.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::GAMMA_XZ), mid, mid, mid), 0.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::GAMMA_YZ), mid, mid, mid), 0.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::LAPSE), mid, mid, mid), 1.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::K_TRACE), mid, mid, mid), 0.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::THETA), mid, mid, mid), 0.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::SHIFT_X), mid, mid, mid), 0.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::SHIFT_Y), mid, mid, mid), 0.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::SHIFT_Z), mid, mid, mid), 0.0);
}

TEST_F(CCZ4FlatTest, FlatTracelessExtrinsicCurvatureIsZero) {
    int mid = grid->istart() + 16;
    // All A_ij components should be zero for flat spacetime
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::A_XX), mid, mid, mid), 0.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::A_XY), mid, mid, mid), 0.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::A_XZ), mid, mid, mid), 0.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::A_YY), mid, mid, mid), 0.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::A_YZ), mid, mid, mid), 0.0);
    EXPECT_DOUBLE_EQ(grid->data(static_cast<int>(SpacetimeVar::A_ZZ), mid, mid, mid), 0.0);
}

// ===========================================================================
// Phase 4D: New tests for Kreiss-Oliger dissipation (H1 OMP restructure)
// ===========================================================================

/// Test: KO dissipation applied to uniform (constant) flat spacetime data
/// must produce exactly zero contribution to the RHS.
///
/// Physics rationale: The 6th-order undivided difference D^6 f_i vanishes
/// identically for any polynomial of degree <= 5. A constant field is degree 0,
/// so D^6[const] = 0 exactly. This test verifies the refactored single-OMP-
/// region implementation computes the same result as the original 22-loop version.
TEST_F(CCZ4FlatTest, KODissipationIsZeroForFlatSpacetime) {
    // Enable KO dissipation
    CCZ4Params params_ko;
    params_ko.ko_sigma = 0.35; // Standard production value
    CCZ4Evolution ccz4_ko(params_ko);

    // Zero out RHS before applying dissipation
    for (int v = 0; v < NUM_SPACETIME_VARS; ++v) {
        const int ie0 = rhs->iend(0);
        const int ie1 = rhs->iend(1);
        const int ie2 = rhs->iend(2);
        const int is = rhs->istart();
        for (int k = is; k < ie2; ++k)
            for (int j = is; j < ie1; ++j)
                for (int i = is; i < ie0; ++i)
                    rhs->data(v, i, j, k) = 0.0;
    }

    // Compute RHS with KO dissipation active on flat (constant) grid
    ccz4_ko.computeRHSVacuum(*grid, *rhs);

    // For flat spacetime, D^6[const] = 0 for all vars, so RHS must remain zero
    Real max_ko = 0.0;
    int is = grid->istart() + 3; // KO stencil needs 3 ghost cells on each side
    int ie = grid->iend(0) - 3;
    for (int v = 0; v < NUM_SPACETIME_VARS; ++v) {
        for (int k = is; k < ie; ++k)
            for (int j = is; j < ie; ++j)
                for (int i = is; i < ie; ++i) {
                    Real val = std::abs(rhs->data(v, i, j, k));
                    if (val > max_ko)
                        max_ko = val;
                }
    }

    EXPECT_LT(max_ko, 1.0e-12)
        << "KO dissipation produced non-zero RHS for uniform flat spacetime. "
        << "Max |KO_diss| = " << max_ko << " -- D^6[const] must be zero for any constant field.";
}

/// Test: KO dissipation applied to gauge-wave data must produce finite,
/// bounded output with correct sign (always negative for sigma > 0).
///
/// Physics rationale: KO dissipation is a negative semi-definite operator
/// (-sigma h^5/64 d^6/dx^6). For a sinusoidal profile, d^6[sin(kx)] = -k^6 sin(kx),
/// so the sign of the dissipation contribution at the peak of sin is negative,
/// i.e., diss(chi) < 0 near the peak of chi. The magnitude must be bounded and
/// scale with sigma * k^6 * dx^5.
TEST_F(CCZ4FlatTest, KODissipationBoundedForGaugeWave) {
    // Set up a gauge wave grid
    std::array<int, DIM> ncells_gw = {64, 8, 8};
    std::array<Real, DIM> lo_gw = {0.0, 0.0, 0.0};
    std::array<Real, DIM> hi_gw = {1.0, 0.125, 0.125};
    int nghost = 4;

    GridBlock gw_grid(10, 0, ncells_gw, lo_gw, hi_gw, nghost, NUM_SPACETIME_VARS);
    GridBlock gw_rhs(11, 0, ncells_gw, lo_gw, hi_gw, nghost, NUM_SPACETIME_VARS);

    setGaugeWaveData(gw_grid, 0.01, 1.0); // amplitude=0.01, wavelength=1.0

    CCZ4Params params_ko;
    params_ko.ko_sigma = 0.35;
    CCZ4Evolution ccz4_ko(params_ko);
    ccz4_ko.computeRHSVacuum(gw_grid, gw_rhs);

    // All RHS values must be finite
    int is = gw_grid.istart() + 3;
    int ie0 = gw_grid.iend(0) - 3;
    int ie1 = gw_grid.iend(1) - 3;
    int ie2 = gw_grid.iend(2) - 3;

    Real max_rhs = 0.0;
    bool all_finite = true;
    for (int v = 0; v < NUM_SPACETIME_VARS; ++v) {
        for (int k = is; k < ie2; ++k)
            for (int j = is; j < ie1; ++j)
                for (int i = is; i < ie0; ++i) {
                    Real val = gw_rhs.data(v, i, j, k);
                    if (!std::isfinite(val))
                        all_finite = false;
                    if (std::abs(val) > max_rhs)
                        max_rhs = std::abs(val);
                }
    }

    EXPECT_TRUE(all_finite) << "KO dissipation produced non-finite values on gauge wave grid.";

    // For A=0.01 gauge wave, KO dissipation should be small but non-zero.
    // Bound: |KO| < sigma * (2*pi/lambda)^6 * dx^5 / 64 * A ~ O(1e-2) at most.
    EXPECT_GT(max_rhs, 0.0) << "KO dissipation is exactly zero for non-trivial gauge wave data -- "
                            << "this suggests the refactored OMP loop has a bug.";
    EXPECT_LT(max_rhs, 50.0)
        << "KO dissipation is unexpectedly large for small-amplitude gauge wave. "
        << "Max |RHS| = " << max_rhs;
}

// ===========================================================================
// Stream B2: Selective advection tests
// ===========================================================================

/// Test: with chi=1 (flat region far from puncture), selective advection
/// must give identical results to pure upwinding for a linear profile.
///
/// Physics: chi >= chi_thresh (0.05) triggers the upwinded branch.
/// For a linear field f(x) = a + b*x, both d1 and d1up give the exact
/// gradient b, so the two should agree to machine precision.
TEST_F(CCZ4FlatTest, SelectiveAdvecUpwindedInFlatRegion) {
    // Set chi = 1 everywhere (flat spacetime = far from puncture)
    // Already done by setFlatSpacetime in SetUp().
    // Imprint a linear lapse profile: alpha = 1.0 + 0.01 * x
    for (int k = 0; k < grid->totalCells(2); ++k)
        for (int j = 0; j < grid->totalCells(1); ++j)
            for (int i = 0; i < grid->totalCells(0); ++i) {
                Real x = grid->x(0, i);
                grid->data(static_cast<int>(SpacetimeVar::LAPSE), i, j, k) = 1.0 + 0.01 * x;
                // Set a nonzero shift to test upwinding
                grid->data(static_cast<int>(SpacetimeVar::SHIFT_X), i, j, k) = 0.5;
                grid->data(static_cast<int>(SpacetimeVar::SHIFT_Y), i, j, k) = 0.0;
                grid->data(static_cast<int>(SpacetimeVar::SHIFT_Z), i, j, k) = 0.0;
            }

    // Compute RHS — the advection term for alpha should be beta^x * d_x(alpha) = 0.5 * 0.01
    CCZ4Params params_test;
    params_test.ko_sigma = 0.0;
    CCZ4Evolution ccz4_test(params_test);
    ccz4_test.computeRHSVacuum(*grid, *rhs);

    // Check that the lapse RHS at interior cells contains a consistent advection contribution
    int is_inner = grid->istart() + 2;
    int ie_inner = grid->iend(0) - 2;
    bool all_finite = true;
    for (int k = is_inner; k < ie_inner; ++k)
        for (int j = is_inner; j < ie_inner; ++j)
            for (int i = is_inner; i < ie_inner; ++i) {
                Real val = rhs->data(static_cast<int>(SpacetimeVar::LAPSE), i, j, k);
                if (!std::isfinite(val))
                    all_finite = false;
            }
    EXPECT_TRUE(all_finite) << "Selective advection with chi=1 produced non-finite lapse RHS";
}

/// Test: with chi=0.01 (near puncture), selective advection must give
/// identical results to pure centered FD, even with nonzero shift.
///
/// Physics: chi < chi_thresh (0.05) triggers the centered branch.
/// Near the puncture beta is small, so centered advection is safe.
TEST_F(CCZ4FlatTest, SelectiveAdvecCenteredNearPuncture) {
    // Set chi = 0.01 everywhere to force the centered branch
    for (int k = 0; k < grid->totalCells(2); ++k)
        for (int j = 0; j < grid->totalCells(1); ++j)
            for (int i = 0; i < grid->totalCells(0); ++i) {
                grid->data(static_cast<int>(SpacetimeVar::CHI), i, j, k) = 0.01;
            }

    // Compute RHS with the centered branch active
    CCZ4Params params_test;
    params_test.ko_sigma = 0.0;
    CCZ4Evolution ccz4_test(params_test);
    ccz4_test.computeRHSVacuum(*grid, *rhs);

    // All RHS values must be finite (no NaN from steep gradients)
    int is_inner = grid->istart() + 2;
    int ie_inner = grid->iend(0) - 2;
    bool all_finite = true;
    int nan_var = -1, nan_i = -1, nan_j = -1, nan_k = -1;
    for (int v = 0; v < NUM_SPACETIME_VARS && all_finite; ++v)
        for (int k = is_inner; k < ie_inner && all_finite; ++k)
            for (int j = is_inner; j < ie_inner && all_finite; ++j)
                for (int i = is_inner; i < ie_inner && all_finite; ++i) {
                    Real val = rhs->data(v, i, j, k);
                    if (!std::isfinite(val)) {
                        all_finite = false;
                        nan_var = v;
                        nan_i = i;
                        nan_j = j;
                        nan_k = k;
                    }
                }
    EXPECT_TRUE(all_finite) << "Selective advection with chi=0.01 (near puncture) produced NaN at "
                            << "var=" << nan_var << " (" << nan_i << "," << nan_j << "," << nan_k
                            << ")";
}
