// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Liran M. Schwartz

#include "granite/evolution_loop.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>

#ifdef GRANITE_USE_MPI
#include <mpi.h>
#endif

namespace granite {

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

        // (One-shot NaN diagnostic removed — goto-based control flow crossed
        // variable initialization scopes, which is UB under C++17 §9.7/3.
        // The per-step NaN scan in runEvolutionLoop() provides equivalent coverage.)
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

void runEvolutionLoop(SimulationContext& ctx) {
    auto& params = ctx.params;
    auto& hierarchy = ctx.hierarchy;
    auto& ccz4 = ctx.ccz4;
    auto& grmhd = ctx.grmhd;
    auto& writer = ctx.writer;
    auto& active_bundles = ctx.active_bundles;
    auto& id_to_index = ctx.id_to_index;
    Real& t = ctx.t;
    int& step = ctx.step;
    Real& dt = ctx.dt;
    int initial_ncells = ctx.initial_ncells;

    // The physics callback for AMR subcycling
    auto evolve_func = [&](std::vector<GridBlock*>& cur_blocks, Real cur_dt) {
        ctx.syncBlocks();
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
}

} // namespace granite
