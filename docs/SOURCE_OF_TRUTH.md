# GRANITE-NR — Source of Truth Registry

For v0.6.8, authoritative sources are ranked as follows.
Any contradiction between sources must be resolved in this order:

  1. Runtime code (src/core/cli_parser.cpp + src/core/simulation_setup.cpp) — what the binary actually reads.
  2. Benchmark YAML files (benchmarks/) — canonical runnable examples.
  3. Root README.md — user-facing install/build/run workflow.
  4. docs/ — technical reference (currently being synchronized with v0.6.8).
  5. Wiki — public navigation layer; may temporarily lag during restructuring.

## Currently accepted top-level YAML keys (runtime parser, v0.6.8)
  grid, time, ccz4, io, initial_data, black_holes, amr

## Known drift areas (resolved in v0.6.8 stabilization pass)
- Pacing: All roadmap milestones have been shifted +2 months due to solo-developer constraints.
- Wiki Parameter Reference shows older conceptual schema keys
  (simulation, domain, dissipation, time_integration, boundary, diagnostics)
  that are not parsed at runtime.
- Some Wiki links still reference LiranOG/Granite; correct slug is LiranOG/Granite-NR.
- Some README doc-table paths use the old flat layout; actual layout is nested.
