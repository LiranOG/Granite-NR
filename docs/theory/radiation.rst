M1 Radiation Transport
======================

.. note::
   **v0.7 TARGET STUB:** The M1 module is implemented in ``src/radiation/m1.cpp`` (416 lines) and passes numerical smoke tests (``tests/radiation/test_m1_diffusion.cpp``).
   **Status:** Built and tested, but NOT formally wired into the main RK3 evolution loop. Full coupling is a primary v0.7 target.

**GRANITE** implements grey (frequency-integrated) M1 radiation transport for both photon and neutrino fields. The engine follows the formalism established by Minerbo (1978) and incorporates the general-relativistic extension developed by Shibata et al. (2011).

Evolved Variables
-----------------

The M1 system evolves two distinct radiation fields per species:

.. list-table:: 
   :widths: 20 80
   :header-rows: 1
   :class: tight-table

   * - Variable
     - Physical Quantity
   * - :math:`E_r`
     - Radiation energy density
   * - :math:`F^i_r`
     - Radiation flux vector (3 components)

Closure Relation: Eddington Factor
----------------------------------

The key constitutive relation for the transport equations is the Minerbo (1978) variable Eddington factor:

.. math::

   f_{Edd}(\xi) = \frac{3 + 4\xi^2}{5 + 2\sqrt{4 - 3\xi^2}}

where the reduced flux is defined as :math:`\xi = |F_r| / (c\, E_r) \in [0, 1]`.

**Key Asymptotic Limits:**
- :math:`\xi \to 0` (Isotropic / optically thick): :math:`f_{Edd} \to 1/3`
- :math:`\xi \to 1` (Free-streaming radiation): :math:`f_{Edd} \to 1`

Source Terms
------------

Absorption and scattering opacities (:math:`\kappa_a` and :math:`\kappa_s`) strictly couple the radiation field to the matter field:

.. math::

   S_E = -\kappa_a c E_r + 4\pi \kappa_a B(T)

where :math:`B(T)` represents the Planck function evaluated at the local matter temperature :math:`T`.

Coupling Status
---------------

> [!WARNING]
> The M1 module remains structurally decoupled from the main time loop until two critical prerequisites are satisfied (both v0.7 roadmap targets):

1. **Checkpoint-Restart:** The M1 state must be systematically preserved across HPC restarts.
2. **M1–GRMHD Coupling:** The exact matter–radiation energy-momentum exchange must be wired atomically into the SSP-RK3 substep sequence.

References
----------

- Minerbo, G. N. (1978). *J. Quant. Spectrosc. Radiat. Transfer* **20**, 541–545.
- Shibata, M. et al. (2011). *Prog. Theor. Phys.* **125**, 1255–1287.
- Müller, B., Janka, H.-T. & Marek, A. (2010). *ApJ* **756**, 84.
