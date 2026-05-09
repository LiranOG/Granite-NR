import re
from pathlib import Path

p = Path('wiki_staging/Parameter-Reference.md')
text = p.read_text(encoding='utf-8')

# 1. Insert Schema Sync Warning
banner = '''

> **⚠️ Schema sync notice — v0.6.7.x:** The runtime parser in `src/main.cpp` currently accepts these top-level YAML sections: `grid`, `time`, `ccz4`, `io`, `initial_data`, `black_holes`, `amr`. The older conceptual schema keys (`simulation`, `domain`, `dissipation`, `time_integration`, `boundary`, `diagnostics`) are not parsed at runtime. Until the compatibility layer lands, use the YAML files under `benchmarks/` as the canonical runnable examples. See [docs/SOURCE_OF_TRUTH.md](https://github.com/LiranOG/Granite-NR/blob/main/docs/SOURCE_OF_TRUTH.md).'''

text = text.replace('# ⚙️ Parameter Reference\n', '# ⚙️ Parameter Reference\n' + banner + '\n')

# 2. Replace old params.yaml with new one
old_yaml = '''# GRANITE v0.6.7.2 — Canonical Parameter File
# See: docs/DEVELOPER_GUIDE.md §18 for full description
# See: wiki/Parameter-Reference for complete reference

simulation:
  name:              "B2_eq_production"
  output_dir:        "./output/B2_eq"
  total_time:        500.0
  checkpoint_every:  5000

domain:
  size:  48.0
  nx:    64
  ny:    64
  nz:    64

amr:
  levels: 4
  refinement:
    criteria:
      - variable:   chi
        threshold:  0.15
      - variable:   alpha
        threshold:  0.3

initial_data:
  type: two_punctures
  bh1:
    mass:      0.5
    position:  [5.0, 0.0, 0.0]
    momentum:  [0.0, 0.0840, 0.0]
    spin:      [0.0, 0.0, 0.0]
  bh2:
    mass:      0.5
    position:  [-5.0, 0.0, 0.0]
    momentum:  [0.0, -0.0840, 0.0]
    spin:      [0.0, 0.0, 0.0]

ccz4:
  kappa1:  0.02
  kappa2:  0.0
  eta:     2.0

dissipation:
  ko_sigma:  0.1

time_integration:
  cfl:  0.25

boundary:
  type:  sommerfeld

diagnostics:
  output_every:  100
  gw_extraction:
    enabled:  true
    radii:    [50, 100, 150, 200, 300, 500]
  ah_finder:
    enabled:  true
    every:    10

io:
  output_every:      100
  checkpoint_every:  5000
  format:            hdf5'''

new_yaml = '''# Minimal runtime-correct params.yaml for v0.6.7.2
# Verified with: single_puncture benchmark

grid:
  ncells: [64, 64, 64]
  domain_lo: [-48.0, -48.0, -48.0]
  domain_hi:  [48.0,  48.0,  48.0]
  ghost_cells: 4

time:
  cfl: 0.25
  t_final: 120.0
  max_steps: 10000000

ccz4:
  kappa1: 0.02
  kappa2: 0.0
  eta: 2.0
  ko_sigma: 0.1   # Safe value. See Parameter Reference notes on over-dissipation.

initial_data:
  type: brill_lindquist

black_holes:
  - mass: 1.0
    position: [0.0, 0.0, 0.0]
    momentum: [0.0, 0.0, 0.0]
    spin:     [0.0, 0.0, 0.0]

io:
  output_dir: output/single_puncture
  output_interval: 2
  checkpoint_interval: 100

# Optional AMR block (parsed when present):
# amr:
#   max_levels: 3
#   refine_threshold: 0.1
#   regrid_interval: 16
#   buffer_cells: 4'''

# Normalise newlines to ensure a match
text = text.replace('\r\n', '\n')
old_yaml = old_yaml.replace('\r\n', '\n')

if old_yaml in text:
    text = text.replace(old_yaml, new_yaml)
else:
    print('WARNING: old yaml not found!')

p.write_text(text, encoding='utf-8')
