#!/bin/bash
# =============================================================================
#  slurm_submit.sh — GRANITE SLURM Submission Template
# =============================================================================
#
#  Designed for:
#    NERSC Perlmutter  (Cray EX, AMD EPYC 7763, NVIDIA A100)
#    ARCHER2           (HPE Cray EX, AMD EPYC 7742)
#    Jülich JURECA-DC  (AMD EPYC 7742, NVIDIA A100)
#
#  Usage
#  -----
#    1. Edit the "USER CONFIGURATION" section below.
#    2. Submit:  sbatch slurm_submit.sh
#
#  Scaling modes
#  -------------
#    Strong scaling : Keep problem size fixed, vary node count → speedup.
#    Weak scaling   : Scale problem domain proportionally with nodes.
#
#    For strong scaling: fix BENCHMARK, increase SLURM_NODES.
#    For weak scaling  : create larger param files with AMR level or domain
#                        extent that increases proportionally with rank count.
#
# =============================================================================

# ─────────────────────────── SLURM DIRECTIVES ────────────────────────────────

#SBATCH --job-name=granite_scaling
#SBATCH --account=CHANGE_ME                 # HPC project/allocation code
#SBATCH --partition=regular                 # Queue: regular | debug | gpu
#SBATCH --nodes=4                           # Number of compute nodes
#SBATCH --ntasks-per-node=4                 # MPI ranks per node
#SBATCH --cpus-per-task=16                  # OpenMP threads per rank
#SBATCH --mem=240G                          # Memory per node (Perlmutter: 256G)
#SBATCH --time=04:00:00                     # Wall-clock limit (HH:MM:SS)
#SBATCH --output=logs/granite_%j.out        # stdout log (%j = SLURM job ID)
#SBATCH --error=logs/granite_%j.err         # stderr log
#SBATCH --mail-type=BEGIN,END,FAIL
#SBATCH --mail-user=CHANGE_ME@institution.edu

# Optional GPU acceleration (uncomment for CUDA build):
##SBATCH --gpus-per-task=1
##SBATCH --constraint=gpu

# =============================================================================
# USER CONFIGURATION — edit these variables only
# =============================================================================

# ── GRANITE paths ──────────────────────────────────────────────────────────
GRANITE_ROOT="/path/to/GRANITE"                        # Absolute path to repo root
GRANITE_BIN="${GRANITE_ROOT}/build/bin/granite_main"   # Built binary
BENCHMARK="${GRANITE_ROOT}/benchmarks/B2_eq/params.yaml"
HPC_WRAPPER="${GRANITE_ROOT}/scripts/run_granite_hpc.py"

# ── Output / telemetry ───────────────────────────────────────────────────
SCRATCH_DIR="/scratch/${USER}/granite_${SLURM_JOB_ID}"
LOG_DIR="${SCRATCH_DIR}/logs"
AMR_TELEMETRY="${SCRATCH_DIR}/amr_telemetry_${SLURM_JOB_ID}.jsonl"

# ── Thread / NUMA control ────────────────────────────────────────────────
# By default, matches SLURM allocation exactly.  Override to force values.
OMP_THREADS="${SLURM_CPUS_PER_TASK}"   # = --cpus-per-task
DISABLE_NUMA=false                      # true → clears all NUMA affinity hooks
NUMACTL_ARGS="--interleave=all"         # ignored when DISABLE_NUMA=true

# =============================================================================
# JOB SETUP
# =============================================================================

set -euo pipefail

echo "======================================================================"
echo "  GRANITE HPC JOB — SLURM"
echo "  Job ID    : ${SLURM_JOB_ID}"
echo "  Nodes     : ${SLURM_JOB_NUM_NODES}"
echo "  MPI ranks : $((SLURM_JOB_NUM_NODES * SLURM_NTASKS_PER_NODE))"
echo "  OMP/rank  : ${OMP_THREADS}"
echo "  Benchmark : ${BENCHMARK}"
echo "  Scratch   : ${SCRATCH_DIR}"
echo "  AMR log   : ${AMR_TELEMETRY}"
echo "  Timestamp : $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
echo "======================================================================"

mkdir -p "${LOG_DIR}" "${SCRATCH_DIR}"

# ── Module loading (site-specific — uncomment/modify as needed) ─────────────
# NERSC Perlmutter:
# module load PrgEnv-gnu cray-mpich hdf5 cmake/3.24
#
# ARCHER2:
# module load gcc/11.2.0 cray-mpich/8.1.23 cray-hdf5/1.12.1.1 cmake/3.23.2
#
# Jülich JURECA-DC:
# module load GCC OpenMPI HDF5 CMake

# Verify binary
if [[ ! -x "${GRANITE_BIN}" ]]; then
    echo "ERROR: Binary not found or not executable: ${GRANITE_BIN}"
    echo "       Build GRANITE first:"
    echo "         mkdir -p ${GRANITE_ROOT}/build && cd ${GRANITE_ROOT}/build"
    echo "         cmake .. -DCMAKE_BUILD_TYPE=Release"
    echo "         make -j\$(nproc)"
    exit 1
fi

# =============================================================================
# ENVIRONMENT SETUP
# =============================================================================

TOTAL_MPI_RANKS=$((SLURM_JOB_NUM_NODES * SLURM_NTASKS_PER_NODE))

export OMP_NUM_THREADS="${OMP_THREADS}"
export OMP_PLACES="cores"
export OMP_PROC_BIND="close"

if [[ "${DISABLE_NUMA}" == "true" ]]; then
    NUMA_FLAG="--disable-numa-bind"
    export OMP_PROC_BIND="false"
    echo "INFO: NUMA binding explicitly disabled."
else
    NUMA_FLAG="--numactl-args ${NUMACTL_ARGS}"
    echo "INFO: NUMA binding enabled (${NUMACTL_ARGS})"
fi

# =============================================================================
# LAUNCH
# =============================================================================

echo ""
echo ">>> Launching GRANITE at $(date -u '+%H:%M:%S UTC') ..."
echo ""

srun --ntasks="${TOTAL_MPI_RANKS}"                \
     --ntasks-per-node="${SLURM_NTASKS_PER_NODE}" \
     --cpus-per-task="${SLURM_CPUS_PER_TASK}"     \
  python3 "${HPC_WRAPPER}"                        \
      "${GRANITE_BIN}"                            \
      "${BENCHMARK}"                              \
      --omp-threads "${OMP_THREADS}"              \
      --mpi-ranks "${TOTAL_MPI_RANKS}"            \
      --mpi-launcher srun                         \
      --mpi-extra-args "--ntasks=${TOTAL_MPI_RANKS} --ntasks-per-node=${SLURM_NTASKS_PER_NODE}" \
      ${NUMA_FLAG}                                \
      --amr-telemetry-file "${AMR_TELEMETRY}"

EXITCODE=$?

# =============================================================================
# POST-RUN SUMMARY
# =============================================================================

echo ""
echo "======================================================================"
echo "  GRANITE JOB COMPLETE"
echo "  Exit code : ${EXITCODE}"
echo "  Wall used : $((SECONDS / 3600))h $(((SECONDS % 3600) / 60))m $((SECONDS % 60))s"
echo "  AMR log   : ${AMR_TELEMETRY}"
echo "  Logs in   : ${LOG_DIR}"
echo "======================================================================"

if [[ -f "${AMR_TELEMETRY}" ]]; then
    N_RECORDS=$(grep -c '"wall_s"' "${AMR_TELEMETRY}" || true)
    echo "  AMR subcycle events recorded: ${N_RECORDS}"
fi

exit "${EXITCODE}"
