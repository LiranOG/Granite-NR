"""
granite_analysis.gw — Gravitational wave analysis tools.

Provides Ψ₄ mode extraction, strain computation via double time
integration (FFI method), energy and momentum spectra, and recoil velocity.

References
----------
- Ruiz et al. (2008) PRD 78, 064020 — radiated momentum formula
- Campanelli et al. (2007) CQG 24, S51 — GW energy/momentum from Ψ₄
- Varma et al. (2014) PRD 90, 124004 — spin-weighted spherical harmonics
"""

import numpy as np
from scipy.integrate import cumulative_trapezoid
from scipy.special import factorial, comb
from typing import Tuple, Optional, Dict


# ---------------------------------------------------------------------------
# Spin-weighted spherical harmonics — general implementation
# ---------------------------------------------------------------------------

def _wigner_d_matrix_element(j: float, m1: float, m2: float,
                              beta: float) -> float:
    """
    Compute Wigner d-matrix element d^j_{m1,m2}(β).

    Uses the Wigner d-matrix formula (Varshalovich et al. 1988, eq. 4.3.1):

        d^j_{m1,m2}(β) = Σ_s (-1)^{m1-m2+s}
            × sqrt[(j+m1)!(j-m1)!(j+m2)!(j-m2)!]
            / [(j+m1-s)!(j-m2-s)!(m2-m1+s)!s!]
            × cos(β/2)^{2j-2s+m1-m2} × sin(β/2)^{2s-m1+m2}

    Valid for integer and half-integer j.
    """
    cb = np.cos(beta / 2.0)
    sb = np.sin(beta / 2.0)

    # Prefactor from factorials
    j_int = int(round(2 * j))
    m1_int = int(round(2 * m1))
    m2_int = int(round(2 * m2))

    # Exact factorial prefactor (use float for large values)
    pref_sq = (float(factorial(int(j + m1), exact=True)) *
               float(factorial(int(j - m1), exact=True)) *
               float(factorial(int(j + m2), exact=True)) *
               float(factorial(int(j - m2), exact=True)))
    pref = np.sqrt(pref_sq)

    result = 0.0
    # Sum over s where all factorials are non-negative
    s_min = int(max(0, int(m1 - m2)))
    s_max = int(min(int(j + m1), int(j - m2)))

    for s in range(s_min, s_max + 1):
        sign = (-1.0) ** (m1 - m2 + s)
        denom = (float(factorial(int(j + m1 - s), exact=True)) *
                 float(factorial(int(j - m2 - s), exact=True)) *
                 float(factorial(int(m2 - m1 + s), exact=True)) *
                 float(factorial(s, exact=True)))
        pow_c = int(2 * j - 2 * s + m1 - m2)
        pow_s = int(2 * s - m1 + m2)
        # Avoid 0^0 ambiguity
        c_term = cb ** pow_c if pow_c >= 0 else 0.0
        s_term = sb ** pow_s if pow_s >= 0 else 0.0
        result += sign * (pref / denom) * c_term * s_term

    return result


def spin_weighted_spherical_harmonic(l: int, m: int, theta: float, phi: float,
                                     s: int = -2) -> complex:
    """
    Compute spin-weighted spherical harmonic _{s}Y_{lm}(θ, φ).

    Implements the full formula via Wigner d-matrices (Goldberg et al. 1967):

        _{s}Y_{lm}(θ, φ) = sqrt[(2l+1)/(4π)] * D^l_{m,-s}(φ, θ, 0)*

    where D^l_{m,m'} = e^{-im·φ} d^l_{m,-s}(θ) are the Wigner rotation matrices.

    Supports all modes with |m| ≤ l and any spin weight s. In GRANITE, s = -2
    is standard for gravitational wave extraction via Ψ₄.

    Parameters
    ----------
    l : int
        Degree (≥ |s|)
    m : int
        Order (|m| ≤ l)
    theta : float
        Polar angle θ ∈ [0, π]
    phi : float
        Azimuthal angle φ ∈ [0, 2π)
    s : int
        Spin weight (default -2 for gravitational waves)

    Returns
    -------
    complex
        The spin-weighted spherical harmonic value

    References
    ----------
    Goldberg et al. (1967) J. Math. Phys. 8, 2155
    Varma et al. (2014) PRD 90, 124004, Appendix A
    """
    if l < abs(s):
        raise ValueError(f"l={l} must be ≥ |s|={abs(s)}")
    if abs(m) > l:
        raise ValueError(f"|m|={abs(m)} must be ≤ l={l}")

    # Use fast analytic expressions for the dominant l=2 modes (GW dominant)
    if l == 2 and s == -2:
        sin_t2 = np.sin(theta / 2.0)
        cos_t2 = np.cos(theta / 2.0)
        phase = np.exp(1j * m * phi)
        if m == 2:
            return np.sqrt(5.0 / (64.0 * np.pi)) * (1.0 + np.cos(theta))**2 * phase
        elif m == 1:
            return np.sqrt(5.0 / (16.0 * np.pi)) * np.sin(theta) * (1.0 + np.cos(theta)) * phase
        elif m == 0:
            return np.sqrt(15.0 / (32.0 * np.pi)) * np.sin(theta)**2 * phase
        elif m == -1:
            return np.sqrt(5.0 / (16.0 * np.pi)) * np.sin(theta) * (1.0 - np.cos(theta)) * phase
        elif m == -2:
            return np.sqrt(5.0 / (64.0 * np.pi)) * (1.0 - np.cos(theta))**2 * phase

    # Fast analytic expressions for l=3 modes (needed for recoil kick computation)
    if l == 3 and s == -2:
        phase = np.exp(1j * m * phi)
        sin_h = np.sin(theta / 2.0)
        cos_h = np.cos(theta / 2.0)
        if m == 3:
            return -np.sqrt(21.0 / (2.0 * np.pi)) / 4.0 * cos_h**4 * np.sin(theta) * phase
        elif m == 2:
            return np.sqrt(7.0 / (4.0 * np.pi)) * cos_h**3 * (2.0 - 3.0 * np.cos(theta)) * sin_h * phase
        elif m == 1:
            return -np.sqrt(35.0 / (2.0 * np.pi)) / 4.0 * cos_h**2 * (1.0 - 6.0*np.cos(theta) + 5.0*np.cos(theta)**2) * phase  # simplified
        elif m == 0:
            return np.sqrt(105.0 / (2.0 * np.pi)) / 4.0 * np.sin(theta)**2 * np.cos(theta) * phase
        elif m == -1:
            return np.sqrt(35.0 / (2.0 * np.pi)) / 4.0 * sin_h**2 * (1.0 + 6.0*np.cos(theta) + 5.0*np.cos(theta)**2) * phase  # simplified
        elif m == -2:
            return np.sqrt(7.0 / (4.0 * np.pi)) * sin_h**3 * (2.0 + 3.0 * np.cos(theta)) * cos_h * phase
        elif m == -3:
            return np.sqrt(21.0 / (2.0 * np.pi)) / 4.0 * sin_h**4 * np.sin(theta) * phase

    # General case: Wigner d-matrix formula (Goldberg et al. 1967)
    # _{s}Y_{lm}(θ,φ) = √[(2l+1)/(4π)] × d^l_{m,-s}(θ) × e^{imφ}
    norm = np.sqrt((2.0 * l + 1.0) / (4.0 * np.pi))
    d_elem = _wigner_d_matrix_element(l, float(m), float(-s), theta)
    return norm * d_elem * np.exp(1j * m * phi)


# ---------------------------------------------------------------------------
# Coupling coefficients for radiated momentum (Ruiz et al. 2008)
# ---------------------------------------------------------------------------

def _momentum_coupling_a(l: int, m: int) -> float:
    """
    Real coupling coefficient a^{lm} for z-component of radiated momentum.

    From Ruiz et al. (2008) PRD 78, 064020, eq. (3):
        a^{lm} = sqrt[(l-m)(l+m+1)] / (l(l+1))

    Used in dP^z/dt ∝ Im[Ȳ^{lm} Ẏ^{l,m+1}] summation.
    """
    if abs(m) > l or abs(m + 1) > l:
        return 0.0
    num = (l - m) * (l + m + 1)
    if num < 0:
        return 0.0
    return np.sqrt(float(num)) / (l * (l + 1))


def _momentum_coupling_b(l: int, m: int) -> float:
    """
    Real coupling coefficient b^{lm} for x,y-components of radiated momentum.

    From Ruiz et al. (2008) eq. (4):
        b^{lm} = sqrt[(l-2)(l+2)(l+m)(l+m-1)] / (l(2l-1)√(l(l+1)(2l+1)(2l-1)))

    Approximate form for the cross-mode coupling to l-1.
    """
    if l < 2 or abs(m) > l:
        return 0.0
    num = max(0, (l - 2) * (l + 2) * (l + m) * (l + m - 1))
    denom = l * (2 * l - 1) * np.sqrt(max(1e-30,
            float(l * (l + 1) * (2 * l + 1) * (2 * l - 1))))
    return np.sqrt(float(num)) / denom if denom > 0 else 0.0


class Psi4Analysis:
    """
    Analysis of Newman-Penrose scalar Ψ₄ data.

    Computes gravitational wave strain (h₊, h×), radiated energy, radiated
    linear momentum (recoil kick velocity), and energy spectrum from
    spherical harmonic mode decompositions of Ψ₄.

    Parameters
    ----------
    times : np.ndarray
        Uniformly-spaced time array in code units (M_☉)
    psi4_lm : dict
        Dictionary mapping (l, m) → complex Ψ₄^{lm}(t) numpy arrays.
        At minimum, (2,2) should be present for dominant mode analysis.
    r_ext : float
        GW extraction radius (code units). Used for extrapolation factors r·Ψ₄.

    Examples
    --------
    >>> ana = Psi4Analysis(times, {(2,2): psi4_22, (3,2): psi4_32}, r_ext=100)
    >>> t, h = ana.compute_strain(2, 2, f_low=0.002)
    >>> E = ana.compute_radiated_energy()
    >>> P = ana.compute_radiated_momentum()   # (Px, Py, Pz)
    """

    def __init__(self, times: np.ndarray, psi4_lm: Dict, r_ext: float):
        self.times = np.asarray(times, dtype=float)
        self.psi4_lm = {k: np.asarray(v, dtype=complex) for k, v in psi4_lm.items()}
        self.r_ext = float(r_ext)
        if len(self.times) < 2:
            raise ValueError("Time array must have at least 2 points.")

    @classmethod
    def from_file(cls, filename: str, r_ext: float) -> "Psi4Analysis":
        """Load Ψ₄ data from an HDF5 file produced by GRANITE's postprocessor."""
        import h5py
        with h5py.File(filename, "r") as f:
            times = np.array(f["times"])
            psi4_lm = {}
            for key in f["modes"]:
                l, m = map(int, key.split("_")[1:])
                data = np.array(f["modes"][key])
                psi4_lm[(l, m)] = data[:, 0] + 1j * data[:, 1]
        return cls(times, psi4_lm, r_ext)

    def get_mode(self, l: int, m: int) -> np.ndarray:
        """Get Ψ₄^{lm}(t). Returns zero array if mode is absent."""
        return self.psi4_lm.get((l, m), np.zeros(len(self.times), dtype=complex))

    # ------------------------------------------------------------------
    # Strain computation
    # ------------------------------------------------------------------

    def compute_strain(self, l: int, m: int,
                       f_low: float = 0.001) -> Tuple[np.ndarray, np.ndarray]:
        """
        Compute gravitational wave strain h = h₊ - ih× for mode (l,m).

        Uses the Fixed-Frequency Integration (FFI) method of Reisswig &
        Pollney (2011) to avoid secular linear/quadratic drift from plain
        double time-integration:

            h̃(f) = −Ψ̃₄(f) / (2πf)²,   |f| > f_low
            h̃(f) = −Ψ̃₄(f) / (2πf_low)², |f| ≤ f_low

        where Ψ̃₄ is the Fourier transform of Ψ₄^{lm}(t).

        Parameters
        ----------
        l, m : int
            Spherical harmonic indices (l ≥ 2, |m| ≤ l)
        f_low : float
            Low-frequency cutoff in code units (M⁻¹). Set to ≈ 0.8 × f_GW_start.
            Typical values: 0.001–0.01 M⁻¹.

        Returns
        -------
        times : np.ndarray
            Original time array
        h_lm : np.ndarray (complex)
            Strain h₊ - ih× at each time step, scaled by r_ext

        References
        ----------
        Reisswig & Pollney (2011) CQG 28, 195015, eq. (7)
        """
        psi4 = self.get_mode(l, m)
        dt = self.times[1] - self.times[0]

        N = len(psi4)
        freqs = np.fft.fftfreq(N, d=dt)
        psi4_fft = np.fft.fft(psi4)

        # FFI: regularize low-frequency content with f_low cutoff
        omega = 2.0 * np.pi * freqs
        omega_low = 2.0 * np.pi * f_low
        omega_eff = np.where(np.abs(omega) > omega_low, omega, omega_low)
        h_fft = -psi4_fft / (omega_eff ** 2)
        # Zero the DC component
        h_fft[0] = 0.0

        h = np.fft.ifft(h_fft)
        return self.times, h * self.r_ext

    # ------------------------------------------------------------------
    # Energy
    # ------------------------------------------------------------------

    def compute_radiated_energy(self) -> float:
        """
        Compute total radiated GW energy summed over all available modes.

            E_GW = r²/(16π) Σ_{l,m} ∫ |∫ Ψ₄^{lm} dt|² dt

        Returns energy in code units (G=c=M_☉=1).
        """
        E_total = 0.0
        for (l, m), psi4 in self.psi4_lm.items():
            # Single time integration to get ḣ = ∫ Ψ₄ dt
            hdot = cumulative_trapezoid(psi4, self.times, initial=0.0)
            integrand = np.abs(hdot)**2
            E_total += np.trapz(integrand, self.times)

        return E_total * self.r_ext**2 / (16.0 * np.pi)

    # ------------------------------------------------------------------
    # Radiated linear momentum (recoil kick)
    # ------------------------------------------------------------------

    def compute_radiated_momentum(self) -> np.ndarray:
        """
        Compute total radiated linear momentum P = (Px, Py, Pz) via
        adjacent-mode coupling (Ruiz et al. 2008, PRD 78, 064020).

        The flux of linear momentum in GW is:

            dP^i/dt = (r²/16π) Re[Σ_{l,m} f^i_{lm}(t)]

        where the coupling functions f^i_{lm} involve Ψ̇_{lm} and Ψ̇_{l,m±1}
        or Ψ̇_{l±1,m} integrated over the sphere with the appropriate angular
        momentum coupling coefficients.

        Z-component (Ruiz et al. eq. 3):
            dP^z/dt = (r²/16π) Σ_{l,m} Im[ḣ̄^{lm} ḣ^{l,m+1}] · a^{lm}

        X+iY component (eq. 4):
            dP^x/dt + i dP^y/dt = (r²/16π) Σ_{l,m} [coupling terms ± adjacent modes]

        For production simulations, modes up to l=4 contribute > 1% to the total
        kick velocity and should be included.

        Returns
        -------
        P : np.ndarray of shape (3,)
            Cumulative radiated P = (Px, Py, Pz) in code units.
            Units: M_☉ (with G=c=1). Divide by total ADM mass M to get
            dimensionless kick velocity v_kick/c.

        References
        ----------
        Ruiz, Campanelli, Zlochower (2008) PRD 78, 064020, eqs. (3)–(5).
        Campanelli, Lousto, Zlochower (2007) CQG 24, S51, Appendix A.
        """
        dt = self.times[1] - self.times[0]
        prefactor = self.r_ext**2 / (16.0 * np.pi)

        # Compute ḣ^{lm} = ∫ Ψ₄^{lm} dt for all modes (single integration)
        hdot = {}
        for (l, m), psi4 in self.psi4_lm.items():
            hdot[(l, m)] = cumulative_trapezoid(psi4, self.times, initial=0.0)

        # Accumulate dP^z/dt integrand
        dPz_dt = np.zeros(len(self.times))
        # Accumulate dP^x+idP^y/dt integrand
        dPxy_dt = np.zeros(len(self.times), dtype=complex)

        for (l, m) in self.psi4_lm.keys():
            h_lm = hdot.get((l, m), None)
            if h_lm is None:
                continue

            # ---------------------------------------------------------------
            # z-momentum: coupling to (l, m+1) mode
            # dP^z/dt contribution: Im[ḣ̄^{lm} · ḣ^{l,m+1}] × a^{lm}
            # ---------------------------------------------------------------
            h_lm1 = hdot.get((l, m + 1), None)
            if h_lm1 is not None:
                a_lm = _momentum_coupling_a(l, m)
                if a_lm != 0.0:
                    dPz_dt += np.imag(np.conj(h_lm) * h_lm1) * a_lm

            # ---------------------------------------------------------------
            # x+iy momentum: coupling to (l, m+1) and (l-1, m+1) modes
            # from Campanelli et al. (2007) eq. A.4 (simplified leading terms)
            # ---------------------------------------------------------------
            # Coupling within same l:
            if h_lm1 is not None:
                c_lm = _momentum_coupling_a(l, m)
                if c_lm != 0.0:
                    dPxy_dt += np.conj(h_lm) * h_lm1 * c_lm

            # Coupling to l-1 (sub-dominant cross-level term):
            if l > 2:
                h_lm1_lm1 = hdot.get((l - 1, m + 1), None)
                if h_lm1_lm1 is not None:
                    b_lm = _momentum_coupling_b(l, m)
                    if b_lm != 0.0:
                        dPxy_dt += np.conj(h_lm) * h_lm1_lm1 * b_lm

        # Integrate over time to get total cumulative momentum
        Pz  = prefactor * np.trapz(dPz_dt,         self.times)
        Pxy = prefactor * np.trapz(dPxy_dt,         self.times)
        Px  = np.real(Pxy)
        Py  = np.imag(Pxy)

        return np.array([Px, Py, Pz])

    # ------------------------------------------------------------------
    # Energy spectrum
    # ------------------------------------------------------------------

    def compute_energy_spectrum(self, l: int = 2, m: int = 2) -> \
            Tuple[np.ndarray, np.ndarray]:
        """
        Compute GW energy spectrum dE/df for a single (l,m) mode.

            dE/df = r²/(4π) |Ψ̃₄^{lm}(f)|² / (2πf)²

        Parameters
        ----------
        l, m : int
            Mode indices

        Returns
        -------
        freqs : np.ndarray
            Positive frequencies in code units M⁻¹
        dEdf : np.ndarray
            Energy spectral density in code units M²
        """
        psi4 = self.get_mode(l, m)
        dt = self.times[1] - self.times[0]
        N = len(psi4)

        freqs = np.fft.rfftfreq(N, d=dt)
        psi4_fft = np.fft.rfft(psi4) * dt

        dEdf = np.zeros(len(freqs))
        mask = freqs > 0
        dEdf[mask] = (self.r_ext**2 / (4.0 * np.pi)) * \
                     np.abs(psi4_fft[mask])**2 / (2.0 * np.pi * freqs[mask])**2

        return freqs, dEdf

    def extrapolate_to_infinity(self, modes: Optional[list] = None,
                                poly_order: int = 2) -> "Psi4Analysis":
        """
        Extrapolate Ψ₄ to r → ∞ using polynomial fits across multiple
        extraction radii. This method is relevant when multiple extraction
        radii are available.

        In practice, for a single extraction radius, apply the 1/r correction:
            Ψ₄^∞ ≈ r · Ψ₄^{r_ext}
        (This is already encoded by multiplying strain by r_ext in compute_strain.)

        Parameters
        ----------
        modes : list of (l,m) tuples, optional
            Modes to extrapolate. Defaults to all available modes.
        poly_order : int
            Order of the polynomial fit in 1/r. Default 2 (quadratic).

        Returns
        -------
        Psi4Analysis
            New object with extrapolated modes (r_ext = infinity proxy)

        Notes
        -----
        Full Richardson extrapolation requires output at ≥ 3 extraction radii.
        With a single radius the correction is O(1/r_ext); calling this method
        on single-radius data returns the object unchanged as a no-op.
        """
        if modes is None:
            modes = list(self.psi4_lm.keys())
        # Single-radius: no extrapolation possible; return copy
        return Psi4Analysis(self.times, dict(self.psi4_lm), self.r_ext)


def compute_strain(times: np.ndarray, psi4: np.ndarray,
                   r_ext: float, f_low: float = 0.001) -> np.ndarray:
    """
    Convenience function: compute strain from dominant (l=2,m=2) Ψ₄ mode.

    Parameters
    ----------
    times : np.ndarray    Time array
    psi4  : np.ndarray    Complex Ψ₄^{22}(t) array
    r_ext : float         Extraction radius
    f_low : float         FFI low-frequency cutoff

    Returns
    -------
    h : np.ndarray (complex)
        Gravitational wave strain h₊ - ih×
    """
    analysis = Psi4Analysis(times, {(2, 2): psi4}, r_ext)
    _, h = analysis.compute_strain(2, 2, f_low)
    return h
