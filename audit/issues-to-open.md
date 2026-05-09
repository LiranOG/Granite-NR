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
