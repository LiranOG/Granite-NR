"""
granite_analysis.reader — HDF5 dataset reader for GRANITE output.
"""

import h5py
import numpy as np
from pathlib import Path
from typing import Optional, List, Dict, Tuple


class GraniteDataset:
    """
    Reader for GRANITE HDF5 output files.

    Usage:
        ds = GraniteDataset("output/")
        ds.list_timesteps()
        rho = ds.read_field("rho", step=100)
        ts = ds.read_timeseries("constraints/hamiltonian_l2")
    """

    def __init__(self, output_dir: str):
        self.output_dir = Path(output_dir)
        self._index: Optional[Dict] = None

    def list_timesteps(self) -> List[int]:
        """List all available output timestep indices."""
        files = sorted(self.output_dir.glob("output_*.h5"))
        return [int(f.stem.split("_")[1]) for f in files]

    def list_checkpoints(self) -> List[int]:
        """List all available checkpoint indices."""
        files = sorted(self.output_dir.glob("checkpoint_*.h5"))
        return [int(f.stem.split("_")[1]) for f in files]

    def read_field(self, variable: str, step: int,
                   level: int = 0, block: int = 0) -> np.ndarray:
        """
        Read a 3D field variable at a given timestep.

        Parameters
        ----------
        variable : str
            Variable name (e.g., 'chi', 'rho', 'lapse', 'Bx')
        step : int
            Timestep index
        level : int
            AMR refinement level
        block : int
            Block index within the level

        Returns
        -------
        np.ndarray
            3D array of field values
        """
        filename = self.output_dir / f"output_{step:06d}.h5"
        with h5py.File(filename, "r") as f:
            group = f[f"level_{level}/block_{block}"]
            return np.array(group[variable])

    def read_metadata(self, step: int) -> Dict:
        """Read simulation metadata (time, parameters) for a timestep."""
        filename = self.output_dir / f"output_{step:06d}.h5"
        with h5py.File(filename, "r") as f:
            meta = {}
            for key in f.attrs:
                meta[key] = f.attrs[key]
            return meta

    def read_timeseries(self, dataset_name: str) -> Tuple[np.ndarray, np.ndarray]:
        """
        Read a 1D time series (e.g., constraint norms, GW modes).

        Returns (times, values) arrays.
        """
        filename = self.output_dir / "timeseries.h5"
        with h5py.File(filename, "r") as f:
            data = np.array(f[dataset_name])
            return data[:, 0], data[:, 1]

    def read_horizon(self, step: int, horizon_id: int = 0) -> Dict:
        """Read apparent horizon diagnostics for a timestep."""
        filename = self.output_dir / f"output_{step:06d}.h5"
        with h5py.File(filename, "r") as f:
            group = f[f"horizons/horizon_{horizon_id}"]
            return {
                "area": float(group.attrs["area"]),
                "mass": float(group.attrs["irreducible_mass"]),
                "spin": float(group.attrs["spin"]),
                "centroid": np.array(group.attrs["centroid"]),
            }

    def grid_info(self, step: int, level: int = 0, block: int = 0) -> Dict:
        """Read grid metadata (domain, spacing, etc.)."""
        filename = self.output_dir / f"output_{step:06d}.h5"
        with h5py.File(filename, "r") as f:
            group = f[f"level_{level}/block_{block}"]
            return {
                "lo": np.array(group.attrs["lower_corner"]),
                "hi": np.array(group.attrs["upper_corner"]),
                "dx": np.array(group.attrs["cell_spacing"]),
                "ncells": np.array(group.attrs["ncells"]),
            }
