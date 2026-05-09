/**
 * @file test_types.cpp
 * @brief Unit tests for core type definitions and index conventions.
 */
#include "granite/core/types.hpp"

#include <gtest/gtest.h>
#include <set>

using namespace granite;

TEST(TypesTest, SymmetricIndexSymmetry) {
    // symIdx(i,j) == symIdx(j,i) for all i,j
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            EXPECT_EQ(symIdx(i, j), symIdx(j, i));
}

TEST(TypesTest, SymmetricIndexValues) {
    EXPECT_EQ(symIdx(0, 0), 0); // XX
    EXPECT_EQ(symIdx(0, 1), 1); // XY
    EXPECT_EQ(symIdx(0, 2), 2); // XZ
    EXPECT_EQ(symIdx(1, 1), 3); // YY
    EXPECT_EQ(symIdx(1, 2), 4); // YZ
    EXPECT_EQ(symIdx(2, 2), 5); // ZZ
}

TEST(TypesTest, SymmetricIndexCoversAll) {
    // 6 unique components for symmetric 3-tensor
    std::set<int> indices;
    for (int i = 0; i < 3; ++i)
        for (int j = i; j < 3; ++j)
            indices.insert(symIdx(i, j));
    EXPECT_EQ(indices.size(), 6u);
}

TEST(TypesTest, VariableCounts) {
    EXPECT_EQ(NUM_SPACETIME_VARS, 22);
    EXPECT_EQ(NUM_HYDRO_VARS, 9); // D, SX, SY, SZ, TAU, BX, BY, BZ, DYE
    EXPECT_EQ(NUM_PRIMITIVE_VARS, 11);
    EXPECT_EQ(NUM_RADIATION_VARS, 4);
}

TEST(TypesTest, PhysicalConstants) {
    EXPECT_NEAR(constants::PI, 3.14159265358979, 1e-12);
    EXPECT_GT(constants::G_CGS, 0.0);
    EXPECT_GT(constants::C_CGS, 0.0);
    EXPECT_GT(constants::MSUN_CGS, 0.0);
}

TEST(TypesTest, UnitConversions) {
    // 1 solar mass in geometric units should give ~1.5 km in length
    Real L = constants::LENGTH_TO_CGS(1.0); // Length corresponding to 1 M_sun
    Real L_km = L / 1.0e5;
    EXPECT_NEAR(L_km, 1.477, 0.01); // Schwarzschild radius / 2 ≈ 1.48 km
}

TEST(TypesTest, Index3DFlatIndex) {
    Index3D idx(3, 4, 5);
    EXPECT_EQ(idx.flat(10, 10), 3 + 10 * (4 + 10 * 5));
}
