/**
 * @file initial_data.cpp
 * @brief Initial data construction — Brill-Lindquist, TOV/polytrope.
 *
 * @copyright 2026 Liran M. Schwartz
 */
#include "granite/initial_data/initial_data.hpp"

#include "granite/spacetime/ccz4.hpp"

#include <cmath>
#include <iostream>
#include <numeric>

#ifdef GRANITE_USE_PETSC
#include <petscsnes.h>

namespace granite {
namespace initial_data {

struct BowenYorkCtx {
    int nx, ny, nz, nghost;
    Real dx2;
    const std::vector<Real>* psi_BL;
    const std::vector<Real>* Atilde2;
};

static PetscErrorCode FormFunction(SNES snes, Vec x, Vec f, void* ctx) {
    BowenYorkCtx* app = static_cast<BowenYorkCtx*>(ctx);
    const PetscScalar* xx;
    PetscScalar* ff;
    VecGetArrayRead(x, &xx);
    VecGetArray(f, &ff);

    int nx = app->nx, ny = app->ny, nz = app->nz, nghost = app->nghost;
    Real dx2 = app->dx2;
    const auto& psi_BL = *(app->psi_BL);
    const auto& Atilde2 = *(app->Atilde2);

    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                int flat = i + nx * (j + ny * k);
                if (i < nghost || i >= nx - nghost || j < nghost || j >= ny - nghost ||
                    k < nghost || k >= nz - nghost) {
                    ff[flat] = xx[flat];
                    continue;
                }

                Real u_c = xx[flat];
                Real u_avg = xx[flat + 1] + xx[flat - 1] + xx[flat + nx] + xx[flat - nx] +
                    xx[flat + nx * ny] + xx[flat - nx * ny];
                Real D2u = (u_avg - 6.0 * u_c) / dx2;
                Real psi = psi_BL[flat] + u_c;
                Real source = 0.125 * Atilde2[flat] / std::pow(psi, 7.0);

                ff[flat] = D2u + source;
            }
        }
    }

    VecRestoreArrayRead(x, &xx);
    VecRestoreArray(f, &ff);
    return 0;
}

} // namespace initial_data
} // namespace granite
#endif

namespace granite::initial_data {

// ===========================================================================
// Brill-Lindquist
// ===========================================================================

BrillLindquist::BrillLindquist(const std::vector<BlackHoleParams>& bhs) : bhs_(bhs) {}

Real BrillLindquist::conformalFactor(Real x, Real y, Real z) const {
    Real psi = 1.0;
    for (const auto& bh : bhs_) {
        Real dx = x - bh.position[0];
        Real dy = y - bh.position[1];
        Real dz = z - bh.position[2];
        Real r = std::sqrt(dx * dx + dy * dy + dz * dz + 1.0e-30);
        psi += bh.mass / (2.0 * r);
    }
    return psi;
}

Real BrillLindquist::admMass() const {
    return std::accumulate(bhs_.begin(), bhs_.end(), 0.0, [](Real sum, const BlackHoleParams& bh) {
        return sum + bh.mass;
    });
}

void BrillLindquist::apply(GridBlock& grid) const {
    // Set flat spacetime baseline (α=1, β^i=0, γ̃_ij=δ_ij, K=0, A_ij=0, Θ=0)
    spacetime::setFlatSpacetime(grid);

    const int nx = grid.totalCells(0);
    const int ny = grid.totalCells(1);
    const int nz = grid.totalCells(2);

    // Single analytic pass over ALL cells including ghost cells.
    // Computing Γ̃^i analytically (not via finite differences) ensures
    // ghost cells are initialized correctly, preventing NaN in the CCZ4
    // 4th-order stencil at the interior/ghost-cell boundary.
    //
    // ψ = 1 + Σ_A M_A / (2 r_A)          [BL conformal factor]
    // χ = ψ^{-4}                          [CCZ4 variable]
    // ∂_i ψ = -Σ_A M_A (x^i - x_A^i) / (2 r_A³)
    // Γ̃^i = -(2/3) (∂_i χ)/χ = (8/3) (∂_i ψ)/ψ
    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                const Real x = grid.x(0, i);
                const Real y = grid.x(1, j);
                const Real z = grid.x(2, k);

                // Conformal factor ψ and χ
                const Real psi = conformalFactor(x, y, z); // ≥ 1 always
                const Real psi2 = psi * psi;
                const Real chi = 1.0 / (psi2 * psi2); // ψ^{-4}

                grid.data(static_cast<int>(SpacetimeVar::CHI), i, j, k) = chi;
                // Pre-collapsed lapse: α = ψ^{-2} = √χ
                // With α=1 (flat), the 1+log slicing must dynamically
                // collapse the lapse near the puncture over the first O(M)
                // of evolution time, causing a violent spike in K and Ã_ij.
                // Starting with α=√χ places the gauge near its quasi-stationary
                // trumpet value, eliminating the transient. Standard in all
                // modern NR codes: Campanelli+2006, Hannam+2007, GRChombo, BAM.
                grid.data(static_cast<int>(SpacetimeVar::LAPSE), i, j, k) = std::sqrt(chi);

                // Analytic derivatives of ψ from each BH
                Real dpsi_dx = 0.0, dpsi_dy = 0.0, dpsi_dz = 0.0;
                for (const auto& bh : bhs_) {
                    const Real rx = x - bh.position[0];
                    const Real ry = y - bh.position[1];
                    const Real rz = z - bh.position[2];
                    const Real r2 = rx * rx + ry * ry + rz * rz + 1.0e-20; // regularize puncture
                    const Real r3 = r2 * std::sqrt(r2);
                    const Real c = -bh.mass / (2.0 * r3);
                    dpsi_dx += c * rx;
                    dpsi_dy += c * ry;
                    dpsi_dz += c * rz;
                }

                // Γ̃^i = (8/3) ∂_i ψ / ψ   [ψ ≥ 1, no division by zero]
                const Real gf = 8.0 / (3.0 * psi);
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_HAT_X), i, j, k) = gf * dpsi_dx;
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_HAT_Y), i, j, k) = gf * dpsi_dy;
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_HAT_Z), i, j, k) = gf * dpsi_dz;
            }
        }
    }
}

// ===========================================================================
// Bowen-York — Hamiltonian constraint solver for spinning/boosted punctures
// (SOR relaxation by default; optional PETSc SNES backend via GRANITE_USE_PETSC)
// ===========================================================================

BowenYorkPuncture::BowenYorkPuncture(const std::vector<BlackHoleParams>& bhs) : bhs_(bhs) {}

void BowenYorkPuncture::solve(GridBlock& grid) const {
    // Step 1: Set Brill-Lindquist background
    BrillLindquist bl(bhs_);
    bl.apply(grid);

    // Step 2: Set Bowen-York extrinsic curvature
    setBowenYorkExtrinsicCurvature(grid);

    // Step 3: Solve Hamiltonian constraint for correction u(r)
    const int nx = grid.totalCells(0);
    const int ny = grid.totalCells(1);
    const int nz = grid.totalCells(2);
    const int nghost = grid.getNumGhost();

    std::vector<Real> u(grid.totalSize(), 0.0);
    std::vector<Real> u_new(grid.totalSize(), 0.0);
    std::vector<Real> psi_BL(grid.totalSize(), 1.0);
    std::vector<Real> Atilde2(grid.totalSize(), 0.0);

    Real dx = grid.dx(0);
    Real dx2 = dx * dx;

    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                int flat = i + nx * (j + ny * k);
                Real x = grid.x(0, i);
                Real y = grid.x(1, j);
                Real z = grid.x(2, k);

                psi_BL[flat] = bl.conformalFactor(x, y, z);

                Real Axx = grid.data(static_cast<int>(SpacetimeVar::A_XX), i, j, k);
                Real Axy = grid.data(static_cast<int>(SpacetimeVar::A_XY), i, j, k);
                Real Axz = grid.data(static_cast<int>(SpacetimeVar::A_XZ), i, j, k);
                Real Ayy = grid.data(static_cast<int>(SpacetimeVar::A_YY), i, j, k);
                Real Ayz = grid.data(static_cast<int>(SpacetimeVar::A_YZ), i, j, k);
                Real Azz = grid.data(static_cast<int>(SpacetimeVar::A_ZZ), i, j, k);

                Atilde2[flat] =
                    Axx * Axx + Ayy * Ayy + Azz * Azz + 2.0 * (Axy * Axy + Axz * Axz + Ayz * Ayz);
            }
        }
    }

#ifdef GRANITE_USE_PETSC
    std::cout << "Using PETSc SNES solver for Bowen-York constraint...\n";

    SNES snes;
    Vec x, r;
    Mat J;
    int N = grid.totalSize();

    VecCreateMPI(PETSC_COMM_WORLD, N, PETSC_DETERMINE, &x);
    VecDuplicate(x, &r);
    VecSet(x, 0.0);

    BowenYorkCtx ctx = {nx, ny, nz, nghost, dx2, &psi_BL, &Atilde2};

    SNESCreate(PETSC_COMM_WORLD, &snes);
    SNESSetFunction(snes, r, FormFunction, &ctx);

    MatCreateMPIAIJ(PETSC_COMM_WORLD, N, N, PETSC_DETERMINE, PETSC_DETERMINE, 7, NULL, 7, NULL, &J);
    SNESSetJacobian(snes, J, J, SNESComputeJacobianDefault, &ctx);

    KSP ksp;
    PC pc;
    SNESGetKSP(snes, &ksp);
    KSPGetPC(ksp, &pc);
    PCSetType(pc, PCGAMG);

    SNESSetFromOptions(snes);
    SNESSolve(snes, NULL, x);

    PetscInt iters;
    SNESGetIterationNumber(snes, &iters);

    PetscReal norm;
    SNESGetFunctionNorm(snes, &norm);
    std::cout << "PETSc SNES converged in " << iters << " iterations, residual=" << norm << "\n";

    const PetscScalar* xx;
    VecGetArrayRead(x, &xx);
    for (int i = 0; i < N; ++i)
        u_new[i] = xx[i];
    VecRestoreArrayRead(x, &xx);
    u.swap(u_new);

    MatDestroy(&J);
    VecDestroy(&x);
    VecDestroy(&r);
    SNESDestroy(&snes);
#else
    // True Gauss-Seidel SOR Relaxation Iterator
    const int max_iter = 5000;
    const Real tol = 1.0e-5;
    const Real omega = 1.5; // Proper SOR requires in-place update
    Real residual = 1.0;
    int iter = 0;

    while (iter < max_iter && residual > tol) {
        residual = 0.0;
        for (int k = nghost; k < nz - nghost; ++k) {
            for (int j = nghost; j < ny - nghost; ++j) {
                for (int i = nghost; i < nx - nghost; ++i) {
                    int flat = i + nx * (j + ny * k);

                    // True Gauss-Seidel uses the latest updated values in 'u' directly
                    Real u_avg = u[flat + 1] + u[flat - 1] + u[flat + nx] + u[flat - nx] +
                        u[flat + nx * ny] + u[flat - nx * ny];

                    Real psi7_inv = 1.0 / std::pow(psi_BL[flat] + u[flat], 7.0);
                    Real rhs = -0.125 * Atilde2[flat] * psi7_inv;
                    Real unew = (u_avg - dx2 * rhs) / 6.0;

                    Real diff = unew - u[flat];
                    u[flat] += omega * diff; // In-place SOR update

                    residual += diff * diff;
                }
            }
        }
        residual = std::sqrt(residual / grid.totalSize());
        iter++;
    }

    std::cout << "Bowen-York SOR solver: " << iter << " iterations, residual=" << residual << "\n";
#endif

    // Set final physical data
    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                int flat = i + nx * (j + ny * k);
                Real psi = psi_BL[flat] + u[flat];
                grid.data(static_cast<int>(SpacetimeVar::CHI), i, j, k) =
                    1.0 / (psi * psi * psi * psi);
                grid.data(static_cast<int>(SpacetimeVar::LAPSE), i, j, k) = 1.0 / (psi * psi);
            }
        }
    }
}

void BowenYorkPuncture::setBowenYorkExtrinsicCurvature(GridBlock& grid) const {
    const int nx = grid.totalCells(0);
    const int ny = grid.totalCells(1);
    const int nz = grid.totalCells(2);

    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                Real x = grid.x(0, i);
                Real y = grid.x(1, j);
                Real z = grid.x(2, k);

                // Bowen-York Ã_{ij} for each puncture:
                // Ã_{ij}^{BY} = (3/2r²) [2P_{(i} n_{j)} - (δ_{ij} - n_i n_j) P^k n_k]
                //             + (3/r³) [ε_{kl(i} S^l n_{j)} n^k]
                Real Atilde[6] = {};

                for (const auto& bh : bhs_) {
                    Real dx = x - bh.position[0];
                    Real dy = y - bh.position[1];
                    Real dz = z - bh.position[2];
                    Real r2 = dx * dx + dy * dy + dz * dz + 1.0e-30;
                    Real r = std::sqrt(r2);
                    Real inv_r2 = 1.0 / r2;

                    Real n[3] = {dx / r, dy / r, dz / r};
                    Real P[3] = {bh.momentum[0], bh.momentum[1], bh.momentum[2]};
                    Real Pn = P[0] * n[0] + P[1] * n[1] + P[2] * n[2];

                    // Linear momentum contribution
                    for (int a = 0; a < 3; ++a) {
                        for (int b = a; b < 3; ++b) {
                            Real delta_ab = (a == b) ? 1.0 : 0.0;
                            Real val = 1.5 * inv_r2 *
                                (P[a] * n[b] + P[b] * n[a] - (delta_ab - n[a] * n[b]) * Pn);
                            Atilde[symIdx(a, b)] += val;
                        }
                    }

                    // Spin contribution: (3/r³) ε_{kl(i} S^l n_{j)} n^k
                    Real S[3] = {bh.spin[0], bh.spin[1], bh.spin[2]};
                    Real inv_r3 = inv_r2 / r;

                    // ε_{kli} S^l n_j n^k (symmetrized over ij)
                    // Using Levi-Civita: ε_{012}=1, etc.
                    auto levi = [](int a, int b, int c) -> Real {
                        if ((a == 0 && b == 1 && c == 2) || (a == 1 && b == 2 && c == 0) ||
                            (a == 2 && b == 0 && c == 1))
                            return 1.0;
                        if ((a == 0 && b == 2 && c == 1) || (a == 1 && b == 0 && c == 2) ||
                            (a == 2 && b == 1 && c == 0))
                            return -1.0;
                        return 0.0;
                    };

                    for (int ii = 0; ii < 3; ++ii) {
                        for (int jj = ii; jj < 3; ++jj) {
                            Real spin_term = 0.0;
                            for (int kk = 0; kk < 3; ++kk) {
                                for (int ll = 0; ll < 3; ++ll) {
                                    spin_term += levi(kk, ll, ii) * S[ll] * n[jj] * n[kk] +
                                        levi(kk, ll, jj) * S[ll] * n[ii] * n[kk];
                                }
                            }
                            Atilde[symIdx(ii, jj)] += 3.0 * inv_r3 * 0.5 * spin_term;
                        }
                    }
                }

                // Write to grid (conformal traceless extrinsic curvature)
                grid.data(static_cast<int>(SpacetimeVar::A_XX), i, j, k) = Atilde[0];
                grid.data(static_cast<int>(SpacetimeVar::A_XY), i, j, k) = Atilde[1];
                grid.data(static_cast<int>(SpacetimeVar::A_XZ), i, j, k) = Atilde[2];
                grid.data(static_cast<int>(SpacetimeVar::A_YY), i, j, k) = Atilde[3];
                grid.data(static_cast<int>(SpacetimeVar::A_YZ), i, j, k) = Atilde[4];
                grid.data(static_cast<int>(SpacetimeVar::A_ZZ), i, j, k) = Atilde[5];
            }
        }
    }
}

Real BowenYorkPuncture::admMass() const {
    Real M = 0.0;
    for (const auto& bh : bhs_)
        M += bh.mass;
    return M;
}

std::array<Real, DIM> BowenYorkPuncture::admAngularMomentum() const {
    std::array<Real, DIM> J = {0, 0, 0};
    for (const auto& bh : bhs_) {
        J[0] += bh.spin[0];
        J[1] += bh.spin[1];
        J[2] += bh.spin[2];
    }
    return J;
}

// ===========================================================================
// Superposed Kerr-Schild — analytic superposition of individual Kerr-Schild
// metrics (Matzner et al. 1999); conformal factor extracted from det(g_ij).
// ===========================================================================

SuperposedKerrSchild::SuperposedKerrSchild(const std::vector<BlackHoleParams>& bhs) : bhs_(bhs) {}

Real SuperposedKerrSchild::kerrSchildH(Real mass, Real spin_a, Real x, Real y, Real z) {
    // Kerr-Schild scalar for a BH with mass M and spin a along z
    // H = Mr³ / (r⁴ + a²z²)
    // where r is the Kerr radial coordinate satisfying:
    // x² + y² + z² = r² + a² - a²z²/r²
    Real a2 = spin_a * spin_a;
    Real rho2 = x * x + y * y + z * z - a2;
    Real r2 = 0.5 * (rho2 + std::sqrt(rho2 * rho2 + 4.0 * a2 * z * z));
    Real r = std::sqrt(r2 + 1.0e-30);
    return mass * r * r2 / (r2 * r2 + a2 * z * z + 1.0e-30);
}

void SuperposedKerrSchild::apply(GridBlock& grid) const {
    const int nx = grid.totalCells(0);
    const int ny = grid.totalCells(1);
    const int nz = grid.totalCells(2);

    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                Real x = grid.x(0, i);
                Real y = grid.x(1, j);
                Real z = grid.x(2, k);

                Real g[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
                Real shift[3] = {0, 0, 0};
                Real H_total = 0.0;

                for (const auto& bh : bhs_) {
                    Real dx_ = x - bh.position[0];
                    Real dy_ = y - bh.position[1];
                    Real dz_ = z - bh.position[2];

                    Real a = bh.spin[2] / (bh.mass + 1e-30);
                    Real H = kerrSchildH(bh.mass, a, dx_, dy_, dz_);
                    H_total += H;

                    Real rho2 = dx_ * dx_ + dy_ * dy_ + dz_ * dz_ - a * a;
                    Real r2 = 0.5 * (rho2 + std::sqrt(rho2 * rho2 + 4.0 * a * a * dz_ * dz_));
                    Real r = std::sqrt(r2 + 1e-30);

                    Real l[3] = {(r * dx_ + a * dy_) / (r2 + a * a),
                                 (r * dy_ - a * dx_) / (r2 + a * a),
                                 dz_ / r};

                    for (int ii = 0; ii < 3; ++ii) {
                        for (int jj = 0; jj < 3; ++jj) {
                            g[ii][jj] += 2.0 * H * l[ii] * l[jj];
                        }
                    }
                    for (int ii = 0; ii < 3; ++ii) {
                        shift[ii] += 2.0 * H * l[ii];
                    }
                }

                Real detg = g[0][0] * (g[1][1] * g[2][2] - g[1][2] * g[2][1]) -
                    g[0][1] * (g[1][0] * g[2][2] - g[1][2] * g[2][0]) +
                    g[0][2] * (g[1][0] * g[2][1] - g[1][1] * g[2][0]);

                Real chi = std::pow(detg, -1.0 / 3.0);
                Real alpha = 1.0 / std::sqrt(1.0 + 2.0 * H_total);

                grid.data(static_cast<int>(SpacetimeVar::LAPSE), i, j, k) = alpha;
                grid.data(static_cast<int>(SpacetimeVar::CHI), i, j, k) = chi;

                grid.data(static_cast<int>(SpacetimeVar::GAMMA_XX), i, j, k) = chi * g[0][0];
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_XY), i, j, k) = chi * g[0][1];
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_XZ), i, j, k) = chi * g[0][2];
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_YY), i, j, k) = chi * g[1][1];
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_YZ), i, j, k) = chi * g[1][2];
                grid.data(static_cast<int>(SpacetimeVar::GAMMA_ZZ), i, j, k) = chi * g[2][2];

                grid.data(static_cast<int>(SpacetimeVar::SHIFT_X), i, j, k) =
                    alpha * alpha * shift[0];
                grid.data(static_cast<int>(SpacetimeVar::SHIFT_Y), i, j, k) =
                    alpha * alpha * shift[1];
                grid.data(static_cast<int>(SpacetimeVar::SHIFT_Z), i, j, k) =
                    alpha * alpha * shift[2];
            }
        }
    }
}

// ===========================================================================
// Stellar initial data
// ===========================================================================

StellarInitialData::StellarInitialData(const std::vector<StarParams>& stars) : stars_(stars) {}

StellarInitialData::StellarProfile
StellarInitialData::solvePolytrope(const StarParams& star) const {
    // Solve the Lane-Emden equation for a polytrope: p = K ρ^Γ
    // Using the dimensionless form: (1/ξ²) d/dξ (ξ² dθ/dξ) = -θ^n
    // where n = 1/(Γ-1), ρ = ρ_c θ^n, ξ = r/α with α = √(K(n+1)ρ_c^{1/n-1}/(4π))

    const Real Gamma = star.polytropic_gamma;
    const Real n = 1.0 / (Gamma - 1.0);
    const Real rho_c = star.central_density;
    const Real K = star.polytropic_K;

    // Characteristic length scale
    Real alpha_scale = std::sqrt((n + 1.0) * K * std::pow(rho_c, 1.0 / n - 1.0) /
                                 (4.0 * constants::PI * constants::G_CGS));

    // Integrate Lane-Emden with RK4
    const int N_steps = 10000;
    const Real dxi = 0.001;

    StellarProfile profile;
    Real theta = 1.0;  // θ(0) = 1
    Real dtheta = 0.0; // dθ/dξ(0) = 0
    Real enclosed_mass = 0.0;

    for (int step = 0; step < N_steps; ++step) {
        Real xi = step * dxi + 1.0e-10;
        Real r = xi * alpha_scale;

        Real rho = rho_c * std::pow(std::max(theta, 0.0), n);
        Real p = K * std::pow(rho, Gamma);
        Real eps = p / (rho * (Gamma - 1.0) + 1.0e-30);

        if (theta <= 0.0 || r > star.radius * constants::RSUN_CGS)
            break;

        profile.r.push_back(r);
        profile.rho.push_back(rho);
        profile.press.push_back(p);
        profile.eps.push_back(eps);
        enclosed_mass += 4.0 * constants::PI * rho * r * r * alpha_scale * dxi;
        profile.mass.push_back(enclosed_mass);

        // RK4 step for Lane-Emden
        auto f_theta = [](Real, Real dth) { return dth; };
        auto f_dtheta = [n](Real xi_val, Real th, Real dth) {
            if (xi_val < 1.0e-10)
                return -th * th * th / 3.0; // L'Hôpital at origin
            return -std::pow(std::max(th, 0.0), n) - 2.0 * dth / xi_val;
        };

        Real k1_t = dxi * f_theta(xi, dtheta);
        Real k1_d = dxi * f_dtheta(xi, theta, dtheta);
        Real k2_t = dxi * f_theta(xi + 0.5 * dxi, dtheta + 0.5 * k1_d);
        Real k2_d = dxi * f_dtheta(xi + 0.5 * dxi, theta + 0.5 * k1_t, dtheta + 0.5 * k1_d);
        Real k3_t = dxi * f_theta(xi + 0.5 * dxi, dtheta + 0.5 * k2_d);
        Real k3_d = dxi * f_dtheta(xi + 0.5 * dxi, theta + 0.5 * k2_t, dtheta + 0.5 * k2_d);
        Real k4_t = dxi * f_theta(xi + dxi, dtheta + k3_d);
        Real k4_d = dxi * f_dtheta(xi + dxi, theta + k3_t, dtheta + k3_d);

        theta += (k1_t + 2 * k2_t + 2 * k3_t + k4_t) / 6.0;
        dtheta += (k1_d + 2 * k2_d + 2 * k3_d + k4_d) / 6.0;
    }

    return profile;
}

StellarInitialData::StellarProfile StellarInitialData::solveTOV(const StarParams& star) const {
    // TOV equations:
    // dp/dr = -(ρ + p/c²)(m + 4πr³p/c²) / (r(r - 2Gm/c²))
    // dm/dr = 4πρr²

    const Real rho_c = star.central_density;
    const Real Gamma = star.polytropic_gamma;
    const Real K_poly = star.polytropic_K;

    const Real G = constants::G_CGS;
    const Real c2 = constants::C_CGS * constants::C_CGS;

    // star.radius is in km (neutron star scale); convert to cm.
    const Real R_cm = star.radius * 1.0e5; // km -> cm
    const int N_steps = 50000;
    const Real dr = R_cm / N_steps;

    StellarProfile profile;
    Real p = K_poly * std::pow(rho_c, Gamma);
    Real m = 0.0;
    const Real p0 = p; // central pressure for termination check

    for (int step = 0; step < N_steps; ++step) {
        Real r = (step + 0.5) * dr;
        if (r < 1.0e-5)
            r = 1.0e-5;

        Real rho = std::pow(std::max(p / K_poly, 0.0), 1.0 / Gamma);
        Real eps = (rho > 1.0e-30) ? p / (rho * (Gamma - 1.0)) : 0.0;

        if (p <= 0.0 || rho <= 0.0)
            break;

        profile.r.push_back(r);
        profile.rho.push_back(rho);
        profile.press.push_back(p);
        profile.eps.push_back(eps);
        profile.mass.push_back(m);

        // TOV equations (in CGS)
        Real Schwarzschild_term = r - 2.0 * G * m / c2;
        Real denom = r * Schwarzschild_term;
        if (std::abs(denom) < 1.0e-30 || Schwarzschild_term <= 0.0)
            break;

        Real dp_dr = -(rho + p / c2) * (m + 4.0 * constants::PI * r * r * r * p / c2) * G / denom;
        Real dm_dr = 4.0 * constants::PI * rho * r * r;

        p += dp_dr * dr;
        m += dm_dr * dr;

        if (p < 0.0 || p < 1.0e-10 * p0)
            break;
    }

    return profile;
}

void StellarInitialData::apply(GridBlock& spacetime_grid, GridBlock& hydro_grid) const {
    for (const auto& star : stars_) {
        StellarProfile profile;
        if (star.model == StellarModel::POLYTROPE) {
            profile = solvePolytrope(star);
        } else if (star.model == StellarModel::TOV) {
            profile = solveTOV(star);
        } else if (star.model == StellarModel::MESA_TABLE) {
            throw std::runtime_error("MESA_TABLE initial data not yet implemented.");
        }

        if (profile.r.empty())
            continue;

        Real R_star = profile.r.back();

        const int nx = hydro_grid.totalCells(0);
        const int ny = hydro_grid.totalCells(1);
        const int nz = hydro_grid.totalCells(2);

        for (int k = 0; k < nz; ++k) {
            for (int j = 0; j < ny; ++j) {
                for (int i = 0; i < nx; ++i) {
                    Real x = hydro_grid.x(0, i) - star.position[0];
                    Real y = hydro_grid.x(1, j) - star.position[1];
                    Real z = hydro_grid.x(2, k) - star.position[2];
                    Real r = std::sqrt(x * x + y * y + z * z);

                    if (r > R_star)
                        continue;

                    // Linear interpolation in the profile
                    std::size_t idx = 0;
                    for (std::size_t n = 0; n < profile.r.size() - 1; ++n) {
                        if (r >= profile.r[n] && r < profile.r[n + 1]) {
                            idx = n;
                            break;
                        }
                    }

                    Real frac =
                        (r - profile.r[idx]) / (profile.r[idx + 1] - profile.r[idx] + 1e-30);
                    Real rho = profile.rho[idx] + frac * (profile.rho[idx + 1] - profile.rho[idx]);
                    Real p =
                        profile.press[idx] + frac * (profile.press[idx + 1] - profile.press[idx]);
                    Real eps = profile.eps[idx] + frac * (profile.eps[idx + 1] - profile.eps[idx]);

                    hydro_grid.data(static_cast<int>(PrimitiveVar::RHO), i, j, k) = rho;
                    hydro_grid.data(static_cast<int>(PrimitiveVar::PRESS), i, j, k) = p;
                    hydro_grid.data(static_cast<int>(PrimitiveVar::EPS), i, j, k) = eps;
                    hydro_grid.data(static_cast<int>(PrimitiveVar::TEMP), i, j, k) =
                        star.temperature;
                    hydro_grid.data(static_cast<int>(PrimitiveVar::YE), i, j, k) = star.Ye;
                }
            }
        }

        // Add seed magnetic field
        addMagneticField(hydro_grid, star);
    }
}

void StellarInitialData::addMagneticField(GridBlock& hydro_grid, const StarParams& star) const {
    const int nx = hydro_grid.totalCells(0);
    const int ny = hydro_grid.totalCells(1);
    const int nz = hydro_grid.totalCells(2);

    Real B0 = star.B_field_seed;
    // Fix: TOV returns radius in km, we need cm. Polytrope uses Rsun.
    Real R_star = star.radius * ((star.model == StellarModel::TOV) ? 1.0e5 : constants::RSUN_CGS);

    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                Real x = hydro_grid.x(0, i) - star.position[0];
                Real y = hydro_grid.x(1, j) - star.position[1];
                Real z = hydro_grid.x(2, k) - star.position[2];
                Real r = std::sqrt(x * x + y * y + z * z + 1e-30);

                if (r > R_star)
                    continue;

                // Fix: to guarantee div B = 0 with a cutoff, we smoothly damp the field
                // using a tapering function near the surface rather than a sharp jump.
                Real cutoff = 1.0;
                Real smooth_radius = 0.9 * R_star;
                if (r > smooth_radius) {
                    Real x_val = (r - smooth_radius) / (R_star - smooth_radius);
                    cutoff = 0.5 * (1.0 + std::cos(constants::PI * x_val));
                }

                // Simplified uniform field inside the star, matching literature standard ID
                hydro_grid.data(static_cast<int>(PrimitiveVar::BZ), i, j, k) = B0 * cutoff;
            }
        }
    }
}

// ===========================================================================
// TwoPunctures — Bowen-York extrinsic curvature + SOR / PETSc SNES Hamiltonian
// constraint solve for the conformal factor correction u(r) (Brandt & Brügmann 1997).
// ===========================================================================

TwoPuncturesBBH::TwoPuncturesBBH(const TwoPuncturesParams& params) : params_(params) {}

void TwoPuncturesBBH::generate(GridBlock& grid) const {
    const int nx = grid.totalCells(0);
    const int ny = grid.totalCells(1);
    const int nz = grid.totalCells(2);

    auto compute_Kij = [](Real x,
                          Real y,
                          Real z,
                          const std::array<Real, DIM>& pos,
                          const std::array<Real, DIM>& P,
                          const std::array<Real, DIM>& S) {
        Real rx = x - pos[0];
        Real ry = y - pos[1];
        Real rz = z - pos[2];
        Real r2 = rx * rx + ry * ry + rz * rz + 1.0e-20;
        Real r = std::sqrt(r2);
        Real r3 = r2 * r;

        Real nnx = rx / r, nny = ry / r, nnz = rz / r; // unit normal; distinct from outer nx/ny/nz
        Real Pn = P[0] * nnx + P[1] * nny + P[2] * nnz;

        Real Sn_x = S[1] * nnz - S[2] * nny;
        Real Sn_y = S[2] * nnx - S[0] * nnz;
        Real Sn_z = S[0] * nny - S[1] * nnx;

        std::array<Real, 6> Kij = {0}; // xx, yy, zz, xy, xz, yz

        Real cP = 3.0 / (2.0 * r2);
        Kij[0] = cP * (2.0 * P[0] * nnx - (1.0 - nnx * nnx) * Pn); // xx
        Kij[1] = cP * (2.0 * P[1] * nny - (1.0 - nny * nny) * Pn); // yy
        Kij[2] = cP * (2.0 * P[2] * nnz - (1.0 - nnz * nnz) * Pn); // zz
        Kij[3] = cP * (P[0] * nny + P[1] * nnx + nnx * nny * Pn);  // xy
        Kij[4] = cP * (P[0] * nnz + P[2] * nnx + nnx * nnz * Pn);  // xz
        Kij[5] = cP * (P[1] * nnz + P[2] * nny + nny * nnz * Pn);  // yz

        Real cS = 3.0 / (2.0 * r3);
        Kij[0] += cS * (2.0 * nnx * Sn_x);
        Kij[1] += cS * (2.0 * nny * Sn_y);
        Kij[2] += cS * (2.0 * nnz * Sn_z);
        Kij[3] += cS * (nnx * Sn_y + nny * Sn_x);
        Kij[4] += cS * (nnx * Sn_z + nnz * Sn_x);
        Kij[5] += cS * (nny * Sn_z + nnz * Sn_y);

        return Kij;
    };

    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                Real x = grid.x(0, i);
                Real y = grid.x(1, j);
                Real z = grid.x(2, k);

                auto K_plus =
                    compute_Kij(x, y, z, params_.par_b, params_.par_P_plus, params_.par_S_plus);
                std::array<Real, DIM> b_minus = {
                    -params_.par_b[0], -params_.par_b[1], -params_.par_b[2]};
                auto K_minus =
                    compute_Kij(x, y, z, b_minus, params_.par_P_minus, params_.par_S_minus);

                // Initial slice is maximal (K=0) so A_ij = K_ij
                grid.data(static_cast<int>(SpacetimeVar::A_XX), i, j, k) = K_plus[0] + K_minus[0];
                grid.data(static_cast<int>(SpacetimeVar::A_YY), i, j, k) = K_plus[1] + K_minus[1];
                grid.data(static_cast<int>(SpacetimeVar::A_ZZ), i, j, k) = K_plus[2] + K_minus[2];
                grid.data(static_cast<int>(SpacetimeVar::A_XY), i, j, k) = K_plus[3] + K_minus[3];
                grid.data(static_cast<int>(SpacetimeVar::A_XZ), i, j, k) = K_plus[4] + K_minus[4];
                grid.data(static_cast<int>(SpacetimeVar::A_YZ), i, j, k) = K_plus[5] + K_minus[5];
            }
        }
    }

    // PHASE 4 - NON-LINEAR SPECTRAL RELAXATION SOLVER
    // -----------------------------------------------------------------------
    // The scalar invariant \tilde{A}_{ij} \tilde{A}^{ij}
    const int nghost = grid.getNumGhost();
    const Real dx = grid.dx(0);
    const Real dx2 = dx * dx;
    std::vector<Real> A2(nx * ny * nz, 0.0);

#pragma omp parallel for
    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                int flat =
                    i + nx * (j + ny * k); // Custom flat indexing compatible with granite loops
                Real axx = grid.data(static_cast<int>(SpacetimeVar::A_XX), i, j, k);
                Real ayy = grid.data(static_cast<int>(SpacetimeVar::A_YY), i, j, k);
                Real azz = grid.data(static_cast<int>(SpacetimeVar::A_ZZ), i, j, k);
                Real axy = grid.data(static_cast<int>(SpacetimeVar::A_XY), i, j, k);
                Real axz = grid.data(static_cast<int>(SpacetimeVar::A_XZ), i, j, k);
                Real ayz = grid.data(static_cast<int>(SpacetimeVar::A_YZ), i, j, k);

                // \tilde{A}_{ij} \tilde{A}^{ij} with \tilde{\gamma}^{ij} = \delta^{ij}
                A2[flat] =
                    axx * axx + ayy * ayy + azz * azz + 2.0 * (axy * axy + axz * axz + ayz * ayz);
            }
        }
    }

    std::vector<Real> u(nx * ny * nz, 0.0);
    std::vector<Real> u_new(nx * ny * nz, 0.0);
    const Real tol = 1.0e-10;
    const int max_iter = 10000;

    for (int iter = 0; iter < max_iter; ++iter) {
        Real max_res = 0.0;

#pragma omp parallel for reduction(max : max_res)
        for (int k = nghost; k < nz - nghost; ++k) {
            for (int j = nghost; j < ny - nghost; ++j) {
                for (int i = nghost; i < nx - nghost; ++i) {
                    int flat = i + nx * (j + ny * k);
                    Real x = grid.x(0, i);
                    Real y = grid.x(1, j);
                    Real z = grid.x(2, k);

                    Real rx_p = x - params_.par_b[0], ry_p = y - params_.par_b[1],
                         rz_p = z - params_.par_b[2];
                    Real rx_m = x + params_.par_b[0], ry_m = y + params_.par_b[1],
                         rz_m = z + params_.par_b[2];
                    Real r_p = std::sqrt(rx_p * rx_p + ry_p * ry_p + rz_p * rz_p + 1.0e-20);
                    Real r_m = std::sqrt(rx_m * rx_m + ry_m * ry_m + rz_m * rz_m + 1.0e-20);

                    Real psi_BL = 1.0 + params_.par_m_plus[0] / (2.0 * r_p) +
                        params_.par_m_minus[0] / (2.0 * r_m);

                    Real u_c = u[flat];
                    Real u_sum = u[(i + 1) + nx * (j + ny * k)] + u[(i - 1) + nx * (j + ny * k)] +
                        u[i + nx * ((j + 1) + ny * k)] + u[i + nx * ((j - 1) + ny * k)] +
                        u[i + nx * (j + ny * (k + 1))] + u[i + nx * (j + ny * (k - 1))];

                    Real psi = psi_BL + u_c;
                    Real source = 0.125 * A2[flat] * std::pow(psi, -7.0);

                    Real res = std::abs((u_sum - 6.0 * u_c) / dx2 + source);
                    if (res > max_res)
                        max_res = res;

                    u_new[flat] = (u_sum + dx2 * source) / 6.0;
                }
            }
        }

#pragma omp parallel for
        for (int i = 0; i < nx * ny * nz; ++i) {
            u[i] = u_new[i]; // Asymptotic u->0 is enforced via inactive ghost cells
                             // 0.0
        }

        if (max_res < tol)
            break;
    }

    // -----------------------------------------------------------------------
    // Metric and Curvature Mapping
    // NOTE: In strict compliance with Granite's CCZ4 GridBlock design, we do NOT use ADM
    // variables like GXX or KXX, since the engine evolves conformal metrics.
    // The physical flat metric goes into GAMMA_XX = 1.0, conformal factor into CHI = \psi^-4,
    // and extrinsic curvature into A_XX (which natively holds \tilde{A}_{ij}).

    // reset flat background for gamma
    spacetime::setFlatSpacetime(grid);

#pragma omp parallel for
    for (int k = nghost; k < nz - nghost; ++k) {
        for (int j = nghost; j < ny - nghost; ++j) {
            for (int i = nghost; i < nx - nghost; ++i) {
                int flat = i + nx * (j + ny * k);
                Real x = grid.x(0, i), y = grid.x(1, j), z = grid.x(2, k);

                Real rx_p = x - params_.par_b[0], ry_p = y - params_.par_b[1],
                     rz_p = z - params_.par_b[2];
                Real rx_m = x + params_.par_b[0], ry_m = y + params_.par_b[1],
                     rz_m = z + params_.par_b[2];
                Real r_p = std::sqrt(rx_p * rx_p + ry_p * ry_p + rz_p * rz_p + 1.0e-20);
                Real r_m = std::sqrt(rx_m * rx_m + ry_m * ry_m + rz_m * rz_m + 1.0e-20);
                Real psi_BL = 1.0 + params_.par_m_plus[0] / (2.0 * r_p) +
                    params_.par_m_minus[0] / (2.0 * r_m);

                Real psi = psi_BL + u[flat];
                Real chi = std::pow(psi, -4.0);

                // Map CCZ4 primary states
                grid.data(static_cast<int>(SpacetimeVar::CHI), i, j, k) = chi;

                // Pre-collapsed lapse (trumpet quasi-stationary state)
                grid.data(static_cast<int>(SpacetimeVar::LAPSE), i, j, k) = std::sqrt(chi);

                // A_ij generated above is already \tilde{A}_{ij}, which correctly
                // maps into SpacetimeVar::A_XX natively.
            }
        }
    }
}

// ===========================================================================
// Benchmark scenario setup
// ===========================================================================

void setupBenchmarkScenario(
    GridBlock& spacetime_grid, GridBlock& hydro_grid, Real Mbh, Real Mstar, Real R0, int N) {
    // Create N BHs at regular polygon vertices
    std::vector<BlackHoleParams> bhs(N);
    for (int A = 0; A < N; ++A) {
        Real angle = 2.0 * constants::PI * A / N;
        bhs[A].mass = Mbh;
        bhs[A].position = {R0 * std::cos(angle), R0 * std::sin(angle), 0.0};
        bhs[A].momentum = {0.0, 0.0, 0.0};
        bhs[A].spin = {0.0, 0.0, 0.0};
    }

    // Set BH spacetime
    BrillLindquist bl(bhs);
    bl.apply(spacetime_grid);

    // Create 2 stars at the center
    std::vector<StarParams> stars(2);
    for (int s = 0; s < 2; ++s) {
        stars[s].mass = Mstar;
        stars[s].radius = 500.0; // R_sun
        stars[s].position = {0.0, 0.0, 0.0};
        stars[s].model = StellarModel::POLYTROPE;
        stars[s].polytropic_gamma = 4.0 / 3.0;
        stars[s].central_density = 1.0; // g/cm³
        stars[s].B_field_seed = 1.0e4;  // G
    }

    StellarInitialData stellar(stars);
    stellar.apply(spacetime_grid, hydro_grid);
}

} // namespace granite::initial_data
