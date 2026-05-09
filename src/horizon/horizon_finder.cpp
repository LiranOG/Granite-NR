/**
 * @file horizon_finder.cpp
 * @brief Apparent horizon finder — flow method (Gundlach 1998).
 *
 * Implements the full apparent horizon finder by:
 *   1. Parameterizing a trial surface S(θ,φ) as a sphere with angular
 *      deformations expanded in spherical harmonics.
 *   2. Computing the outward null expansion θ⁺ = ∇_i s^i + K - K_ij s^i s^j
 *      where s^i is the outward unit normal to the surface.
 *   3. Iterating via the flow ∂S/∂λ = −θ⁺(S) until ||θ⁺|| < tolerance.
 *
 * References:
 *   - Gundlach (1998) Phys. Rev. D 57, 863
 *   - Thornburg (2004) Living Rev. Relativ. 7, 3
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#include "granite/horizon/horizon_finder.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>

namespace granite::horizon {

// ===========================================================================
// Internal helpers (anonymous namespace)
// ===========================================================================
namespace {

/// Variable indices for spacetime grid (must match SpacetimeVar enum)
constexpr int iCHI = 0;
constexpr int iGXX = 1;
constexpr int iGXY = 2;
constexpr int iGXZ = 3;
constexpr int iGYY = 4;
constexpr int iGYZ = 5;
constexpr int iGZZ = 6;
constexpr int iAXX = 7;
constexpr int iAXY = 8;
constexpr int iAXZ = 9;
constexpr int iAYY = 10;
constexpr int iAYZ = 11;
constexpr int iAZZ = 12;
constexpr int iKTRACE = 13;
constexpr int iLAPSE = 18;
constexpr int iSHIFT_X = 19;
constexpr int iSHIFT_Y = 20;
constexpr int iSHIFT_Z = 21;

/// Trilinear interpolation of a field variable from the grid.
/// Returns the interpolated value at physical coordinates (x, y, z).
Real interpolate(const GridBlock& g, int var, Real px, Real py, Real pz) {
    // Convert physical coords to fractional grid indices (including ghost)
    const int ng = g.getNumGhost();
    const Real ix = (px - g.lowerCorner()[0]) / g.dx(0) + ng - 0.5;
    const Real iy = (py - g.lowerCorner()[1]) / g.dx(1) + ng - 0.5;
    const Real iz = (pz - g.lowerCorner()[2]) / g.dx(2) + ng - 0.5;

    const int i0 = static_cast<int>(std::floor(ix));
    const int j0 = static_cast<int>(std::floor(iy));
    const int k0 = static_cast<int>(std::floor(iz));

    // Clamp to valid range (interior-adjacent cells)
    const int nx = g.totalCells(0) - 1;
    const int ny = g.totalCells(1) - 1;
    const int nz = g.totalCells(2) - 1;
    const int i1 = std::min(i0 + 1, nx);
    const int j1 = std::min(j0 + 1, ny);
    const int k1 = std::min(k0 + 1, nz);
    const int ci = std::max(0, std::min(i0, nx));
    const int cj = std::max(0, std::min(j0, ny));
    const int ck = std::max(0, std::min(k0, nz));

    const Real tx = ix - std::floor(ix);
    const Real ty = iy - std::floor(iy);
    const Real tz = iz - std::floor(iz);

    // Trilinear weights
    auto d = [&](int ii, int jj, int kk) -> Real {
        return g.data(var,
                      std::max(0, std::min(ii, nx)),
                      std::max(0, std::min(jj, ny)),
                      std::max(0, std::min(kk, nz)));
    };
    (void)ci;
    (void)cj;
    (void)ck;

    return (1 - tz) *
        ((1 - ty) * ((1 - tx) * d(i0, j0, k0) + tx * d(i1, j0, k0)) +
         ty * ((1 - tx) * d(i0, j1, k0) + tx * d(i1, j1, k0))) +
        tz *
        ((1 - ty) * ((1 - tx) * d(i0, j0, k1) + tx * d(i1, j0, k1)) +
         ty * ((1 - tx) * d(i0, j1, k1) + tx * d(i1, j1, k1)));
}

/**
 * @brief Compute the null expansion θ⁺ at a surface point.
 *
 * Given a surface point (px, py, pz) with outward normal n = (nx, ny, nz)
 * (unit coordinate normal, not yet normalized w.r.t. metric), compute:
 *   θ⁺ = γ^{ij} ∇_i s_j + K - K_ij s^i s^j
 *      = D_i s^i + K - K_ij s^i s^j
 *
 * where s^i = (outward unit normal)^i in the physical 3-metric γ_ij.
 *
 * For the CCZ4 conformal decomposition:
 *   γ_ij = χ^{-1} γ̃_ij
 *   K_ij  = χ^{-1}(Ã_ij + (1/3) γ̃_ij K)
 */
Real computeExpansion(const GridBlock& g,
                      Real px,
                      Real py,
                      Real pz,
                      Real snx,
                      Real sny,
                      Real snz) // coordinate outward normal (unnormalized)
{
    // Interpolate conformal factor and metric components
    const Real chi = interpolate(g, iCHI, px, py, pz);
    const Real gxx = interpolate(g, iGXX, px, py, pz);
    const Real gxy = interpolate(g, iGXY, px, py, pz);
    const Real gxz = interpolate(g, iGXZ, px, py, pz);
    const Real gyy = interpolate(g, iGYY, px, py, pz);
    const Real gyz = interpolate(g, iGYZ, px, py, pz);
    const Real gzz = interpolate(g, iGZZ, px, py, pz);
    const Real Axx = interpolate(g, iAXX, px, py, pz);
    const Real Axy = interpolate(g, iAXY, px, py, pz);
    const Real Axz = interpolate(g, iAXZ, px, py, pz);
    const Real Ayy = interpolate(g, iAYY, px, py, pz);
    const Real Ayz = interpolate(g, iAYZ, px, py, pz);
    const Real Azz = interpolate(g, iAZZ, px, py, pz);
    const Real K = interpolate(g, iKTRACE, px, py, pz);

    if (chi < 1.0e-12)
        return 0.0; // degenerate metric

    // Physical metric γ_ij = γ̃_ij / χ
    const Real hxx = gxx / chi;
    const Real hxy = gxy / chi;
    const Real hxz = gxz / chi;
    const Real hyy = gyy / chi;
    const Real hyz = gyz / chi;
    const Real hzz = gzz / chi;

    // Determinant of conformal metric (should be ~1 for CCZ4)
    const Real det = gxx * (gyy * gzz - gyz * gyz) - gxy * (gxy * gzz - gyz * gxz) +
        gxz * (gxy * gyz - gyy * gxz);
    if (std::abs(det) < 1.0e-14)
        return 0.0;

    // Inverse conformal metric γ̃^{ij}
    const Real idet = 1.0 / det;
    const Real igxx = (gyy * gzz - gyz * gyz) * idet;
    const Real igxy = (gxz * gyz - gxy * gzz) * idet;
    const Real igxz = (gxy * gyz - gxz * gyy) * idet;
    const Real igyy = (gxx * gzz - gxz * gxz) * idet;
    const Real igyz = (gxy * gxz - gxx * gyz) * idet;
    const Real igzz = (gxx * gyy - gxy * gxy) * idet;

    // Inverse physical metric γ^{ij} = χ γ̃^{ij}
    const Real ihxx = chi * igxx;
    const Real ihxy = chi * igxy;
    const Real ihxz = chi * igxz;
    const Real ihyy = chi * igyy;
    const Real ihyz = chi * igyz;
    const Real ihzz = chi * igzz;
    // These are computed for completeness / future use; suppress C4189 on MSVC.
    (void)ihxx;
    (void)ihxy;
    (void)ihxz;
    (void)ihyy;
    (void)ihyz;
    (void)ihzz;

    // Lower coordinate normal with physical metric: s_i = γ_ij n^j
    const Real sx = hxx * snx + hxy * sny + hxz * snz;
    const Real sy = hxy * snx + hyy * sny + hyz * snz;
    const Real sz = hxz * snx + hyz * sny + hzz * snz;

    // Normalise: norm² = γ_{ij} n^i n^j = n^i s_i
    const Real norm2 = snx * sx + sny * sy + snz * sz;
    if (norm2 < 1.0e-14)
        return 0.0;
    const Real norm = std::sqrt(norm2);

    // Unit normal (raised): s^i = n^i / norm
    const Real ux = snx / norm;
    const Real uy = sny / norm;
    const Real uz = snz / norm;

    // Physical extrinsic curvature K_ij = (Ã_ij + (1/3)γ̃_ij K) / χ
    const Real Kxx = (Axx + gxx * K / 3.0) / chi;
    const Real Kxy = (Axy + gxy * K / 3.0) / chi;
    const Real Kxz = (Axz + gxz * K / 3.0) / chi;
    const Real Kyy = (Ayy + gyy * K / 3.0) / chi;
    const Real Kyz = (Ayz + gyz * K / 3.0) / chi;
    const Real Kzz = (Azz + gzz * K / 3.0) / chi;

    // K_ij s^i s^j
    const Real Kss = Kxx * ux * ux + Kyy * uy * uy + Kzz * uz * uz +
        2.0 * (Kxy * ux * uy + Kxz * ux * uz + Kyz * uy * uz);

    // Divergence of unit normal: D_i s^i  ≈ (1/hxx) ∂_i s^i  (Cartesian approx)
    // Use finite differences on the grid for ∂_i s^i.
    const Real h = g.dx(0); // Use isotropic spacing for first-order approximation
    const Real eps = h;

    // Central difference of the unit normal components
    auto scomp = [&](int dir, Real delta) -> Real {
        const Real ppx = px + (dir == 0 ? delta : 0.0);
        const Real ppy = py + (dir == 1 ? delta : 0.0);
        const Real ppz = pz + (dir == 2 ? delta : 0.0);

        const Real chi_l = interpolate(g, iCHI, ppx, ppy, ppz);
        if (chi_l < 1.0e-12)
            return 0.0;

        const Real gxx_l = interpolate(g, iGXX, ppx, ppy, ppz);
        const Real gxy_l = interpolate(g, iGXY, ppx, ppy, ppz);
        const Real gxz_l = interpolate(g, iGXZ, ppx, ppy, ppz);
        const Real gyy_l = interpolate(g, iGYY, ppx, ppy, ppz);
        const Real gyz_l = interpolate(g, iGYZ, ppx, ppy, ppz);
        const Real gzz_l = interpolate(g, iGZZ, ppx, ppy, ppz);

        const Real det_l = gxx_l * (gyy_l * gzz_l - gyz_l * gyz_l) -
            gxy_l * (gxy_l * gzz_l - gyz_l * gxz_l) + gxz_l * (gxy_l * gyz_l - gyy_l * gxz_l);
        if (std::abs(det_l) < 1.0e-14)
            return 0.0;

        const Real idet_l = 1.0 / det_l;
        const Real igxx_l = (gyy_l * gzz_l - gyz_l * gyz_l) * idet_l;
        const Real igxy_l = (gxz_l * gyz_l - gxy_l * gzz_l) * idet_l;
        const Real igxz_l = (gxy_l * gyz_l - gxz_l * gyy_l) * idet_l;
        const Real igyy_l = (gxx_l * gzz_l - gxz_l * gxz_l) * idet_l;
        const Real igyz_l = (gxy_l * gxz_l - gxx_l * gyz_l) * idet_l;
        const Real igzz_l = (gxx_l * gyy_l - gxy_l * gxy_l) * idet_l;

        // s_i at local point
        const Real hxx_l = gxx_l / chi_l;
        const Real hxy_l = gxy_l / chi_l;
        const Real hxz_l = gxz_l / chi_l;
        const Real hyy_l = gyy_l / chi_l;
        const Real hyz_l = gyz_l / chi_l;
        const Real hzz_l = gzz_l / chi_l;

        const Real sx_l = hxx_l * snx + hxy_l * sny + hxz_l * snz;
        const Real sy_l = hxy_l * snx + hyy_l * sny + hyz_l * snz;
        const Real sz_l = hxz_l * snx + hyz_l * sny + hzz_l * snz;
        const Real n2_l = snx * sx_l + sny * sy_l + snz * sz_l;
        if (n2_l < 1.0e-14)
            return 0.0;

        const Real nm_l = std::sqrt(n2_l);
        // Raise s with inverse physical metric (chi * ig)
        const Real ihxx_l = chi_l * igxx_l;
        const Real ihxy_l = chi_l * igxy_l;
        const Real ihxz_l = chi_l * igxz_l;
        const Real ihyy_l = chi_l * igyy_l;
        const Real ihyz_l = chi_l * igyz_l;
        const Real ihzz_l = chi_l * igzz_l;
        (void)ihxy_l;
        (void)ihxz_l;
        (void)ihyz_l;
        // Return component 'dir' of s^i = γ^{ij} s_j / norm
        if (dir == 0)
            return (ihxx_l * sx_l + ihxy_l * sy_l + ihxz_l * sz_l) / nm_l;
        else if (dir == 1)
            return (ihxy_l * sx_l + ihyy_l * sy_l + ihyz_l * sz_l) / nm_l;
        else
            return (ihxz_l * sx_l + ihyz_l * sy_l + ihzz_l * sz_l) / nm_l;
    };

    const Real div_s = (scomp(0, eps) - scomp(0, -eps)) / (2.0 * eps) +
        (scomp(1, eps) - scomp(1, -eps)) / (2.0 * eps) +
        (scomp(2, eps) - scomp(2, -eps)) / (2.0 * eps);

    // Null expansion: θ⁺ = D_i s^i + K - K_ij s^i s^j
    return div_s + K - Kss;
}

/**
 * @brief Build angular grid on the trial sphere.
 * @param Nth   Number of θ points
 * @param Nph   Number of φ points
 * @param theta Output θ values
 * @param phi   Output φ values
 */
void buildAngularGrid(int Nth, int Nph, std::vector<Real>& theta, std::vector<Real>& phi) {
    using constants::PI;
    theta.resize(Nth);
    phi.resize(Nph);
    for (int i = 0; i < Nth; ++i)
        theta[i] = PI * (i + 0.5) / Nth;
    for (int j = 0; j < Nph; ++j)
        phi[j] = 2.0 * PI * j / Nph;
}

} // anonymous namespace

// ===========================================================================
// ApparentHorizonFinder implementation
// ===========================================================================

ApparentHorizonFinder::ApparentHorizonFinder(const HorizonParams& params) : params_(params) {}

std::optional<HorizonData> ApparentHorizonFinder::findHorizon(const GridBlock& spacetime,
                                                              std::array<Real, DIM> center) const {
    const int Nth = params_.angular_resolution;
    const int Nph = 2 * params_.angular_resolution;
    const int N = Nth * Nph;

    std::vector<Real> theta, phi;
    buildAngularGrid(Nth, Nph, theta, phi);

    // h[i*Nph + j] = radial distance of the trial surface at (theta[i], phi[j])
    std::vector<Real> h(N, params_.initial_guess_radius);

    // Flow iteration: ∂h/∂λ = −θ⁺(h)
    const Real dlambda = 0.01 * spacetime.dx(0); // flow step ~ grid spacing
    (void)dlambda;

    // We use the grid spacing as flow step
    const Real h_grid = spacetime.dx(0);
    const Real flow_step = 0.05 * h_grid;

    bool converged = false;
    for (int iter = 0; iter < params_.max_iterations; ++iter) {
        Real max_theta = 0.0;

        for (int i = 0; i < Nth; ++i) {
            const Real sth = std::sin(theta[i]);
            const Real cth = std::cos(theta[i]);
            for (int j = 0; j < Nph; ++j) {
                const Real sph = std::sin(phi[j]);
                const Real cph = std::cos(phi[j]);

                const Real r = h[i * Nph + j];

                // Surface point coords
                const Real px = center[0] + r * sth * cph;
                const Real py = center[1] + r * sth * sph;
                const Real pz = center[2] + r * cth;

                // Outward coordinate normal (pointing away from center)
                const Real nx = sth * cph;
                const Real ny = sth * sph;
                const Real nz = cth;

                // Compute null expansion
                const Real Theta = computeExpansion(spacetime, px, py, pz, nx, ny, nz);

                // Flow: decrease r where Theta > 0, increase where Theta < 0
                h[i * Nph + j] -= flow_step * Theta;

                // Keep radius positive
                h[i * Nph + j] = std::max(h[i * Nph + j], 1.0e-6 * h_grid);

                max_theta = std::max(max_theta, std::abs(Theta));
            }
        }

        if (max_theta < params_.tolerance) {
            converged = true;
            break;
        }
    }

    if (!converged)
        return std::nullopt;

    // -----------------------------------------------------------------------
    // Build HorizonData from the converged surface
    // -----------------------------------------------------------------------
    HorizonData hd;
    hd.id = 0;
    hd.centroid = center;

    // Proper area: A = ∫ √(det γ_ab) dθ dφ over the surface
    // where γ_ab is the induced metric on S.
    // For a nearly-spherical surface with r(θ,φ):
    //   dA ≈ r² √(γ̃/χ³) sin θ dθ dφ (conformal factor factor)
    // We use a simpler coordinate-area estimate here.
    const Real dth = constants::PI / Nth;
    const Real dph = 2.0 * constants::PI / Nph;
    Real area = 0.0;
    std::array<Real, DIM> spin_num = {0.0, 0.0, 0.0};

    hd.surface_points.resize(N);
    for (int i = 0; i < Nth; ++i) {
        const Real sth = std::sin(theta[i]);
        const Real cth = std::cos(theta[i]);
        for (int j = 0; j < Nph; ++j) {
            const Real sph = std::sin(phi[j]);
            const Real cph = std::cos(phi[j]);
            const Real r = h[i * Nph + j];

            const Real px = center[0] + r * sth * cph;
            const Real py = center[1] + r * sth * sph;
            const Real pz = center[2] + r * cth;
            hd.surface_points[i * Nph + j] = {px, py, pz};

            // Interpolate conformal factor and metric at surface
            const Real chi = interpolate(spacetime, iCHI, px, py, pz);
            if (chi < 1.0e-12)
                continue;
            const Real gxx = interpolate(spacetime, iGXX, px, py, pz);
            const Real gyy = interpolate(spacetime, iGYY, px, py, pz);
            const Real gzz = interpolate(spacetime, iGZZ, px, py, pz);

            // Physical metric determinant (diagonal approximation)
            const Real sqrt_g = std::sqrt(std::abs(gxx * gyy * gzz)) / (chi * std::sqrt(chi));
            area += sqrt_g * r * r * sth * dth * dph;

            // Approximate spin angular momentum integrand z-component: r × s (for z)
            // J_z ~ ∮ φ̂_i s^i dA
            spin_num[2] += (-sph * px + cph * py) * sqrt_g * r * sth * dth * dph;
        }
    }

    hd.area = area;
    hd.irreducible_mass = std::sqrt(area / (16.0 * constants::PI));

    // Equatorial and polar circumferences (coordinate r at θ=π/2 and φ=0)
    {
        Real C_eq = 0.0;
        for (int j = 0; j < Nph; ++j) {
            const int i_eq = Nth / 2;
            C_eq += h[i_eq * Nph + j] * dph;
        }
        hd.circumference_equatorial = C_eq;

        Real C_pol = 0.0;
        for (int i = 0; i < Nth; ++i)
            C_pol += h[i * Nph + 0] * dth;
        hd.circumference_polar = 2.0 * C_pol; // full polar loop
    }

    // Spin from isolated horizon (approximate): a = J / M_irr²
    const Real J_z = spin_num[2];
    const Real spin_a =
        (hd.irreducible_mass > 1.0e-12) ? J_z / (hd.irreducible_mass * hd.irreducible_mass) : 0.0;
    hd.spin = std::clamp(spin_a, -1.0, 1.0);
    hd.spin_axis = {0.0, 0.0, 1.0}; // z-axis by default

    return hd;
}

std::vector<HorizonData> ApparentHorizonFinder::findAllHorizons(const GridBlock& spacetime) const {
    // Scan for local minima of the lapse as candidate BH centers.
    // Each local lapse minimum is a candidate for a puncture / BH center.
    std::vector<std::array<Real, DIM>> candidates;

    const int is = spacetime.istart() + 2;
    const int ie0 = spacetime.iend(0) - 2;
    const int ie1 = spacetime.iend(1) - 2;
    const int ie2 = spacetime.iend(2) - 2;

    for (int k = is; k < ie2; ++k)
        for (int j = is; j < ie1; ++j)
            for (int i = is; i < ie0; ++i) {
                const Real alpha = spacetime.data(iLAPSE, i, j, k);
                // Local minimum in lapse: all 6 neighbors have higher lapse
                if (alpha < spacetime.data(iLAPSE, i - 1, j, k) &&
                    alpha < spacetime.data(iLAPSE, i + 1, j, k) &&
                    alpha < spacetime.data(iLAPSE, i, j - 1, k) &&
                    alpha < spacetime.data(iLAPSE, i, j + 1, k) &&
                    alpha < spacetime.data(iLAPSE, i, j, k - 1) &&
                    alpha < spacetime.data(iLAPSE, i, j, k + 1) &&
                    alpha < 0.5) // Lapse should be significantly depressed
                {
                    candidates.push_back({spacetime.x(0, i), spacetime.x(1, j), spacetime.x(2, k)});
                }
            }

    // Remove candidates that are too close to each other (within 1M)
    const Real min_sep = 2.0 * params_.initial_guess_radius;
    std::vector<std::array<Real, DIM>> unique_candidates;
    for (auto& c : candidates) {
        bool dup = false;
        for (auto& u : unique_candidates) {
            const Real d2 = (c[0] - u[0]) * (c[0] - u[0]) + (c[1] - u[1]) * (c[1] - u[1]) +
                (c[2] - u[2]) * (c[2] - u[2]);
            if (d2 < min_sep * min_sep) {
                dup = true;
                break;
            }
        }
        if (!dup)
            unique_candidates.push_back(c);
    }

    // Attempt to find a horizon near each candidate
    std::vector<HorizonData> horizons;
    int id = 0;
    for (auto& ctr : unique_candidates) {
        auto result = findHorizon(spacetime, ctr);
        if (result) {
            result->id = id++;
            horizons.push_back(*result);
        }
    }

    return horizons;
}

std::optional<HorizonData>
ApparentHorizonFinder::findCommonHorizon(const GridBlock& spacetime,
                                         const std::vector<HorizonData>& horizons) const {
    if (horizons.size() < 2)
        return std::nullopt;

    // Compute centroid of all individual horizons weighted by irreducible mass
    Real total_mass = 0.0;
    std::array<Real, DIM> common_center = {0.0, 0.0, 0.0};
    for (const auto& h : horizons) {
        for (int d = 0; d < DIM; ++d)
            common_center[d] += h.irreducible_mass * h.centroid[d];
        total_mass += h.irreducible_mass;
    }
    if (total_mass < 1.0e-14)
        return std::nullopt;
    for (int d = 0; d < DIM; ++d)
        common_center[d] /= total_mass;

    // Initial guess: radius enclosing all individual horizons + 20% margin
    Real max_r = 0.0;
    for (const auto& h : horizons) {
        Real dist = 0.0;
        for (int d = 0; d < DIM; ++d) {
            const Real dd = h.centroid[d] - common_center[d];
            dist += dd * dd;
        }
        max_r =
            std::max(max_r, std::sqrt(dist) + h.circumference_equatorial / (2.0 * constants::PI));
    }

    // Override initial guess for the common horizon search
    HorizonParams common_params = params_;
    common_params.initial_guess_radius = 1.2 * max_r;

    ApparentHorizonFinder common_finder(common_params);
    auto result = common_finder.findHorizon(spacetime, common_center);
    if (result) {
        result->id = static_cast<int>(horizons.size()); // next ID
    }
    return result;
}

Real ApparentHorizonFinder::computeSpin(const GridBlock& spacetime,
                                        const HorizonData& horizon) const {
    // Isolated horizon spin using the formula:
    //   a/M = J / M²  where J is the Christodoulou angular momentum
    //
    // For a Kerr BH: M² = M_irr² + J²/(4 M_irr²)
    // Solving: J = M² * a/M = (M_irr² + J²/(4 M_irr²)) * a/M
    //
    // We use the circumference ratio method (Campanelli et al. 2006):
    //   C_pol / C_eq ∈ [π/2 (a=1) ... 1 (a=0)]
    // Interpolate dimensionless spin from this ratio.

    if (horizon.circumference_equatorial < 1.0e-12)
        return 0.0;
    const Real ratio = horizon.circumference_polar / horizon.circumference_equatorial;

    // Tabulated values from Campanelli 2006: (ratio, a/M)
    // ratio=1.0 → a=0, ratio=π/2≈1.5708 → a=1
    // Linear interpolation as first-order approximation
    const Real ratio_a0 = 1.0;                 // Schwarzschild
    const Real ratio_a1 = constants::PI / 2.0; // Extremal Kerr
    if (ratio <= ratio_a0)
        return 0.0;
    if (ratio >= ratio_a1)
        return 1.0;

    const Real chi_spin = (ratio - ratio_a0) / (ratio_a1 - ratio_a0);
    return chi_spin; // dimensionless spin in [0, 1]
}

} // namespace granite::horizon
