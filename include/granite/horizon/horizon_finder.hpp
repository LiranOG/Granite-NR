/**
 * @file granite/horizon/horizon_finder.hpp
 * @brief Apparent horizon finder — flow method.
 *
 * @copyright 2026 Liran M. Schwartz
 * @license GPL-3.0-or-later
 */
#pragma once

#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"

#include <optional>
#include <vector>

namespace granite::horizon {

struct HorizonParams {
    int max_iterations = 200;        ///< Max flow iterations
    Real tolerance = 1.0e-10;        ///< Convergence tolerance
    int angular_resolution = 50;     ///< Points in θ, φ on the trial surface
    Real initial_guess_radius = 1.0; ///< Initial trial sphere radius (in M)
    int find_every = 10;             ///< Attempt finding every N time steps
};

/**
 * @struct HorizonData
 * @brief Result of a successful horizon find.
 */
struct HorizonData {
    int id;                                            ///< Horizon identifier
    std::array<Real, DIM> centroid;                    ///< Coordinate centroid
    Real area;                                         ///< Proper area A
    Real irreducible_mass;                             ///< M_irr = √(A/16π)
    Real spin;                                         ///< Dimensionless spin a/M (from IH)
    std::array<Real, DIM> spin_axis;                   ///< Spin direction
    Real circumference_equatorial;                     ///< Equatorial circumference
    Real circumference_polar;                          ///< Polar circumference
    std::vector<std::array<Real, DIM>> surface_points; ///< Surface embedding
};

/**
 * @class ApparentHorizonFinder
 * @brief Locates apparent horizons using the flow method (Gundlach 1998).
 */
class ApparentHorizonFinder {
public:
    explicit ApparentHorizonFinder(const HorizonParams& params);
    ~ApparentHorizonFinder() = default;

    /**
     * @brief Attempt to find an apparent horizon near a given point.
     *
     * @param spacetime  Grid with metric data
     * @param center     Initial guess for horizon center
     * @return HorizonData if found, std::nullopt otherwise
     */
    std::optional<HorizonData> findHorizon(const GridBlock& spacetime,
                                           std::array<Real, DIM> center) const;

    /**
     * @brief Find all apparent horizons in the domain.
     *
     * Scans for local minima of the lapse as candidate centers,
     * then attempts to find horizons near each.
     */
    std::vector<HorizonData> findAllHorizons(const GridBlock& spacetime) const;

    /**
     * @brief Check if a common horizon encloses multiple individual horizons.
     *
     * @param spacetime Grid with metric data
     * @param horizons  Previously found individual horizons
     * @return Common HorizonData if found
     */
    std::optional<HorizonData> findCommonHorizon(const GridBlock& spacetime,
                                                 const std::vector<HorizonData>& horizons) const;

    /**
     * @brief Compute isolated-horizon spin diagnostics.
     *
     * Uses the Killing-vector method on the horizon surface.
     */
    Real computeSpin(const GridBlock& spacetime, const HorizonData& horizon) const;

    const HorizonParams& params() const { return params_; }

private:
    HorizonParams params_;
};

} // namespace granite::horizon
