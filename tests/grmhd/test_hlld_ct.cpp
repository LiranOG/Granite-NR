/**
 * @file test_hlld_ct.cpp
 * @brief Tests for HLLD Riemann solver and Constrained Transport (CT).
 *
 * CT tests use DISCRETE vector potential initialization to guarantee that
 * the initial B field is EXACTLY divergence-free under the forward-difference
 * operator used in divergenceB(). The identity div(curl(A)) = 0 holds
 * algebraically for any A when both operators use the same 1-cell forward
 * stencil -- no boundary conditions are needed for this cancellation.
 */
#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"
#include "granite/grmhd/grmhd.hpp"

#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>

using namespace granite;
using namespace granite::grmhd;

// ===========================================================================
// Helpers
// ===========================================================================

static std::shared_ptr<EquationOfState> makeIdealEOS() {
    return std::make_shared<IdealGasEOS>(5.0 / 3.0);
}

static std::array<Real, NUM_HYDRO_VARS>
primToConserved(Real rho, Real vx, Real vy, Real vz, Real p, Real eps, Real Bx, Real By, Real Bz) {
    Real B2 = Bx * Bx + By * By + Bz * Bz;
    Real v2 = vx * vx + vy * vy + vz * vz;
    std::array<Real, NUM_HYDRO_VARS> U;
    U[static_cast<int>(HydroVar::D)] = rho;
    U[static_cast<int>(HydroVar::SX)] = rho * vx;
    U[static_cast<int>(HydroVar::SY)] = rho * vy;
    U[static_cast<int>(HydroVar::SZ)] = rho * vz;
    U[static_cast<int>(HydroVar::TAU)] = 0.5 * rho * v2 + rho * eps + 0.5 * B2;
    U[static_cast<int>(HydroVar::BX)] = Bx;
    U[static_cast<int>(HydroVar::BY)] = By;
    U[static_cast<int>(HydroVar::BZ)] = Bz;
    (void)p;
    return U;
}

static std::array<Real, NUM_PRIMITIVE_VARS>
makePrimState(Real rho, Real vx, Real vy, Real vz, Real p, Real eps, Real Bx, Real By, Real Bz) {
    std::array<Real, NUM_PRIMITIVE_VARS> P{};
    P[static_cast<int>(PrimitiveVar::RHO)] = rho;
    P[static_cast<int>(PrimitiveVar::VX)] = vx;
    P[static_cast<int>(PrimitiveVar::VY)] = vy;
    P[static_cast<int>(PrimitiveVar::VZ)] = vz;
    P[static_cast<int>(PrimitiveVar::PRESS)] = p;
    P[static_cast<int>(PrimitiveVar::BX)] = Bx;
    P[static_cast<int>(PrimitiveVar::BY)] = By;
    P[static_cast<int>(PrimitiveVar::BZ)] = Bz;
    P[static_cast<int>(PrimitiveVar::EPS)] = eps;
    return P;
}

// ===========================================================================
// Discrete vector potential initialization.
//
// Given Az(i,j,k) evaluated as a function of cell-center coordinates, compute
// B using the SAME 1-cell forward-difference stencil as the CT update:
//   Bx[i,j,k] = (Az[i,j+1,k] - Az[i,j,k]) / dy
//   By[i,j,k] = -(Az[i+1,j,k] - Az[i,j,k]) / dx
//   Bz[i,j,k] = 0
//
// Proof that divB = 0 for ALL cells (no BCs required):
//   (Bx[i+1]-Bx[i])/dx + (By[j+1]-By[j])/dy
//   = [(Az[i+1,j+1]-Az[i+1,j]) - (Az[i,j+1]-Az[i,j])] / (dx*dy)
//   + [-(Az[i+1,j+1]-Az[i,j+1]) + (Az[i+1,j]-Az[i,j])] / (dx*dy)
//   = 0  (exact algebraic cancellation, term by term)
//
// The extended coordinate x(dim, idx) is valid for any idx (it is a linear
// formula), so Az at j+1=tot1 or i+1=tot0 is well-defined.
// ===========================================================================

template <typename AzFunc>
static void initBfromDiscreteVectorPotential(GridBlock& hydro_grid, AzFunc Az_fn) {
    constexpr int iBXh = static_cast<int>(HydroVar::BX);
    constexpr int iBYh = static_cast<int>(HydroVar::BY);
    constexpr int iBZh = static_cast<int>(HydroVar::BZ);

    const int tot0 = hydro_grid.totalCells(0);
    const int tot1 = hydro_grid.totalCells(1);
    const int tot2 = hydro_grid.totalCells(2);
    const Real inv_dx = 1.0 / hydro_grid.dx(0);
    const Real inv_dy = 1.0 / hydro_grid.dx(1);

    for (int k = 0; k < tot2; ++k)
        for (int j = 0; j < tot1; ++j)
            for (int i = 0; i < tot0; ++i) {
                // Cell-center coordinates (extended formula valid outside [lo,hi])
                Real x0 = hydro_grid.x(0, i);
                Real x1 = hydro_grid.x(0, i + 1);
                Real y0 = hydro_grid.x(1, j);
                Real y1 = hydro_grid.x(1, j + 1);
                Real z0 = hydro_grid.x(2, k);

                Real Az_ij = Az_fn(x0, y0, z0);
                Real Az_ijp = Az_fn(x0, y1, z0);
                Real Az_ipj = Az_fn(x1, y0, z0);

                // Forward-difference curl: same stencil as CT update
                hydro_grid.data(iBXh, i, j, k) = (Az_ijp - Az_ij) * inv_dy;
                hydro_grid.data(iBYh, i, j, k) = -(Az_ipj - Az_ij) * inv_dx;
                hydro_grid.data(iBZh, i, j, k) = 0.0;
            }
}

// ===========================================================================
// Periodic ghost-cell fill for one variable in all 3 directions.
//
// After a CT update (which only modifies interior cells), the ghost cells
// contain stale B values. Filling them periodically ensures:
//  - The EMF lambdas in subsequent CT steps see consistent B at ghost cells.
//  - The divergenceB diagnostic at boundary-interior cells reads the correct
//    updated value from the wrapped ghost (not the original stale value).
// ===========================================================================

static void fillPeriodicGhosts(GridBlock& grid, int var_idx) {
    const int is = grid.istart();
    const int ie0 = grid.iend(0);
    const int ie1 = grid.iend(1);
    const int ie2 = grid.iend(2);
    const int tot0 = grid.totalCells(0);
    const int tot1 = grid.totalCells(1);
    const int tot2 = grid.totalCells(2);
    const int n0 = ie0 - is;
    const int n1 = ie1 - is;
    const int n2 = ie2 - is;

    // X direction
    for (int k = 0; k < tot2; ++k)
        for (int j = 0; j < tot1; ++j)
            for (int g = 0; g < is; ++g) {
                grid.data(var_idx, g, j, k) = grid.data(var_idx, g + n0, j, k);
                grid.data(var_idx, ie0 + g, j, k) = grid.data(var_idx, is + g, j, k);
            }

    // Y direction
    for (int k = 0; k < tot2; ++k)
        for (int i = 0; i < tot0; ++i)
            for (int g = 0; g < is; ++g) {
                grid.data(var_idx, i, g, k) = grid.data(var_idx, i, g + n1, k);
                grid.data(var_idx, i, ie1 + g, k) = grid.data(var_idx, i, is + g, k);
            }

    // Z direction
    for (int j = 0; j < tot1; ++j)
        for (int i = 0; i < tot0; ++i)
            for (int g = 0; g < is; ++g) {
                grid.data(var_idx, i, j, g) = grid.data(var_idx, i, j, g + n2);
                grid.data(var_idx, i, j, ie2 + g) = grid.data(var_idx, i, j, is + g);
            }
}

static void fillPeriodicGhostsAllB(GridBlock& grid) {
    fillPeriodicGhosts(grid, static_cast<int>(HydroVar::BX));
    fillPeriodicGhosts(grid, static_cast<int>(HydroVar::BY));
    fillPeriodicGhosts(grid, static_cast<int>(HydroVar::BZ));
}

static void fillPeriodicGhostsAllV(GridBlock& grid) {
    fillPeriodicGhosts(grid, static_cast<int>(PrimitiveVar::VX));
    fillPeriodicGhosts(grid, static_cast<int>(PrimitiveVar::VY));
    fillPeriodicGhosts(grid, static_cast<int>(PrimitiveVar::VZ));
}

// ===========================================================================
// HLLD test fixture
// ===========================================================================

class HLLDTest : public ::testing::Test {
protected:
    void SetUp() override {
        GRMHDParams params;
        params.riemann_solver = RiemannSolver::HLLD;
        eos_ = makeIdealEOS();
        grmhd_ = std::make_unique<GRMHDEvolution>(params, eos_);
    }
    std::shared_ptr<EquationOfState> eos_;
    std::unique_ptr<GRMHDEvolution> grmhd_;
};

TEST_F(HLLDTest, ReducesToHydroForZeroB) {
    const Real gamma = 5.0 / 3.0;
    Real rhoL = 1.0, pL = 1.0, epsL = pL / (rhoL * (gamma - 1.0));
    Real rhoR = 0.125, pR = 0.1, epsR = pR / (rhoR * (gamma - 1.0));

    auto PL = makePrimState(rhoL, 0.0, 0.0, 0.0, pL, epsL, 0.0, 0.0, 0.0);
    auto PR = makePrimState(rhoR, 0.0, 0.0, 0.0, pR, epsR, 0.0, 0.0, 0.0);
    auto UL = primToConserved(rhoL, 0.0, 0.0, 0.0, pL, epsL, 0.0, 0.0, 0.0);
    auto UR = primToConserved(rhoR, 0.0, 0.0, 0.0, pR, epsR, 0.0, 0.0, 0.0);

    std::array<Real, NUM_HYDRO_VARS> flux{};
    grmhd_->computeHLLDFlux(UL, UR, PL, PR, 0.0, 0, flux);

    EXPECT_GT(flux[static_cast<int>(HydroVar::D)], 0.0)
        << "Mass flux must be positive for Sod problem";
    for (int n = 0; n < NUM_HYDRO_VARS; ++n)
        EXPECT_TRUE(std::isfinite(flux[n])) << "Flux[" << n << "] not finite";
    EXPECT_DOUBLE_EQ(flux[static_cast<int>(HydroVar::BX)], 0.0);
    EXPECT_DOUBLE_EQ(flux[static_cast<int>(HydroVar::BY)], 0.0);
    EXPECT_DOUBLE_EQ(flux[static_cast<int>(HydroVar::BZ)], 0.0);
}

TEST_F(HLLDTest, BrioWuShockTubeFluxBounded) {
    const Real gamma = 5.0 / 3.0;
    Real Bn = 0.75;
    Real rhoL = 1.0, pL = 1.0, ByL = 1.0, BzL = 0.0;
    Real rhoR = 0.125, pR = 0.1, ByR = -1.0, BzR = 0.0;
    Real epsL = pL / (rhoL * (gamma - 1.0));
    Real epsR = pR / (rhoR * (gamma - 1.0));

    auto PL = makePrimState(rhoL, 0.0, 0.0, 0.0, pL, epsL, Bn, ByL, BzL);
    auto PR = makePrimState(rhoR, 0.0, 0.0, 0.0, pR, epsR, Bn, ByR, BzR);
    auto UL = primToConserved(rhoL, 0.0, 0.0, 0.0, pL, epsL, Bn, ByL, BzL);
    auto UR = primToConserved(rhoR, 0.0, 0.0, 0.0, pR, epsR, Bn, ByR, BzR);

    std::array<Real, NUM_HYDRO_VARS> flux{};
    grmhd_->computeHLLDFlux(UL, UR, PL, PR, Bn, 0, flux);

    for (int n = 0; n < NUM_HYDRO_VARS; ++n)
        EXPECT_TRUE(std::isfinite(flux[n])) << "Brio-Wu flux[" << n << "] not finite";
    EXPECT_GT(flux[static_cast<int>(HydroVar::D)], 0.0) << "Brio-Wu mass flux must be positive";
    EXPECT_NEAR(flux[static_cast<int>(HydroVar::BX)], 0.0, 1e-15);
    EXPECT_NE(flux[static_cast<int>(HydroVar::BY)], 0.0) << "By flux must be non-zero for Brio-Wu";
}

TEST_F(HLLDTest, ContactDiscontinuityPreserved) {
    const Real gamma = 5.0 / 3.0;
    Real Bn = 1.0, By = 0.5, Bz = 0.0, p = 1.0;
    Real rhoL = 2.0, epsL = p / (rhoL * (gamma - 1.0));
    Real rhoR = 1.0, epsR = p / (rhoR * (gamma - 1.0));

    auto PL = makePrimState(rhoL, 0.0, 0.0, 0.0, p, epsL, Bn, By, Bz);
    auto PR = makePrimState(rhoR, 0.0, 0.0, 0.0, p, epsR, Bn, By, Bz);
    auto UL = primToConserved(rhoL, 0.0, 0.0, 0.0, p, epsL, Bn, By, Bz);
    auto UR = primToConserved(rhoR, 0.0, 0.0, 0.0, p, epsR, Bn, By, Bz);

    std::array<Real, NUM_HYDRO_VARS> flux{};
    grmhd_->computeHLLDFlux(UL, UR, PL, PR, Bn, 0, flux);

    EXPECT_NEAR(flux[static_cast<int>(HydroVar::D)], 0.0, 1e-12)
        << "Mass flux across stationary contact must be zero";
    Real ptot = p + 0.5 * (Bn * Bn + By * By + Bz * Bz);
    EXPECT_NEAR(flux[static_cast<int>(HydroVar::SX)], ptot - Bn * Bn, 1e-12)
        << "Normal momentum flux = total pressure minus magnetic tension";
}

TEST_F(HLLDTest, DirectionalSymmetry) {
    const Real gamma = 5.0 / 3.0;
    Real rho = 1.0, p = 1.0, eps = p / (rho * (gamma - 1.0));
    Real v = 0.5, B = 0.5;

    auto PX = makePrimState(rho, v, 0.0, 0.0, p, eps, B, B, 0.0);
    auto UX = primToConserved(rho, v, 0.0, 0.0, p, eps, B, B, 0.0);
    auto PY = makePrimState(rho, 0.0, v, 0.0, p, eps, B, B, 0.0);
    auto UY = primToConserved(rho, 0.0, v, 0.0, p, eps, B, B, 0.0);

    std::array<Real, NUM_HYDRO_VARS> fluxX{}, fluxY{};
    grmhd_->computeHLLDFlux(UX, UX, PX, PX, B, 0, fluxX);
    grmhd_->computeHLLDFlux(UY, UY, PY, PY, B, 1, fluxY);

    EXPECT_NEAR(fluxX[static_cast<int>(HydroVar::D)], fluxY[static_cast<int>(HydroVar::D)], 1e-12)
        << "Mass flux must be symmetric across dimensions";
}

// ===========================================================================
// CT test fixture
// ===========================================================================

class CTTest : public ::testing::Test {
protected:
    void SetUp() override {
        ncells = {16, 16, 16};
        lo = {-1.0, -1.0, -1.0};
        hi = {1.0, 1.0, 1.0};
        nghost = 4;

        hydro = std::make_unique<GridBlock>(0, 0, ncells, lo, hi, nghost, NUM_HYDRO_VARS);
        prims = std::make_unique<GridBlock>(1, 0, ncells, lo, hi, nghost, NUM_PRIMITIVE_VARS);
        spacetime = std::make_unique<GridBlock>(2, 0, ncells, lo, hi, nghost, NUM_SPACETIME_VARS);

        GRMHDParams params;
        params.use_constrained_transport = true;
        eos_ = makeIdealEOS();
        grmhd_ = std::make_unique<GRMHDEvolution>(params, eos_);

        // Initialize flat spacetime
        int tot0 = spacetime->totalCells(0);
        int tot1 = spacetime->totalCells(1);
        int tot2 = spacetime->totalCells(2);
        for (int k = 0; k < tot2; ++k)
            for (int j = 0; j < tot1; ++j)
                for (int i = 0; i < tot0; ++i) {
                    spacetime->data(static_cast<int>(SpacetimeVar::CHI), i, j, k) = 1.0;
                    spacetime->data(static_cast<int>(SpacetimeVar::GAMMA_XX), i, j, k) = 1.0;
                    spacetime->data(static_cast<int>(SpacetimeVar::GAMMA_YY), i, j, k) = 1.0;
                    spacetime->data(static_cast<int>(SpacetimeVar::GAMMA_ZZ), i, j, k) = 1.0;
                    spacetime->data(static_cast<int>(SpacetimeVar::LAPSE), i, j, k) = 1.0;
                }
    }

    // Initialize fluid primitives and hydro conserved (non-B) at all cells.
    void initFluid(Real rho, Real vx, Real vy, Real vz, Real p) {
        const Real gamma = 5.0 / 3.0;
        const Real eps = p / (rho * (gamma - 1.0));
        int tot0 = hydro->totalCells(0);
        int tot1 = hydro->totalCells(1);
        int tot2 = hydro->totalCells(2);
        for (int k = 0; k < tot2; ++k)
            for (int j = 0; j < tot1; ++j)
                for (int i = 0; i < tot0; ++i) {
                    hydro->data(static_cast<int>(HydroVar::D), i, j, k) = rho;
                    hydro->data(static_cast<int>(HydroVar::SX), i, j, k) = rho * vx;
                    hydro->data(static_cast<int>(HydroVar::SY), i, j, k) = rho * vy;
                    hydro->data(static_cast<int>(HydroVar::SZ), i, j, k) = 0.0;
                    hydro->data(static_cast<int>(HydroVar::TAU), i, j, k) = rho * eps;

                    prims->data(static_cast<int>(PrimitiveVar::RHO), i, j, k) = rho;
                    prims->data(static_cast<int>(PrimitiveVar::VX), i, j, k) = vx;
                    prims->data(static_cast<int>(PrimitiveVar::VY), i, j, k) = vy;
                    prims->data(static_cast<int>(PrimitiveVar::VZ), i, j, k) = vz;
                    prims->data(static_cast<int>(PrimitiveVar::PRESS), i, j, k) = p;
                    prims->data(static_cast<int>(PrimitiveVar::EPS), i, j, k) = eps;
                }
        fillPeriodicGhostsAllV(*prims);
    }

    std::array<int, DIM> ncells;
    std::array<Real, DIM> lo, hi;
    int nghost;
    std::unique_ptr<GridBlock> hydro, prims, spacetime;
    std::shared_ptr<EquationOfState> eos_;
    std::unique_ptr<GRMHDEvolution> grmhd_;
};

// ===========================================================================
// Test: div-B preserved after a single CT update.
//
// Strategy: initialize B as the discrete curl of Az = sin(pi*x)*sin(pi*y).
// By the algebraic identity div(curl(A)) = 0, the initial divB is machine-
// zero under the same forward-difference operator used in divergenceB.
// After one CT step (with periodic ghost fills), divB must remain ~ 0.
// ===========================================================================

TEST_F(CTTest, DivBPreservedUnderCTUpdate) {
    initFluid(1.0, 0.1, 0.1, 0.0, 1.0);

    // Discrete curl of Az = sin(pi*x)*sin(pi*y)
    initBfromDiscreteVectorPotential(*hydro, [](Real x, Real y, Real /*z*/) {
        return std::sin(constants::PI * x) * std::sin(constants::PI * y);
    });

    // Copy B to prims
    int tot0 = hydro->totalCells(0);
    int tot1 = hydro->totalCells(1);
    int tot2 = hydro->totalCells(2);
    for (int k = 0; k < tot2; ++k)
        for (int j = 0; j < tot1; ++j)
            for (int i = 0; i < tot0; ++i) {
                prims->data(static_cast<int>(PrimitiveVar::BX), i, j, k) =
                    hydro->data(static_cast<int>(HydroVar::BX), i, j, k);
                prims->data(static_cast<int>(PrimitiveVar::BY), i, j, k) =
                    hydro->data(static_cast<int>(HydroVar::BY), i, j, k);
                prims->data(static_cast<int>(PrimitiveVar::BZ), i, j, k) =
                    hydro->data(static_cast<int>(HydroVar::BZ), i, j, k);
            }

    // Periodic ghost fill so CT stencil accesses valid B at boundaries
    fillPeriodicGhostsAllB(*hydro);
    fillPeriodicGhostsAllB(*prims);

    // Initial divB must be machine-zero by construction
    Real divB_before = GRMHDEvolution::divergenceB(*hydro);
    EXPECT_LT(divB_before, 1e-12) << "Discrete vector potential must give initial divB = 0."
                                  << " Got: " << divB_before;

    // Apply one CT step
    Real dt = 0.001;
    grmhd_->computeCTUpdate(*spacetime, *prims, *hydro, dt);

    // Refresh ghost cells with updated interior values (periodic BC)
    fillPeriodicGhostsAllB(*hydro);

    Real divB_after = GRMHDEvolution::divergenceB(*hydro);

    // CT must preserve div-B to machine precision
    EXPECT_LT(divB_after, 1e-10) << "CT must preserve div-B after one step."
                                 << " Before: " << divB_before << ", After: " << divB_after;
}

// ===========================================================================
// Test: uniform B with zero velocity leaves B unchanged.
// ===========================================================================

TEST_F(CTTest, UniformBFieldDivBIsZero) {
    const Real Bx0 = 1.0, By0 = 0.5, Bz0 = 0.3;
    initFluid(1.0, 0.0, 0.0, 0.0, 1.0);

    int tot0 = hydro->totalCells(0);
    int tot1 = hydro->totalCells(1);
    int tot2 = hydro->totalCells(2);

    for (int k = 0; k < tot2; ++k)
        for (int j = 0; j < tot1; ++j)
            for (int i = 0; i < tot0; ++i) {
                hydro->data(static_cast<int>(HydroVar::BX), i, j, k) = Bx0;
                hydro->data(static_cast<int>(HydroVar::BY), i, j, k) = By0;
                hydro->data(static_cast<int>(HydroVar::BZ), i, j, k) = Bz0;
                prims->data(static_cast<int>(PrimitiveVar::BX), i, j, k) = Bx0;
                prims->data(static_cast<int>(PrimitiveVar::BY), i, j, k) = By0;
                prims->data(static_cast<int>(PrimitiveVar::BZ), i, j, k) = Bz0;
            }

    Real divB = GRMHDEvolution::divergenceB(*hydro);
    EXPECT_LT(divB, 1e-14) << "Uniform B must have divB = 0 exactly";

    grmhd_->computeCTUpdate(*spacetime, *prims, *hydro, 0.01);

    // B must remain uniform (all EMFs are zero with v=0)
    int ic = hydro->istart() + 3;
    EXPECT_NEAR(hydro->data(static_cast<int>(HydroVar::BX), ic, ic, ic), Bx0, 1e-13);
    EXPECT_NEAR(hydro->data(static_cast<int>(HydroVar::BY), ic, ic, ic), By0, 1e-13);
    EXPECT_NEAR(hydro->data(static_cast<int>(HydroVar::BZ), ic, ic, ic), Bz0, 1e-13);
}

// ===========================================================================
// Test: div-B does not grow under 10 repeated CT steps.
//
// Uses discrete curl of Az = sin(2*pi*x)*sin(2*pi*y) for initialization.
// Periodic ghost fills are applied before each step so the CT stencil sees
// consistent (up-to-date) B values at all ghost cells.
// ===========================================================================

TEST_F(CTTest, DivBZeroAfterMultipleSteps) {
    initFluid(1.0, 0.2, 0.1, 0.0, 1.0);

    // Discrete curl of Az = sin(2*pi*x)*sin(2*pi*y)
    initBfromDiscreteVectorPotential(*hydro, [](Real x, Real y, Real /*z*/) {
        return std::sin(2.0 * constants::PI * x) * std::sin(2.0 * constants::PI * y);
    });

    // Copy B to prims
    int tot0 = hydro->totalCells(0);
    int tot1 = hydro->totalCells(1);
    int tot2 = hydro->totalCells(2);
    for (int k = 0; k < tot2; ++k)
        for (int j = 0; j < tot1; ++j)
            for (int i = 0; i < tot0; ++i) {
                prims->data(static_cast<int>(PrimitiveVar::BX), i, j, k) =
                    hydro->data(static_cast<int>(HydroVar::BX), i, j, k);
                prims->data(static_cast<int>(PrimitiveVar::BY), i, j, k) =
                    hydro->data(static_cast<int>(HydroVar::BY), i, j, k);
                prims->data(static_cast<int>(PrimitiveVar::BZ), i, j, k) =
                    hydro->data(static_cast<int>(HydroVar::BZ), i, j, k);
            }

    // Initial periodic fill
    fillPeriodicGhostsAllB(*hydro);
    fillPeriodicGhostsAllB(*prims);

    Real divB_init = GRMHDEvolution::divergenceB(*hydro);
    EXPECT_LT(divB_init, 1e-12) << "Initial divB must be machine-zero. Got: " << divB_init;

    // Run 10 CT steps with periodic ghost fills each iteration
    Real dt = 0.0005;
    for (int step = 0; step < 10; ++step) {
        grmhd_->computeCTUpdate(*spacetime, *prims, *hydro, dt);
        // Refresh ghost cells so next step has consistent stencil values
        fillPeriodicGhostsAllB(*hydro);
    }

    Real divB_final = GRMHDEvolution::divergenceB(*hydro);

    EXPECT_LT(divB_final, 1e-10) << "div-B must remain near machine-zero after 10 CT steps."
                                 << " Initial: " << divB_init << ", Final: " << divB_final;
}
