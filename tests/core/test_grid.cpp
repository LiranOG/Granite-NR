/**
 * @file test_grid.cpp
 * @brief Unit tests for GridBlock.
 *
 * Covers construction, data access, coordinate mapping, and the
 * new Phase 5 APIs: buffer packing/unpacking and neighbor metadata.
 */
#include "granite/core/grid.hpp"
#include "granite/core/types.hpp"

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <numeric>

using namespace granite;

class GridBlockTest : public ::testing::Test {
protected:
    void SetUp() override {
        ncells = {16, 16, 16};
        lo = {-1.0, -1.0, -1.0};
        hi = {1.0, 1.0, 1.0};
        nghost = 4;
        nvars = 5;
        grid = std::make_unique<GridBlock>(0, 0, ncells, lo, hi, nghost, nvars);
    }

    std::array<int, DIM> ncells;
    std::array<Real, DIM> lo, hi;
    int nghost, nvars;
    std::unique_ptr<GridBlock> grid;
};

// ---------------------------------------------------------------------------
// Construction & basic properties
// ---------------------------------------------------------------------------

TEST_F(GridBlockTest, ConstructionCellCounts) {
    EXPECT_EQ(grid->totalCells(0), 24); // 16 + 2*4
    EXPECT_EQ(grid->totalCells(1), 24);
    EXPECT_EQ(grid->totalCells(2), 24);
}

TEST_F(GridBlockTest, CellSpacing) {
    Real expected_dx = 2.0 / 16.0; // (hi - lo) / ncells
    EXPECT_NEAR(grid->dx(0), expected_dx, 1e-14);
    EXPECT_NEAR(grid->dx(1), expected_dx, 1e-14);
    EXPECT_NEAR(grid->dx(2), expected_dx, 1e-14);
}

TEST_F(GridBlockTest, CoordinateMapping) {
    // Cell center at index nghost should be at lo + 0.5*dx
    Real expected_x0 = -1.0 + 0.5 * grid->dx(0);
    EXPECT_NEAR(grid->x(0, nghost), expected_x0, 1e-14);

    // Cell center at index nghost + ncells - 1 should be near hi - 0.5*dx
    Real expected_xN = 1.0 - 0.5 * grid->dx(0);
    EXPECT_NEAR(grid->x(0, nghost + ncells[0] - 1), expected_xN, 1e-14);
}

TEST_F(GridBlockTest, DataAccessSymmetry) {
    // Write and read back
    grid->data(2, 5, 6, 7) = 3.14159;
    EXPECT_DOUBLE_EQ(grid->data(2, 5, 6, 7), 3.14159);
}

TEST_F(GridBlockTest, InitializedToZero) {
    for (int v = 0; v < nvars; ++v) {
        for (int k = 0; k < grid->totalCells(2); ++k)
            for (int j = 0; j < grid->totalCells(1); ++j)
                for (int i = 0; i < grid->totalCells(0); ++i)
                    EXPECT_DOUBLE_EQ(grid->data(v, i, j, k), 0.0);
    }
}

TEST_F(GridBlockTest, TotalSize) {
    EXPECT_EQ(grid->totalSize(), 24u * 24u * 24u);
}

TEST_F(GridBlockTest, InteriorRange) {
    EXPECT_EQ(grid->istart(), 4);
    EXPECT_EQ(grid->iend(0), 20);
    EXPECT_EQ(grid->iend(1), 20);
    EXPECT_EQ(grid->iend(2), 20);
}

TEST_F(GridBlockTest, RawDataPointer) {
    // rawData pointer should be accessible and pointing into data storage.
    Real* p = grid->rawData(0);
    ASSERT_NE(p, nullptr);
    p[0] = 42.0;
    EXPECT_DOUBLE_EQ(grid->data(0, 0, 0, 0), 42.0);
}

// ---------------------------------------------------------------------------
// Phase 5: Neighbor & rank tracking
// ---------------------------------------------------------------------------

TEST_F(GridBlockTest, DefaultRankIsZero) {
    EXPECT_EQ(grid->getRank(), 0);
}

TEST_F(GridBlockTest, SetAndGetRank) {
    grid->setRank(7);
    EXPECT_EQ(grid->getRank(), 7);
}

TEST_F(GridBlockTest, DefaultNeighborIdsAreNegative) {
    for (int dir = 0; dir < 6; ++dir) {
        EXPECT_LT(grid->getNeighborId(dir), 0)
            << "Neighbor ID for direction " << dir << " should be -1 by default.";
    }
}

TEST_F(GridBlockTest, SetAndGetNeighborId) {
    grid->setNeighborId(0, 10);
    grid->setNeighborId(1, 11);
    EXPECT_EQ(grid->getNeighborId(0), 10);
    EXPECT_EQ(grid->getNeighborId(1), 11);
    EXPECT_LT(grid->getNeighborId(2), 0); // still unset
}

TEST_F(GridBlockTest, SetAndGetNeighborRank) {
    grid->setNeighborRank(2, 5);
    grid->setNeighborRank(3, 6);
    EXPECT_EQ(grid->getNeighborRank(2), 5);
    EXPECT_EQ(grid->getNeighborRank(3), 6);
    EXPECT_EQ(grid->getNeighborRank(0), -1); // still default
}

// ---------------------------------------------------------------------------
// Phase 5: Buffer Pack / Unpack (ghost zone exchange)
// ---------------------------------------------------------------------------

class GridBlockBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Asymmetric cell counts to verify non-square slabs correctly
        ncells_ = {8, 12, 6};
        lo_ = {0.0, 0.0, 0.0};
        hi_ = {1.0, 1.0, 1.0};
        nghost_ = 2;
        nvars_ = 3;
        grid_ = std::make_unique<GridBlock>(0, 0, ncells_, lo_, hi_, nghost_, nvars_);

        // Fill interior with a known pattern: data(v, i, j, k) = v*1000 + i*100 + j*10 + k
        for (int v = 0; v < nvars_; ++v) {
            for (int k = 0; k < grid_->totalCells(2); ++k)
                for (int j = 0; j < grid_->totalCells(1); ++j)
                    for (int i = 0; i < grid_->totalCells(0); ++i)
                        grid_->data(v, i, j, k) =
                            static_cast<Real>(v * 1000 + i * 100 + j * 10 + k);
        }
    }

    std::array<int, DIM> ncells_;
    std::array<Real, DIM> lo_, hi_;
    int nghost_, nvars_;
    std::unique_ptr<GridBlock> grid_;
};

TEST_F(GridBlockBufferTest, PackBufferSizeXLow) {
    // Face 0 = X-low: nghost_ slabs in X × totalCells(1) × totalCells(2)
    std::vector<Real> buf;
    grid_->packBoundary(0, buf);

    int expected_nx = nghost_;
    int expected_ny = grid_->totalCells(1);
    int expected_nz = grid_->totalCells(2);
    std::size_t expected =
        static_cast<std::size_t>(nvars_ * expected_nx * expected_ny * expected_nz);
    EXPECT_EQ(buf.size(), expected) << "packBoundary(face=0) produced wrong buffer size.";
}

TEST_F(GridBlockBufferTest, PackBufferSizeZHigh) {
    // Face 5 = Z-high: totalCells(0) × totalCells(1) × nghost_ slabs in Z
    std::vector<Real> buf;
    grid_->packBoundary(5, buf);

    int expected_nx = grid_->totalCells(0);
    int expected_ny = grid_->totalCells(1);
    int expected_nz = nghost_;
    std::size_t expected =
        static_cast<std::size_t>(nvars_ * expected_nx * expected_ny * expected_nz);
    EXPECT_EQ(buf.size(), expected) << "packBoundary(face=5) produced wrong buffer size.";
}

TEST_F(GridBlockBufferTest, PackAndUnpackRoundtrip) {
    // Pack the X-low interior face into a buffer
    std::vector<Real> send_buf;
    grid_->packBoundary(0, send_buf); // face 0 = X-low interior face

    // Create a fresh destination grid of the same dimensions
    auto dest = std::make_unique<GridBlock>(1, 0, ncells_, lo_, hi_, nghost_, nvars_);

    // Unpack into the X-high ghost zone of destination (opposite face = 1)
    dest->unpackBoundary(1, send_buf); // face 1 = X-high ghost zone

    // Verify: the X-high ghost slab of dest should match X-low interior of grid_
    int is = grid_->istart(0);
    for (int v = 0; v < nvars_; ++v) {
        for (int k = 0; k < grid_->totalCells(2); ++k)
            for (int j = 0; j < grid_->totalCells(1); ++j)
                for (int ng = 0; ng < nghost_; ++ng) {
                    Real src_val = grid_->data(v, is + ng, j, k);
                    int dst_i = grid_->totalCells(0) - nghost_ + ng;
                    Real dest_val = dest->data(v, dst_i, j, k);
                    EXPECT_DOUBLE_EQ(dest_val, src_val)
                        << "Mismatch at v=" << v << " ng=" << ng << " j=" << j << " k=" << k;
                }
    }
}

TEST_F(GridBlockBufferTest, AllSixFacesPackCorrectly) {
    // Each face should produce a non-empty buffer
    for (int face = 0; face < 6; ++face) {
        std::vector<Real> buf;
        grid_->packBoundary(face, buf);
        EXPECT_GT(buf.size(), 0u) << "packBoundary(face=" << face << ") returned empty buffer.";
    }
}

TEST_F(GridBlockBufferTest, OppositeFaceSymmetry) {
    // face 0 (X-low) and face 1 (X-high) should produce same-size buffers
    std::vector<Real> buf0, buf1;
    grid_->packBoundary(0, buf0);
    grid_->packBoundary(1, buf1);
    EXPECT_EQ(buf0.size(), buf1.size());

    // Likewise for Y and Z
    std::vector<Real> buf2, buf3;
    grid_->packBoundary(2, buf2);
    grid_->packBoundary(3, buf3);
    EXPECT_EQ(buf2.size(), buf3.size());

    std::vector<Real> buf4, buf5;
    grid_->packBoundary(4, buf4);
    grid_->packBoundary(5, buf5);
    EXPECT_EQ(buf4.size(), buf5.size());
}

// ---------------------------------------------------------------------------
// Phase 5 H2: Flat contiguous memory layout validation
// Tests verify that the H2 refactor (vector<vector<Real>> → vector<Real>)
// produces a single contiguous allocation with correct stride arithmetic.
// ---------------------------------------------------------------------------

class GridBlockFlatLayoutTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::array<int, 3> nc = {8, 8, 8};
        std::array<Real, 3> lo = {0.0, 0.0, 0.0};
        std::array<Real, 3> hi = {1.0, 1.0, 1.0};
        grid_ = std::make_unique<GridBlock>(0, 0, nc, lo, hi, 2, 5);
    }
    std::unique_ptr<GridBlock> grid_;
};

/// Verify that rawData(v) is offset from rawData(0) by exactly v×totalSize() words.
/// This proves the flat layout is a single contiguous allocation.
TEST_F(GridBlockFlatLayoutTest, FlatBufferContiguity) {
    std::size_t stride = grid_->totalSize();

    Real* p0 = grid_->rawData(0);
    Real* p1 = grid_->rawData(1);
    Real* p4 = grid_->rawData(4);

    ASSERT_NE(p0, nullptr);
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p4, nullptr);

    EXPECT_EQ(static_cast<std::size_t>(p1 - p0), stride)
        << "rawData(1) - rawData(0) must equal totalSize()";
    EXPECT_EQ(static_cast<std::size_t>(p4 - p0), 4 * stride)
        << "rawData(4) - rawData(0) must equal 4 * totalSize()";
}

/// Verify that rawData() and data() index the same memory location.
TEST_F(GridBlockFlatLayoutTest, RawDataEquivalentToAccessor) {
    int ng = grid_->istart();
    int nx = grid_->totalCells(0);
    int ny = grid_->totalCells(1);

    grid_->data(2, ng + 1, ng + 2, ng + 3) = 271.828;

    int flat_idx = (ng + 1) + nx * ((ng + 2) + ny * (ng + 3));
    Real val_via_raw = grid_->rawData(2)[flat_idx];

    EXPECT_DOUBLE_EQ(val_via_raw, 271.828)
        << "rawData() and data() must address the same memory location";
}

/// Verify flatBuffer() returns rawData(0) and writes propagate through data().
TEST_F(GridBlockFlatLayoutTest, FlatBufferSpansAllVars) {
    Real* flat = grid_->flatBuffer();
    Real* raw0 = grid_->rawData(0);

    EXPECT_EQ(flat, raw0)
        << "flatBuffer() must equal rawData(0) — both point to start of allocation";

    std::size_t stride = grid_->totalSize();
    flat[3 * static_cast<std::ptrdiff_t>(stride)] = 99.9; // var=3, cell_flat=0
    EXPECT_DOUBLE_EQ(grid_->data(3, 0, 0, 0), 99.9)
        << "flatBuffer() write must be visible through data() accessor";
}

/// Verify that writing different values to all 5 variables at the same cell
/// produces no aliasing between variables.
TEST_F(GridBlockFlatLayoutTest, MultiVarWriteReadBack) {
    int ng = grid_->istart();
    for (int v = 0; v < 5; ++v) {
        grid_->data(v, ng, ng, ng) = static_cast<Real>(v * 100.0 + 7.77);
    }
    for (int v = 0; v < 5; ++v) {
        Real expected = static_cast<Real>(v * 100.0 + 7.77);
        EXPECT_DOUBLE_EQ(grid_->data(v, ng, ng, ng), expected)
            << "Variable aliasing detected at v=" << v;
    }
}
