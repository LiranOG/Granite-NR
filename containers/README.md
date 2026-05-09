# GRANITE – Container Infrastructure (containers/)

> [!NOTE]
> **Welcome to the Deployment Hub.** This directory contains the Docker and Apptainer/Singularity configuration files required to build, test, and deploy the GRANITE engine in fully isolated, reproducible environments.

## Directory Overview

| Component | Responsibility |
|-----------|----------------|
| `Dockerfile` | The master container definition. Provisions an Ubuntu 22.04 base, installs OpenMPI and HDF5, and compiles the engine. |
| `granite.def` | Apptainer/Singularity definition file for secure deployment on HPC clusters. |

## Directory Structure

```text
containers/
├── Dockerfile                    # Multi-stage Docker build definition
├── granite.def                   # Apptainer/Singularity definition
└── README.md                     # You are here
```

> [!WARNING]
> **Performance Note:** While containers guarantee environmental reproducibility, native execution via WSL2 or a bare-metal Linux installation is strongly recommended for production runs to avoid containerization I/O and MPI networking overhead.
