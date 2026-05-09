Gravitational Wave Extraction
=============================

.. note::
   **v0.7 TARGET STUB:** The GW extraction infrastructure is fully implemented on the analysis side in ``python/granite_analysis/gw.py``, but production activation within the C++ engine is targeted for v0.7. 
   See the `GW Extraction wiki page <https://github.com/LiranOG/Granite/wiki/Gravitational-Wave-Extraction>`_ for comprehensive details.

**GRANITE** extracts gravitational wave signals directly from the evolved spacetime using the rigorous Newman-Penrose :math:`\Psi_4` scalar formalism `[Newman1962]`_.

Newman-Penrose :math:`\Psi_4`
-----------------------------

The gravitational wave content is encoded natively in the Weyl scalar:

.. math::

   \Psi_4 = -\left(\ddot{h}_+ - i\ddot{h}_\times\right) \frac{1}{2r}

where :math:`h_+` and :math:`h_\times` are the two fundamental GW polarization amplitudes.

Spherical Harmonic Decomposition
--------------------------------

To isolate specific modes, :math:`\Psi_4` is decomposed into spin-weight :math:`s=-2` spherical harmonics:

.. math::

   \Psi_4(r, \theta, \phi) = \sum_{\ell \geq 2} \sum_{|m| \leq \ell}
       \Psi_4^{\ell m}(r) \; {}_{-2}Y_{\ell m}(\theta, \phi)

> [!NOTE]
> The dominant mode for quasi-circular binary inspiral is naturally :math:`(\ell, m) = (2, \pm 2)`.

Strain Recovery
---------------

Double time-integration of :math:`\Psi_4` recovers the physical dimensionless strain:

.. math::

   \tilde{h}(\omega) = \frac{\tilde{\Psi}_4(\omega)}{\max(\omega, \omega_0)^2}

The engine utilizes the fixed-frequency integration method of Reisswig & Pollney (2011) to effectively suppress the low-frequency drift that typically accumulates in naive double-integration schemes.

Extraction Radii
----------------

:math:`\Psi_4` is extracted at 6 distinct coordinate radii: :math:`r = 50, 75, 100, 150, 200, 500\, r_g`. 
Richardson extrapolation to asymptotic infinity :math:`r \to \infty` is performed using the three outermost radii.

Recoil Kick
-----------

The final recoil velocity of the remnant is computed via the Ruiz-Campanelli-Zlochower (2008) adjacent-mode coupling formula:

.. math::

   \frac{dP^z}{dt} = \frac{r^2}{16\pi} \sum_{\ell, m}
       \text{Im}\left[\dot{\bar{h}}^{\ell m} \cdot \dot{h}^{\ell, m+1}\right] a^{\ell m}

where the coupling coefficient is defined as :math:`a^{\ell m} = \sqrt{(\ell-m)(\ell+m+1)}\,/\,[\ell(\ell+1)]`.

> [!WARNING]
> **Implementation Status:** The C++ ``postprocess::computeRecoilVelocity()`` currently throws ``std::runtime_error`` (safeguard preventing invalid outputs). However, the Python-side ``Psi4Analysis.compute_radiated_momentum()`` is fully implemented and tested (Phase 5 M4 fix).

.. [Newman1962] Newman, E. & Penrose, R. (1962). *J. Math. Phys.* **3**, 566–578.

References
----------

- Reisswig, C. & Pollney, D. (2011). *Class. Quant. Grav.* **28**, 195015.
- Ruiz, M., Campanelli, M. & Zlochower, Y. (2008). *Phys. Rev. D* **78**, 064020.
