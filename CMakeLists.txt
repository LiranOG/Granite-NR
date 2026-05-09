# =============================================================================
# GRANITE — General-Relativistic Adaptive N-body Integrated Tool
#            for Extreme Astrophysics
#
# Top-level CMake configuration
# =============================================================================
cmake_minimum_required(VERSION 3.20)

project(GRANITE
    VERSION 0.6.7
    DESCRIPTION "General-Relativistic Adaptive N-body Integrated Tool for Extreme Astrophysics"
    HOMEPAGE_URL "https://github.com/LiranOG/Granite"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(GRANITE_ENABLE_MPI      "Enable MPI parallelism"                   ON)
option(GRANITE_ENABLE_OPENMP   "Enable OpenMP threading"                  ON)
option(GRANITE_ENABLE_GPU      "Enable GPU acceleration (CUDA/HIP)"       OFF)
option(GRANITE_GPU_BACKEND     "GPU backend: CUDA or HIP"                 "CUDA")
option(GRANITE_ENABLE_HDF5     "Enable HDF5 I/O"                          ON)
option(GRANITE_ENABLE_PETSC    "Enable PETSc for elliptic solvers"         OFF)
option(GRANITE_ENABLE_TESTS    "Build unit and integration tests"          ON)
option(GRANITE_ENABLE_DOCS     "Build documentation"                       OFF)
option(GRANITE_ENABLE_PYTHON   "Install Python analysis package"           OFF)
option(GRANITE_DOUBLE_PRECISION "Use double precision (always true for NR)" ON)
# Code-coverage instrumentation (GCC / Clang only — requires lcov)
option(ENABLE_COVERAGE         "Enable gcov/lcov code-coverage reporting" OFF)

# ---------------------------------------------------------------------------
# C++ standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Build type defaults
# ---------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# ------------------------------------------------------------
# Compiler flags
# ------------------------------------------------------------
if(MSVC)
    set(GRANITE_WARNING_FLAGS "/W4 /wd4100 /wd4127 /wd4505")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    set(GRANITE_WARNING_FLAGS "-Wall -Wextra -Wpedantic -Wshadow -Wconversion")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GRANITE_WARNING_FLAGS}")

if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG   "/Zi /Od /RTC1 /DGRANITE_DEBUG")
    # /Ox  = maximum speed (/O2 + /Ob2 + /Ot + /Oy + /GF + /Gy)
    # /arch:AVX2 = emit AVX2 + FMA instructions
    # /fp:fast   = allow algebraic FP simplifications (FMA pipeline)
    # /GL        = whole-program optimisation (link-time)
    # /MT        = statically link CRT (portable Release binary)
    set(CMAKE_CXX_FLAGS_RELEASE "/Ox /Ob2 /arch:AVX2 /fp:fast /GL /MT /DNDEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
else()
    set(CMAKE_CXX_FLAGS_DEBUG         "-g -O0 -DGRANITE_DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE       "-O3 -DNDEBUG -march=native -ffast-math")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
endif()

# ---------------------------------------------------------------------------
# Code-coverage instrumentation
# Only meaningful on GCC or Clang; MSVC does not support --coverage.
# Usage:
#   cmake -B build -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build --target coverage
# ---------------------------------------------------------------------------
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "[GRANITE] Code coverage: --coverage flags enabled")
        # Instrument all translation units with gcov-compatible data
        set(CMAKE_CXX_FLAGS    "${CMAKE_CXX_FLAGS}    --coverage")
        set(CMAKE_C_FLAGS      "${CMAKE_C_FLAGS}      --coverage")
        # Link the gcov / libgcov runtime into every executable
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    else()
        message(WARNING
            "[GRANITE] ENABLE_COVERAGE=ON is ignored — "
            "coverage instrumentation requires GCC or Clang, "
            "but the active compiler is '${CMAKE_CXX_COMPILER_ID}'."
        )
    endif()
endif()

# ---------------------------------------------------------------------------
# Output directories
# ---------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ---------------------------------------------------------------------------
# Find required and optional dependencies
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Windows / MSVC dependency hints
# Must run BEFORE find_package() calls so CMake searches the right paths.
# ---------------------------------------------------------------------------
if(WIN32)
    # ── MS-MPI ────────────────────────────────────────────────────────────
    # MS-MPI sets MSMPI_INC and MSMPI_LIB64 in the system environment.
    # Prefer those over any vcpkg-installed stub so we get the real wrapper.
    if(DEFINED ENV{MSMPI_INC} AND DEFINED ENV{MSMPI_LIB64})
        set(MPI_C_INCLUDE_PATH    "$ENV{MSMPI_INC}"       CACHE PATH "MS-MPI Include"  FORCE)
        set(MPI_CXX_INCLUDE_PATH  "$ENV{MSMPI_INC}"       CACHE PATH "MS-MPI Include"  FORCE)
        set(MPI_C_LIB_NAMES   msmpi CACHE STRING "MS-MPI C lib"   FORCE)
        set(MPI_CXX_LIB_NAMES msmpi CACHE STRING "MS-MPI CXX lib" FORCE)
        set(MPI_msmpi_LIBRARY "$ENV{MSMPI_LIB64}/msmpi.lib" CACHE FILEPATH "MS-MPI lib" FORCE)
        message(STATUS "[GRANITE] MS-MPI hint → INC=$ENV{MSMPI_INC}  LIB=$ENV{MSMPI_LIB64}")
    elseif(DEFINED ENV{MPI_INC} AND DEFINED ENV{MPI_LIB})
        # Fallback: user-set MPI_INC / MPI_LIB vars from setup_windows.ps1
        set(MPI_C_INCLUDE_PATH    "$ENV{MPI_INC}"    CACHE PATH "MPI Include" FORCE)
        set(MPI_CXX_INCLUDE_PATH  "$ENV{MPI_INC}"    CACHE PATH "MPI Include" FORCE)
        set(MPI_msmpi_LIBRARY     "$ENV{MPI_LIB}"    CACHE FILEPATH "MPI lib" FORCE)
        set(MPI_C_LIB_NAMES   msmpi CACHE STRING "" FORCE)
        set(MPI_CXX_LIB_NAMES msmpi CACHE STRING "" FORCE)
        message(STATUS "[GRANITE] MPI hint (env) → INC=$ENV{MPI_INC}")
    endif()

    # ── HDF5 (vcpkg or standalone installer) ──────────────────────────────
    # vcpkg exports HDF5 as an imported target but the legacy FindHDF5 module
    # also needs HDF5_DIR pointing at the cmake config directory.
    if(DEFINED ENV{HDF5_DIR})
        list(PREPEND CMAKE_PREFIX_PATH "$ENV{HDF5_DIR}")
        set(HDF5_DIR "$ENV{HDF5_DIR}" CACHE PATH "HDF5 cmake config dir" FORCE)
        message(STATUS "[GRANITE] HDF5 hint → $ENV{HDF5_DIR}")
    endif()
    # If only the root install dir is known (e.g. C:/Program Files/HDF_Group/HDF5/...)
    if(DEFINED ENV{HDF5_ROOT})
        list(PREPEND CMAKE_PREFIX_PATH "$ENV{HDF5_ROOT}")
        message(STATUS "[GRANITE] HDF5_ROOT hint → $ENV{HDF5_ROOT}")
    endif()
endif()

# ── MPI ───────────────────────────────────────────────────────────────────
if(GRANITE_ENABLE_MPI)
    find_package(MPI COMPONENTS CXX)
    if(NOT MPI_FOUND)
        message(WARNING
            "[GRANITE] MPI not found — building WITHOUT MPI.\n"
            "  Install MS-MPI (https://aka.ms/msmpi) and re-run CMake, or\n"
            "  disable with -DGRANITE_ENABLE_MPI=OFF."
        )
        set(GRANITE_ENABLE_MPI OFF CACHE BOOL "" FORCE)
    else()
        add_definitions(-DGRANITE_USE_MPI)
        message(STATUS "[GRANITE] MPI found: ${MPI_CXX_COMPILER} (${MPI_CXX_VERSION})")
    endif()
endif()

# ── OpenMP ────────────────────────────────────────────────────────────────
# On MSVC we force /openmp:llvm which enables OpenMP 3.1+ (task, collapse, etc.)
# and performs superior data-parallel code-gen compared to the legacy /openmp flag.
if(GRANITE_ENABLE_OPENMP)
    if(MSVC)
        # Inject the LLVM OpenMP switch before find_package so FindOpenMP picks
        # it up correctly instead of the antique vcomp stub.
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /openmp:llvm")
        # Silence the deprecation of /openmp vs /openmp:llvm that MSVC 19.29+ emits
        add_compile_options(/wd4068)
    endif()
    find_package(OpenMP REQUIRED COMPONENTS CXX)
    if(OpenMP_FOUND)
        add_definitions(-DGRANITE_USE_OPENMP)
        message(STATUS "[GRANITE] OpenMP version: ${OpenMP_CXX_VERSION}")
        if(MSVC AND OpenMP_CXX_VERSION VERSION_LESS "3.0")
            message(WARNING
                "[GRANITE] OpenMP < 3.0 detected. Ensure VS 2019 16.9+ / 2022 is installed\n"
                "  and /openmp:llvm is supported. Perf will be sub-optimal with version 2.0."
            )
        endif()
    endif()
endif()

# GPU (CUDA or HIP)
if(GRANITE_ENABLE_GPU)
    if(GRANITE_GPU_BACKEND STREQUAL "CUDA")
        enable_language(CUDA)
        find_package(CUDAToolkit REQUIRED)
        add_definitions(-DGRANITE_USE_CUDA)
        set(CMAKE_CUDA_STANDARD 17)
        set(CMAKE_CUDA_ARCHITECTURES "70;80;90")
        message(STATUS "CUDA enabled: ${CUDAToolkit_VERSION}")
    elseif(GRANITE_GPU_BACKEND STREQUAL "HIP")
        find_package(hip REQUIRED)
        add_definitions(-DGRANITE_USE_HIP)
        message(STATUS "HIP enabled")
    else()
        message(FATAL_ERROR "Unknown GPU backend: ${GRANITE_GPU_BACKEND}")
    endif()
endif()

# ── HDF5 ─────────────────────────────────────────────────────────────────
if(GRANITE_ENABLE_HDF5)
    set(HDF5_PREFER_PARALLEL FALSE) # MS-MPI parallel HDF5 is rarely available on Windows
    # Try modern config-mode find first (vcpkg, Conda, standalone installer)
    find_package(HDF5 CONFIG QUIET COMPONENTS C CXX)
    if(NOT HDF5_FOUND)
        # Legacy module-mode fallback
        find_package(HDF5 REQUIRED COMPONENTS C CXX)
    endif()
    if(HDF5_FOUND)
        add_definitions(-DGRANITE_USE_HDF5)
        # Determine whether the modern imported target or legacy vars are available
        if(TARGET hdf5::hdf5-shared OR TARGET hdf5::hdf5_cpp-shared)
            set(GRANITE_HDF5_TARGET_MODE TRUE)
        elseif(TARGET hdf5-shared OR TARGET hdf5_cpp-shared)
            set(GRANITE_HDF5_TARGET_MODE TRUE)
        else()
            set(GRANITE_HDF5_TARGET_MODE FALSE)
        endif()
        message(STATUS "[GRANITE] HDF5 ${HDF5_VERSION} | target-mode=${GRANITE_HDF5_TARGET_MODE} | parallel=${HDF5_IS_PARALLEL}")
    else()
        message(FATAL_ERROR
            "[GRANITE] HDF5 not found.\n"
            "  vcpkg:  .\\vcpkg install hdf5:x64-windows\n"
            "  or set HDF5_DIR=<path>/hdf5/share/cmake/hdf5 and re-run CMake.\n"
            "  Disable with -DGRANITE_ENABLE_HDF5=OFF."
        )
    endif()
endif()

# PETSc (for elliptic solvers)
if(GRANITE_ENABLE_PETSC)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PETSC REQUIRED IMPORTED_TARGET PETSc)
    add_definitions(-DGRANITE_USE_PETSC)
    message(STATUS "PETSc enabled: ${PETSC_VERSION}")
endif()

# yaml-cpp (Required for parameter parsing)
# Prefer a system-installed yaml-cpp (e.g. from apt/Homebrew/conda) to avoid
# a slow network fetch during CI. Fall back to FetchContent only if not found.
find_package(yaml-cpp QUIET)
if(NOT yaml-cpp_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG        0.8.0
    )
    set(YAML_CPP_BUILD_TESTS   OFF CACHE BOOL "Disable yaml-cpp tests"   FORCE)
    set(YAML_CPP_BUILD_TOOLS   OFF CACHE BOOL "Disable yaml-cpp tools"   FORCE)
    set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "Disable yaml-cpp contrib" FORCE)
    FetchContent_MakeAvailable(yaml-cpp)
    message(STATUS "yaml-cpp: using FetchContent (system package not found)")
else()
    message(STATUS "yaml-cpp: using system installation (${yaml-cpp_VERSION})")
endif()

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# ---------------------------------------------------------------------------
# Collect all source modules into a static library
# ---------------------------------------------------------------------------
# Core infrastructure
file(GLOB_RECURSE CORE_SOURCES       src/core/*.cpp)
file(GLOB_RECURSE SPACETIME_SOURCES  src/spacetime/*.cpp)
file(GLOB_RECURSE GRMHD_SOURCES     src/grmhd/*.cpp)
file(GLOB_RECURSE RADIATION_SOURCES  src/radiation/*.cpp)
file(GLOB_RECURSE NEUTRINO_SOURCES   src/neutrino/*.cpp)
file(GLOB_RECURSE AMR_SOURCES        src/amr/*.cpp)
file(GLOB_RECURSE INITIAL_SOURCES    src/initial_data/*.cpp)
file(GLOB_RECURSE HORIZON_SOURCES    src/horizon/*.cpp)
file(GLOB_RECURSE IO_SOURCES         src/io/*.cpp)
file(GLOB_RECURSE POSTPROC_SOURCES   src/postprocess/*.cpp)

add_library(granite_lib STATIC
    ${CORE_SOURCES}
    ${SPACETIME_SOURCES}
    ${GRMHD_SOURCES}
    ${RADIATION_SOURCES}
    ${NEUTRINO_SOURCES}
    ${AMR_SOURCES}
    ${INITIAL_SOURCES}
    ${HORIZON_SOURCES}
    ${IO_SOURCES}
    ${POSTPROC_SOURCES}
)

target_include_directories(granite_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link dependencies
if(GRANITE_ENABLE_MPI)
    target_link_libraries(granite_lib PUBLIC MPI::MPI_CXX)
endif()
if(GRANITE_ENABLE_OPENMP)
    target_link_libraries(granite_lib PUBLIC OpenMP::OpenMP_CXX)
endif()
if(GRANITE_ENABLE_HDF5)
    if(GRANITE_HDF5_TARGET_MODE)
        # Prefer imported targets (vcpkg / modern cmake config)
        foreach(_hdf5_tgt hdf5::hdf5-shared hdf5::hdf5_cpp-shared hdf5-shared hdf5_cpp-shared HDF5::HDF5)
            if(TARGET ${_hdf5_tgt})
                target_link_libraries(granite_lib PUBLIC ${_hdf5_tgt})
            endif()
        endforeach()
    else()
        # Legacy fallback via FindHDF5 module variables
        target_link_libraries(granite_lib PUBLIC ${HDF5_LIBRARIES})
        target_include_directories(granite_lib PUBLIC ${HDF5_INCLUDE_DIRS})
    endif()
endif()
if(GRANITE_ENABLE_PETSC)
    target_link_libraries(granite_lib PUBLIC PkgConfig::PETSC)
endif()
target_link_libraries(granite_lib PUBLIC yaml-cpp::yaml-cpp)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(granite_main src/main.cpp)
target_link_libraries(granite_main PRIVATE granite_lib)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(GRANITE_ENABLE_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    include(GoogleTest)
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Coverage report target
# Generates coverage/index.html and coverage.info from a fresh test run.
#
# Prerequisites (Ubuntu/Debian):
#   sudo apt-get install -y lcov
#
# How to use (from the build directory):
#   cmake -B build -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build --target coverage
#
# The resulting coverage.info is the file uploaded to Codecov.
# ---------------------------------------------------------------------------
if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    find_program(LCOV_EXECUTABLE    lcov    REQUIRED HINTS /usr/bin /usr/local/bin)
    find_program(GENHTML_EXECUTABLE genhtml REQUIRED HINTS /usr/bin /usr/local/bin)

    # ── Detect the gcov tool that matches the active compiler ────────────
    # lcov must use the same gcov version the compiler embedded in .gcno files.
    # On Ubuntu systems with multiple GCC versions, /usr/bin/gcov may point to
    # a different version than the compiler, causing an "incompatible versions"
    # error.  We prefer gcov-<MAJOR> (e.g. gcov-12) over the generic gcov.
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        string(REGEX MATCH "^[0-9]+" _GCC_MAJOR "${CMAKE_CXX_COMPILER_VERSION}")
        find_program(GCOV_EXECUTABLE
            NAMES gcov-${_GCC_MAJOR} gcov
            HINTS /usr/bin /usr/local/bin
        )
        if(NOT GCOV_EXECUTABLE)
            set(GCOV_EXECUTABLE gcov)
        endif()
        message(STATUS "[GRANITE] Coverage: using gcov tool → ${GCOV_EXECUTABLE}")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # For Clang we need a thin wrapper: `llvm-cov gcov`
        # Create a small shell script at configure time so lcov can call it.
        set(GCOV_WRAPPER_SCRIPT "${CMAKE_BINARY_DIR}/llvm-cov-wrapper.sh")
        find_program(_LLVM_COV llvm-cov HINTS /usr/bin /usr/local/bin)
        if(_LLVM_COV)
            file(WRITE "${GCOV_WRAPPER_SCRIPT}"
                "#!/bin/sh\nexec \"${_LLVM_COV}\" gcov \"$@\"\n")
            # Make executable at build time (file(WRITE) doesn't chmod)
            file(CHMOD "${GCOV_WRAPPER_SCRIPT}" PERMISSIONS
                OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE)
            set(GCOV_EXECUTABLE "${GCOV_WRAPPER_SCRIPT}")
            message(STATUS "[GRANITE] Coverage: using llvm-cov wrapper → ${GCOV_EXECUTABLE}")
        else()
            set(GCOV_EXECUTABLE gcov)
            message(WARNING "[GRANITE] llvm-cov not found; falling back to gcov")
        endif()
    endif()

    # Absolute path to the granite_tests binary (set by add_subdirectory(tests))
    set(COVERAGE_TEST_BINARY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/granite_tests")

    add_custom_target(coverage
        # ── Step 1: Reset all counters to zero ────────────────────────────
        COMMAND ${LCOV_EXECUTABLE}
                --gcov-tool ${GCOV_EXECUTABLE}
                --zerocounters
                --directory ${CMAKE_BINARY_DIR}

        # ── Step 2: Run the test suite ────────────────────────────────────
        COMMAND ${COVERAGE_TEST_BINARY}

        # ── Step 3: Capture coverage data ─────────────────────────────────
        # --ignore-errors mismatch: suppresses "mismatched end line" errors
        #   that arise from C++ templates, macros, and inlined functions whose
        #   line numbers geninfo cannot perfectly reconcile with the source.
        # --ignore-errors unused:   suppresses warnings about .gcno files with
        #   no matching .gcda (e.g. files never reached by any test).
        COMMAND ${LCOV_EXECUTABLE}
                --gcov-tool ${GCOV_EXECUTABLE}
                --capture
                --directory ${CMAKE_BINARY_DIR}
                --output-file coverage.info
                --ignore-errors mismatch,unused

        # ── Step 4: Strip third-party & system headers ────────────────────
        #   Remove Google Test internals, system stdlib, and build-time
        #   FetchContent copies from the report.
        COMMAND ${LCOV_EXECUTABLE}
                --gcov-tool ${GCOV_EXECUTABLE}
                --remove coverage.info
                    '/usr/*'
                    '${CMAKE_BINARY_DIR}/_deps/*'
                    '*/googletest/*'
                --output-file coverage.info
                --ignore-errors unused,mismatch

        # ── Step 5: Generate HTML report ──────────────────────────────────
        COMMAND ${GENHTML_EXECUTABLE}
                coverage.info
                --output-directory coverage
                --title "GRANITE Coverage Report"
                --legend
                --show-details

        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS granite_tests
        COMMENT
            "[GRANITE] Generating code coverage report → "
            "${CMAKE_BINARY_DIR}/coverage/index.html"
        VERBATIM
    )

    message(STATUS
        "[GRANITE] 'coverage' target registered — "
        "run: cmake --build build --target coverage"
    )
endif()

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------
install(TARGETS granite_main granite_lib
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/granite DESTINATION include)
install(DIRECTORY benchmarks DESTINATION share/granite)

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=====================================================")
message(STATUS "  GRANITE Build Configuration  v${PROJECT_VERSION}")
message(STATUS "=====================================================")
message(STATUS "  Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ compiler:     ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Platform:         ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  MPI:              ${GRANITE_ENABLE_MPI}")
message(STATUS "  OpenMP:           ${GRANITE_ENABLE_OPENMP} (ver ${OpenMP_CXX_VERSION})")
message(STATUS "  GPU:              ${GRANITE_ENABLE_GPU} (${GRANITE_GPU_BACKEND})")
message(STATUS "  HDF5:             ${GRANITE_ENABLE_HDF5} (${HDF5_VERSION})")
message(STATUS "  PETSc:            ${GRANITE_ENABLE_PETSC}")
message(STATUS "  Tests:            ${GRANITE_ENABLE_TESTS}")
message(STATUS "  Coverage:         ${ENABLE_COVERAGE}")
message(STATUS "  Double precision: ${GRANITE_DOUBLE_PRECISION}")
if(MSVC)
message(STATUS "  MSVC Release:     /Ox /arch:AVX2 /fp:fast /GL /openmp:llvm")
endif()
message(STATUS "=====================================================")
message(STATUS "")
