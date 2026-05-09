/**
 * @file test_polytrope.cpp
 * @brief Tests for polytropic stellar initial data.
 *
 * Exercises StellarInitialData::solvePolytrope() and solveTOV()
 * confirming physically correct output (positive density, monotone
 * profiles, mass conservation).
 */
#include "granite/core/types.hpp"
#include "granite/initial_data/initial_data.hpp"

#include <cmath>
#include <gtest/gtest.h>

using namespace granite;
using namespace granite::initial_data;

// Helper: build a StarParams with safe defaults
static StarParams makeStarParams(Real gamma, Real K, Real rho_c, Real mass, Real radius) {
    StarParams star;
    star.mass = mass;
    star.radius = radius;
    star.polytropic_gamma = gamma;
    star.polytropic_K = K;
    star.central_density = rho_c;
    star.position = {0.0, 0.0, 0.0}; // explicit — defaults exist but make clear
    return star;
}

// ---------------------------------------------------------------------------
// Lane-Emden / Polytrope solver
// ---------------------------------------------------------------------------

TEST(PolytropeTest, LaneEmdenGamma5_3) {
    auto star = makeStarParams(5.0 / 3.0, 1.0, 1.0, 1.0, 1.0);

    StellarInitialData sid({star});
    auto profile = sid.solvePolytrope(star);

    // Profile should have positive values
    EXPECT_GT(profile.r.size(), 10u);
    EXPECT_GT(profile.rho[0], 0.0);
    EXPECT_GE(profile.press[0], 0.0);

    // Density should be monotonically decreasing
    for (std::size_t i = 1; i < profile.rho.size(); ++i) {
        EXPECT_LE(profile.rho[i], profile.rho[i - 1] + 1e-15)
            << "Density not monotone at index " << i;
    }

    // Central density should match input
    EXPECT_NEAR(profile.rho[0], star.central_density, 0.1);

    // Enclosed mass should increase monotonically
    for (std::size_t i = 1; i < profile.mass.size(); ++i) {
        EXPECT_GE(profile.mass[i], profile.mass[i - 1] - 1e-15)
            << "Enclosed mass not monotone at index " << i;
    }
}

TEST(PolytropeTest, RadiationDominatedGamma4_3) {
    auto star = makeStarParams(4.0 / 3.0, 1.0, 1.0, 4300.0, 500.0);

    StellarInitialData sid({star});
    auto profile = sid.solvePolytrope(star);

    EXPECT_GT(profile.r.size(), 10u);
    EXPECT_GT(profile.rho[0], 0.0);
}

TEST(PolytropeTest, PressureFollowsPolytropicLaw) {
    // P = K ρ^Γ must hold throughout the profile
    Real gamma = 5.0 / 3.0;
    Real K = 1.0;
    auto star = makeStarParams(gamma, K, 1.0, 1.0, 1.0);

    StellarInitialData sid({star});
    auto profile = sid.solvePolytrope(star);

    for (std::size_t i = 0; i < profile.rho.size(); ++i) {
        if (profile.rho[i] < 1e-14)
            continue; // skip atmosphere floor
        Real p_expected = K * std::pow(profile.rho[i], gamma);
        EXPECT_NEAR(profile.press[i], p_expected, 1e-8 * p_expected + 1e-20)
            << "Polytropic law violated at index " << i << ": rho=" << profile.rho[i];
    }
}

TEST(PolytropeTest, EnclMassPositiveAtSurface) {
    auto star = makeStarParams(5.0 / 3.0, 1.0, 1.0, 1.0, 1.0);
    StellarInitialData sid({star});
    auto profile = sid.solvePolytrope(star);

    EXPECT_GT(profile.mass.back(), 0.0) << "Total enclosed mass should be positive.";
}

// ---------------------------------------------------------------------------
// TOV solver
// ---------------------------------------------------------------------------

TEST(PolytropeTest, TOVSolver) {
    auto star = makeStarParams(2.0, 100.0, 1.0e15, 1.4, 10.0);

    StellarInitialData sid({star});
    auto profile = sid.solveTOV(star);

    // TOV should produce a valid profile
    EXPECT_GT(profile.r.size(), 5u);
    EXPECT_GT(profile.rho[0], 0.0);
    EXPECT_GT(profile.press[0], 0.0);
}

TEST(PolytropeTest, TOVDensityMonotone) {
    auto star = makeStarParams(2.0, 100.0, 1.0e15, 1.4, 10.0);
    StellarInitialData sid({star});
    auto profile = sid.solveTOV(star);

    for (std::size_t i = 1; i < profile.rho.size(); ++i) {
        EXPECT_LE(profile.rho[i], profile.rho[i - 1] + 1e-15)
            << "TOV density not monotone at index " << i;
    }
}

TEST(PolytropeTest, TOVPressureMonotone) {
    auto star = makeStarParams(2.0, 100.0, 1.0e15, 1.4, 10.0);
    StellarInitialData sid({star});
    auto profile = sid.solveTOV(star);

    for (std::size_t i = 1; i < profile.press.size(); ++i) {
        EXPECT_LE(profile.press[i], profile.press[i - 1] + 1e-15)
            << "TOV pressure not monotone at index " << i;
    }
}
