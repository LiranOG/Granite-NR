"""
granite_analysis.plotting — Publication-quality plotting utilities.
"""

import numpy as np
import matplotlib.pyplot as plt
from matplotlib import rcParams
from typing import Optional, Tuple, List


def set_granite_style():
    """Set matplotlib style for GRANITE publications."""
    rcParams.update({
        "font.family": "serif",
        "font.serif": ["Computer Modern Roman", "Times New Roman"],
        "font.size": 12,
        "axes.labelsize": 14,
        "axes.titlesize": 14,
        "legend.fontsize": 11,
        "xtick.labelsize": 12,
        "ytick.labelsize": 12,
        "figure.figsize": (8, 6),
        "figure.dpi": 150,
        "savefig.dpi": 300,
        "savefig.bbox": "tight",
        "text.usetex": False,
        "axes.grid": True,
        "grid.alpha": 0.3,
        "lines.linewidth": 1.5,
    })


class GranitePlotter:
    """
    Publication-quality plotting for GRANITE simulation output.

    Usage:
        plotter = GranitePlotter()
        plotter.plot_strain(times, h22, title="GW Strain")
        plotter.plot_constraints(times, ham_l2, mom_l2)
        plotter.save("figure.pdf")
    """

    COLORS = {
        "primary": "#7c3aed",
        "secondary": "#14b8a6",
        "accent": "#f59e0b",
        "danger": "#f43f5e",
        "blue": "#3b82f6",
        "gray": "#94a3b8",
    }

    def __init__(self):
        set_granite_style()

    def plot_strain(self, times: np.ndarray, h: np.ndarray,
                    mode: str = "(2,2)", title: Optional[str] = None,
                    ax: Optional[plt.Axes] = None) -> plt.Axes:
        """Plot gravitational wave strain h₊ and h×."""
        if ax is None:
            fig, ax = plt.subplots()

        ax.plot(times, np.real(h), color=self.COLORS["primary"],
                label=rf"$h_+^{{{mode}}}$")
        ax.plot(times, np.imag(h), color=self.COLORS["secondary"],
                label=rf"$h_\times^{{{mode}}}$", linestyle="--")
        ax.set_xlabel(r"$t / M$")
        ax.set_ylabel(r"$r \cdot h$")
        ax.legend()
        if title:
            ax.set_title(title)
        return ax

    def plot_constraints(self, times: np.ndarray,
                         hamiltonian: np.ndarray,
                         momentum: Optional[np.ndarray] = None,
                         ax: Optional[plt.Axes] = None) -> plt.Axes:
        """Plot constraint violation norms over time."""
        if ax is None:
            fig, ax = plt.subplots()

        ax.semilogy(times, hamiltonian, color=self.COLORS["danger"],
                     label=r"$\|H\|_2$")
        if momentum is not None:
            ax.semilogy(times, momentum, color=self.COLORS["blue"],
                         label=r"$\|M^i\|_2$")
        ax.set_xlabel(r"$t / M$")
        ax.set_ylabel("Constraint violation")
        ax.legend()
        return ax

    def plot_horizon_mass(self, times: np.ndarray,
                          masses: List[np.ndarray],
                          labels: Optional[List[str]] = None,
                          ax: Optional[plt.Axes] = None) -> plt.Axes:
        """Plot apparent horizon masses over time."""
        if ax is None:
            fig, ax = plt.subplots()

        colors = [self.COLORS["primary"], self.COLORS["secondary"],
                  self.COLORS["accent"], self.COLORS["danger"],
                  self.COLORS["blue"]]

        for i, m in enumerate(masses):
            label = labels[i] if labels else f"BH {i+1}"
            ax.plot(times[:len(m)], m, color=colors[i % len(colors)],
                    label=label)

        ax.set_xlabel(r"$t / M$")
        ax.set_ylabel(r"$M_{\rm irr} / M_\odot$")
        ax.legend()
        return ax

    def plot_energy_spectrum(self, freqs: np.ndarray, dEdf: np.ndarray,
                             ax: Optional[plt.Axes] = None) -> plt.Axes:
        """Plot GW energy spectrum dE/df."""
        if ax is None:
            fig, ax = plt.subplots()

        ax.loglog(freqs, dEdf, color=self.COLORS["primary"])
        ax.set_xlabel(r"$f$ [Hz]")
        ax.set_ylabel(r"$dE/df$ [erg/Hz]")
        ax.set_title("Gravitational Wave Energy Spectrum")
        return ax

    def plot_convergence(self, dx_values: np.ndarray,
                         errors: np.ndarray,
                         expected_order: float = 4.0,
                         ax: Optional[plt.Axes] = None) -> plt.Axes:
        """Plot convergence test: error vs resolution."""
        if ax is None:
            fig, ax = plt.subplots()

        ax.loglog(dx_values, errors, "o-", color=self.COLORS["primary"],
                  label="Measured")

        # Reference slope
        ref = errors[0] * (dx_values / dx_values[0])**expected_order
        ax.loglog(dx_values, ref, "--", color=self.COLORS["gray"],
                  label=f"Order {expected_order:.0f}")

        ax.set_xlabel(r"$\Delta x$")
        ax.set_ylabel("Error norm")
        ax.legend()
        ax.set_title("Convergence Test")
        return ax

    @staticmethod
    def save(filename: str, fig: Optional[plt.Figure] = None):
        """Save current or specified figure."""
        if fig is None:
            plt.savefig(filename)
        else:
            fig.savefig(filename)
        print(f"Saved: {filename}")
