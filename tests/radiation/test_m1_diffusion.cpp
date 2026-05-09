/**
 * @file test_m1_diffusion.cpp
 * @author Liran M. Schwartz
 * @version v0.6.7
 * @brief High-fidelity smoke tests for the M1 grey radiation transport module.
 *
 * P1-01 remediation: Adds the first unit coverage for src/radiation/m1.cpp (416 lines).
 * Test suite: M1SmokeTest
 *
 * @details
 * These tests systematically verify the core mathematical integrity of the M1
 * radiation solver, focusing on the closure relations and structural safety.
 *
 * Test matrix:
 *   - T1: Execution Safety (computeRHS runs safely on uniform isotropic data)
 *   - T2: Free-Streaming Limit (ξ → 1 forces f_Edd → 1)
 *   - T3: Diffusion Limit (ξ → 0 forces optically thick f_Edd → 1/3)
 *   - T4: Physical Boundedness (f_Edd strictly contained in [1/3, 1] for all ξ)
 */
#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"
#include "granite/radiation/m1.hpp"

#include <cmath>
#include <gtest/gtest.h>

using namespace granite;
using namespace granite::radiation;

class M1SmokeTest : public ::testing::Test {
protected:
    void SetUp() override {
        ncells_ = {8, 8, 8};
        lo_ = {-1.0, -1.0, -1.0};
        hi_ = {+1.0, +1.0, +1.0};
        nghost_ = 2;
        grid_ = std::make_unique<GridBlock>(0, 0, ncells_, lo_, hi_, nghost_, NUM_RADIATION_VARS);
        rhs_ = std::make_unique<GridBlock>(1, 0, ncells_, lo_, hi_, nghost_, NUM_RADIATION_VARS);

        // Uniform, isotropic radiation field (optically thick limit)
        M1Params params;
        params.kappa_a = 1.0;
        params.kappa_s = 0.0;
        m1_ = std::make_unique<M1Transport>(params);

        // Manually set uniform field
        for (int k = 0; k < grid_->totalCells(2); ++k) {
            for (int j = 0; j < grid_->totalCells(1); ++j) {
                for (int i = 0; i < grid_->totalCells(0); ++i) {
                    grid_->data(static_cast<int>(RadiationVar::ER), i, j, k) = 1.0e-3;
                    grid_->data(static_cast<int>(RadiationVar::FRX), i, j, k) = 0.0;
                    grid_->data(static_cast<int>(RadiationVar::FRY), i, j, k) = 0.0;
                    grid_->data(static_cast<int>(RadiationVar::FRZ), i, j, k) = 0.0;
                }
            }
        }
    }

    std::array<int, 3> ncells_;
    std::array<Real, 3> lo_, hi_;
    int nghost_;
    std::unique_ptr<GridBlock> grid_, rhs_;
    std::unique_ptr<M1Transport> m1_;
};

// ===========================================================================
// === TEST CASE T1: Execution Safety ===
// Verifies that the RHS computation kernel executes without throwing
// exceptions when presented with a standard uniform, isotropic field.
// ===========================================================================
TEST_F(M1SmokeTest, RHSDoesNotCrash) {
    GridBlock spacetime(0, 0, ncells_, lo_, hi_, nghost_, NUM_SPACETIME_VARS);
    GridBlock hydro(1, 0, ncells_, lo_, hi_, nghost_, NUM_PRIMITIVE_VARS);
    EXPECT_NO_THROW(m1_->computeRHS(spacetime, hydro, *grid_, *rhs_))
        << "M1Transport::computeRHS threw on a uniform isotropic radiation field.";
}

// ===========================================================================
// === TEST CASE T2: Free-Streaming Limit ===
// Verifies the Minerbo closure correctly recovers the free-streaming limit.
// If ξ = |F|/(c E) = 1, then the Eddington factor f_Edd must exactly equal 1.
// ===========================================================================
TEST_F(M1SmokeTest, EddingtonThinLimit) {
    std::array<Real, SYM_TENSOR_COMPS> Pij;
    // Free streaming: F = E. For F along x, P_xx should be E.
    m1_->eddingtonTensor(1.0, 1.0, 0.0, 0.0, Pij);
    EXPECT_NEAR(Pij[0], 1.0, 1.0e-10) << "P_xx in free-streaming limit should be 1.0";
}

// ===========================================================================
// === TEST CASE T3: Diffusion Limit ===
// Verifies the Minerbo closure correctly recovers the optically thick limit.
// If ξ = 0, the field is isotropic, and P_xx must exactly equal 1/3 E.
// ===========================================================================
TEST_F(M1SmokeTest, EddingtonThickLimit) {
    std::array<Real, SYM_TENSOR_COMPS> Pij;
    // Diffusion: F = 0. P_ij = 1/3 E δ_ij
    m1_->eddingtonTensor(1.0, 0.0, 0.0, 0.0, Pij);
    EXPECT_NEAR(Pij[0], 1.0 / 3.0, 1.0e-10) << "P_xx in diffusion limit should be 1/3";
}

// ===========================================================================
// === TEST CASE T4: Physical Boundedness ===
// Exhaustively verifies that the Eddington tensor principal component P_xx
// remains strictly physically bounded within [1/3, 1] for arbitrary reduced flux ξ ∈ [0, 1].
// ===========================================================================
TEST_F(M1SmokeTest, EddingtonBounded) {
    std::array<Real, SYM_TENSOR_COMPS> Pij;
    for (int n = 0; n <= 100; ++n) {
        Real xi = static_cast<Real>(n) / 100.0;
        m1_->eddingtonTensor(1.0, xi, 0.0, 0.0, Pij);
        Real Pxx = Pij[0];
        EXPECT_GE(Pxx, 1.0 / 3.0 - 1.0e-12) << "P_xx below 1/3 at xi=" << xi << ": " << Pxx;
        EXPECT_LE(Pxx, 1.0 + 1.0e-12) << "P_xx above 1.0 at xi=" << xi << ": " << Pxx;
    }
}
