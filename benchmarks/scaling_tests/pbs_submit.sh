#!/bin/bash
# =============================================================================
#  pbs_submit.sh — GRANITE PBS/Torque Submission Template
# =============================================================================
#
#  Designed for:
#    Legacy PBS/Torque clusters
#    PBS Professional (PBSPro) deployments
#    OpenPBS (NCSA Blue Waters era style)
#
#  Usage
#  -----
#    1. Edit the "USER CONFIGURATION" section below.
#    2. Submit:  qsub pbs_submit.sh
#
# =============================================================================

# ─────────────────────────── PBS DIRECTIVES ──────────────────────────────────

#PBS -N granite_scaling
#PBS -P CHANGE_ME                           # Project / allocation code
#PBS -q normal                              # Queue name
#PBS -l nodes=4:ppn=16                      # 4 nodes, 16 processors per node
#PBS -l walltime=04:00:00                   # Wall-clock limit (HH:MM:SS)
#PBS -l mem=240gb                           # Memory per node
#PBS -j oe                                  # Merge stdout + stderr
#PBS -o logs/granite_${PBS_JOBID}.log
#PBS -m abe                                 # Mail on Abort, Begin, End
#PBS -M CHANGE_ME@institution.edu

# =============================================================================
# USER CONFIGURATION — edit these variables only
# =============================================================================

GRANITE_ROOT="/path/to/GRANITE"
GRANITE_BIN="${GRANITE_ROOT}/build/bin/granite_main"
BENCHMARK="${GRANITE_ROOT}/benchmarks/B2_eq/params.yaml"
HPC_WRAPPER="${GRANITE_ROOT}/scripts/run_granite_hpc.py"

SCRATCH_DIR="/scratch/${USER}/granite_${PBS_JOBID}"
LOG_DIR="${SCRATCH_DIR}/logs"
AMR_TELEMETRY="${SCRATCH_DIR}/amr_telemetry_${PBS_JOBID}.jsonl"

# ── Thread / NUMA control ────────────────────────────────────────────────
OMP_THREADS=16           # Should match ppn / MPI_RANKS_PER_NODE
MPI_RANKS_PER_NODE=4     # Sub-node MPI decomposition
DISABLE_NUMA=false

# =============================================================================
# JOB SETUP
# =============================================================================

set -euo pipefail
cd "${PBS_O_WORKDIR}"

NODE_COUNT=$(sort -u "${PBS_NODEFILE}" | wc -l)
TOTAL_MPI_RANKS=$((NODE_COUNT * MPI_RANKS_PER_NODE))

echo "======================================================================"
echo "  GRANITE HPC JOB — PBS/Torque"
echo "  Job ID    : ${PBS_JOBID}"
echo "  Nodes     : ${NODE_COUNT}"
echo "  MPI ranks : ${TOTAL_MPI_RANKS}"
echo "  OMP/rank  : ${OMP_THREADS}"
echo "  Benchmark : ${BENCHMARK}"
echo "  Scratch   : ${SCRATCH_DIR}"
echo "  AMR log   : ${AMR_TELEMETRY}"
echo "  Timestamp : $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
echo "======================================================================"

mkdir -p "${LOG_DIR}" "${SCRATCH_DIR}"

# ── Module loading (site-specific) ───────────────────────────────────────────
# module load gcc openmpi hdf5 cmake
# (uncomment and modify for your cluster's module system)

if [[ ! -x "${GRANITE_BIN}" ]]; then
    echo "ERROR: Binary not found: ${GRANITE_BIN}"
    exit 1
fi

# =============================================================================
# ENVIRONMENT SETUP
# =============================================================================

export OMP_NUM_THREADS="${OMP_THREADS}"
export OMP_PLACES="cores"
export OMP_PROC_BIND="close"

if [[ "${DISABLE_NUMA}" == "true" ]]; then
    NUMA_FLAG="--disable-numa-bind"
    export OMP_PROC_BIND="false"
    echo "INFO: NUMA binding explicitly disabled."
else
    NUMA_FLAG="--numactl-args --interleave=all"
    echo "INFO: NUMA binding enabled (--interleave=all)"
fi

# =============================================================================
# LAUNCH
# =============================================================================

echo ""
echo ">>> Launching GRANITE at $(date -u '+%H:%M:%S UTC') ..."
echo ""

mpirun -np "${TOTAL_MPI_RANKS}"                \
       -machinefile "${PBS_NODEFILE}"           \
       --map-by ppr:${MPI_RANKS_PER_NODE}:node \
  python3 "${HPC_WRAPPER}"                      \
      "${GRANITE_BIN}"                          \
      "${BENCHMARK}"                            \
      --omp-threads "${OMP_THREADS}"            \
      --mpi-ranks "${TOTAL_MPI_RANKS}"          \
      --mpi-launcher mpirun                     \
      ${NUMA_FLAG}                              \
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
echo "======================================================================"

if [[ -f "${AMR_TELEMETRY}" ]]; then
    N_RECORDS=$(grep -c '"wall_s"' "${AMR_TELEMETRY}" || true)
    echo "  AMR subcycle events recorded: ${N_RECORDS}"
fi

exit "${EXITCODE}"
