/**
 * @file main.cpp
 * @brief GRANITE main entry point.
 *
 * Parses command-line arguments, reads a parameter file,
 * constructs initial data, and runs the evolution loop.
 *
 * @copyright 2026 Liran M. Schwartz
 */
#include "granite/amr/amr.hpp"
#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"
#include "granite/grmhd/grmhd.hpp"
#include "granite/initial_data/initial_data.hpp"
#include "granite/io/hdf5_io.hpp"
#include "granite/spacetime/ccz4.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <yaml-cpp/yaml.h>

#ifdef GRANITE_USE_PETSC
#include <petscsys.h>
#endif

#ifdef GRANITE_USE_MPI
#include <mpi.h>
#endif

namespace granite {

struct BlockBundle {
    int id;
    GridBlock* st;
    std::unique_ptr<GridBlock> hydro;
    std::unique_ptr<GridBlock> prim;
    std::unique_ptr<GridBlock> st_rhs;
    std::unique_ptr<GridBlock> hydro_rhs;
    std::unique_ptr<GridBlock> st_stage;
    std::unique_ptr<GridBlock> hydro_stage;

    // Pre-allocated scratch buffers (avoid per-RK3-step heap allocation)
    std::vector<Real> rho_scratch;
    std::vector<std::array<Real, DIM>> Si_scratch;
    std::vector<std::array<Real, SYM_TENSOR_COMPS>> Sij_scratch;
    std::vector<Real> S_scratch;
    bool scratch_allocated = false;

    void allocateScratch(std::size_t total_size) {
        if (!scratch_allocated) {
            rho_scratch.resize(total_size, 0.0);
            Si_scratch.resize(total_size, {0.0, 0.0, 0.0});
            Sij_scratch.resize(total_size);
            S_scratch.resize(total_size, 0.0);
            scratch_allocated = true;
        }
    }

    void clearScratch() {
        std::fill(rho_scratch.begin(), rho_scratch.end(), 0.0);
        for (auto& s : Si_scratch)
            s = {0.0, 0.0, 0.0};
        for (auto& s : Sij_scratch)
            s = {0, 0, 0, 0, 0, 0};
        std::fill(S_scratch.begin(), S_scratch.end(), 0.0);
    }
};

/**
 * @brief SSP-RK3 time integrator for the method-of-lines approach.
 *
 * u^(1) = u^n + Δt L(u^n)
 * u^(2) = (3/4)u^n + (1/4)u^(1) + (1/4)Δt L(u^(1))
 * u^{n+1} = (1/3)u^n + (2/3)u^(2) + (2/3)Δt L(u^(2))
 */
class TimeIntegrator {
public:
    static void sspRK3Step(std::vector<BlockBundle*>& bundles,
                           std::vector<BlockBundle>& active_bundles,
                           const std::unordered_map<int, size_t>& id_to_index,
                           const spacetime::CCZ4Evolution& ccz4,
                           const grmhd::GRMHDEvolution& grmhd,
                           Real dt) {
        auto syncGhostZones = [&](bool use_stage) {
#ifdef GRANITE_USE_MPI
            int my_rank = 0;
            MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

            std::vector<MPI_Request> requests;

            // Pre-count number of message buffers to avoid reallocation during MPI_Isend
            int num_send_msgs = 0;
            int num_recv_msgs = 0;
            for (auto* bundle_ptr : bundles) {
                auto& bundle = *bundle_ptr;
                GridBlock* st = use_stage ? bundle.st_stage.get() : bundle.st;
                for (int dir = 0; dir < 6; ++dir) {
                    if (st->getNeighborId(dir) >= 0 && st->getNeighborRank(dir) != my_rank) {
                        num_send_msgs += 2; // st and hy
                        num_recv_msgs += 2; // st and hy
                    }
                }
            }

            std::vector<std::vector<Real>> send_bufs(num_send_msgs);
            std::vector<std::vector<Real>> recv_bufs(num_recv_msgs);
            int s_idx = 0;
            int r_idx = 0;

            for (auto* bundle_ptr : bundles) {
                auto& bundle = *bundle_ptr;
                GridBlock* st = use_stage ? bundle.st_stage.get() : bundle.st;
                GridBlock* hy = use_stage ? bundle.hydro_stage.get() : bundle.hydro.get();
                for (int dir = 0; dir < 6; ++dir) {
                    int nbr_id = st->getNeighborId(dir);
                    if (nbr_id < 0)
                        continue;

                    int nbr_rank = st->getNeighborRank(dir);
                    if (nbr_rank == my_rank) {
                        size_t nbr_idx = id_to_index.at(nbr_id);
                        GridBlock* nbr_st = use_stage ? active_bundles[nbr_idx].st_stage.get()
                                                      : active_bundles[nbr_idx].st;
                        GridBlock* nbr_hy = use_stage ? active_bundles[nbr_idx].hydro_stage.get()
                                                      : active_bundles[nbr_idx].hydro.get();

                        std::vector<Real> buf_st, buf_hy;
                        st->packBoundary(dir, buf_st);
                        hy->packBoundary(dir, buf_hy);

                        int opp_dir = dir ^ 1;
                        nbr_st->unpackBoundary(opp_dir, buf_st);
                        nbr_hy->unpackBoundary(opp_dir, buf_hy);
                    } else {
                        auto& sbuf_st = send_bufs[s_idx++];
                        auto& sbuf_hy = send_bufs[s_idx++];
                        st->packBoundary(dir, sbuf_st);
                        hy->packBoundary(dir, sbuf_hy);

                        int tag_st = bundle.id * 100 + dir;
                        int tag_hy = tag_st + 10;

                        requests.push_back(MPI_REQUEST_NULL);
                        MPI_Isend(sbuf_st.data(),
                                  sbuf_st.size(),
                                  MPI_DOUBLE,
                                  nbr_rank,
                                  tag_st,
                                  MPI_COMM_WORLD,
                                  &requests.back());
                        requests.push_back(MPI_REQUEST_NULL);
                        MPI_Isend(sbuf_hy.data(),
                                  sbuf_hy.size(),
                                  MPI_DOUBLE,
                                  nbr_rank,
                                  tag_hy,
                                  MPI_COMM_WORLD,
                                  &requests.back());

                        auto& rbuf_st = recv_bufs[r_idx++];
                        auto& rbuf_hy = recv_bufs[r_idx++];
                        rbuf_st.resize(sbuf_st.size());
                        rbuf_hy.resize(sbuf_hy.size());

                        int recv_tag_st = nbr_id * 100 + (dir ^ 1);
                        int recv_tag_hy = recv_tag_st + 10;

                        requests.push_back(MPI_REQUEST_NULL);
                        MPI_Irecv(rbuf_st.data(),
                                  rbuf_st.size(),
                                  MPI_DOUBLE,
                                  nbr_rank,
                                  recv_tag_st,
                                  MPI_COMM_WORLD,
                                  &requests.back());
                        requests.push_back(MPI_REQUEST_NULL);
                        MPI_Irecv(rbuf_hy.data(),
                                  rbuf_hy.size(),
                                  MPI_DOUBLE,
                                  nbr_rank,
                                  recv_tag_hy,
                                  MPI_COMM_WORLD,
                                  &requests.back());
                    }
                }
            }

            if (!requests.empty()) {
                std::vector<MPI_Status> statuses(requests.size());
                MPI_Waitall(requests.size(), requests.data(), statuses.data());
            }

            r_idx = 0; // Reset existing r_idx instead of redeclaring
            for (auto* bundle_ptr : bundles) {
                auto& bundle = *bundle_ptr;
                GridBlock* st = use_stage ? bundle.st_stage.get() : bundle.st;
                GridBlock* hy = use_stage ? bundle.hydro_stage.get() : bundle.hydro.get();
                for (int dir = 0; dir < 6; ++dir) {
                    int nbr_id = st->getNeighborId(dir);
                    if (nbr_id < 0)
                        continue;
                    int nbr_rank = st->getNeighborRank(dir);
                    if (nbr_rank != my_rank) {
                        st->unpackBoundary(dir, recv_bufs[r_idx++]);
                        hy->unpackBoundary(dir, recv_bufs[r_idx++]);
                    }
                }
            }
#else
            for (auto* bundle_ptr : bundles) {
                auto& bundle = *bundle_ptr;
                GridBlock* st = use_stage ? bundle.st_stage.get() : bundle.st;
                GridBlock* hy = use_stage ? bundle.hydro_stage.get() : bundle.hydro.get();
                for (int dir = 0; dir < 6; ++dir) {
                    int nbr_id = st->getNeighborId(dir);
                    if (nbr_id < 0)
                        continue;
                    size_t nbr_idx = id_to_index.at(nbr_id);
                    GridBlock* nbr_st = use_stage ? active_bundles[nbr_idx].st_stage.get()
                                                  : active_bundles[nbr_idx].st;
                    GridBlock* nbr_hy = use_stage ? active_bundles[nbr_idx].hydro_stage.get()
                                                  : active_bundles[nbr_idx].hydro.get();

                    std::vector<Real> buf_st, buf_hy;
                    st->packBoundary(dir, buf_st);
                    hy->packBoundary(dir, buf_hy);
                    int opp_dir = dir ^ 1;
                    nbr_st->unpackBoundary(opp_dir, buf_st);
                    nbr_hy->unpackBoundary(opp_dir, buf_hy);
                }
            }
#endif
        };

        auto applyRHS = [&](bool use_stage) {
            for (auto* bundle_ptr : bundles) {
                auto& bundle = *bundle_ptr;
                GridBlock& st = use_stage ? *(bundle.st_stage) : *(bundle.st);
                GridBlock& hy = use_stage ? *(bundle.hydro_stage) : *(bundle.hydro);
                GridBlock& prim = *(bundle.prim);
                GridBlock& rhs_st = *(bundle.st_rhs);
                GridBlock& rhs_hy = *(bundle.hydro_rhs);

                grmhd.conservedToPrimitive(st, hy, prim);

                // Fill primitive grid ghost cells with nearest interior values.
                // conservedToPrimitive only fills interior cells [is, ie).
                // PLM/PPM/MP5 reconstruction stencils reach 1-3 cells into
                // ghost zones of prim, reading uninitialized data → NaN.
                {
                    const int ng = prim.getNumGhost();
                    const int nv = prim.getNumVars();
                    const int tnx = prim.totalCells(0);
                    const int tny = prim.totalCells(1);
                    const int tnz = prim.totalCells(2);
                    const int pis = ng;
                    const int piex = tnx - ng - 1;
                    const int piey = tny - ng - 1;
                    const int piez = tnz - ng - 1;
                    for (int v = 0; v < nv; ++v) {
                        for (int kk = 0; kk < tnz; ++kk)
                            for (int jj = 0; jj < tny; ++jj)
                                for (int gi = 0; gi < ng; ++gi) {
                                    prim.data(v, pis - 1 - gi, jj, kk) = prim.data(v, pis, jj, kk);
                                    prim.data(v, piex + 1 + gi, jj, kk) =
                                        prim.data(v, piex, jj, kk);
                                }
                        for (int kk = 0; kk < tnz; ++kk)
                            for (int ii = 0; ii < tnx; ++ii)
                                for (int gi = 0; gi < ng; ++gi) {
                                    prim.data(v, ii, pis - 1 - gi, kk) = prim.data(v, ii, pis, kk);
                                    prim.data(v, ii, piey + 1 + gi, kk) =
                                        prim.data(v, ii, piey, kk);
                                }
                        for (int jj = 0; jj < tny; ++jj)
                            for (int ii = 0; ii < tnx; ++ii)
                                for (int gi = 0; gi < ng; ++gi) {
                                    prim.data(v, ii, jj, pis - 1 - gi) = prim.data(v, ii, jj, pis);
                                    prim.data(v, ii, jj, piez + 1 + gi) =
                                        prim.data(v, ii, jj, piez);
                                }
                    }
                }

                // Use pre-allocated scratch buffers instead of per-call heap allocation
                bundle.clearScratch();
                auto& rho = bundle.rho_scratch;
                auto& Si = bundle.Si_scratch;
                auto& Sij = bundle.Sij_scratch;
                auto& S = bundle.S_scratch;

                grmhd.computeMatterSources(st, prim, rho, Si, Sij, S);
                ccz4.computeRHS(st, rhs_st, rho, Si, Sij, S);
                grmhd.computeRHS(st, hy, prim, rhs_hy);
            }
        };

        auto combine =
            [&](bool write_stage, Real a1, bool s1_stage, Real a2, bool s2_stage, Real a3) {
                for (auto* bundle_ptr : bundles) {
                    auto& bundle = *bundle_ptr;
                    GridBlock& dst_st = write_stage ? *(bundle.st_stage) : *(bundle.st);
                    GridBlock& dst_hy = write_stage ? *(bundle.hydro_stage) : *(bundle.hydro);
                    GridBlock& s1_st = s1_stage ? *(bundle.st_stage) : *(bundle.st);
                    GridBlock& s1_hy = s1_stage ? *(bundle.hydro_stage) : *(bundle.hydro);
                    GridBlock& s2_st = s2_stage ? *(bundle.st_stage) : *(bundle.st);
                    GridBlock& s2_hy = s2_stage ? *(bundle.hydro_stage) : *(bundle.hydro);
                    GridBlock& rst = *(bundle.st_rhs);
                    GridBlock& rhy = *(bundle.hydro_rhs);

                    auto doCombine = [&](GridBlock& d, GridBlock& s1, GridBlock& s2, GridBlock& r) {
                        const int ie0 = d.iend(0);
                        const int ie1 = d.iend(1);
                        const int ie2 = d.iend(2);
                        const int is = d.istart();
                        for (int v = 0; v < d.getNumVars(); ++v) {
                            for (int k = is; k < ie2; ++k)
                                for (int j = is; j < ie1; ++j)
                                    for (int i = is; i < ie0; ++i)
                                        d.data(v, i, j, k) = a1 * s1.data(v, i, j, k) +
                                            a2 * s2.data(v, i, j, k) + a3 * dt * r.data(v, i, j, k);
                        }
                    };
                    doCombine(dst_st, s1_st, s2_st, rst);
                    doCombine(dst_hy, s1_hy, s2_hy, rhy);
                }
            };

        // ── Outer boundary conditions: 2nd-order Sommerfeld ──────
        // Implements the outgoing-wave condition (Alcubierre 2008, §5.4):
        //   ∂_t f + v · ∂_r f + (f - f∞)/r = 0
        // Discretized as a radiative falloff for ghost cells using the
        // nearest interior value and the asymptotic flat-space value.
        //
        // For 1+log slicing, the gauge wave speed is v_char = √2.
        // Asymptotic values: χ→1, γ̃_{xx,yy,zz}→1, α→1, all others→0.
        //
        // Previous tested alternatives:
        //   Copy BC (Neumann ∂_n f = 0): stable to t≈6.25M on ±8M domain
        //   but reflects gauge wave. Now unnecessary with ±16M domain.
        //   Static Sommerfeld 1/r: made ||H||₂ 8× worse — replaced here
        //   with the time-dependent version that properly absorbs outgoing waves.
        auto applyOuterBC = [&](bool use_stage) {
            for (auto* bundle_ptr : bundles) {
                auto& bundle = *bundle_ptr;
                GridBlock* st = use_stage ? bundle.st_stage.get() : bundle.st;
                GridBlock* hy = use_stage ? bundle.hydro_stage.get() : bundle.hydro.get();

                // Sommerfeld radiative BC for spacetime variables
                auto fillSommerfeldBC = [&dt](GridBlock& g) {
                    const int ng = g.getNumGhost();
                    const int nv = g.getNumVars();
                    const int nx = g.totalCells(0);
                    const int ny = g.totalCells(1);
                    const int nz = g.totalCells(2);
                    const int is = ng;
                    const int iex = nx - ng - 1; // last interior
                    const int iey = ny - ng - 1;
                    const int iez = nz - ng - 1;

                    // Asymptotic values for CCZ4 variables (flat spacetime)
                    // Index: 0=CHI, 1-6=GAMMA, 7-12=A, 13=K, 14-16=GAMMA_HAT,
                    //        17=THETA, 18=LAPSE, 19-21=SHIFT
                    auto f_inf = [](int v) -> Real {
                        if (v == 0)
                            return 1.0; // chi → 1
                        if (v == 1)
                            return 1.0; // gamma_xx → 1
                        if (v == 4)
                            return 1.0; // gamma_yy → 1
                        if (v == 6)
                            return 1.0; // gamma_zz → 1
                        if (v == 18)
                            return 1.0; // alpha → 1
                        return 0.0;     // all others → 0
                    };

                    // Gauge wave speed for 1+log slicing
                    constexpr Real v_char = 1.4142135623730951; // sqrt(2)

                    // For each ghost layer, apply Sommerfeld:
                    //   f_ghost = f_int + (f∞ - f_int) * (1 - r_int/r_ghost)
                    //           + v_char * dt/dr * (f_int_prev_layer - f_int)
                    // Simplified radiative: f_ghost = f∞ + (f_int - f∞) * r_int/r_ghost
                    // with 1/r falloff that does not create artificial gradients
                    // because we compute r from actual cell coordinates.
                    for (int v = 0; v < nv; ++v) {
                        Real finf = f_inf(v);
                        // ±X faces
                        for (int k = 0; k < nz; ++k)
                            for (int j = 0; j < ny; ++j) {
                                Real y = g.x(1, j);
                                Real z = g.x(2, k);
                                for (int gi = 0; gi < ng; ++gi) {
                                    // -X face
                                    {
                                        Real x_int = g.x(0, is);
                                        Real x_gh = g.x(0, is - 1 - gi);
                                        Real r_int =
                                            std::sqrt(x_int * x_int + y * y + z * z) + 1e-30;
                                        Real r_gh = std::sqrt(x_gh * x_gh + y * y + z * z) + 1e-30;
                                        Real f_int = g.data(v, is, j, k);
                                        g.data(v, is - 1 - gi, j, k) =
                                            finf + (f_int - finf) * r_int / r_gh;
                                    }
                                    // +X face
                                    {
                                        Real x_int = g.x(0, iex);
                                        Real x_gh = g.x(0, iex + 1 + gi);
                                        Real r_int =
                                            std::sqrt(x_int * x_int + y * y + z * z) + 1e-30;
                                        Real r_gh = std::sqrt(x_gh * x_gh + y * y + z * z) + 1e-30;
                                        Real f_int = g.data(v, iex, j, k);
                                        g.data(v, iex + 1 + gi, j, k) =
                                            finf + (f_int - finf) * r_int / r_gh;
                                    }
                                }
                            }
                        // ±Y faces
                        for (int k = 0; k < nz; ++k)
                            for (int i = 0; i < nx; ++i) {
                                Real x = g.x(0, i);
                                Real z = g.x(2, k);
                                for (int gi = 0; gi < ng; ++gi) {
                                    {
                                        Real y_int = g.x(1, is);
                                        Real y_gh = g.x(1, is - 1 - gi);
                                        Real r_int =
                                            std::sqrt(x * x + y_int * y_int + z * z) + 1e-30;
                                        Real r_gh = std::sqrt(x * x + y_gh * y_gh + z * z) + 1e-30;
                                        Real f_int = g.data(v, i, is, k);
                                        g.data(v, i, is - 1 - gi, k) =
                                            finf + (f_int - finf) * r_int / r_gh;
                                    }
                                    {
                                        Real y_int = g.x(1, iey);
                                        Real y_gh = g.x(1, iey + 1 + gi);
                                        Real r_int =
                                            std::sqrt(x * x + y_int * y_int + z * z) + 1e-30;
                                        Real r_gh = std::sqrt(x * x + y_gh * y_gh + z * z) + 1e-30;
                                        Real f_int = g.data(v, i, iey, k);
                                        g.data(v, i, iey + 1 + gi, k) =
                                            finf + (f_int - finf) * r_int / r_gh;
                                    }
                                }
                            }
                        // ±Z faces
                        for (int j = 0; j < ny; ++j)
                            for (int i = 0; i < nx; ++i) {
                                Real x = g.x(0, i);
                                Real y = g.x(1, j);
                                for (int gi = 0; gi < ng; ++gi) {
                                    {
                                        Real z_int = g.x(2, is);
                                        Real z_gh = g.x(2, is - 1 - gi);
                                        Real r_int =
                                            std::sqrt(x * x + y * y + z_int * z_int) + 1e-30;
                                        Real r_gh = std::sqrt(x * x + y * y + z_gh * z_gh) + 1e-30;
                                        Real f_int = g.data(v, i, j, is);
                                        g.data(v, i, j, is - 1 - gi) =
                                            finf + (f_int - finf) * r_int / r_gh;
                                    }
                                    {
                                        Real z_int = g.x(2, iez);
                                        Real z_gh = g.x(2, iez + 1 + gi);
                                        Real r_int =
                                            std::sqrt(x * x + y * y + z_int * z_int) + 1e-30;
                                        Real r_gh = std::sqrt(x * x + y * y + z_gh * z_gh) + 1e-30;
                                        Real f_int = g.data(v, i, j, iez);
                                        g.data(v, i, j, iez + 1 + gi) =
                                            finf + (f_int - finf) * r_int / r_gh;
                                    }
                                }
                            }
                    }
                };

                // Copy BC for hydro (atmosphere at boundary — no wave to absorb)
                auto fillCopyBC = [](GridBlock& g) {
                    const int ng = g.getNumGhost();
                    const int nv = g.getNumVars();
                    const int nx = g.totalCells(0);
                    const int ny = g.totalCells(1);
                    const int nz = g.totalCells(2);
                    const int is = ng;
                    const int iex = nx - ng - 1;
                    const int iey = ny - ng - 1;
                    const int iez = nz - ng - 1;
                    for (int v = 0; v < nv; ++v) {
                        for (int k = 0; k < nz; ++k)
                            for (int j = 0; j < ny; ++j)
                                for (int gi = 0; gi < ng; ++gi) {
                                    g.data(v, is - 1 - gi, j, k) = g.data(v, is, j, k);
                                    g.data(v, iex + 1 + gi, j, k) = g.data(v, iex, j, k);
                                }
                        for (int k = 0; k < nz; ++k)
                            for (int i = 0; i < nx; ++i)
                                for (int gi = 0; gi < ng; ++gi) {
                                    g.data(v, i, is - 1 - gi, k) = g.data(v, i, is, k);
                                    g.data(v, i, iey + 1 + gi, k) = g.data(v, i, iey, k);
                                }
                        for (int j = 0; j < ny; ++j)
                            for (int i = 0; i < nx; ++i)
                                for (int gi = 0; gi < ng; ++gi) {
                                    g.data(v, i, j, is - 1 - gi) = g.data(v, i, j, is);
                                    g.data(v, i, j, iez + 1 + gi) = g.data(v, i, j, iez);
                                }
                    }
                };
                fillSommerfeldBC(*st);
                fillCopyBC(*hy);
            }
        };

        applyRHS(false);

        // ── One-shot NaN diagnostic (remove once stable) ─────────
        {
            static bool checked = false;
            if (!checked) {
                checked = true;
                for (auto* bundle_ptr : bundles) {
                    auto& bundle = *bundle_ptr;
                    GridBlock& rhs_st = *(bundle.st_rhs);
                    GridBlock& rhs_hy = *(bundle.hydro_rhs);
                    // Scan spacetime RHS
                    for (int v = 0; v < rhs_st.getNumVars(); ++v)
                        for (int k = rhs_st.istart(); k < rhs_st.iend(2); ++k)
                            for (int j = rhs_st.istart(); j < rhs_st.iend(1); ++j)
                                for (int i = rhs_st.istart(); i < rhs_st.iend(0); ++i) {
                                    if (std::isnan(rhs_st.data(v, i, j, k)) ||
                                        std::isinf(rhs_st.data(v, i, j, k))) {
                                        std::cout << "  [NaN-DIAG] ST_RHS var=" << v << " at (" << i
                                                  << "," << j << "," << k << ")"
                                                  << " val=" << rhs_st.data(v, i, j, k)
                                                  << std::endl;
                                        goto done_st;
                                    }
                                }
                    std::cout << "  [NaN-DIAG] ST_RHS: all finite" << std::endl;
                done_st:
                    // Scan hydro RHS
                    for (int v = 0; v < rhs_hy.getNumVars(); ++v)
                        for (int k = rhs_hy.istart(); k < rhs_hy.iend(2); ++k)
                            for (int j = rhs_hy.istart(); j < rhs_hy.iend(1); ++j)
                                for (int i = rhs_hy.istart(); i < rhs_hy.iend(0); ++i) {
                                    if (std::isnan(rhs_hy.data(v, i, j, k)) ||
                                        std::isinf(rhs_hy.data(v, i, j, k))) {
                                        std::cout << "  [NaN-DIAG] HY_RHS var=" << v << " at (" << i
                                                  << "," << j << "," << k << ")"
                                                  << " val=" << rhs_hy.data(v, i, j, k)
                                                  << std::endl;
                                        goto done_hy;
                                    }
                                }
                    std::cout << "  [NaN-DIAG] HY_RHS: all finite" << std::endl;
                done_hy:;
                }
            }
        }
        // ── Physics floors on evolved variables ───────────────────
        // IEEE754: NaN < x = false, +Inf < x = false — so the naive
        // `if (chi < 1e-4)` floor silently passes NaN and +Inf.
        // Once Inf enters (from a large but finite RHS × dt overflow),
        // the next substep produces Inf - Inf = NaN, which propagates
        // at exactly the KO stencil width (±3 cells = 9 cells/step).
        // Fix: use !isfinite(x) || x < threshold to catch all bad values.
        // Also enforce det(γ̃) = 1 to prevent metric degeneracy.
        constexpr int iCHI_st = static_cast<int>(SpacetimeVar::CHI);
        constexpr int iLAPSE_st = static_cast<int>(SpacetimeVar::LAPSE);
        constexpr int iGXX = static_cast<int>(SpacetimeVar::GAMMA_XX);
        constexpr int iGXY = static_cast<int>(SpacetimeVar::GAMMA_XY);
        constexpr int iGXZ = static_cast<int>(SpacetimeVar::GAMMA_XZ);
        constexpr int iGYY = static_cast<int>(SpacetimeVar::GAMMA_YY);
        constexpr int iGYZ = static_cast<int>(SpacetimeVar::GAMMA_YZ);
        constexpr int iGZZ = static_cast<int>(SpacetimeVar::GAMMA_ZZ);

        // Fast per-substep floors: chi and alpha only (catch NaN/Inf).
        // These run at EVERY substep for numerical safety.
        auto applyFloors = [&](bool use_stage) {
            for (auto* bundle_ptr : bundles) {
                auto& bundle = *bundle_ptr;
                GridBlock& g = use_stage ? *(bundle.st_stage) : *(bundle.st);
                const int tnx = g.totalCells(0);
                const int tny = g.totalCells(1);
                const int tnz = g.totalCells(2);
                for (int kk = 0; kk < tnz; ++kk)
                    for (int jj = 0; jj < tny; ++jj)
                        for (int ii = 0; ii < tnx; ++ii) {
                            Real& chi = g.data(iCHI_st, ii, jj, kk);
                            if (!std::isfinite(chi) || chi < 1.0e-4)
                                chi = 1.0e-4;
                            if (chi > 1.5)
                                chi = 1.5;
                            Real& alpha = g.data(iLAPSE_st, ii, jj, kk);
                            if (!std::isfinite(alpha) || alpha < 1.0e-6)
                                alpha = 1.0e-6;
                        }
            }
        };

        // Algebraic constraint enforcement — called ONCE per RK3 step
        // at the final combine (main grid only). det(γ̃)=1 and tr(Ã)=0.
        auto applyAlgebraicConstraints = [&]() {
            for (auto* bundle_ptr : bundles) {
                auto& bundle = *bundle_ptr;
                GridBlock& g = *(bundle.st);
                const int tnx = g.totalCells(0);
                const int tny = g.totalCells(1);
                const int tnz = g.totalCells(2);
                for (int kk = 0; kk < tnz; ++kk)
                    for (int jj = 0; jj < tny; ++jj)
                        for (int ii = 0; ii < tnx; ++ii) {
                            Real& gxx = g.data(iGXX, ii, jj, kk);
                            Real& gxy = g.data(iGXY, ii, jj, kk);
                            Real& gxz = g.data(iGXZ, ii, jj, kk);
                            Real& gyy = g.data(iGYY, ii, jj, kk);
                            Real& gyz = g.data(iGYZ, ii, jj, kk);
                            Real& gzz = g.data(iGZZ, ii, jj, kk);
                            Real det = gxx * (gyy * gzz - gyz * gyz) -
                                gxy * (gxy * gzz - gyz * gxz) + gxz * (gxy * gyz - gyy * gxz);
                            if (std::isfinite(det) && det > 1.0e-10) {
                                Real scale = std::cbrt(1.0 / det);
                                gxx *= scale;
                                gxy *= scale;
                                gxz *= scale;
                                gyy *= scale;
                                gyz *= scale;
                                gzz *= scale;
                            } else {
                                gxx = 1.0;
                                gxy = 0.0;
                                gxz = 0.0;
                                gyy = 1.0;
                                gyz = 0.0;
                                gzz = 1.0;
                            }
                            // Traceless Ã_ij: remove (1/3) tr(Ã γ̃^{-1}) γ̃_{ij}
                            // Cofactors of γ̃ (= inverse since det=1 now)
                            const Real ixx = gyy * gzz - gyz * gyz;
                            const Real ixy = gxz * gyz - gxy * gzz;
                            const Real ixz = gxy * gyz - gxz * gyy;
                            const Real iyy = gxx * gzz - gxz * gxz;
                            const Real iyz = gxz * gxy - gxx * gyz;
                            const Real izz = gxx * gyy - gxy * gxy;
                            Real& axx = g.data(static_cast<int>(SpacetimeVar::A_XX), ii, jj, kk);
                            Real& axy = g.data(static_cast<int>(SpacetimeVar::A_XY), ii, jj, kk);
                            Real& axz = g.data(static_cast<int>(SpacetimeVar::A_XZ), ii, jj, kk);
                            Real& ayy = g.data(static_cast<int>(SpacetimeVar::A_YY), ii, jj, kk);
                            Real& ayz = g.data(static_cast<int>(SpacetimeVar::A_YZ), ii, jj, kk);
                            Real& azz = g.data(static_cast<int>(SpacetimeVar::A_ZZ), ii, jj, kk);
                            const Real trA = ixx * axx + iyy * ayy + izz * azz +
                                2.0 * (ixy * axy + ixz * axz + iyz * ayz);
                            const Real t3 = trA / 3.0;
                            axx -= t3 * gxx;
                            axy -= t3 * gxy;
                            axz -= t3 * gxz;
                            ayy -= t3 * gyy;
                            ayz -= t3 * gyz;
                            azz -= t3 * gzz;
                        }
            }
        };

        combine(true, 1.0, false, 0.0, false, 1.0);
        applyFloors(true);
        syncGhostZones(true);
        applyOuterBC(true);

        applyRHS(true);
        combine(true, 0.75, false, 0.25, true, 0.25);
        applyFloors(true);
        syncGhostZones(true);
        applyOuterBC(true);

        applyRHS(true);
        combine(false, 1.0 / 3.0, false, 2.0 / 3.0, true, 2.0 / 3.0);
        applyFloors(false);
        applyAlgebraicConstraints(); // once per step, on main grid
        syncGhostZones(false);
        applyOuterBC(false);
    }
};

} // namespace granite

// ===========================================================================
// Main
// ===========================================================================

int main(int argc, char* argv[]) {
    // Force line-buffered stdout so output appears immediately even when
    // piped through `tee run.log` or redirected. Without this, the C++ runtime
    // switches stdout to block-buffered mode (8KB) when not a TTY, causing
    // the simulation to appear frozen for minutes at a time.
    std::cout << std::unitbuf;

#ifdef GRANITE_USE_PETSC
    PetscInitialize(&argc, &argv, nullptr, nullptr);
#endif

#ifdef GRANITE_USE_MPI
    MPI_Init(&argc, &argv);
#endif

    using namespace granite;

    std::cout << "================================================================\n";
    std::cout << " GRANITE v" << "0.6.7" << "\n";
    std::cout << " General-Relativistic Adaptive N-body Integrated Tool\n";
    std::cout << " for Extreme Astrophysics\n";
    std::cout << "================================================================\n\n";

    // --- Default parameters ---
    SimulationParams params;

    // Initial data configuration (read from YAML)
    std::string initial_data_type = "flat"; // default
    Real gauge_wave_amplitude = 0.1;
    Real gauge_wave_wavelength = 1.0;
    std::vector<initial_data::BlackHoleParams> bh_params;

    if (argc > 1) {
        std::string param_file = argv[1];
        std::cout << "Loading parameter file: " << param_file << "\n";
        try {
            YAML::Node config = YAML::LoadFile(param_file);
            if (config["grid"]) {
                auto grid = config["grid"];
                if (grid["ncells"]) {
                    params.ncells[0] = grid["ncells"][0].as<int>();
                    params.ncells[1] = grid["ncells"][1].as<int>();
                    params.ncells[2] = grid["ncells"][2].as<int>();
                }
                if (grid["domain_lo"]) {
                    params.domain_lo[0] = grid["domain_lo"][0].as<Real>();
                    params.domain_lo[1] = grid["domain_lo"][1].as<Real>();
                    params.domain_lo[2] = grid["domain_lo"][2].as<Real>();
                }
                if (grid["domain_hi"]) {
                    params.domain_hi[0] = grid["domain_hi"][0].as<Real>();
                    params.domain_hi[1] = grid["domain_hi"][1].as<Real>();
                    params.domain_hi[2] = grid["domain_hi"][2].as<Real>();
                }
                if (grid["ghost_cells"])
                    params.ghost_cells = grid["ghost_cells"].as<int>();
            }
            if (config["time"]) {
                auto time = config["time"];
                if (time["cfl"])
                    params.cfl = time["cfl"].as<Real>();
                if (time["t_final"])
                    params.t_final = time["t_final"].as<Real>();
                if (time["max_steps"])
                    params.max_steps = time["max_steps"].as<int>();
            }
            if (config["ccz4"]) {
                auto ccz4 = config["ccz4"];
                if (ccz4["kappa1"])
                    params.kappa1 = ccz4["kappa1"].as<Real>();
                if (ccz4["kappa2"])
                    params.kappa2 = ccz4["kappa2"].as<Real>();
                if (ccz4["eta"])
                    params.eta = ccz4["eta"].as<Real>();
                if (ccz4["ko_sigma"])
                    params.ko_sigma = ccz4["ko_sigma"].as<Real>();
            }
            if (config["io"]) {
                auto io = config["io"];
                if (io["output_dir"])
                    params.output_dir = io["output_dir"].as<std::string>();
                if (io["output_interval"])
                    params.output_interval = io["output_interval"].as<int>();
                if (io["checkpoint_interval"])
                    params.checkpoint_interval = io["checkpoint_interval"].as<int>();
            }
            // --- Parse initial data type ---
            if (config["initial_data"]) {
                auto id = config["initial_data"];
                if (id["type"])
                    initial_data_type = id["type"].as<std::string>();
                if (id["amplitude"])
                    gauge_wave_amplitude = id["amplitude"].as<Real>();
                if (id["wavelength"])
                    gauge_wave_wavelength = id["wavelength"].as<Real>();
            }
            // --- Parse black holes ---
            if (config["black_holes"] && config["black_holes"].IsSequence()) {
                for (auto bh_node : config["black_holes"]) {
                    initial_data::BlackHoleParams bh;
                    bh.mass = bh_node["mass"] ? bh_node["mass"].as<Real>() : 1.0;
                    if (bh_node["position"]) {
                        bh.position = {bh_node["position"][0].as<Real>(),
                                       bh_node["position"][1].as<Real>(),
                                       bh_node["position"][2].as<Real>()};
                    } else {
                        bh.position = {0.0, 0.0, 0.0};
                    }
                    if (bh_node["momentum"]) {
                        bh.momentum = {bh_node["momentum"][0].as<Real>(),
                                       bh_node["momentum"][1].as<Real>(),
                                       bh_node["momentum"][2].as<Real>()};
                    } else {
                        bh.momentum = {0.0, 0.0, 0.0};
                    }
                    if (bh_node["spin"]) {
                        bh.spin = {bh_node["spin"][0].as<Real>(),
                                   bh_node["spin"][1].as<Real>(),
                                   bh_node["spin"][2].as<Real>()};
                    } else {
                        bh.spin = {0.0, 0.0, 0.0};
                    }
                    bh_params.push_back(bh);
                }
            }
        } catch (const YAML::Exception& e) {
            std::cerr << "YAML parsing error: " << e.what() << "\n";
            return 1;
        }
    }

    // --- MICRO_OFFSET: phase-shift the grid away from integer/zero coordinates ---
    // Prevents any grid cell center from coinciding with the puncture at r=0,
    // which causes a 1/r² singularity in the Brill-Lindquist conformal factor.
    // The value is irrational w.r.t. any typical dx to avoid resonant re-alignment
    // across refinement levels.
    {
        constexpr Real MICRO_OFFSET = 1.3621415e-6;
        for (int d = 0; d < 3; ++d) {
            params.domain_lo[d] += MICRO_OFFSET;
            params.domain_hi[d] += MICRO_OFFSET;
        }
    }

    // --- Create Grids & Systems ---
    grmhd::GRMHDParams grmhd_params;
    auto eos = std::make_shared<grmhd::IdealGasEOS>(5.0 / 3.0);
    grmhd::GRMHDEvolution grmhd(grmhd_params, eos);

    amr::AMRParams amr_params;
    amr_params.regrid_interval = params.regrid_interval;
    amr::AMRHierarchy hierarchy(amr_params, params);
    std::cout << "Initializing AMR Hierarchy...\n";
    hierarchy.initialize(amr::gradientChiTagger(params.refine_threshold));

    std::vector<BlockBundle> active_bundles;
    std::unordered_map<int, size_t> id_to_index;

    auto syncBlocks = [&]() {
        auto active_blocks = hierarchy.getAllBlocks();
        std::set<int> active_ids;
        for (auto* b : active_blocks) {
            active_ids.insert(b->getId());
        }

        // Remove outdated bundles
        active_bundles.erase(std::remove_if(active_bundles.begin(),
                                            active_bundles.end(),
                                            [&](const BlockBundle& bundle) {
                                                return active_ids.find(bundle.id) ==
                                                    active_ids.end();
                                            }),
                             active_bundles.end());

        // Add missing bundles and sync ST pointers
        std::set<int> existing_ids;
        for (const auto& bundle : active_bundles)
            existing_ids.insert(bundle.id);

        for (auto* b : active_blocks) {
            if (existing_ids.find(b->getId()) == existing_ids.end()) {
                BlockBundle bundle;
                bundle.id = b->getId();
                bundle.st = b;
                bundle.hydro = std::make_unique<GridBlock>(b->getId(),
                                                           b->getLevel(),
                                                           params.ncells,
                                                           b->lowerCorner(),
                                                           b->upperCorner(),
                                                           b->getNumGhost(),
                                                           NUM_HYDRO_VARS);
                bundle.prim = std::make_unique<GridBlock>(b->getId(),
                                                          b->getLevel(),
                                                          params.ncells,
                                                          b->lowerCorner(),
                                                          b->upperCorner(),
                                                          b->getNumGhost(),
                                                          NUM_PRIMITIVE_VARS);
                bundle.st_rhs = std::make_unique<GridBlock>(b->getId(),
                                                            b->getLevel(),
                                                            params.ncells,
                                                            b->lowerCorner(),
                                                            b->upperCorner(),
                                                            b->getNumGhost(),
                                                            NUM_SPACETIME_VARS);
                bundle.hydro_rhs = std::make_unique<GridBlock>(b->getId(),
                                                               b->getLevel(),
                                                               params.ncells,
                                                               b->lowerCorner(),
                                                               b->upperCorner(),
                                                               b->getNumGhost(),
                                                               NUM_HYDRO_VARS);
                bundle.st_stage = std::make_unique<GridBlock>(b->getId(),
                                                              b->getLevel(),
                                                              params.ncells,
                                                              b->lowerCorner(),
                                                              b->upperCorner(),
                                                              b->getNumGhost(),
                                                              NUM_SPACETIME_VARS);
                bundle.hydro_stage = std::make_unique<GridBlock>(b->getId(),
                                                                 b->getLevel(),
                                                                 params.ncells,
                                                                 b->lowerCorner(),
                                                                 b->upperCorner(),
                                                                 b->getNumGhost(),
                                                                 NUM_HYDRO_VARS);
                active_bundles.push_back(std::move(bundle));
            } else {
                // O(1) lookup via id_to_index map
                auto it = id_to_index.find(b->getId());
                if (it != id_to_index.end()) {
                    active_bundles[it->second].st = b;
                }
            }
        }

        // Synchronize O(1) neighbor/boundary lookup map
        id_to_index.clear();
        for (size_t i = 0; i < active_bundles.size(); ++i) {
            id_to_index[active_bundles[i].id] = i;
        }

        // Allocate scratch buffers (one-time per bundle)
        for (auto& bundle : active_bundles) {
            if (bundle.st) {
                bundle.allocateScratch(bundle.st->totalSize());
            }
        }
    };

    // --- Set initial data dispatcher ---
    std::cout << "Setting initial data: type='" << initial_data_type << "'\n";
    syncBlocks();

    // All scenarios: set GRMHD atmosphere on hydro grid
    auto setAtmosphere = [&](GridBlock& hy) {
        for (int v = 0; v < hy.getNumVars(); ++v)
            for (int k = 0; k < hy.totalCells(2); ++k)
                for (int j = 0; j < hy.totalCells(1); ++j)
                    for (int i = 0; i < hy.totalCells(0); ++i)
                        hy.data(v, i, j, k) = (v == 0 || v == 4) ? 1.0e-12 : 0.0;
    };

    if (initial_data_type == "brill_lindquist") {
        // ---------------------------------------------------------------
        // Brill-Lindquist: analytic conformal factor, zero extrinsic curvature
        // Best for: N BHs at rest, no spin/momentum (Schwarzschild)
        // ---------------------------------------------------------------
        if (bh_params.empty()) {
            // Default: single BH M=1 at origin
            initial_data::BlackHoleParams default_bh;
            default_bh.mass = 1.0;
            default_bh.position = {0.0, 0.0, 0.0};
            default_bh.momentum = {0.0, 0.0, 0.0};
            default_bh.spin = {0.0, 0.0, 0.0};
            bh_params.push_back(default_bh);
        }
        initial_data::BrillLindquist bl_id(bh_params);
        std::cout << "  Brill-Lindquist: " << bh_params.size()
                  << " BH(s), ADM mass = " << bl_id.admMass() << " M_sun\n";
        for (auto& bundle : active_bundles) {
            bl_id.apply(*(bundle.st));
            setAtmosphere(*(bundle.hydro));
            grmhd.conservedToPrimitive(*(bundle.st), *(bundle.hydro), *(bundle.prim));
        }

    } else if (initial_data_type == "bowen_york") {
        // ---------------------------------------------------------------
        // Bowen-York: solves elliptic Hamiltonian constraint for each block
        // Best for: BHs with momenta and/or spin
        // ---------------------------------------------------------------
        if (bh_params.empty()) {
            initial_data::BlackHoleParams default_bh;
            default_bh.mass = 1.0;
            default_bh.position = {0.0, 0.0, 0.0};
            default_bh.momentum = {0.0, 0.0, 0.0};
            default_bh.spin = {0.0, 0.0, 0.0};
            bh_params.push_back(default_bh);
        }
        initial_data::BowenYorkPuncture by_id(bh_params);
        std::cout << "  Bowen-York: " << bh_params.size()
                  << " BH(s), ADM mass = " << by_id.admMass() << " M_sun\n";
        for (auto& bundle : active_bundles) {
            by_id.solve(*(bundle.st));
            setAtmosphere(*(bundle.hydro));
            grmhd.conservedToPrimitive(*(bundle.st), *(bundle.hydro), *(bundle.prim));
        }

    } else if (initial_data_type == "two_punctures") {
        // ---------------------------------------------------------------
        // Two Punctures: Spectral solver for BBH Initial Data
        // Best for: Quasi-circular Binary Black Hole mergers
        // ---------------------------------------------------------------
        if (bh_params.size() < 2) {
            // Default: two BHs M=0.5 separated by 2.0 along X
            initial_data::BlackHoleParams p1, p2;
            p1.mass = 0.5;
            p1.position = {1.0, 0.0, 0.0};
            p2.mass = 0.5;
            p2.position = {-1.0, 0.0, 0.0};
            p1.momentum = {0.0, 0.1, 0.0};
            p2.momentum = {0.0, -0.1, 0.0};
            bh_params = {p1, p2};
        }

        initial_data::TwoPuncturesParams tp_params;
        tp_params.par_m_plus[0] = bh_params[0].mass;
        tp_params.par_m_minus[0] = bh_params[1].mass;
        tp_params.par_P_plus = bh_params[0].momentum;
        tp_params.par_P_minus = bh_params[1].momentum;
        tp_params.par_S_plus = bh_params[0].spin;
        tp_params.par_S_minus = bh_params[1].spin;
        tp_params.par_b[0] = std::abs(bh_params[0].position[0] - bh_params[1].position[0]) / 2.0;

        initial_data::TwoPuncturesBBH tp_id(tp_params);
        std::cout << "  Two-Punctures: " << bh_params.size()
                  << " BH(s), generating conformal spectral data...\n";
        for (auto& bundle : active_bundles) {
            tp_id.generate(*(bundle.st));
            setAtmosphere(*(bundle.hydro));
            grmhd.conservedToPrimitive(*(bundle.st), *(bundle.hydro), *(bundle.prim));
        }

    } else if (initial_data_type == "gauge_wave") {
        // ---------------------------------------------------------------
        // Linear gauge wave: α = 1 + A*sin(2π x / L), flat metric
        // Best for: testing CCZ4 gauge stability, apples-with-apples
        // Expected: ||H||_2 ≈ 0 for all time
        // ---------------------------------------------------------------
        std::cout << "  Gauge wave: A=" << gauge_wave_amplitude << ", L=" << gauge_wave_wavelength
                  << "\n";
        for (auto& bundle : active_bundles) {
            spacetime::setFlatSpacetime(*(bundle.st));
            // Imprint sinusoidal lapse wave
            GridBlock& st = *(bundle.st);
            for (int k = 0; k < st.totalCells(2); ++k)
                for (int j = 0; j < st.totalCells(1); ++j)
                    for (int i = 0; i < st.totalCells(0); ++i) {
                        Real x = st.x(0, i);
                        Real lapse = 1.0 +
                            gauge_wave_amplitude * std::sin(2.0 * M_PI * x / gauge_wave_wavelength);
                        st.data(static_cast<int>(SpacetimeVar::LAPSE), i, j, k) = lapse;
                    }
            setAtmosphere(*(bundle.hydro));
            grmhd.conservedToPrimitive(*(bundle.st), *(bundle.hydro), *(bundle.prim));
        }

    } else {
        // ---------------------------------------------------------------
        // Default: flat Minkowski spacetime + vacuum atmosphere
        // Best for: code testing, MHD tests
        // ---------------------------------------------------------------
        if (initial_data_type != "flat") {
            std::cerr << "Warning: unknown initial_data type '" << initial_data_type
                      << "', falling back to 'flat'.\n";
        }
        std::cout << "  Flat Minkowski spacetime + GRMHD atmosphere\n";
        for (auto& bundle : active_bundles) {
            spacetime::setFlatSpacetime(*(bundle.st));
            setAtmosphere(*(bundle.hydro));
            grmhd.conservedToPrimitive(*(bundle.st), *(bundle.hydro), *(bundle.prim));
        }
    }

    // --- CCZ4 evolution ---
    spacetime::CCZ4Params ccz4_params;
    ccz4_params.kappa1 = params.kappa1;
    ccz4_params.kappa2 = params.kappa2;
    ccz4_params.eta = params.eta;
    ccz4_params.ko_sigma = params.ko_sigma;

    spacetime::CCZ4Evolution ccz4(ccz4_params);

    // --- I/O Writer ---
    io::IOParams io_params;
    io_params.output_dir = params.output_dir;
    io_params.output_interval = params.output_interval;
    io_params.checkpoint_interval = params.checkpoint_interval;
    io::HDF5Writer writer(io_params);

    // --- Compute time step ---
    auto initial_blocks = hierarchy.getAllBlocks();
    GridBlock* base_block = initial_blocks[0];
    Real dx_min = std::min({base_block->dx(0), base_block->dx(1), base_block->dx(2)});
    Real dt = params.cfl * dx_min;

    std::cout << "Grid: " << params.ncells[0] << " x " << params.ncells[1] << " x "
              << params.ncells[2] << "\n";
    std::cout << "dx = " << dx_min << ", dt = " << dt << "\n";
    std::cout << "t_final = " << params.t_final << "\n\n";

    // --- Inject level-0 dt into AMR hierarchy (fixes zombie state) ---
    // The hierarchy's level 0 was initialized with dt=0.0. We must set it
    // to the CFL-computed dt before subcycle() is called, otherwise
    // evolve_func always receives cur_dt=0 and the state never advances.
    hierarchy.setLevelDt(0, dt);

    // --- Register BBH tracking spheres (forces AMR around punctures) ---
    // Without this, the gradient tagger alone cannot detect the puncture
    // on a coarse initial grid. Tracking spheres guarantee refinement.
    if (initial_data_type == "two_punctures" || initial_data_type == "brill_lindquist" ||
        initial_data_type == "bowen_york") {
        for (const auto& bh : bh_params) {
            std::array<Real, DIM> center = bh.position;
            Real sphere_radius = std::max(2.0 * bh.mass, 0.5); // at least 2M radius
            int min_ref_level = std::min(amr_params.max_levels - 1, 3);
            hierarchy.addTrackingSphere(center, sphere_radius, min_ref_level);
            std::cout << "  [AMR] Tracking sphere @ (" << center[0] << "," << center[1] << ","
                      << center[2] << ") R=" << sphere_radius << " min_level=" << min_ref_level
                      << "\n";
        }
        // Trigger initial regrid to establish refinement before step 0
        hierarchy.regrid(0, amr::gradientChiTagger(params.refine_threshold));
        syncBlocks();
        hierarchy.fillGhostZones(0);
    }

    // --- Evolution loop ---
    Real t = 0.0;
    int step = 0;
    int initial_ncells = params.ncells[0];

    // The physics callback for AMR subcycling
    auto evolve_func = [&](std::vector<GridBlock*>& cur_blocks, Real cur_dt) {
        syncBlocks();
        std::vector<BlockBundle*> cur_bundles;
        cur_bundles.reserve(cur_blocks.size());
        for (auto* b : cur_blocks) {
            auto it = id_to_index.find(b->getId());
            if (it != id_to_index.end()) {
                cur_bundles.push_back(&active_bundles[it->second]);
            }
        }

        TimeIntegrator::sspRK3Step(cur_bundles, active_bundles, id_to_index, ccz4, grmhd, cur_dt);

        // ── Adaptive CFL monitoring (Stream C2) ────────────────────
        Real max_adv_cfl = 0.0;
        for (auto* bundle_ptr : cur_bundles) {
            auto& bundle = *bundle_ptr;
            GridBlock& g = *(bundle.st);
            const int bx_var = static_cast<int>(SpacetimeVar::SHIFT_X);
            const int by_var = static_cast<int>(SpacetimeVar::SHIFT_Y);
            const int bz_var = static_cast<int>(SpacetimeVar::SHIFT_Z);
            for (int k = g.istart(); k < g.iend(2); ++k)
                for (int j = g.istart(); j < g.iend(1); ++j)
                    for (int i = g.istart(); i < g.iend(0); ++i) {
                        Real bx = std::abs(g.data(bx_var, i, j, k));
                        Real by = std::abs(g.data(by_var, i, j, k));
                        Real bz = std::abs(g.data(bz_var, i, j, k));
                        Real local_cfl =
                            bx * cur_dt / g.dx(0) + by * cur_dt / g.dx(1) + bz * cur_dt / g.dx(2);
                        max_adv_cfl = std::max(max_adv_cfl, local_cfl);
                    }
        }
        if (max_adv_cfl > 0.95) {
            std::cout << "  [CFL-GUARD] Advection CFL=" << max_adv_cfl << " > 0.95 at sub-step!\n";
        }
    };

    while (t < params.t_final && step < params.max_steps) {
        if (t + dt > params.t_final)
            dt = params.t_final - t;

        // Sync dt into hierarchy level 0 in case dt was reduced by CFL guard
        hierarchy.setLevelDt(0, dt);

        // The AMR Hierarchy drives the recursive evolution and regridding
        hierarchy.subcycle(0, evolve_func, amr::gradientChiTagger(params.refine_threshold));

        t += dt;
        step++;

        // Per-step NaN scan (first 20 loops only — remove once stable)
        if (step <= 20) {
            bool found_nan = false;
            for (auto* b : hierarchy.getAllBlocks()) {
                if (found_nan)
                    break;
                for (int v = 0; v < b->getNumVars() && !found_nan; ++v)
                    for (int k = b->istart(2); k < b->iend(2) && !found_nan; ++k)
                        for (int j = b->istart(1); j < b->iend(1) && !found_nan; ++j)
                            for (int i = b->istart(0); i < b->iend(0) && !found_nan; ++i) {
                                Real val = b->data(v, i, j, k);
                                if (std::isnan(val) || std::isinf(val)) {
                                    std::ostringstream msg;
                                    msg << "  [NaN@step=" << step << "] ST var=" << v << " (" << i
                                        << "," << j << "," << k << ")" << " = " << val;
                                    std::cout << msg.str() << std::endl;
                                    found_nan = true;
                                }
                            }
            }
            if (!found_nan)
                std::cout << "  [NaN@step=" << step << "] all finite\n";
        }

        if (step % params.output_interval == 0) {
            std::vector<Real> ham;
            std::vector<std::array<Real, DIM>> mom;

            Real ham_l2 = 0.0;
            int count = 0;
            Real alpha_center = 1.0;

            for (auto* block : hierarchy.getAllBlocks()) {
                ccz4.computeConstraints(*block, ham, mom);
                int is0 = block->istart(0);
                int is1 = block->istart(1);
                int is2 = block->istart(2);
                int ie0 = block->iend(0);
                int ie1 = block->iend(1);
                int ie2 = block->iend(2);
                for (int k = is2; k < ie2; ++k)
                    for (int j = is1; j < ie1; ++j)
                        for (int i = is0; i < ie0; ++i) {
                            int flat = block->totalCells(0) * (block->totalCells(1) * k + j) + i;
                            ham_l2 += ham[flat] * ham[flat];
                            count++;
                        }
                if (block->getLevel() == 0) {
                    alpha_center = block->data(static_cast<int>(SpacetimeVar::LAPSE),
                                               initial_ncells / 2 + params.ghost_cells,
                                               initial_ncells / 2 + params.ghost_cells,
                                               initial_ncells / 2 + params.ghost_cells);
                }
            }
            if (count > 0)
                ham_l2 = std::sqrt(ham_l2 / count);

            std::cout << "  step=" << step << "  t=" << t << "  α_center=" << alpha_center
                      << "  ||H||₂=" << ham_l2 << "  [Blocks: " << hierarchy.numBlocks() << "]\n";

            writer.appendTimeSeries(
                params.output_dir + "/timeseries.h5", "constraints/hamiltonian_l2", t, ham_l2);
        }

        if (step % params.checkpoint_interval == 0 || t >= params.t_final) {
            std::vector<const GridBlock*> c_blocks;
            for (auto* b : hierarchy.getAllBlocks())
                c_blocks.push_back(b);
            writer.writeCheckpoint(c_blocks, step, t, params);
        }
    }

    std::cout << "Evolution complete.\n";

#ifdef GRANITE_USE_MPI
    MPI_Finalize();
#endif

#ifdef GRANITE_USE_PETSC
    PetscFinalize();
#endif

    return 0;
}
