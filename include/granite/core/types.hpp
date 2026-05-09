/**
 * @file granite/core/types.hpp
 * @brief Fundamental type definitions and constants for GRANITE.
 *
 * This header defines the core numeric types, physical constants, index
 * conventions, and compile-time configuration used throughout the codebase.
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace granite {

// ===========================================================================
// Floating-point precision
// ===========================================================================

#ifdef GRANITE_SINGLE_PRECISION
using Real = float;
#else
using Real = double;
#endif

constexpr Real REAL_EPS = std::numeric_limits<Real>::epsilon();
constexpr Real REAL_MAX = std::numeric_limits<Real>::max();
constexpr Real REAL_MIN = std::numeric_limits<Real>::min();

// ===========================================================================
// Spatial dimensions
// ===========================================================================

constexpr int DIM = 3;               ///< Spatial dimension (always 3)
constexpr int SPACETIME_DIM = 4;     ///< Spacetime dimension
constexpr int SYM_TENSOR_COMPS = 6;  ///< Independent components of symmetric 3-tensor
constexpr int FULL_TENSOR_COMPS = 9; ///< Components of general 3-tensor

// ===========================================================================
// Index conventions for symmetric 3-tensors (row-major lower-triangle)
// ===========================================================================
// Mapping: (i,j) -> flat index for i <= j
//   (0,0)->0, (0,1)->1, (0,2)->2, (1,1)->3, (1,2)->4, (2,2)->5

constexpr int symIdx(int i, int j) {
    return (i <= j) ? (i * DIM - i * (i - 1) / 2 + j - i) : (j * DIM - j * (j - 1) / 2 + i - j);
}

// ===========================================================================
// Physical constants (CGS)
// ===========================================================================

namespace constants {
constexpr Real G_CGS = 6.67430e-8;         ///< Gravitational constant [cm³/(g·s²)]
constexpr Real C_CGS = 2.99792458e10;      ///< Speed of light [cm/s]
constexpr Real MSUN_CGS = 1.98892e33;      ///< Solar mass [g]
constexpr Real RSUN_CGS = 6.9570e10;       ///< Solar radius [cm]
constexpr Real PC_CGS = 3.08567758e18;     ///< Parsec [cm]
constexpr Real YEAR_S = 3.15576e7;         ///< Year [s]
constexpr Real SIGMA_SB = 5.67037442e-5;   ///< Stefan-Boltzmann [erg/(cm²·s·K⁴)]
constexpr Real K_BOLTZ = 1.380649e-16;     ///< Boltzmann constant [erg/K]
constexpr Real M_PROTON = 1.672621898e-24; ///< Proton mass [g]
constexpr Real HBAR = 1.054571817e-27;     ///< Reduced Planck [erg·s]
constexpr Real PI = 3.14159265358979323846;

/// Geometric unit conversions: G = c = 1
/// Length unit: M_sun in cm
constexpr Real LENGTH_TO_CGS(Real M_code) {
    return M_code * G_CGS * MSUN_CGS / (C_CGS * C_CGS);
}
constexpr Real TIME_TO_CGS(Real M_code) {
    return M_code * G_CGS * MSUN_CGS / (C_CGS * C_CGS * C_CGS);
}
constexpr Real MASS_TO_CGS(Real M_code) {
    return M_code * MSUN_CGS;
}
} // namespace constants

// ===========================================================================
// Grid cell index type
// ===========================================================================

struct Index3D {
    int i, j, k;

    constexpr Index3D() : i(0), j(0), k(0) {}
    constexpr Index3D(int i_, int j_, int k_) : i(i_), j(j_), k(k_) {}

    constexpr int flat(int nx, int ny) const { return i + nx * (j + ny * k); }
};

// ===========================================================================
// Variable field identifiers
// ===========================================================================

/// CCZ4 evolved variables
enum class SpacetimeVar : int {
    CHI,         ///< Conformal factor χ = e^{-4φ}
    GAMMA_XX,    ///< Conformal metric γ̃_xx
    GAMMA_XY,    ///< Conformal metric γ̃_xy
    GAMMA_XZ,    ///< Conformal metric γ̃_xz
    GAMMA_YY,    ///< Conformal metric γ̃_yy
    GAMMA_YZ,    ///< Conformal metric γ̃_yz
    GAMMA_ZZ,    ///< Conformal metric γ̃_zz
    A_XX,        ///< Traceless extrinsic curvature Ã_xx
    A_XY,        ///< Ã_xy
    A_XZ,        ///< Ã_xz
    A_YY,        ///< Ã_yy
    A_YZ,        ///< Ã_yz
    A_ZZ,        ///< Ã_zz
    K_TRACE,     ///< Trace of extrinsic curvature K
    GAMMA_HAT_X, ///< Conformal Christoffel Γ̃^x
    GAMMA_HAT_Y, ///< Γ̃^y
    GAMMA_HAT_Z, ///< Γ̃^z
    THETA,       ///< CCZ4 constraint damping scalar
    LAPSE,       ///< Lapse α
    SHIFT_X,     ///< Shift β^x
    SHIFT_Y,     ///< β^y
    SHIFT_Z,     ///< β^z
    NUM_VARS     ///< Total number of spacetime variables = 22
};

constexpr int NUM_SPACETIME_VARS = static_cast<int>(SpacetimeVar::NUM_VARS);

/// GRMHD conserved variables
enum class HydroVar : int {
    D,       ///< Conserved density D = rho*W
    SX,      ///< Conserved momentum S_x = rho*h*W^2 * v_x
    SY,      ///< S_y
    SZ,      ///< S_z
    TAU,     ///< Conserved energy tau = rho*h*W^2 - p - D
    BX,      ///< Magnetic field B^x
    BY,      ///< B^y
    BZ,      ///< B^z
    DYE,     ///< Conserved electron number D*Ye = rho*W*Ye (for tabulated EOS)
    NUM_VARS ///< Total = 9
};

constexpr int NUM_HYDRO_VARS = static_cast<int>(HydroVar::NUM_VARS);

/// GRMHD primitive variables
enum class PrimitiveVar : int {
    RHO,   ///< Rest-mass density ρ
    VX,    ///< 3-velocity v^x
    VY,    ///< v^y
    VZ,    ///< v^z
    PRESS, ///< Gas pressure p
    BX,    ///< Magnetic field B^x
    BY,    ///< B^y
    BZ,    ///< B^z
    EPS,   ///< Specific internal energy ε
    TEMP,  ///< Temperature T (for tabulated EOS)
    YE,    ///< Electron fraction Y_e
    NUM_VARS
};

constexpr int NUM_PRIMITIVE_VARS = static_cast<int>(PrimitiveVar::NUM_VARS);

/// Radiation variables (M1)
enum class RadiationVar : int {
    ER,  ///< Radiation energy density E_r
    FRX, ///< Radiation flux F_r^x
    FRY, ///< F_r^y
    FRZ, ///< F_r^z
    NUM_VARS
};

constexpr int NUM_RADIATION_VARS = static_cast<int>(RadiationVar::NUM_VARS);

// ===========================================================================
// Simulation parameters
// ===========================================================================

struct SimulationParams {
    // Grid
    std::array<int, DIM> ncells = {64, 64, 64};
    std::array<Real, DIM> domain_lo = {-100.0, -100.0, -100.0};
    std::array<Real, DIM> domain_hi = {100.0, 100.0, 100.0};
    int ghost_cells = 4; ///< Ghost zone width (4 for 4th-order FD)

    // Time
    Real cfl = 0.25;
    Real t_final = 1000.0;
    int max_steps = 1000000;

    // CCZ4 parameters
    Real kappa1 = 0.02;  ///< Constraint damping κ₁
    Real kappa2 = 0.0;   ///< Constraint damping κ₂
    Real eta = 2.0;      ///< Γ-driver damping η
    Real ko_sigma = 0.1; ///< Kreiss-Oliger dissipation strength

    // GRMHD
    Real atm_density = 1.0e-12; ///< Atmosphere density floor
    Real lorentz_max = 100.0;   ///< Maximum Lorentz factor

    // AMR
    int max_levels = 3; ///< Max refinement levels (keep ≤5 for local testing)
    int refinement_ratio = 2;
    int regrid_interval = 4;
    Real refine_threshold = 0.1;    ///< Gradient threshold to trigger refinement
    Real derefine_threshold = 0.05; ///< Gradient threshold to allow de-refinement
    bool use_truncation_error =
        false; ///< Enable Richardson truncation-error tagger (disabled by default; ~15% overhead)

    // I/O
    int checkpoint_interval = 1000;
    int output_interval = 10;
    std::string output_dir = "output";
};

} // namespace granite
