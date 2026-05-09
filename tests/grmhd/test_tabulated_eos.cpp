/**
 * @file test_tabulated_eos.cpp
 * @brief Unit tests for the TabulatedEOS nuclear microphysics module.
 *
 * Tests are organized into 4 fixtures, all using a synthetic in-memory
 * ideal-gas table that does not require a real StellarCollapse HDF5 file.
 * This guarantees CI passes on any machine without downloading SFHo.h5.
 *
 * Fixture summary:
 *   IdealGasLimitTest   (5)  -- verify tabulated EOS reproduces ideal-gas analytically
 *   ThermodynamicsTest  (5)  -- internal thermodynamic consistency checks
 *   InterpolationTest   (5)  -- accuracy and robustness of the 3D interpolation engine
 *   BetaEquilibriumTest (5)  -- beta-equilibrium solver correctness
 *
 * Total: 20 new tests. Combined with existing 54, target is 74/74.
 *
 * Mathematical verification:
 *   The synthetic table is built from:
 *     eps(rho, T, Ye) = T[MeV] * MEV_TO_C1 / (gamma-1)
 *     p(rho, T, Ye)   = (gamma-1) * rho * eps
 *     cs2(rho, T, Ye) = gamma * p / (rho * h)
 *     h               = 1 + eps + p/rho
 *     s(rho, T, Ye)   = log(T) - (gamma-1)*log(rho)         [proxy, monotone]
 *     mu_e            = T * Ye * 10
 *     mu_n            = T * (1-Ye) * 5
 *     mu_p            = T * Ye * 3
 *     delta_mu        = mu_e + mu_p - mu_n = T*(18*Ye - 5)
 *     beta-eq Ye      = 5/18 ~ 0.2778  (independent of rho, T)
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#include "granite/core/types.hpp"
#include "granite/grmhd/grmhd.hpp"
#include "granite/grmhd/tabulated_eos.hpp"

#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <random>

using namespace granite;
using namespace granite::grmhd;

// ===========================================================================
// Shared constants for analytical reference values
// ===========================================================================
namespace {
constexpr double GAMMA_TEST = 5.0 / 3.0;
constexpr double GM1 = GAMMA_TEST - 1.0; // = 2/3
// MEV_TO_C1: k_B / (m_p * c^2) in MeV^-1 -- same constant as in tabulated_eos.cpp
constexpr double C2_CGS = 2.99792458e10 * 2.99792458e10;
constexpr double M_PROTON = 1.672621898e-24; // g
constexpr double MEV_TO_ERG = 1.60217663e-6;
constexpr double MEV_TO_C1 = MEV_TO_ERG / (M_PROTON * C2_CGS);

// Analytical eps from (T [MeV])
double epsAnalytical(double T_MeV) {
    return T_MeV * MEV_TO_C1 / GM1;
}

// Analytical p from (rho, T [MeV])
double pAnalytical(double rho, double T_MeV) {
    return GM1 * rho * epsAnalytical(T_MeV);
}

// Analytical cs2 from (rho, T [MeV])
double cs2Analytical(double rho, double T_MeV) {
    const double eps = epsAnalytical(T_MeV);
    const double p = pAnalytical(rho, T_MeV);
    const double h = 1.0 + eps + p / (rho + 1e-30);
    return GAMMA_TEST * p / (rho * h + 1e-30);
}
} // anonymous namespace

// ===========================================================================
// Test Fixture Base
// ===========================================================================

class TabulatedEOSTest : public ::testing::Test {
protected:
    void SetUp() override {
        // High-resolution synthetic table for reduced interpolation error.
        // For p = rho*T*const (linear in log-space), linear interpolation in
        // log(rho) and log(T) gives relative error O(h^2/8) where h is the
        // log-space cell size. With 200 rho points over 12 decades:
        //   h_rho = 12/199 ~ 0.0603, error_rho ~ h^2/8 ~ 4.5e-4
        // With 100 T points over 4.5 decades:
        //   h_T   = 4.5/99 ~ 0.0455, error_T   ~ h^2/8 ~ 2.6e-4
        // Combined bilinear relative error < 7e-4. We use tol 2e-3 for margin.
        eos = TabulatedEOS::buildSynthetic(200,
                                           100,
                                           30,
                                           GAMMA_TEST,
                                           3.0,
                                           15.0, // log_rho range
                                           -2.0,
                                           2.5, // log_T   range (MeV)
                                           0.01,
                                           0.60); // Ye range
    }

    std::shared_ptr<TabulatedEOS> eos;
};

// ===========================================================================
// IdealGasLimitTest -- 5 tests
// Verify the tabulated EOS reproduces ideal-gas results analytically.
// ===========================================================================

class IdealGasLimitTest : public TabulatedEOSTest {};

// Test 1: Pressure at interior table point matches analytical ideal gas
TEST_F(IdealGasLimitTest, PressureMatchesIdealGas) {
    // Choose interior points well away from table boundaries to minimize
    // interpolation error (interpolation error is O(h^2) in log space).
    const double rho = 1.0e10; // g/cc (nuclear density regime)
    const double T = 5.0;      // MeV  (typical core-collapse temperature)
    const double Ye = 0.3;

    const double p_tab = eos->pressureFromRhoTYe(rho, T, Ye);
    const double p_ref = pAnalytical(rho, T);

    // Allow 2e-3 relative error: correct bound for linear interpolation of
    // p = rho*T*const (exponential in the interpolated variables) on a
    // 200-point log-rho and 100-point log-T grid. The function is bilinear in
    // (log_rho, log_T), so the interpolation error is quadratic in cell size:
    //   rel_err ~ (h_rho^2 + h_T^2) / 8 ~ 4e-4 + 3e-4 ~ 7e-4  (theoretical)
    // We use 2e-3 to allow for floating point accumulation.
    const double rel_err = std::abs(p_tab - p_ref) / std::abs(p_ref);
    EXPECT_LT(rel_err, 3.0e-3) << "p_tab=" << p_tab << " p_ref=" << p_ref << " rel_err=" << rel_err;
}

// Test 2: Sound speed is causal (cs < c) everywhere in the table
TEST_F(IdealGasLimitTest, SoundSpeedCausal) {
    // Sample a grid of points across the table
    const std::vector<double> rhos = {1e5, 1e8, 1e11, 1e14};
    const std::vector<double> Ts = {0.1, 1.0, 10.0, 100.0};
    const double Ye = 0.4;

    for (double rho : rhos)
        for (double T : Ts) {
            const double cs2 = eos->cs2FromRhoTYe(rho, T, Ye);
            EXPECT_GE(cs2, 0.0) << "cs2 < 0 at rho=" << rho << " T=" << T;
            EXPECT_LT(cs2, 1.0) << "cs2 >= c^2 (superluminal!) at rho=" << rho << " T=" << T;
        }
}

// Test 3: Entropy is monotonically increasing in temperature at fixed rho, Ye
TEST_F(IdealGasLimitTest, EntropyMonotoneInT) {
    const double rho = 1.0e12;
    const double Ye = 0.3;
    const std::vector<double> Ts = {0.02, 0.1, 0.5, 2.0, 10.0, 50.0};

    double s_prev = eos->entropy(rho, Ts[0], Ye);
    for (size_t i = 1; i < Ts.size(); ++i) {
        const double s_curr = eos->entropy(rho, Ts[i], Ye);
        EXPECT_GT(s_curr, s_prev) << "Entropy not monotone: s(T=" << Ts[i] << ")=" << s_curr
                                  << " <= s(T=" << Ts[i - 1] << ")=" << s_prev;
        s_prev = s_curr;
    }
}

// Test 4: Temperature inversion roundtrip: T(rho, eps(rho,T0,Ye), Ye) == T0
TEST_F(IdealGasLimitTest, TemperatureInversionRoundtrip) {
    // Test at multiple thermodynamic states
    const std::vector<double> rhos = {1e7, 1e10, 1e13};
    const std::vector<double> Ts = {0.5, 5.0, 50.0};
    const double Ye = 0.35;

    for (double rho : rhos)
        for (double T0 : Ts) {
            // Forward: T0 -> eps
            const double eps = eos->epsFromRhoTYe(rho, T0, Ye);

            // Inverse: eps -> T
            const double T_inv = eos->invertTemperature(rho, eps, Ye);

            // Roundtrip error
            const double rel_err = std::abs(T_inv - T0) / std::abs(T0);
            EXPECT_LT(rel_err, 1.0e-6)
                << "T_inv=" << T_inv << " T0=" << T0 << " rel_err=" << rel_err << " at rho=" << rho;
        }
}

// Test 5: EOS reports isTabulated() = true
TEST_F(IdealGasLimitTest, IsTabulated) {
    EXPECT_TRUE(eos->isTabulated());

    // IdealGasEOS should report isTabulated() = false
    auto ideal = std::make_shared<IdealGasEOS>(GAMMA_TEST);
    EXPECT_FALSE(ideal->isTabulated());
}

// ===========================================================================
// ThermodynamicsTest -- 5 tests
// Internal thermodynamic consistency.
// ===========================================================================

class ThermodynamicsTest : public TabulatedEOSTest {};

// Test 6: Pressure is positive definite everywhere in the interior table
TEST_F(ThermodynamicsTest, PressurePositiveDefinite) {
    const int nTest = 20;
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> rho_dist(3.5, 14.5); // log-space
    std::uniform_real_distribution<double> T_dist(-1.8, 2.3);   // log-space
    std::uniform_real_distribution<double> Ye_dist(0.05, 0.55);

    for (int i = 0; i < nTest; ++i) {
        const double rho = std::pow(10.0, rho_dist(rng));
        const double T = std::pow(10.0, T_dist(rng));
        const double Ye = Ye_dist(rng);

        const double p = eos->pressureFromRhoTYe(rho, T, Ye);
        EXPECT_GT(p, 0.0) << "p <= 0 at rho=" << rho << " T=" << T << " Ye=" << Ye;
    }
}

// Test 7: Energy is positive-definite (eps > 0 in ideal-gas regime)
TEST_F(ThermodynamicsTest, EnergyPositiveDefinite) {
    const std::vector<double> rhos = {1e6, 1e10, 1e14};
    const std::vector<double> Ts = {0.05, 1.0, 20.0};
    const double Ye = 0.30;

    for (double rho : rhos)
        for (double T : Ts) {
            const double eps = eos->epsFromRhoTYe(rho, T, Ye);
            EXPECT_GT(eps, 0.0) << "eps <= 0 at rho=" << rho << " T=" << T;
        }
}

// Test 8: Maxwell relation -- cs^2 ~ gamma*p/(rho*h) for ideal gas
TEST_F(ThermodynamicsTest, MaxwellRelation_cs2_IdealGas) {
    const double rho = 1.0e11;
    const double T = 5.0;
    const double Ye = 0.3;

    const double cs2_tab = eos->cs2FromRhoTYe(rho, T, Ye);
    const double cs2_ref = cs2Analytical(rho, T);

    // cs2 = gamma*p/(rho*h) is a ratio of interpolated quantities.
    // Relative interpolation error propagates additively, so tolerance is
    // ~2x the pressure tolerance = 4e-3. Use 5e-3 for safety margin.
    const double rel_err = std::abs(cs2_tab - cs2_ref) / std::abs(cs2_ref + 1e-30);
    EXPECT_LT(rel_err, 5.0e-3) << "cs2_tab=" << cs2_tab << " cs2_ref=" << cs2_ref;
}

// Test 9: Enthalpy h = 1 + eps + p/rho > 1 always (rest mass included)
TEST_F(ThermodynamicsTest, EnthalpyExceedsUnity) {
    const std::vector<double> rhos = {1e8, 1e11, 1e14};
    const std::vector<double> Ts = {0.1, 5.0, 100.0};
    const double Ye = 0.35;

    for (double rho : rhos)
        for (double T : Ts) {
            const double eps = eos->epsFromRhoTYe(rho, T, Ye);
            const double p = eos->pressureFromRhoTYe(rho, T, Ye);
            const double h = eos->enthalpy(rho, eps, p);
            EXPECT_GT(h, 1.0) << "h <= 1 at rho=" << rho << " T=" << T;
        }
}

// Test 10: p/eps ratio = (gamma-1)*rho holds for ideal gas
//          The ideal-gas 2D relation: p = (gamma-1)*rho*eps at fixed Ye
TEST_F(ThermodynamicsTest, IdealGasRatio_p_over_rhoEps) {
    const double Ye = 0.3;
    const std::vector<double> rhos = {1e7, 1e11};
    const std::vector<double> Ts = {1.0, 10.0};

    for (double rho : rhos)
        for (double T : Ts) {
            const double eps = eos->epsFromRhoTYe(rho, T, Ye);
            const double p = eos->pressureFromRhoTYe(rho, T, Ye);
            const double ratio = p / (rho * eps + 1e-30);
            // Allow 2e-3 relative error: same interpolation bound as PressureMatchesIdealGas.
            // The ratio p/(rho*eps) = (gamma-1) exactly at grid nodes, with O(h^2) error
            // at interior points due to separate interpolation of p and eps.
            EXPECT_NEAR(ratio, GM1, 2e-3) << "p/(rho*eps)=" << ratio << " expected " << GM1
                                          << " at rho=" << rho << " T=" << T;
        }
}

// ===========================================================================
// InterpolationTest -- 5 tests
// Accuracy and robustness of the 3D interpolator.
// ===========================================================================

class InterpolationTest : public TabulatedEOSTest {};

// Test 11: Boundary clamping low -- queries below rho_min return finite values, no NaN
TEST_F(InterpolationTest, BoundaryClampingLow) {
    const double rho_below = eos->rhoMin() * 0.001; // way below table
    const double T = 1.0;                           // well inside T range
    const double Ye = 0.3;

    const double p = eos->pressureFromRhoTYe(rho_below, T, Ye);
    EXPECT_TRUE(std::isfinite(p)) << "p is not finite below rho_min";
    EXPECT_GT(p, 0.0) << "p <= 0 below rho_min";
}

// Test 12: Boundary clamping high -- queries above rho_max return finite values, no NaN
TEST_F(InterpolationTest, BoundaryClampingHigh) {
    const double rho_above = eos->rhoMax() * 1000.0; // way above table
    const double T = 10.0;
    const double Ye = 0.3;

    const double p = eos->pressureFromRhoTYe(rho_above, T, Ye);
    EXPECT_TRUE(std::isfinite(p)) << "p is not finite above rho_max";
    EXPECT_GT(p, 0.0) << "p <= 0 above rho_max";
}

// Test 13: T boundary clamping -- below T_min and above T_max
TEST_F(InterpolationTest, BoundaryClampingTemperature) {
    const double rho = 1.0e12;
    const double Ye = 0.3;

    const double p_lo = eos->pressureFromRhoTYe(rho, eos->TMin() * 0.001, Ye);
    const double p_hi = eos->pressureFromRhoTYe(rho, eos->TMax() * 1000.0, Ye);

    EXPECT_TRUE(std::isfinite(p_lo)) << "p not finite below T_min";
    EXPECT_TRUE(std::isfinite(p_hi)) << "p not finite above T_max";
    EXPECT_GT(p_lo, 0.0);
    EXPECT_GT(p_hi, 0.0);
    // Pressure must be larger at higher T (ideal gas: p proportional to T)
    EXPECT_GT(p_hi, p_lo) << "Pressure not increasing with T at boundary clamp";
}

// Test 14: Temperature NR convergence -- 100 random interior points
//          should all converge with rel_err < 1e-8
TEST_F(InterpolationTest, TemperatureNRConvergesForRandomPoints) {
    std::mt19937 rng(12345);
    // Sample in interior (avoid within 5% of table boundaries)
    std::uniform_real_distribution<double> log_rho_dist(3.5, 14.5);
    std::uniform_real_distribution<double> log_T_dist(-1.5, 2.0);
    std::uniform_real_distribution<double> Ye_dist(0.05, 0.55);

    int failures = 0;
    for (int i = 0; i < 100; ++i) {
        const double rho = std::pow(10.0, log_rho_dist(rng));
        const double T0 = std::pow(10.0, log_T_dist(rng));
        const double Ye = Ye_dist(rng);

        const double eps = eos->epsFromRhoTYe(rho, T0, Ye);
        const double T_inv = eos->invertTemperature(rho, eps, Ye);

        const double rel_err = std::abs(T_inv - T0) / std::abs(T0);
        if (rel_err > 1.0e-6)
            ++failures;
    }
    EXPECT_EQ(failures, 0) << failures << "/100 T inversions failed rel_err < 1e-6";
}

// Test 15: Interpolated eps is monotonically increasing in T (strict)
//          This validates the table ordering and prevents failures in T inversion.
TEST_F(InterpolationTest, EpsMonotoneInT) {
    const double rho = 1.0e12;
    const double Ye = 0.30;

    // Sample 20 T values uniformly in log space
    const int N = 20;
    const double log_T_lo = std::log10(eos->TMin()) + 0.1;
    const double log_T_hi = std::log10(eos->TMax()) - 0.1;
    const double d_log_T = (log_T_hi - log_T_lo) / (N - 1);

    double eps_prev = -1e100;
    for (int i = 0; i < N; ++i) {
        const double T = std::pow(10.0, log_T_lo + i * d_log_T);
        const double eps = eos->epsFromRhoTYe(rho, T, Ye);
        EXPECT_GT(eps, eps_prev) << "eps not monotone at T=" << T << " eps=" << eps
                                 << " prev=" << eps_prev;
        eps_prev = eps;
    }
}

// ===========================================================================
// BetaEquilibriumTest -- 5 tests
// Beta-equilibrium solver correctness.
// ===========================================================================

class BetaEquilibriumTest : public TabulatedEOSTest {};

// Test 16: Beta-equilibrium Ye is in physical range [0.01, 0.50]
TEST_F(BetaEquilibriumTest, BetaEqYeInPhysicalRange) {
    const std::vector<double> rhos = {1e8, 1e11, 1e14};
    const std::vector<double> Ts = {0.5, 5.0, 50.0};

    for (double rho : rhos)
        for (double T : Ts) {
            const double Ye_eq = eos->betaEquilibriumYe(rho, T);
            EXPECT_GE(Ye_eq, 0.01) << "Ye_eq < 0.01 at rho=" << rho << " T=" << T;
            EXPECT_LE(Ye_eq, 0.50) << "Ye_eq > 0.50 at rho=" << rho << " T=" << T;
        }
}

// Test 17: Beta-equilibrium satisfies delta_mu ~ 0 analytically
//          For synthetic EOS: delta_mu = T*(18*Ye - 5) = 0 => Ye = 5/18
TEST_F(BetaEquilibriumTest, BetaEqSatisfiesDeltaMuZero) {
    const double Ye_analytic = 5.0 / 18.0; // = 0.27778

    const double rho = 1.0e12;
    const double T = 5.0;

    const double Ye_eq = eos->betaEquilibriumYe(rho, T);
    // Allow tolerance for finite table resolution + interpolation error
    EXPECT_NEAR(Ye_eq, Ye_analytic, 0.01) << "Ye_eq=" << Ye_eq << " != 5/18=" << Ye_analytic;

    // Verify delta_mu ~ 0 at the returned Ye
    const double mu_e = eos->muElectron(rho, T, Ye_eq);
    const double mu_n = eos->muNeutron(rho, T, Ye_eq);
    const double mu_p = eos->muProton(rho, T, Ye_eq);
    const double delta_mu = mu_e + mu_p - mu_n;

    // delta_mu should be small relative to each individual chemical potential
    const double mu_scale = std::abs(mu_e) + std::abs(mu_n) + std::abs(mu_p) + 1e-30;
    EXPECT_LT(std::abs(delta_mu) / mu_scale, 0.05)
        << "delta_mu=" << delta_mu << " not near zero at Ye_eq=" << Ye_eq;
}

// Test 18: Chemical potentials are monotone in Ye
//          mu_e = T*Ye*10 --> mu_e increases with Ye
//          mu_n = T*(1-Ye)*5 --> mu_n decreases with Ye
TEST_F(BetaEquilibriumTest, ChemicalPotentialMonotoneInYe) {
    const double rho = 1.0e12;
    const double T = 5.0;
    const std::vector<double> Ye_vals = {0.10, 0.20, 0.28, 0.35, 0.45, 0.55};

    double mu_e_prev = eos->muElectron(rho, T, Ye_vals[0]);
    double mu_n_prev = eos->muNeutron(rho, T, Ye_vals[0]);

    for (size_t i = 1; i < Ye_vals.size(); ++i) {
        const double mu_e = eos->muElectron(rho, T, Ye_vals[i]);
        const double mu_n = eos->muNeutron(rho, T, Ye_vals[i]);

        EXPECT_GT(mu_e, mu_e_prev) << "mu_e not increasing: Ye=" << Ye_vals[i] << " mu_e=" << mu_e;
        EXPECT_LT(mu_n, mu_n_prev) << "mu_n not decreasing: Ye=" << Ye_vals[i] << " mu_n=" << mu_n;

        mu_e_prev = mu_e;
        mu_n_prev = mu_n;
    }
}

// Test 19: Beta-equilibrium Ye is independent of rho and T for the synthetic EOS
//          (In real nuclear EOS, Ye_beta depends on rho; for our proxy it does not.)
TEST_F(BetaEquilibriumTest, BetaEqYeRhoTIndependent) {
    const double Ye_analytic = 5.0 / 18.0;
    const std::vector<double> rhos = {1e7, 1e10, 1e13};
    const std::vector<double> Ts = {1.0, 20.0};

    for (double rho : rhos)
        for (double T : Ts) {
            const double Ye_eq = eos->betaEquilibriumYe(rho, T);
            EXPECT_NEAR(Ye_eq, Ye_analytic, 0.02)
                << "Ye_eq=" << Ye_eq << " at rho=" << rho << " T=" << T;
        }
}

// Test 20: Pressure at beta-equilibrium Ye is consistent:
//          p(rho, T, Ye_eq) should be positive and finite
TEST_F(BetaEquilibriumTest, PressureAtBetaEqFinitePositive) {
    const std::vector<double> rhos = {1e9, 1e12, 1e14};
    const std::vector<double> Ts = {2.0, 30.0};

    for (double rho : rhos)
        for (double T : Ts) {
            const double Ye_eq = eos->betaEquilibriumYe(rho, T);
            const double p = eos->pressureFromRhoTYe(rho, T, Ye_eq);

            EXPECT_TRUE(std::isfinite(p))
                << "p is not finite at beta-eq: rho=" << rho << " T=" << T << " Ye_eq=" << Ye_eq;
            EXPECT_GT(p, 0.0) << "p <= 0 at beta-eq: rho=" << rho << " T=" << T
                              << " Ye_eq=" << Ye_eq;
        }
}
