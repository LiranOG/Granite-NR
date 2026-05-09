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
