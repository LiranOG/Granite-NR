"""
granite_analysis — Python analysis package for GRANITE simulations.

Provides tools for reading HDF5 output, extracting gravitational-wave
modes, computing strain, plotting, and generating publication-quality
figures.
"""

__version__ = "0.6.7"
__author__ = "Liran M. Schwartz"

from granite_analysis.reader import GraniteDataset
from granite_analysis.gw import Psi4Analysis, compute_strain
from granite_analysis.plotting import GranitePlotter

__all__ = [
    "GraniteDataset",
    "Psi4Analysis",
    "compute_strain",
    "GranitePlotter",
]
