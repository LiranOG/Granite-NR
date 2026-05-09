# ============================================================
# GRANITE Test Suite
# ============================================================
add_executable(granite_tests
    core/test_grid.cpp
    core/test_types.cpp
    spacetime/test_ccz4_flat.cpp
    spacetime/test_gauge_wave.cpp
    initial_data/test_brill_lindquist.cpp
    initial_data/test_polytrope.cpp
    grmhd/test_hlld_ct.cpp
    grmhd/test_tabulated_eos.cpp
    grmhd/test_grmhd_gr.cpp   # Phase 4D: GR-aware HLLE + MP5 reconstruction tests
    grmhd/test_ppm.cpp         # Phase 5B: PPM reconstruction tests (Colella & Woodward 1984)
    # P1-01 smoke tests — first coverage for previously untested subsystems
    amr/test_amr_basic.cpp
    horizon/test_schwarzschild_horizon.cpp
    io/test_hdf5_roundtrip.cpp
    radiation/test_m1_diffusion.cpp
)

target_link_libraries(granite_tests PRIVATE
    granite_lib
    GTest::gtest
    GTest::gtest_main
)

# Auto-discover and register all tests
gtest_discover_tests(granite_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES TIMEOUT 120
)

# ============================================================
# Categorized test labels for selective running
# ============================================================
# ctest -L unit       → fast unit tests
# ctest -L convergence → convergence tests (may take minutes)
# ctest -L benchmark  → full benchmark tests (may take hours)
