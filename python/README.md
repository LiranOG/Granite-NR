# GRANITE – Python Analysis Suite (python/)

> [!NOTE]
> **Welcome to the Data Analysis Framework.** This directory houses `granite_analysis`, a professional Python package designed exclusively for parsing, reducing, and visualizing HDF5 and ASCII telemetry output from the GRANITE C++ engine.

## Directory Overview

| Component | Responsibility |
|-----------|----------------|
| `granite_analysis/` | The core Python module containing data loaders, GW extraction math, and plotting utilities. |
| `setup.py` | Legacy setup script (use `pip install -e .` from root via `pyproject.toml`). |
| `requirements.txt` | Core scientific Python dependencies (NumPy, SciPy, Matplotlib, h5py). |

## Directory Structure

```text
python/
├── granite_analysis/             # Main analysis package
│   ├── __init__.py
│   ├── gw.py                     # Newman-Penrose Psi4 extraction logic
│   ├── hdf5_loader.py            # Grid and constraint data parser
│   └── plotters.py               # 1D/2D visualization wrappers
│
├── requirements.txt              # Standard dependencies
└── setup.py                      # Deprecated in favor of root pyproject.toml
```

> [!IMPORTANT]
> **Installation:** Do not install this package by directly calling `setup.py`. From the root directory, execute: `pip install -e .` to leverage the modern `pyproject.toml` configuration.
