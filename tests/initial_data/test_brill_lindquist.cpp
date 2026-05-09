/**
 * @file test_brill_lindquist.cpp
 * @brief Tests for Brill-Lindquist multi-BH initial data.
 */
#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"
#include "granite/initial_data/initial_data.hpp"

#include <cmath>
#include <gtest/gtest.h>
#include <vector>

using namespace granite;
using namespace granite::initial_data;

TEST(BrillLindquistTest, SingleBHConformalFactor) {
    BlackHoleParams bh;
    bh.mass = 1.0;
    bh.position = {0.0, 0.0, 0.0};
    bh.momentum = {0.0, 0.0, 0.0};
    bh.spin = {0.0, 0.0, 0.0};

    BrillLindquist bl({bh});

    // ψ(r) = 1 + m/(2r) for a single BH at origin
    // At r = 1: ψ = 1 + 0.5 = 1.5
    Real psi = bl.conformalFactor(1.0, 0.0, 0.0);
    EXPECT_NEAR(psi, 1.5, 1e-10);

    // At r = 0.5: ψ = 1 + 1.0 = 2.0
    Real psi2 = bl.conformalFactor(0.5, 0.0, 0.0);
    EXPECT_NEAR(psi2, 2.0, 1e-10);
}

TEST(BrillLindquistTest, TwoBHSymmetry) {
    BlackHoleParams bh1, bh2;
    bh1.mass = 1.0;
    bh1.position = {5.0, 0.0, 0.0};
    bh1.momentum = {0.0, 0.0, 0.0};
    bh1.spin = {0.0, 0.0, 0.0};
    bh2.mass = 1.0;
    bh2.position = {-5.0, 0.0, 0.0};
    bh2.momentum = {0.0, 0.0, 0.0};
    bh2.spin = {0.0, 0.0, 0.0};

    BrillLindquist bl({bh1, bh2});

    // ψ should be symmetric about x=0
    Real psi_pos = bl.conformalFactor(3.0, 0.0, 0.0);
    Real psi_neg = bl.conformalFactor(-3.0, 0.0, 0.0);
    EXPECT_NEAR(psi_pos, psi_neg, 1e-10);

    // At the midpoint: ψ = 1 + 1/(2*5) + 1/(2*5) = 1.2
    Real psi_mid = bl.conformalFactor(0.0, 0.0, 0.0);
    EXPECT_NEAR(psi_mid, 1.2, 1e-10);
}

TEST(BrillLindquistTest, FiveBHPentagon) {
    int N = 5;
    Real R0 = 10.0;
    std::vector<BlackHoleParams> bhs(N);
    for (int A = 0; A < N; ++A) {
        Real angle = 2.0 * constants::PI * A / N;
        bhs[A].mass = 1.0;
        bhs[A].position = {R0 * std::cos(angle), R0 * std::sin(angle), 0.0};
        bhs[A].momentum = {0.0, 0.0, 0.0};
        bhs[A].spin = {0.0, 0.0, 0.0};
    }

    BrillLindquist bl(bhs);

    // At the center (equidistant from all BHs): ψ = 1 + 5 * 1/(2*10) = 1.25
    Real psi_center = bl.conformalFactor(0.0, 0.0, 0.0);
    EXPECT_NEAR(psi_center, 1.25, 1e-10);

    // ADM mass should equal sum of bare masses (in absence of momentum)
    EXPECT_NEAR(bl.admMass(), 5.0, 1e-10);
}

TEST(BrillLindquistTest, ConformalFactorMonotonicallyDecreasing) {
    // ψ should decrease as we move away from a single BH
    BlackHoleParams bh;
    bh.mass = 1.0;
    bh.position = {0.0, 0.0, 0.0};
    bh.momentum = {0.0, 0.0, 0.0};
    bh.spin = {0.0, 0.0, 0.0};

    BrillLindquist bl({bh});

    Real prev = 1e100;
    for (double r : {0.1, 0.5, 1.0, 2.0, 5.0, 10.0, 50.0}) {
        Real psi = bl.conformalFactor(r, 0.0, 0.0);
        EXPECT_LT(psi, prev) << "Conformal factor should decrease at r=" << r;
        prev = psi;
    }

    // Far from BH: ψ → 1
    Real psi_far = bl.conformalFactor(1000.0, 0.0, 0.0);
    EXPECT_NEAR(psi_far, 1.0, 1e-3);
}

TEST(BrillLindquistTest, ApplyToGrid) {
    std::array<int, DIM> ncells = {32, 32, 32};
    std::array<Real, DIM> lo = {-20.0, -20.0, -20.0};
    std::array<Real, DIM> hi = {20.0, 20.0, 20.0};
    int nghost = 4;

    GridBlock grid(0, 0, ncells, lo, hi, nghost, NUM_SPACETIME_VARS);

    BlackHoleParams bh;
    bh.mass = 1.0;
    bh.position = {0.0, 0.0, 0.0};
    bh.momentum = {0.0, 0.0, 0.0};
    bh.spin = {0.0, 0.0, 0.0};

    BrillLindquist bl({bh});
    bl.apply(grid);

    // χ at center should be ψ^{-4}, ψ is large near puncture
    int mid = nghost + ncells[0] / 2;
    Real chi_center = grid.data(static_cast<int>(SpacetimeVar::CHI), mid, mid, mid);
    EXPECT_GT(chi_center, 0.0);
    EXPECT_LT(chi_center, 1.0); // χ < 1 near BH

    // Lapse should be < 1 near BH
    Real alpha_center = grid.data(static_cast<int>(SpacetimeVar::LAPSE), mid, mid, mid);
    EXPECT_GT(alpha_center, 0.0);
    EXPECT_LT(alpha_center, 1.0);

    // Far corner: χ → 1, lapse → 1
    int far = nghost + ncells[0] - 2;
    Real chi_far = grid.data(static_cast<int>(SpacetimeVar::CHI), far, far, far);
    EXPECT_GT(chi_far, 0.9);

    // Conformal metric should be flat (diagonal) everywhere
    Real gxx = grid.data(static_cast<int>(SpacetimeVar::GAMMA_XX), mid, mid, mid);
    Real gyy = grid.data(static_cast<int>(SpacetimeVar::GAMMA_YY), mid, mid, mid);
    Real gzz = grid.data(static_cast<int>(SpacetimeVar::GAMMA_ZZ), mid, mid, mid);
    EXPECT_NEAR(gxx, 1.0, 1e-12);
    EXPECT_NEAR(gyy, 1.0, 1e-12);
    EXPECT_NEAR(gzz, 1.0, 1e-12);
}
