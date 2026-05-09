Initial Data Formulation
========================

.. note::
   **v0.7 TARGET STUB:** See ``src/initial_data/initial_data.cpp`` and the `Initial-Data wiki page <https://github.com/LiranOG/Granite/wiki/Initial-Data>`_ for exhaustive implementation details. This documentation is a stub scheduled for completion in the v0.7 cycle.

**GRANITE** provides three distinct initial data solvers for configuring the spacetime geometry and matter distribution at :math:`t=0`.

Brill-Lindquist (BL)
--------------------

Analytic multi-puncture initial data for :math:`N` non-spinning, momentarily at-rest black holes. The exact conformal factor is given by:

.. math::

   \psi_{BL} = 1 + \sum_{A=1}^N \frac{M_A}{2 |\mathbf{x} - \mathbf{x}_A|}

The extrinsic curvature strictly vanishes: :math:`K_{ij} = 0`.

> [!WARNING]
> **Compatibility:** You MUST use ``boundary.type: copy`` with BL data. Sommerfeld radiative boundary conditions produce an :math:`\|H\|_2` constraint violation that is exactly 8× worse (confirmed via Phase 6 evidence). See ``docs/user_guide/BENCHMARKS.md``.

Bowen-York / Two-Punctures (BY)
-------------------------------

Advanced puncture initial data for spinning, boosted black holes. The conformal factor correction :math:`u` satisfies the Hamiltonian constraint:

.. math::

   \nabla^2 u = -\psi_{BL}^{-7} \tilde{A}^{ij}_{BY} \tilde{A}_{ij,BY} / 8

where :math:`\tilde{A}^{ij}_{BY}` is the Bowen-York extrinsic curvature tensor encoding the linear momenta :math:`P^i` and spins :math:`S^i` of each individual puncture.

> [!NOTE]
> **Solver:** The elliptic equation is solved via a global Newton-Raphson method on the full computational grid. The default quasi-circular tangential momenta for an equal-mass binary at separation :math:`d=10M` is :math:`P_y = \pm 0.0954` (Pfeiffer et al. 2007).

TOV Neutron Star
----------------

Generates a single neutron star in hydrostatic equilibrium by solving the Tolman-Oppenheimer-Volkoff (TOV) equations:

.. math::

   \frac{dp}{dr} = -(\rho + p) \frac{m + 4\pi r^3 p}{r(r - 2m)}

with the integrated mass defined as :math:`m(r) = \int_0^r 4\pi r'^2 \rho \, dr'`.

> [!CAUTION]
> **Unit Warning:** Ensure you use :math:`1\,\text{km} = 1.0 \times 10^5\,\text{cm}` for coordinate conversions and NOT ``RSUN_CGS``. This was historically documented as bug ``TOV`` — never revert. See the ``Known-Fixed-Bugs`` wiki page.

Compatibility Matrix
--------------------

.. list-table:: 
   :widths: 40 30 30
   :header-rows: 1
   :class: tight-table

   * - Initial Data Configuration
     - Sommerfeld BC
     - Copy BC
   * - **Brill-Lindquist**
     - ❌ (×8 worse)
     - ✅ Stable
   * - **Two-Punctures/BY**
     - ✅ Stable
     - ✅ Stable
   * - **TOV Neutron Star**
     - ✅ Stable
     - ✅ Stable

References
----------

- Brill, D. R. & Lindquist, R. W. (1963). *Phys. Rev.* **131**, 471.
- Bowen, J. M. & York, J. W. (1980). *Phys. Rev. D* **21**, 2047.
- Pfeiffer, H. P. et al. (2007). *Class. Quant. Grav.* **24**, S59–S82.
- Tolman, R. C. (1939). *Phys. Rev.* **55**, 364. / Oppenheimer & Volkoff (1939). *Phys. Rev.* **55**, 374.
