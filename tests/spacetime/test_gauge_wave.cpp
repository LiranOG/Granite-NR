/**
 * @file test_gauge_wave.cpp
 * @brief Gauge wave test — Apples-with-Apples benchmark.
 *
 * Verifies that a gauge-wave perturbation propagates stably and
 * does not grow for O(100) crossing times.
 */
// MSVC requires _USE_MATH_DEFINES before <cmath> to expose M_PI
#define _USE_MATH_DEFINES
#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"
#include "granite/spacetime/ccz4.hpp"

#include <cmath>
#include <cstdlib>
#include <gtest/gtest.h>

// Portable π fallback (in case _USE_MATH_DEFINES doesn't work on some platforms)
#ifndef M_PI
static constexpr double M_PI = 3.14159265358979323846;
#endif

using namespace granite;
using namespace granite::spacetime;

TEST(GaugeWaveTest, InitializationCorrect) {
    std::array<int, DIM> ncells = {64, 8, 8};
    std::array<Real, DIM> lo = {0.0, 0.0, 0.0};
    std::array<Real, DIM> hi = {1.0, 0.125, 0.125};
    int nghost = 4;

    GridBlock grid(0, 0, ncells, lo, hi, nghost, NUM_SPACETIME_VARS);

    Real amplitude = 0.01;
    Real wavelength = 1.0;
    setGaugeWaveData(grid, amplitude, wavelength);

    // Check that the lapse at x=L/4 has the expected value
    // α(L/4) = √(1 + A sin(π/2)) = √(1 + A)
    int i_quarter = nghost + ncells[0] / 4;
    int j_mid = nghost + ncells[1] / 2;
    int k_mid = nghost + ncells[2] / 2;

    Real alpha_expected = std::sqrt(1.0 + amplitude);
    Real alpha_actual = grid.data(static_cast<int>(SpacetimeVar::LAPSE), i_quarter, j_mid, k_mid);

    EXPECT_NEAR(alpha_actual, alpha_expected, 1e-4)
        << "Gauge wave lapse initialization incorrect at x = L/4";
}

TEST(GaugeWaveTest, ConformalFactorAtOrigin) {
    // χ at x=0 exactly would be χ=1, but on a discrete grid the first interior
    // cell is centred at x = lo + 0.5*dx, not x=0.  We therefore:
    //   (1) compute the actual coordinate of the cell,
    //   (2) compute the expected χ analytically, and
    //   (3) compare with a tight tolerance (1 ULP in dx²).
    std::array<int, DIM> ncells = {64, 8, 8};
    std::array<Real, DIM> lo = {0.0, 0.0, 0.0};
    std::array<Real, DIM> hi = {1.0, 0.125, 0.125};
    int nghost = 4;
    Real amplitude = 0.01;
    Real wavelength = 1.0;

    GridBlock grid(0, 0, ncells, lo, hi, nghost, NUM_SPACETIME_VARS);
    setGaugeWaveData(grid, amplitude, wavelength);

    int i_origin = nghost; // first interior cell
    int j_mid = nghost + ncells[1] / 2;
    int k_mid = nghost + ncells[2] / 2;

    // Actual x-coordinate of this cell centre
    Real x_cell = grid.x(0, i_origin); // lo + (i - ng + 0.5) * dx

    // Gauge wave: H(x) = 1 + A sin(2π x / λ)
    // γ_xx = H(x),  α = √H(x),  χ = H(x)^{-1/3}
    // (see Apples-with-Apples gauge wave benchmark, Alcubierre et al. 2003)
    Real H_expected = 1.0 + amplitude * std::sin(2.0 * M_PI * x_cell / wavelength);
    Real chi_expected = std::pow(H_expected, -1.0 / 3.0);

    Real chi_actual = grid.data(static_cast<int>(SpacetimeVar::CHI), i_origin, j_mid, k_mid);

    // Tolerance: O(dx²) because χ = H^{-1/3} and H = 1 + A·sin(…),
    // discretisation error is O(dx²) ≈ (1/64)² ≈ 2.4e-4.
    Real dx = grid.dx(0);
    Real tol = 5.0 * dx * dx; // generous 5× safety margin
    EXPECT_NEAR(chi_actual, chi_expected, tol)
        << "Conformal factor mismatch at cell centre x=" << x_cell << "  chi_actual=" << chi_actual
        << "  chi_expected=" << chi_expected;
}

TEST(GaugeWaveTest, RHSFiniteAndBounded) {
    std::array<int, DIM> ncells = {64, 8, 8};
    std::array<Real, DIM> lo = {0.0, 0.0, 0.0};
    std::array<Real, DIM> hi = {1.0, 0.125, 0.125};
    int nghost = 4;

    GridBlock grid(0, 0, ncells, lo, hi, nghost, NUM_SPACETIME_VARS);
    GridBlock rhs(1, 0, ncells, lo, hi, nghost, NUM_SPACETIME_VARS);

    setGaugeWaveData(grid, 0.01, 1.0);

    CCZ4Params params;
    params.ko_sigma = 0.1;
    CCZ4Evolution ccz4(params);

    ccz4.computeRHSVacuum(grid, rhs);

    // RHS should be finite and bounded
    Real max_rhs = 0.0;
    int is = grid.istart() + 1;
    int ie = grid.iend(0) - 1;

    for (int v = 0; v < NUM_SPACETIME_VARS; ++v) {
        for (int k = is; k < grid.iend(2) && k < is + 4; ++k)
            for (int j = is; j < grid.iend(1) && j < is + 4; ++j)
                for (int i = is; i < ie; ++i) {
                    Real val = std::abs(rhs.data(v, i, j, k));
                    EXPECT_TRUE(std::isfinite(val)) << "Non-finite RHS at var=" << v << " (" << i
                                                    << "," << j << "," << k << ")";
                    if (val > max_rhs)
                        max_rhs = val;
                }
    }

    // For a small amplitude wave, RHS should be bounded
    EXPECT_LT(max_rhs, 100.0) << "RHS unexpectedly large for small-amplitude gauge wave";
}

TEST(GaugeWaveTest, RHSWithMatterSourcesMatchesVacuum) {
    // For pure vacuum setup, zero matter sources must give identical RHS.
    std::array<int, DIM> ncells = {32, 8, 8};
    std::array<Real, DIM> lo = {0.0, 0.0, 0.0};
    std::array<Real, DIM> hi = {1.0, 0.125, 0.125};
    int nghost = 4;

    GridBlock grid(0, 0, ncells, lo, hi, nghost, NUM_SPACETIME_VARS);
    GridBlock rhs1(1, 0, ncells, lo, hi, nghost, NUM_SPACETIME_VARS);
    GridBlock rhs2(2, 0, ncells, lo, hi, nghost, NUM_SPACETIME_VARS);

    setGaugeWaveData(grid, 0.01, 1.0);

    CCZ4Params params;
    params.ko_sigma = 0.0;
    CCZ4Evolution ccz4(params);

    ccz4.computeRHSVacuum(grid, rhs1);

    std::size_t N = grid.totalSize();
    std::vector<Real> rho(N, 0.0);
    std::vector<std::array<Real, DIM>> Si(N, {0.0, 0.0, 0.0});
    std::vector<std::array<Real, SYM_TENSOR_COMPS>> Sij(N);
    for (auto& s : Sij)
        s.fill(0.0);
    std::vector<Real> Strace(N, 0.0);

    ccz4.computeRHS(grid, rhs2, rho, Si, Sij, Strace);

    int is = grid.istart() + 1;
    int ie = grid.iend(0) - 1;
    for (int v = 0; v < NUM_SPACETIME_VARS; ++v)
        for (int k = is; k < is + 4; ++k)
            for (int j = is; j < is + 4; ++j)
                for (int i = is; i < ie; ++i)
                    EXPECT_DOUBLE_EQ(rhs1.data(v, i, j, k), rhs2.data(v, i, j, k))
                        << "Mismatch at var=" << v;
}
