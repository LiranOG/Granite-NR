Adaptive Mesh Refinement: Berger-Oliger AMR
===========================================

.. note::
   **v0.7 TARGET STUB:** The AMR implementation is fully operational in ``src/amr/amr.cpp`` (722 lines); however, this theoretical documentation is a stub scheduled for completion in the v0.7 cycle.
   See :doc:`/developer_guide/DEVELOPER_GUIDE` for the full AMR architectural design notes.

**GRANITE** uses block-structured Adaptive Mesh Refinement (AMR) based on the Berger-Oliger algorithm `[BergerOliger1984]`_, featuring recursive subcycling in time and robust trilinear prolongation to ensure boundary consistency across grid resolutions.

Hierarchy Structure
-------------------

The AMR hierarchy consists of **levels** :math:`\ell = 0, 1, \ldots, L_{max}`. Level 0 is the base grid covering the entire simulation domain. Each finer level has a proportionally reduced grid spacing:

.. math::

   \Delta x_\ell = \frac{\Delta x_0}{r^\ell}

where :math:`r = 2` is the refinement ratio (fixed in the current production implementation).

Time Stepping
-------------

Each level is subcycled independently with a local time step:

.. math::

   \Delta t_\ell = \frac{\Delta t_0}{r^\ell}

This ensures that fine levels take proportionally more steps per coarse step, maintaining a strictly consistent Courant-Friedrichs-Lewy (CFL) number across all levels of the hierarchy.

Prolongation and Restriction
----------------------------

- **Prolongation** (coarse → fine, for ghost zone filling): Uses trilinear interpolation, which is exact for polynomials up to degree 1.
- **Restriction** (fine → coarse, after fine level subcycle): Uses volume-weighted averaging, defined as :math:`q_{\text{coarse}} = \frac{1}{r^3} \sum q_{\text{fine}}`.

Refinement Criteria
-------------------

The following physical and geometric tagging variables trigger automatic mesh refinement:

.. list-table:: 
   :widths: 20 80
   :header-rows: 1
   :class: tight-table

   * - Variable
     - Criterion
   * - :math:`\chi`
     - :math:`\chi < \chi_{min}` (detects proximity to punctures/horizons)
   * - :math:`\alpha`
     - :math:`\alpha < \alpha_{min}` (detects geometric lapse collapse)
   * - :math:`\rho`
     - :math:`\rho > \rho_{thresh}` (detects high matter density)
   * - :math:`\mathcal{H}`
     - Identifies regions of Hamiltonian constraint violation

> [!NOTE]
> **Tracking Spheres:** Spheres registered at initialization override algorithmic tagging, forcing mandatory refinement to ``min_level`` around each puncture.

Current Limitations
-------------------

- **Dynamic Regridding:** Partially implemented (criteria evaluated, but block rebuild not yet triggered). Targeted for v0.7.
- **Depth Ceiling:** Maximum stable levels tested in production is 4. (6–12 levels is the design target for future multi-node HPC runs).
- **Refluxing:** No flux-correction at coarse-fine interfaces, presenting a known accuracy limitation in deep-hierarchy simulations.

.. [BergerOliger1984] Berger, M. J. & Oliger, J. (1984). *J. Comput. Phys.* **53**, 484–512.
