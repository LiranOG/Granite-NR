GRMHD: General-Relativistic Magnetohydrodynamics
================================================

.. note::
   **v0.7 TARGET STUB:** The implementation is fully functional within the engine; however, this theoretical documentation is currently a stub scheduled for completion in the v0.7 cycle. 
   See :ref:`grmhd-references` for the key academic literature underlying the current implementation.

**GRANITE** solves the equations of General-Relativistic Magnetohydrodynamics (GRMHD) in the Valencia formulation `[Banyuls1997] <https://arxiv.org/abs/astro-ph/9707082>`_, which casts the equations as a robust system of conservation laws amenable to high-resolution shock-capturing (HRSC) methods.

Governing Equations
-------------------

The GRMHD system in conservation form (in geometric units where :math:`G = c = 1`):

.. math::

   \partial_t \mathbf{U} + \partial_i \mathbf{F}^i(\mathbf{U}) = \mathbf{S}(\mathbf{U}, g_{\mu\nu})

where the conserved variables :math:`\mathbf{U}` are defined as:

.. list-table:: 
   :widths: 20 80
   :header-rows: 1
   :class: tight-table

   * - Variable
     - Physical Quantity
   * - :math:`D`
     - Relativistic mass density
   * - :math:`S_j`
     - Momentum density (3 components)
   * - :math:`\tau`
     - Total energy density (less mass)
   * - :math:`B^i`
     - Magnetic field (3 components)
   * - :math:`DY_e`
     - Electron fraction density

Primitive Variables
-------------------

The primitive variables :math:`(\rho, v^i, \epsilon, p, B^i, Y_e)` are recovered from the conserved variables via a robust Newton-Raphson solve at each cell after every time step (**C2P: Conserved-to-Primitive inversion**).

Numerical Methods
-----------------

- **Reconstruction:** MP5 (default), PPM, or PLM (YAML-selectable)
- **Riemann Solver:** HLLE (default) or HLLD (MHD contact-preserving)
- **Constrained Transport:** Evans & Hawley (1988) — :math:`\nabla \cdot B = 0` maintained to machine precision
- **Coupling to CCZ4:** Facilitated strictly via the ``GRMetric3`` interface (lapse, shift, spatial metric, extrinsic curvature)

.. _grmhd-references:

References
----------

- Banyuls et al. (1997), `arXiv:astro-ph/9707082 <https://arxiv.org/abs/astro-ph/9707082>`_
- Antón et al. (2006), ApJ 637, 296
- Mignone & Bodo (2006), ApJ 636, 503 (HLLD solver)
- Evans & Hawley (1988), ApJ 332, 659 (constrained transport)
