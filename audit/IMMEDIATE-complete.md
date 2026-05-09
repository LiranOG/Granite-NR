# IMMEDIATE-10: Final Validation Scan

## 1. Stale Link Check

$ git grep -E "github\.com/LiranOG/Granite([^-\w]|$)" README.md docs .github benchmarks scripts viz CHANGELOG.md CITATION.cff pyproject.toml CMakeLists.txt


## 2. Tooling Checks

$ python scripts/audit_links.py
Not yet created

$ python scripts/audit_params_schema.py
Not yet created

## 3. Build Check (Expect HDF5 Failure)

$ python scripts/run_granite.py build --build-type Release
05:10:09 ERROR    [granite] 'cmake' not found in PATH.  Install CMake >= 3.18 from https://cmake.org/download/

## 4. Python Test Suite (Expect 8-test baseline)

$ set PYTHONPATH=python&& pytest -q tests/python
'pytest' is not recognized as an internal or external command,
operable program or batch file.

## 5. Git State

$ git log --oneline main..HEAD
f9ad020 docs: add documentation sync notice to README
02c78e3 docs: B2_eq early-inspiral clarification, ko_sigma warning, B5_star concept status
c86eaaa docs: fix README documentation table paths and broken anchor
3e8ecb6 build: propagate v0.6.7.2 patch version to CMake, source banner, pyproject
9aca501 docs+build: replace all stale Granite links with Granite-NR, fix CMakeLists URL
45d7fb0 docs: add SOURCE_OF_TRUTH.md registry
caf2fac chore: add AGENTS.md agent safety contract

$ git diff main..HEAD --stat
.github/CONTRIBUTING.md                 |  2 +-
 .github/ISSUE_TEMPLATE/bug_report.md    |  4 +-
 AGENTS.md                               | 29 +++++++++++++
 CITATION.cff                            |  4 +-
 CMakeLists.txt                          |  4 +-
 README.md                               | 75 ++++++++++++++++++---------------
 benchmarks/B2_eq/params.yaml            | 18 +++++++-
 benchmarks/B5_star/params.yaml          |  7 +++
 benchmarks/scaling_tests/README.md      |  2 +-
 docs/SOURCE_OF_TRUTH.md                 | 20 +++++++++
 docs/citation.bib                       |  2 +-
 docs/design/DEVELOPMENT_JOURNAL.md      | 50 +++++++++++-----------
 docs/design/GENESIS_AND_ARCHITECTURE.md |  2 +-
 docs/developer_guide/DEVELOPER_GUIDE.md |  2 +-
 docs/paper/granite_preprint_v067.tex    |  4 +-
 docs/theory/gw_extraction.rst           |  2 +-
 docs/theory/initial_data.rst            |  2 +-
 docs/user_guide/BENCHMARKS.md           |  2 +-
 pyproject.toml                          |  2 +-
 src/main.cpp                            |  2 +-
 version.txt                             |  1 +
 viz/vortex_eternity/README.md           | 10 ++---
 22 files changed, 163 insertions(+), 83 deletions(-)

## 6. Wiki Diff Sizes

$ wc -l audit/wiki-home-changes.diff
497 audit/wiki-home-changes.diff

$ wc -l audit/wiki-param-ref-changes.diff
647 audit/wiki-param-ref-changes.diff