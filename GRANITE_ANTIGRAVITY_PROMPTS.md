# GRANITE-NR — Antigravity IDE Mission Prompts
**Sources:** AUDIT_IMMEDIATE_24-48H.md (Claude) · AUDIT_COMPREHENSIVE.md (Claude) · AUDIT_GPT.md (GPT)
**Version:** v0.6.7.2 · **Date:** May 2026

---

## ⚠️ GLOBAL WORKFLOW RULES — READ BEFORE ANY MISSION

```
CRITICAL WORKFLOW CONTRACT — applies to every single mission in this file:

1. LOCAL-ONLY. Never run `git push`, `git push origin`, or any remote operation.
   All work stays local. The repository owner (Liran) will personally review
   every change and push to remote himself after full validation.

2. NO AUTO-EXECUTE. Terminal execution policy: Request Review at all times.
   Never switch to "Always proceed."

3. NEVER run destructive git commands:
   rm -rf, git reset --hard, git clean -fdx, git push --force
   All of these require explicit written approval before execution.

4. NEVER modify files under:
   src/spacetime/, src/grmhd/, src/radiation/, src/amr/
   These are physics kernels — out of scope for all doc/tooling missions.

5. NEVER change benchmark scientific values (ko_sigma, mass, momentum, domain
   sizes, cfl) without explicit instruction.

6. After every mission, produce a written artifact:
   - Files changed (with diffs)
   - Commands run and their output
   - Unresolved risks or items that need human review
   - Explicit closing statement: "Ready for owner review — no remote push performed."
```

---

## ════════════════════════════════════════════════════════
## MISSION 0 — Branch Safety Scan & Creation
## Run this first. Do not proceed until it passes.
## ════════════════════════════════════════════════════════

```
CONTEXT:
The repository owner previously attempted to maintain proper branch discipline
but encountered serious CI and infrastructure failures that forced continued work
directly on main. Before creating any new branch, we must verify that main is in
a clean and stable state and that branch creation will not introduce any breakage.
This mission is therefore a full pre-flight check before any edits are made.

IMPORTANT: This mission is READ-ONLY until Step 8. Do not modify any file.

─────────────────────────────────────────────
STEP 1 — Verify working tree state:
  git status
  git log --oneline -10

  Expected: "nothing to commit, working tree clean"
  If dirty (modified/untracked files): STOP. Report exactly which files are
  affected and their status. Do not proceed to branch creation.

─────────────────────────────────────────────
STEP 2 — Verify the build compiles cleanly on main:
  python3 scripts/run_granite.py build --build-type Release 2>&1 | tail -30

  If the build fails: report the exact error output and STOP.
  Do not proceed if the codebase does not build on main — branch creation
  cannot be blamed for a pre-existing build failure, but we must document it.

─────────────────────────────────────────────
STEP 3 — Run the test suite on main and record the baseline:
  PYTHONPATH=python pytest -q tests/python 2>&1

  Record exactly: total tests, passed, failed, skipped.
  If failures exist on main before any changes: document which tests fail.
  This is the baseline. Any regression introduced by our edits will be
  immediately visible because the numbers must match this baseline.

─────────────────────────────────────────────
STEP 4 — CLI smoke test:
  python3 scripts/run_granite.py --help
  python3 scripts/run_granite.py build --help
  python3 scripts/run_granite.py run --help   || echo "[MISSING] run sub-command"
  python3 scripts/run_granite.py test --help  || echo "[MISSING] test sub-command"

  Record all outputs. This establishes the CLI baseline.

─────────────────────────────────────────────
STEP 5 — Python package import:
  source .venv/bin/activate 2>/dev/null || (python3 -m venv .venv && source .venv/bin/activate)
  pip install -e . -q
  python3 -c "import granite_analysis; print('Package import OK')" 2>&1 || echo "[FAIL] Package import"

─────────────────────────────────────────────
STEP 6 — Stale link count (read-only scan, no fixes yet):
  grep -Rn "github\.com/LiranOG/Granite[^-N]" \
    README.md docs .github benchmarks scripts viz CHANGELOG.md \
    CITATION.cff pyproject.toml CMakeLists.txt 2>/dev/null | wc -l

  Record the count. This is the before-count for Mission A-3.

─────────────────────────────────────────────
STEP 7 — Inspect CI workflow for branch-specific triggers:
  cat .github/workflows/ci.yml | head -60

  Check: does the CI trigger on specific branch names that would conflict
  with or misbehave on an audit branch? Report any branch filters found.
  Note: the owner has previously encountered CI failures when branching,
  so this step is critical.

─────────────────────────────────────────────
STEP 8 — Create the audit branch (only if Steps 1-7 show no blocking issues):
  git checkout -b audit/v0.6.7.2-doc-sync

  Immediately verify:
  git status          → must show: On branch audit/v0.6.7.2-doc-sync, nothing to commit
  git log --oneline -3
  git diff main       → must show: empty (no accidental file changes from branch creation)

─────────────────────────────────────────────
STEP 9 — Re-run test suite on the new branch to confirm no regression:
  PYTHONPATH=python pytest -q tests/python 2>&1

  Compare to Step 3 baseline.
  Pass/fail/skip counts MUST be identical.
  If they differ even by one test: STOP. Report the discrepancy before any edits.

─────────────────────────────────────────────
PRODUCE ARTIFACT: audit/M0-branch-safety-scan.md containing:
  - git status output
  - Build result (PASS / FAIL with error)
  - Test baseline on main (N passed, N failed, N skipped)
  - Test result on audit branch (must match)
  - Python package import result
  - Stale link count found
  - CI branch trigger findings
  - Final verdict: SAFE TO PROCEED or BLOCKED (with reason)

Do not start any other mission until this artifact exists and shows SAFE TO PROCEED.
```

---

## ════════════════════════════════════════════════════════
## PART A — FULL NON-PHYSICS MISSION PROMPTS
## (Docs · Links · CLI · Schema · Benchmarks · CI · Tooling)
##
## Explicitly excluded (belong to the Architectural Audit):
##   - Monolith decomposition (src/main.cpp, ccz4.cpp)
##   - Wiring M1 radiation into RK3 loop
##   - Wiring neutrino transport into RK3 loop
##   - Calling horizon_finder from the main evolution
##   - Calling postprocess from the main evolution
##   - Dynamic AMR physics implementation
##   - Checkpoint-restart load path implementation
##   - GPU porting
## ════════════════════════════════════════════════════════

---

## MISSION A-1 — Create AGENTS.md (Agent Safety Contract)

```
BRANCH: audit/v0.6.7.2-doc-sync
PREREQUISITE: Mission 0 artifact must show SAFE TO PROCEED.

Create the file AGENTS.md at the repository root with the following content.
This file is a permanent contract that governs all future AI agent work
on this repository.

══════════════ FILE CONTENT ══════════════
# AGENTS.md — GRANITE-NR Agent Safety Rules

This file governs the behavior of any AI agent (Antigravity or otherwise)
operating on this repository.

## Workflow
- Always work on a dedicated branch. Never commit directly to main.
- Run `git status` before and after every batch of edits.
- Run `PYTHONPATH=python pytest -q tests/python` after any Python change.
- Produce a written artifact for every mission: files changed, commands run,
  tests run, and a list of unresolved risks.

## Hard prohibitions
- Never run `git push`, `git push --force`, or any remote push command.
  The repository owner performs all remote push operations after personal review.
- Never run `rm -rf`, `git reset --hard`, or `git clean -fdx` without explicit
  written approval from the repository owner.
- Never modify src/spacetime/, src/grmhd/, src/radiation/, or src/amr/ unless
  the task explicitly states "physics kernel modification approved."
- Never change benchmark scientific values (ko_sigma, mass, momentum, domain
  sizes, cfl) without explicit instruction and simultaneous docs update.

## Terminal execution policy
- Default: Request Review. Never switch to Always proceed.

## After every mission
- Verify test count matches the pre-mission baseline from Mission 0.
- Produce artifact: files changed, diffs, commands run, unresolved items.
- State explicitly: "Ready for owner review — no remote push performed."
══════════════ END FILE CONTENT ══════════════

After creating the file:
  git add AGENTS.md
  git commit -m "chore: add AGENTS.md agent safety contract"

PRODUCE ARTIFACT: confirm file created, commit hash, no push performed.
```

---

## MISSION A-2 — Create docs/SOURCE_OF_TRUTH.md

```
BRANCH: audit/v0.6.7.2-doc-sync

Create docs/SOURCE_OF_TRUTH.md with the following content:

══════════════ FILE CONTENT ══════════════
# GRANITE-NR — Source of Truth Registry

For v0.6.7.x, authoritative sources are ranked as follows.
Any contradiction between sources must be resolved in this order:

  1. Runtime code (src/main.cpp YAML parser) — what the binary actually reads.
  2. Benchmark YAML files (benchmarks/) — canonical runnable examples.
  3. Root README.md — user-facing install/build/run workflow.
  4. docs/ — technical reference (currently being synchronized with v0.6.7.2).
  5. Wiki — public navigation layer; may temporarily lag during restructuring.

## Currently accepted top-level YAML keys (runtime parser, v0.6.7.2)
  grid, time, ccz4, io, initial_data, black_holes, amr

## Known drift areas (resolved in v0.6.7.2 stabilization pass)
- Wiki Parameter Reference shows older conceptual schema keys
  (simulation, domain, dissipation, time_integration, boundary, diagnostics)
  that are not parsed at runtime.
- Some Wiki links still reference LiranOG/Granite; correct slug is LiranOG/Granite-NR.
- Some README doc-table paths use the old flat layout; actual layout is nested.
══════════════ END FILE CONTENT ══════════════

  git add docs/SOURCE_OF_TRUTH.md
  git commit -m "docs: add SOURCE_OF_TRUTH.md registry"

PRODUCE ARTIFACT: confirm creation and commit hash.
```

---

## MISSION A-3 — Fix All Stale Repository Links

```
BRANCH: audit/v0.6.7.2-doc-sync

The repository was renamed from LiranOG/Granite to LiranOG/Granite-NR.
All stale references to the old name must be replaced.
Exception: CHANGELOG entries that describe the rename event itself may be left as-is.

─────────────────────────────────────────────
STEP 1 — Audit (read-only):
  grep -Rn "github\.com/LiranOG/Granite[^-N]" \
    README.md docs .github benchmarks scripts viz CHANGELOG.md \
    CITATION.cff pyproject.toml CMakeLists.txt 2>/dev/null

  grep -Rn "LiranOG/Granite/blob/main\|LiranOG/Granite/wiki" \
    README.md docs .github benchmarks scripts viz CHANGELOG.md 2>/dev/null

  Record total count before any change.

─────────────────────────────────────────────
STEP 2 — Apply replacements:
  https://github.com/LiranOG/Granite/        → https://github.com/LiranOG/Granite-NR/
  https://github.com/LiranOG/Granite/wiki/   → https://github.com/LiranOG/Granite-NR/wiki/

  In CMakeLists.txt:
    HOMEPAGE_URL "https://github.com/LiranOG/Granite"
    → HOMEPAGE_URL "https://github.com/LiranOG/Granite-NR"

  In CITATION.cff and pyproject.toml: fix any occurrence of the old slug.

─────────────────────────────────────────────
STEP 3 — Fix broken local markdown links:
  grep -Rn "INSTALLATION\.md\|docs/INSTALL\.md" benchmarks docs README.md 2>/dev/null
  Replace:
    ../../INSTALLATION.md   → ../../docs/getting_started/Installation.md
    ../../docs/INSTALL.md   → ../../docs/getting_started/Installation.md

  grep -Rn "\.github/CONTRIBUTING\.md" .github/ README.md 2>/dev/null
  Verify CONTRIBUTING.md exists at the referenced path; report if missing.

─────────────────────────────────────────────
STEP 4 — Acceptance test (must return zero matches):
  grep -Rn "github\.com/LiranOG/Granite[^-N]" \
    README.md docs .github benchmarks scripts viz CHANGELOG.md \
    CITATION.cff pyproject.toml CMakeLists.txt 2>/dev/null

  If any matches remain, fix them before committing.

─────────────────────────────────────────────
STEP 5 — Commit:
  git add -A
  git commit -m "docs: replace all stale LiranOG/Granite links with Granite-NR"

PRODUCE ARTIFACT: every file changed, count before/after, acceptance test output.
```

---

## MISSION A-4 — Fix Version String Inconsistency

```
BRANCH: audit/v0.6.7.2-doc-sync

CHANGELOG documents v0.6.7.2 but the build system and source banner still say 0.6.7.

─────────────────────────────────────────────
STEP 1 — Audit current state:
  grep -n "0\.6\.7" CMakeLists.txt src/main.cpp README.md pyproject.toml CITATION.cff

─────────────────────────────────────────────
STEP 2 — Apply fixes:
  CMakeLists.txt:  VERSION 0.6.7    → VERSION 0.6.7.2
  src/main.cpp:    Find the banner literal string "0.6.7" and update to "0.6.7.2"
                   ONLY change the string literal. Do not modify any logic.
  pyproject.toml:  version = "0.6.7" → version = "0.6.7.2" (if present)
  CITATION.cff:    version: "0.6.7"  → version: "0.6.7.2"  (if present)
  README.md badge: only if it is a hardcoded string, not a dynamic shield URL.

─────────────────────────────────────────────
STEP 3 — Create version.txt at repo root:
  echo "0.6.7.2" > version.txt

─────────────────────────────────────────────
STEP 4 — Verify build still works:
  python3 scripts/run_granite.py build --build-type Release 2>&1 | tail -20
  If it fails, revert only the src/main.cpp change and investigate.

─────────────────────────────────────────────
STEP 5 — Commit:
  git add CMakeLists.txt src/main.cpp pyproject.toml CITATION.cff version.txt README.md
  git commit -m "build: propagate v0.6.7.2 patch version to CMake, source banner, pyproject"

PRODUCE ARTIFACT: grep before/after, build result, commit hash.
```

---

## MISSION A-5 — Fix README Documentation Table Paths

```
BRANCH: audit/v0.6.7.2-doc-sync

The README Documentation table references the OLD flat docs/ layout.
The actual layout since v0.6.7.2 is nested under subdirectories.

─────────────────────────────────────────────
STEP 1 — Confirm actual paths exist on disk:
  ls docs/developer_guide/DEVELOPER_GUIDE.md    || echo "MISSING"
  ls docs/user_guide/BENCHMARKS.md              || echo "MISSING"
  ls docs/user_guide/diagnostic_handbook.md     || echo "MISSING"
  ls docs/getting_started/Installation.md       || echo "MISSING"
  ls docs/theory/SCIENCE.md                     || echo "MISSING"

  Report any MISSING file. Do not create links to non-existent files.

─────────────────────────────────────────────
STEP 2 — Update the Documentation table in README.md:
  docs/DEVELOPER_GUIDE.md      → docs/developer_guide/DEVELOPER_GUIDE.md
  docs/BENCHMARKS.md           → docs/user_guide/BENCHMARKS.md
  docs/diagnostic_handbook.md  → docs/user_guide/diagnostic_handbook.md
  docs/INSTALL.md              → docs/getting_started/Installation.md
  INSTALLATION.md              → docs/getting_started/Installation.md

─────────────────────────────────────────────
STEP 3 — Verify every markdown link in that section resolves to a real file.
  Report any dead link that cannot be fixed (file truly missing from disk).

─────────────────────────────────────────────
STEP 4 — Commit:
  git add README.md
  git commit -m "docs: align README documentation table with actual nested docs/ layout"

PRODUCE ARTIFACT: every corrected path, verification result, any dead links.
```

---

## MISSION A-6 — Fix README Anchor, Known Limitations & Roadmap Tables

```
BRANCH: audit/v0.6.7.2-doc-sync

─────────────────────────────────────────────
Task A — Fix the broken anchor link:
  Search README.md for:
    [Known Limitations](#️-known-limitations-v067)
  The U+FE0F variation selector character before the dash may break renderers.
  Replace with:
    [Known Limitations](#-known-limitations-v067)
  Verify the target heading exists with a matching anchor.

─────────────────────────────────────────────
Task B — Add 4 missing Known Limitations rows (append if not already present):
  | `horizon_finder` built & unit-tested but NOT called from the main evolution loop — AH detection is dead-code at runtime | 🔄 | v0.7 |
  | `postprocess` module built and linked but NEVER invoked from the main loop | 🔄 | v0.7 |
  | `neutrino` transport built and unit-tested but NOT coupled into RK3 (same status as M1 radiation) | 🔄 | v0.7 |
  | `chi_blend_center` / `chi_blend_width` CCZ4 parameters exist in code but are not exposed in the YAML parser | 🔄 | v0.7 |

─────────────────────────────────────────────
Task C — Update roadmap and version history tables (~line 365 and ~line 412):
  Find the row that marks v0.6.7 as the current version.
  Update it or add a v0.6.7.2 row:
    "Python analysis layer overhaul (granite_analysis package),
     docs tree restructure sync, stale link repairs, agent safety contract."

─────────────────────────────────────────────
Task D — Audit comparison matrix (~line 96):
  GRANITE vs Einstein Toolkit / GRChombo / SpECTRE / AthenaK.
  For any cell where GRANITE claims equal-to or better-than a competitor:
  - If a reproducible benchmark artifact proves it: leave as-is.
  - If no artifact exists: add a * footnote:
    "* Not yet independently reproduced. Formal validation planned for v0.8."
  Do not remove any claim. Only add asterisk footnotes.

─────────────────────────────────────────────
Task E — Check .github/CONTRIBUTING.md:
  cat .github/CONTRIBUTING.md 2>/dev/null | wc -l || echo "FILE MISSING"
  If < 30 lines: report as stub. If missing: report the broken README link.
  Do not create content without explicit instruction.

─────────────────────────────────────────────
Commit:
  git add README.md
  git commit -m "docs: fix anchor, add missing limitation rows, update roadmap and version history"

PRODUCE ARTIFACT: all changes with diffs, CONTRIBUTING.md status.
```

---

## MISSION A-7 — Benchmark YAML Wording & Classification Cleanup

```
BRANCH: audit/v0.6.7.2-doc-sync

─────────────────────────────────────────────
Task A — Fix B2_eq/params.yaml header:
  Find any comment containing "targets ~8 orbits + merger + ringdown".
  Replace the full header comment block with:

  # Benchmark: B2_eq — Equal-mass non-spinning BBH early-inspiral stability test
  #
  # STATUS v0.6.7.x:
  # Used for early-inspiral and long-term stability validation through t=500M.
  # No completed merger, ringdown, or waveform-grade SXS validation is claimed
  # for v0.6.7.x. Merger/ringdown validation planned for v0.8-v0.9
  # against SXS:BBH:3634 (equal mass, zero spin, e=0.0003, 9.6 orbits).

─────────────────────────────────────────────
Task B — Add ko_sigma WARNING comment in B2_eq (do NOT change the numeric value):
  Find the line with ko_sigma in B2_eq/params.yaml.
  Add directly above it:

  # WARNING: This value is under active review.
  # Parameter Reference recommends ko_sigma <= 0.1 for single-puncture stability.
  # BBH runs with ko_sigma: 0.35 have been observed to cause BHs to drift apart
  # rather than inspiral, consistent with over-dissipation of gauge dynamics.
  # This will be formally revalidated in v0.7.

─────────────────────────────────────────────
Task C — Add CONCEPT STATUS block to B5_star/params.yaml:
  Insert at the very top of the file:

  # ════════════════════════════════════════════════════════════════
  # STATUS: Flagship scenario specification — CONCEPT TARGET ONLY
  # This file is NOT consumed by the v0.6.7.x runtime parser.
  # Schema uses concept-only keys (scenario, stars, evolution, physics,
  # expected_results, compute_estimate) not parsed by src/main.cpp.
  # Runtime-compatible B5_star configuration is planned for v1.0.
  # ════════════════════════════════════════════════════════════════

─────────────────────────────────────────────
Task D — Check B2_eq amr section and add a documentation comment above it:
  grep -n "^amr" benchmarks/B2_eq/params.yaml
  If present, add above the amr: line:

  # AMR parameters: stored here for documentation and future runtime use.
  # Parsing of amr.max_levels, amr.refine_threshold, amr.regrid_interval,
  # and amr.buffer_cells is implemented in v0.6.7.x. Verify with:
  # python3 scripts/audit_params_schema.py

─────────────────────────────────────────────
Commit:
  git add benchmarks/B2_eq/params.yaml benchmarks/B5_star/params.yaml
  git commit -m "docs: B2_eq early-inspiral clarification, ko_sigma warning, B5_star concept status"

PRODUCE ARTIFACT: full diffs, confirm no numeric values changed.
```

---

## MISSION A-8 — Fix Wiki Home Page (Local Only)

```
BRANCH: audit/v0.6.7.2-doc-sync
IMPORTANT: Work in the Wiki repo. Do NOT git push anything.

─────────────────────────────────────────────
STEP 1 — Clone Wiki locally (sibling of main repo):
  cd ..
  git clone https://github.com/LiranOG/Granite-NR.wiki.git Granite-NR.wiki
  cd Granite-NR.wiki
  git log --oneline -5

─────────────────────────────────────────────
STEP 2 — Add sync banner to Home.md (below the H1 title):

  > 📋 **Documentation sync in progress — v0.6.7.x**
  > GRANITE-NR recently underwent a repository restructuring that reorganized the
  > `docs/` directory tree and renamed the repository from `Granite` to `Granite-NR`.
  > This Wiki is currently being synchronized with the updated `README.md`, `docs/`,
  > CLI interface, and benchmark YAML schema. During this transition some links,
  > command examples, or parameter names may temporarily lag behind the current
  > codebase. For runnable commands and reliable paths, prefer the root `README.md`,
  > `docs/getting_started/Installation.md`, and the YAML files under `benchmarks/`
  > until this notice is removed.

─────────────────────────────────────────────
STEP 3 — Fix Quick Start CLI commands in Home.md:
  Find any block containing:
    python3 scripts/run_granite.py build --release
    python3 scripts/run_granite.py build --tests
  Replace with:
    # Build (Release mode with tests enabled)
    python3 scripts/run_granite.py build --build-type Release --flag GRANITE_ENABLE_TESTS=ON

    # Run C++ unit tests
    python3 scripts/run_granite.py test

    # Run Python analysis tests
    PYTHONPATH=python pytest -q tests/python

─────────────────────────────────────────────
STEP 4 — Add "Where to Start" navigation section if not present:
  ## Where to Start
  1. **New user?** Read the root [README.md](https://github.com/LiranOG/Granite-NR) first.
  2. **Building GRANITE:** [docs/getting_started/Installation.md](https://github.com/LiranOG/Granite-NR/blob/main/docs/getting_started/Installation.md)
  3. **Physics background:** [docs/theory/SCIENCE.md](https://github.com/LiranOG/Granite-NR/blob/main/docs/theory/SCIENCE.md)
  4. **Running benchmarks:** [docs/user_guide/BENCHMARKS.md](https://github.com/LiranOG/Granite-NR/blob/main/docs/user_guide/BENCHMARKS.md)
  5. **Wiki** = quick reference; **`docs/`** = canonical technical reference.

─────────────────────────────────────────────
STEP 5 — Fix stale links in ALL Wiki pages:
  grep -rn "github\.com/LiranOG/Granite[^-N]" *.md
  Apply same replacement as Mission A-3:
    LiranOG/Granite/ → LiranOG/Granite-NR/

─────────────────────────────────────────────
STEP 6 — Stage locally, save diff, do NOT push:
  git add -A
  git diff --staged > ../Granite-NR/audit/wiki-home-changes.diff
  cat ../Granite-NR/audit/wiki-home-changes.diff | head -100

PRODUCE ARTIFACT: diff saved to audit/wiki-home-changes.diff.
State: "Wiki changes staged locally. No push performed.
Owner to review audit/wiki-home-changes.diff and push the Wiki repo manually."
```

---

## MISSION A-9 — Fix Wiki Parameter Reference Page (Local Only)

```
BRANCH: audit/v0.6.7.2-doc-sync (Wiki repo)

─────────────────────────────────────────────
STEP 1 — Open Parameter-Reference.md.

─────────────────────────────────────────────
STEP 2 — Insert schema sync warning at top of page (below H1 title):

  > **⚠️ Schema sync notice — v0.6.7.x:** The runtime parser in `src/main.cpp`
  > currently accepts these top-level YAML sections:
  > `grid`, `time`, `ccz4`, `io`, `initial_data`, `black_holes`, `amr`.
  > The older conceptual schema keys (`simulation`, `domain`, `dissipation`,
  > `time_integration`, `boundary`, `diagnostics`) are not parsed at runtime.
  > Until the compatibility layer lands, use the YAML files under `benchmarks/`
  > as the canonical runnable examples. See
  > [docs/SOURCE_OF_TRUTH.md](https://github.com/LiranOG/Granite-NR/blob/main/docs/SOURCE_OF_TRUTH.md).

─────────────────────────────────────────────
STEP 3 — Find the canonical params.yaml template section.
  Replace only the YAML code block content with this runtime-correct template:

  ```yaml
  # Minimal runtime-correct params.yaml for v0.6.7.2
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
  #   buffer_cells: 4
  ```

─────────────────────────────────────────────
STEP 4 — Stage and save diff, do NOT push:
  git add Parameter-Reference.md
  git diff --staged >> ../Granite-NR/audit/wiki-param-ref-changes.diff

PRODUCE ARTIFACT: diff appended to audit/wiki-param-ref-changes.diff.
State: "No push performed."
```

---

## MISSION A-10 — Reorganize Known Bugs Wiki Page (Local Only)

```
BRANCH: audit/v0.6.7.2-doc-sync (Wiki repo)

─────────────────────────────────────────────
STEP 1 — Find the bugs page:
  ls *.md | grep -i "bug\|known\|issue"
  Open the file found.

─────────────────────────────────────────────
STEP 2 — Restructure into clearly labeled sections:

  ## Fixed Bugs (resolved in v0.6.5 and earlier)
  [keep existing fixed bug content here, unchanged]

  ## Open Known Issues (as of v0.6.7.2)
  [move any items that are known but not yet fixed here]

  ## Known Diagnostic Limitations
  Add this entry if not already present:

  ### alpha_center reads from AMR level 0, not the finest level
  - **Symptom:** alpha_center values around 0.94 during BBH runs or late-time
    single-puncture runs. This is a diagnostic artifact, not a physics result.
    The true trumpet lapse value (~0.3) is only visible at the finest AMR level
    near the puncture.
  - **Status:** Known, documented, not a physics bug. Fix targeted for v0.7.
  - **Implication:** Do not cite alpha_center from level-0 output as evidence of
    gauge failure. It is not.

─────────────────────────────────────────────
STEP 3 — Stage and save diff, do NOT push:
  git add [filename]
  git diff --staged >> ../Granite-NR/audit/wiki-bugs-changes.diff

PRODUCE ARTIFACT: diff saved.
```

---

## MISSION A-11 — Create scripts/audit_params_schema.py

```
BRANCH: audit/v0.6.7.2-doc-sync

Create scripts/audit_params_schema.py with the following content:

══════════════ FILE CONTENT ══════════════
#!/usr/bin/env python3
"""
GRANITE-NR — Benchmark YAML Schema Auditor
Validates that benchmark params.yaml files use only keys the runtime parser accepts.
Usage: python3 scripts/audit_params_schema.py
"""
from pathlib import Path
import sys
import yaml

RUNTIME_KEYS = {"grid", "time", "ccz4", "io", "initial_data", "black_holes", "amr"}
CONCEPT_ONLY = {
    "scenario", "stars", "physics", "expected_results",
    "compute_estimate", "evolution", "output"
}

def audit_file(path: Path) -> bool:
    try:
        data = yaml.safe_load(path.read_text()) or {}
    except yaml.YAMLError as e:
        print(f"[FAIL] {path}: YAML parse error: {e}")
        return False

    keys = set(data.keys())
    ok = True

    concept_found = keys & CONCEPT_ONLY
    if concept_found:
        print(f"[WARN] {path}: concept/future schema keys found: {sorted(concept_found)}")
        print(f"       Mark as concept or add STATUS comment if intentional.")

    unknown = keys - RUNTIME_KEYS - CONCEPT_ONLY
    if unknown:
        print(f"[FAIL] {path}: unknown top-level keys: {sorted(unknown)}")
        ok = False

    if "grid" in data:
        g = data["grid"] or {}
        if "ncells" not in g and "coarse_ncells" not in g:
            print(f"[FAIL] {path}: grid section lacks ncells or coarse_ncells")
            ok = False

    if ok and not concept_found:
        print(f"[OK]   {path}")

    return ok

def main() -> int:
    benchmark_root = Path("benchmarks")
    if not benchmark_root.exists():
        print("ERROR: benchmarks/ not found. Run from repo root.")
        return 1

    files = sorted(benchmark_root.rglob("params.yaml"))
    if not files:
        print("WARNING: No params.yaml files found under benchmarks/")
        return 0

    results = [audit_file(f) for f in files]
    passed = sum(results)
    failed = len(results) - passed
    print(f"\nResult: {passed}/{len(results)} passed, {failed} failed.")
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
══════════════ END FILE CONTENT ══════════════

After creating:
  chmod +x scripts/audit_params_schema.py
  python3 scripts/audit_params_schema.py

Record the output. [FAIL] results should be addressed (Mission A-7 covers most).

  git add scripts/audit_params_schema.py
  git commit -m "tooling: add benchmark YAML schema auditor"

PRODUCE ARTIFACT: script output, any remaining failures.
```

---

## MISSION A-12 — Create scripts/audit_links.py

```
BRANCH: audit/v0.6.7.2-doc-sync

Create scripts/audit_links.py with the following content:

══════════════ FILE CONTENT ══════════════
#!/usr/bin/env python3
"""
GRANITE-NR — Markdown Link Auditor
Checks for stale repository links and broken local paths.
Usage: python3 scripts/audit_links.py
"""
import re
import sys
from pathlib import Path

STALE_PATTERN = re.compile(r"github\.com/LiranOG/Granite[^-N\w]")
LOCAL_LINK_PATTERN = re.compile(r"\[.*?\]\(((?!http)[^)]+\.md[^)]*)\)")

SCAN_PATHS = [
    "README.md", "CONTRIBUTING.md", "CHANGELOG.md",
    "docs", ".github", "benchmarks", "scripts", "viz"
]

def check_file(path: Path, issues: list) -> None:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except Exception as e:
        issues.append(f"[ERROR] Cannot read {path}: {e}")
        return

    for i, line in enumerate(text.splitlines(), 1):
        if STALE_PATTERN.search(line):
            issues.append(f"[STALE-LINK] {path}:{i}: {line.strip()[:120]}")

    for match in LOCAL_LINK_PATTERN.finditer(text):
        ref = match.group(1).split("#")[0].strip()
        if not ref:
            continue
        resolved = (path.parent / ref).resolve()
        if not resolved.exists():
            issues.append(f"[DEAD-LOCAL] {path}: link target not found: {ref}")

def main() -> int:
    issues = []
    for p in SCAN_PATHS:
        root = Path(p)
        if root.is_file():
            check_file(root, issues)
        elif root.is_dir():
            for f in root.rglob("*.md"):
                check_file(f, issues)

    if issues:
        print(f"Found {len(issues)} issue(s):\n")
        for issue in issues:
            print(" ", issue)
        return 1
    else:
        print("All links OK.")
        return 0

if __name__ == "__main__":
    sys.exit(main())
══════════════ END FILE CONTENT ══════════════

After creating:
  chmod +x scripts/audit_links.py
  python3 scripts/audit_links.py

Stale links should be zero if Mission A-3 was completed correctly.

  git add scripts/audit_links.py
  git commit -m "tooling: add markdown link auditor"

PRODUCE ARTIFACT: script output.
```

---

## MISSION A-13 — Create VALIDATION_STATUS.md

```
BRANCH: audit/v0.6.7.2-doc-sync

Create VALIDATION_STATUS.md at the repository root:

══════════════ FILE CONTENT ══════════════
# GRANITE-NR — Validation Status
**Version:** v0.6.7.2 · **Date:** May 2026

"Wired into main loop" = the module is actually called during a production run.
"Dead-code" = compiled and linked but never called at runtime.

| Module | Implemented | Unit-tested | Wired into main loop | End-to-end validated | Target |
|--------|:-----------:|:-----------:|:--------------------:|:--------------------:|--------|
| CCZ4 spacetime evolution | ✅ | ✅ | ✅ | Partial (SP + BBH early-inspiral) | Active |
| GRMHD Valencia solver | ✅ | ✅ | ✅ | Partial (no disk validation) | Active |
| M1 radiation transport | ✅ | ✅ | ❌ | ❌ | v0.7 |
| Neutrino transport | ✅ | ✅ | ❌ | ❌ | v0.7 |
| Berger-Oliger AMR (subcycling) | ✅ | ✅ | ✅ | Partial | Active |
| Dynamic AMR regridding | Partial | ❌ | ❌ | ❌ | v0.7 blocker |
| Checkpoint write | ✅ | Partial | ✅ | Partial | Active |
| Checkpoint resume (--resume) | Partial | ❌ | ❌ | ❌ | v0.7 blocker |
| GW extraction (Ψ₄) | ✅ | Partial | ✅ | Partial | Active |
| Recoil velocity | ❌ (throws) | ❌ | ❌ | ❌ | v0.7+ |
| Horizon finder | ✅ | ✅ | ❌ dead-code | ❌ | v0.7 |
| Postprocess module | ✅ | ✅ | ❌ dead-code | ❌ | v0.7 |
| B2_eq benchmark | ✅ | n/a | ✅ | Early-inspiral only | Active |
| SXS:BBH:3634 validation | ❌ | ❌ | ❌ | ❌ | v0.8–v0.9 |
| B5_star scenario | Concept spec | ❌ | ❌ | ❌ | v1.0 flagship |

## Diagnostic Limitations
- **alpha_center** reads from AMR level 0, not the finest level near the puncture.
  Values ~0.94 in BBH runs are a diagnostic artifact, not a physics result.
  The true trumpet lapse (~0.3) requires finest-level sampling. Fix: v0.7.
- **ko_sigma: 0.35** in B2_eq is under active review. Parameter Reference
  recommends ≤ 0.1 for single-puncture stability.
══════════════ END FILE CONTENT ══════════════

  git add VALIDATION_STATUS.md
  git commit -m "docs: add VALIDATION_STATUS.md module matrix"

PRODUCE ARTIFACT: confirm creation and commit hash.
```

---

## MISSION A-14 — Repository Hygiene Pass

```
BRANCH: audit/v0.6.7.2-doc-sync

─────────────────────────────────────────────
Task A — SPDX license header audit:
  grep -rL "SPDX-License-Identifier" src/ include/ 2>/dev/null | grep -E "\.(cpp|hpp)$"
  For each file missing the header, add at line 1-2:
    // SPDX-License-Identifier: GPL-3.0-or-later
    // Copyright (C) 2026 Liran M. Schwartz
  Do not modify any logic below the header.

─────────────────────────────────────────────
Task B — Verify CITATION.cff ORCID:
  grep -n "orcid" CITATION.cff
  Must be exactly: https://orcid.org/0009-0008-8035-1308
  Fix if incorrect.

─────────────────────────────────────────────
Task C — Add project URLs to pyproject.toml (PEP 621):
  Find [project] section. Add after it if not present:
  [project.urls]
  Homepage = "https://github.com/LiranOG/Granite-NR"
  Repository = "https://github.com/LiranOG/Granite-NR"
  "Bug Tracker" = "https://github.com/LiranOG/Granite-NR/issues"

─────────────────────────────────────────────
Task D — Create .github/CODEOWNERS if not present:
  echo "* @LiranOG" > .github/CODEOWNERS

─────────────────────────────────────────────
Task E — Add clang-format CI enforcement:
  In .github/workflows/ci.yml, add to an appropriate job:
  - name: Verify clang-format compliance
    run: |
      find src include -name '*.cpp' -o -name '*.hpp' | \
        xargs clang-format --dry-run --Werror 2>&1 | head -40

─────────────────────────────────────────────
Task F — VORTEX cross-reference check (read-only):
  grep -n "VORTEX\|LiranOG/VORTEX" README.md
  Report whether the link resolves. Do not modify if the VORTEX repo is private.

─────────────────────────────────────────────
Commit:
  git add src/ include/ CITATION.cff pyproject.toml .github/CODEOWNERS \
          .github/workflows/ci.yml
  git commit -m "chore: SPDX headers, ORCID, CODEOWNERS, pyproject URLs, clang-format CI"

PRODUCE ARTIFACT: files missing SPDX headers, ORCID before/after, VORTEX status.
```

---

## MISSION A-15 — Add CI Quality Gates

```
BRANCH: audit/v0.6.7.2-doc-sync

Create .github/workflows/docs-audit.yml with the following content:

══════════════ FILE CONTENT ══════════════
name: Docs & Schema Audit

on:
  push:
    branches: ["**"]
  pull_request:

jobs:
  docs-link-audit:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: "3.11"
      - name: Install dependencies
        run: pip install pyyaml
      - name: Check for stale Granite links
        run: |
          ! grep -Rn "github\.com/LiranOG/Granite[^-N]" \
            README.md docs .github benchmarks scripts viz CHANGELOG.md \
            CITATION.cff pyproject.toml CMakeLists.txt 2>/dev/null
      - name: Run markdown link auditor
        run: python3 scripts/audit_links.py
      - name: Run benchmark YAML schema auditor
        run: python3 scripts/audit_params_schema.py
      - name: Verify version.txt consistency
        run: |
          VER=$(cat version.txt)
          grep -q "VERSION $VER" CMakeLists.txt || \
            (echo "CMakeLists.txt VERSION does not match version.txt ($VER)" && exit 1)
══════════════ END FILE CONTENT ══════════════

  git add .github/workflows/docs-audit.yml
  git commit -m "ci: add docs-link-audit, schema-audit, version-consistency workflow"

PRODUCE ARTIFACT: workflow file content, confirm no push performed.
```

---

## MISSION A-16 — Create Validation Artifact Directory Structure

```
BRANCH: audit/v0.6.7.2-doc-sync

Create the directory structure for reproducibility artifacts.
Do NOT run any simulations — only create the structure and README templates.

  mkdir -p validation/v0.6.7/single_puncture_64
  mkdir -p validation/v0.6.7/B2_eq_64
  touch validation/v0.6.7/single_puncture_64/.gitkeep
  touch validation/v0.6.7/B2_eq_64/.gitkeep

Create validation/README.md:
  # GRANITE-NR Validation Artifacts
  Each subdirectory under validation/vX.Y.Z/ contains the exact params file,
  reproduction command, commit hash, raw log, parsed telemetry, and expected
  metric tolerances for one benchmark run.
  This structure satisfies the reproducibility standard for software paper submission.

Create validation/v0.6.7/single_puncture_64/README.md:
  # Reproducibility Artifact — single_puncture 64³ (v0.6.7)
  Status: Pending — output files to be added after benchmark runs complete.

  ## Reproduction command
  python3 scripts/run_granite.py run benchmarks/single_puncture/params.yaml

  ## Expected outputs (to be populated)
  - params.yaml — exact input file used
  - run.log — raw simulation log
  - telemetry.csv — step-by-step diagnostics
  - metrics.json — summary statistics with tolerances

  ## Expected metric tolerances (NRCF reference)
  - Hamiltonian constraint ‖H‖₂ at t=100M: < 1e-3
  - alpha_center at t=100M (finest AMR level): ~0.3 (trumpet value)

Create validation/v0.6.7/B2_eq_64/README.md with analogous content for B2_eq.

  git add validation/
  git commit -m "docs: add validation artifact directory structure and README templates"

PRODUCE ARTIFACT: directory listing of created files.
```

---

## MISSION A-17 — Create List of GitHub Issues to Open

```
BRANCH: audit/v0.6.7.2-doc-sync

Create audit/issues-to-open.md. The owner will open these manually on GitHub.

══════════════ FILE CONTENT ══════════════
# GitHub Issues to Open — v0.6.7.2 Stabilization Pass
(Owner to open manually after reviewing this list)

## [docs-sync] Wiki ↔ docs/ path realignment
Labels: documentation, v0.7-blocker
Wiki pages reference the old flat docs/ layout reorganized in v0.6.7.2.
All Wiki page links to docs/ files need updating to the nested structure.

## [docs-sync] Parameter Reference schema does not match runtime parser
Labels: documentation, bug
Wiki shows conceptual schema keys not parsed at runtime. Users copying the
template have those sections silently ignored.

## [docs-sync] Resolve B2_eq ko_sigma discrepancy
Labels: documentation, physics, needs-discussion
Parameter Reference recommends ko_sigma <= 0.1; B2_eq uses 0.35.
Needs formal revalidation or correction with documented reasoning.

## [docs-sync] Mark B5_star as non-runnable concept or add runtime adapter
Labels: documentation, enhancement
benchmarks/B5_star/params.yaml uses a concept schema not consumed by the parser.

## [ci] Add markdown link checker
Labels: ci, tooling
scripts/audit_links.py created. Wire into CI to prevent link regression.

## [ci] Add benchmark YAML schema validator
Labels: ci, tooling
scripts/audit_params_schema.py created. Wire into CI.

## [validation] Populate reproducibility artifacts for single_puncture and B2_eq
Labels: validation, documentation
validation/v0.6.7/ structure created. Needs actual run logs and metrics.json
populated after benchmark runs complete.

## [bug] alpha_center diagnostic reads from AMR level 0, not finest level
Labels: bug, diagnostics, v0.7-blocker
Values ~0.94 are an artifact. Fix requires reading from finest AMR level near puncture.
══════════════ END FILE CONTENT ══════════════

  git add audit/issues-to-open.md
  git commit -m "docs: add list of GitHub issues to open for v0.6.7.2 stabilization"

PRODUCE ARTIFACT: confirm file created. Issues are for owner to open manually.
```

---

## ════════════════════════════════════════════════════════
## PART B — IMMEDIATE 24-48H MISSIONS
## P0 and Tier 1 only — fix today and tomorrow
## ════════════════════════════════════════════════════════

Execute these in order. Each one takes 10-30 minutes.

---

### IMMEDIATE-1 — Branch Safety Scan
```
Run Mission 0 in full. Do not skip any step.
Do not proceed until the artifact shows: SAFE TO PROCEED.
Estimated time: 15 minutes.
```

### IMMEDIATE-2 — Stale Links + CMakeLists URL
```
Run Mission A-3 in full.
Acceptance test must return zero grep matches.
Commit: "docs+build: replace all stale Granite links with Granite-NR, fix CMakeLists URL"
Estimated time: 20 minutes.
```

### IMMEDIATE-3 — Version String
```
Run Mission A-4 in full.
CMakeLists.txt VERSION and src/main.cpp banner must both read 0.6.7.2.
Create version.txt. Verify build still passes.
Estimated time: 10 minutes.
```

### IMMEDIATE-4 — README Paths + Anchor
```
Run Mission A-5 in full (nested docs/ paths).
Run Mission A-6 Task A only (broken anchor fix).
Skip Tasks B-E — those are P1.
Commit: "docs: fix README documentation table paths and broken anchor"
Estimated time: 15 minutes.
```

### IMMEDIATE-5 — Wiki Sync Banner + Quick Start Fix
```
Run Mission A-8 Steps 2, 3, 5 only (banner, CLI commands, stale links).
Skip Step 4 (Where to Start section) — nice to have, not urgent.
Stage locally, save diff, do NOT push.
Estimated time: 20 minutes.
```

### IMMEDIATE-6 — Parameter Reference Schema Warning
```
Run Mission A-9 Steps 2 and 3 only (sync warning + YAML template).
Stage locally, append to wiki diff, do NOT push.
Estimated time: 15 minutes.
```

### IMMEDIATE-7 — Benchmark YAML Cleanup
```
Run Mission A-7 in full (B2_eq wording, ko_sigma warning, B5_star concept status).
No numeric values change.
Commit: "docs: B2_eq early-inspiral clarification, ko_sigma warning, B5_star concept status"
Estimated time: 10 minutes.
```

### IMMEDIATE-8 — AGENTS.md + SOURCE_OF_TRUTH.md
```
Run Mission A-1 and Mission A-2.
Two separate commits.
Estimated time: 5 minutes.
```

### IMMEDIATE-9 — README Sync Banner
```
In README.md, immediately below the Documentation section heading, insert:

> 📋 **Docs sync in progress:** GRANITE-NR completed a repository restructuring
> in v0.6.7.2 that reorganized the `docs/` tree and renamed the repository from
> `Granite` to `Granite-NR`. Some documentation paths, Wiki pages, and older
> links may temporarily be inconsistent with the current codebase. The root
> `README.md`, `docs/getting_started/Installation.md`, and the YAML files under
> `benchmarks/` are the current source of truth while the Wiki and long-form docs
> are being brought into alignment. I am working through these inconsistencies as
> part of the active v0.6.7.x stabilization pass.

Commit: "docs: add documentation sync notice to README"
Estimated time: 5 minutes.
```

### IMMEDIATE-10 — Final Validation & Readiness Check
```
After all IMMEDIATE missions are committed locally, run:

  # Zero stale links
  grep -Rn "github\.com/LiranOG/Granite[^-N]" \
    README.md docs .github benchmarks scripts viz CHANGELOG.md \
    CITATION.cff pyproject.toml CMakeLists.txt 2>/dev/null

  # Tooling (if created)
  python3 scripts/audit_links.py 2>/dev/null || echo "Not yet created"
  python3 scripts/audit_params_schema.py 2>/dev/null || echo "Not yet created"

  # Build and tests still pass
  python3 scripts/run_granite.py build --build-type Release 2>&1 | tail -10
  PYTHONPATH=python pytest -q tests/python 2>&1

  # Git log
  git log --oneline main..HEAD
  git diff main..HEAD --stat

  # Wiki diffs
  cat audit/wiki-home-changes.diff | wc -l
  cat audit/wiki-param-ref-changes.diff | wc -l

PRODUCE FINAL ARTIFACT: audit/IMMEDIATE-complete.md containing all outputs above.
Close with: "All IMMEDIATE missions complete. No remote push performed.
To publish: git push origin audit/v0.6.7.2-doc-sync (main repo)
and push Granite-NR.wiki repo separately after owner review of audit/*.diff files."
```

---

## ════════════════════════════════════════════════════════
## PART C — BANNERS (paste directly, no Antigravity needed)
## ════════════════════════════════════════════════════════

---

### WIKI HOME — Paste below the H1 title

```markdown
> 📋 **Documentation sync in progress — v0.6.7.x**
> GRANITE-NR recently underwent a repository restructuring that reorganized the
> `docs/` directory tree and renamed the repository from `Granite` to `Granite-NR`.
> This Wiki is currently being synchronized with the updated `README.md`, `docs/`,
> CLI interface, and benchmark YAML schema. During this transition some links,
> command examples, or parameter names may temporarily lag behind the current
> codebase. For runnable commands and reliable paths, prefer the root `README.md`,
> `docs/getting_started/Installation.md`, and the YAML files under `benchmarks/`
> until this notice is removed.
```

---

### README — Paste at the top of the Documentation section

```markdown
> 📋 **Docs sync in progress:** GRANITE-NR completed a repository restructuring
> in v0.6.7.2 that reorganized the `docs/` tree and renamed the repository from
> `Granite` to `Granite-NR`. Some documentation paths, Wiki pages, and older
> links may temporarily be inconsistent with the current codebase. The root
> `README.md`, `docs/getting_started/Installation.md`, and the YAML files under
> `benchmarks/` are the current source of truth while the Wiki and long-form docs
> are being brought into alignment. I am working through these inconsistencies as
> part of the active v0.6.7.x stabilization pass.
```

---

## Recommended Commit Order

```
commit 01: chore: add AGENTS.md agent safety contract
commit 02: docs: add SOURCE_OF_TRUTH.md registry
commit 03: docs+build: replace all stale Granite links with Granite-NR, fix CMakeLists URL
commit 04: build: propagate v0.6.7.2 patch version to CMake, source banner, pyproject
commit 05: docs: fix README documentation table paths and broken anchor
commit 06: docs: add documentation sync notice to README
commit 07: docs: B2_eq early-inspiral clarification, ko_sigma warning, B5_star concept status
commit 08: docs: add VALIDATION_STATUS.md module matrix
commit 09: tooling: add benchmark YAML schema auditor
commit 10: tooling: add markdown link auditor
commit 11: chore: SPDX headers, ORCID, CODEOWNERS, pyproject URLs, clang-format CI
commit 12: ci: add docs-link-audit, schema-audit, version-consistency workflow
commit 13: docs: add validation artifact directory structure
commit 14: docs: add GitHub issues list for v0.6.7.2 stabilization pass
```
