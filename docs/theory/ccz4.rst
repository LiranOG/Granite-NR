CCZ4 Formulation
=================

GRANITE evolves the Einstein field equations using the CCZ4 formulation
`[Alic2012] <https://arxiv.org/abs/1106.2254>`_, which extends the BSSN system with constraint-damping terms.

.. math::

   G_{\mu\nu} = 8\pi T_{\mu\nu}

Evolved Variables
-----------------

The 22 evolved variables are:

.. list-table::
   :header-rows: 1
   :widths: 20 40 40

   * - Symbol
     - Description
     - Equation
   * - :math:`\chi`
     - Conformal factor
     - :math:`\chi = e^{-4\phi}`
   * - :math:`\tilde{\gamma}_{ij}`
     - Conformal metric (6 components)
     - :math:`\tilde{\gamma}_{ij} = \chi\, \gamma_{ij}`, :math:`\det \tilde{\gamma} = 1`
   * - :math:`\tilde{A}_{ij}`
     - Traceless extrinsic curvature (6)
     - :math:`\tilde{A}_{ij} = \chi (K_{ij} - \tfrac{1}{3} \gamma_{ij} K)`
   * - :math:`K`
     - Trace of extrinsic curvature
     - :math:`K = \gamma^{ij} K_{ij}`
   * - :math:`\hat{\Gamma}^i`
     - Conformal Christoffels (3)
     - :math:`\hat{\Gamma}^i = -\partial_j \tilde{\gamma}^{ij}`
   * - :math:`\Theta`
     - CCZ4 constraint variable
     - Damps constraint violations exponentially
   * - :math:`\alpha`
     - Lapse function
     - :math:`\partial_t \alpha = -2\alpha K` (1+log)
   * - :math:`\beta^i`
     - Shift vector (3)
     - :math:`\partial_t \beta^i = \tfrac{3}{4} \hat{\Gamma}^i - \eta \beta^i`

Constraint Damping
------------------

The CCZ4 modification adds algebraic terms proportional to :math:`\kappa_1`
and :math:`\kappa_2` that drive the constraint variables :math:`\Theta`
and :math:`Z_i` to zero exponentially:

.. math::

   \partial_t \Theta \supset -\alpha\, \kappa_1 (2 + \kappa_2)\, \Theta

.. math::

   \partial_t K \supset \kappa_1 (1 - \kappa_2)\, \alpha\, \Theta

Typical values: :math:`\kappa_1 = 0.02`, :math:`\kappa_2 = 0`.


References
----------

`[Alic2012] <https://arxiv.org/abs/1106.2254>`_ Alic, D., Bona-Casas, C., Bona, C., Rezzolla, L., & Palenzuela, C.
   (2012). "Conformal and covariant formulation of the Z4 system with
   constraint-violation damping." *Phys. Rev. D*, **85**, 064040.
