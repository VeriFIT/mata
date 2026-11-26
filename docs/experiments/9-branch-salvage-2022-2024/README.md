---
id: EXP-9
title: Salvage audit of retired fork branches (2022–2024)
status: superseded
date: 2026-11-26
base: 4a94d3f2c (fork/uninstall, fork/moves, fork/transducers)
related: []
tags: ["nft", "nfa", "history", "branches"]
---

## Question

The fork `Adda0/mata` carries branches `transducers` (alias `fix_create_identity`,
identical tip `c5f83660b`), `moves`, `uninstall`, and the `noodler_main*` family. Which of
their commits are worth landing in `devel`, and which are already there under another
implementation, so the branches can be deleted without losing work?

## What was tried

Every commit on the three branches was checked against `origin/devel` with `git cherry`
(patch-id equivalence), by commit-subject search in the merged PR history, and by reading
the current API that superseded each idea.

- `fork/uninstall` (1 commit, 2023-08): `scripts/uninstall.sh` plus a `make uninstall`
  target. Superseded by the CMake uninstall target generated from
  `cmake_uninstall.cmake.in` (wired in `CMakeLists.txt`, exposed as `make uninstall` in
  the top-level `Makefile`). Nothing to salvage.
- `fork/moves` (15 commits, 2023-08): `Delta`/`Transitions` iterator refactor ending in
  iterating epsilon and non-epsilon moves separately
  (`349b66e9a`). The final API exists in `devel` as `StatePost::moves_epsilons()` and
  `StatePost::moves_symbols()` with tests (`tests/nfa/delta.cc`); the intermediate
  refactorings were redone by the iterator work in PR #1086. Nothing to salvage.
- `fork/transducers` == `fork/fix_create_identity` (134 commits, 2024-04/05): the
  `lvlfa` → `nft` split and level-based string solving. The directory split, the levels
  API, `insert_identity()` with `JumpMode`, `insert_word_by_levels()`, the reluctant
  replace construction using `insert_identity()`, and the "bugfix: replace from Noodler"
  regression all landed through later, redesigned work (PRs #511, #1008, #1012, #1013,
  and the composition/projection API in `src/nft/composition.cc`). The branch's own
  `print_to_dot` markers (`EPSILON - 99`) were replaced by the explicit `end_marker`
  symbol parameters in `src/applications/strings/replace.cc`. Nothing to salvage.
- The four `noodler_main*` branches are strict ancestors of
  `fork/python_noodler_final` (marked "DO NOT DELETE THIS BRANCH" as the archived Python
  Noodler snapshot); their performance work landed in late 2026 via the issue-972/207 PR
  series. Only the archive marker branch is kept.

The code of the three retired branches is preserved next to this record:
[transducers.patch](transducers.patch) is the squashed diff of the branch against its base
(sources and headers only; the test files landed in `devel` under `tests/nft/` and
`tests/applications/strings/`, so they are not repeated),
[moves.patch](moves.patch) and [uninstall.patch](uninstall.patch) are `git format-patch`
series. All are against the recorded base commits below, so they stay reproducible after
the remote branches are deleted.

## Method

`git merge-base --is-ancestor`, `git cherry`, `git grep` against `origin/devel` trees,
and `gh pr list --author Adda0` cross-referenced by head branch name and commit subject.

## Reproducing

```sh
git fetch fork && git fetch origin
git merge-base --is-ancestor fork/noodler_main fork/noodler_main_fixed   # family chain
git cherry origin/devel fork/transducers                                  # non-equivalent commits
git grep -n "moves_epsilons" origin/devel -- include tests                # supersession evidence
```

## Conclusion

All three branches are fully superseded; no code change is proposed. Delete
`fork/uninstall`, `fork/moves`, `fork/transducers`, `fork/fix_create_identity` and the
`noodler_main`/`noodler_main_fixed`/`noodler_main_optimized`/
`noodler_main_optimized_fixed` predecessors; keep `fork/python_noodler_final` as the
explicit archive.

## Side findings

- The supersession map also cleared 15 smaller fork branches (stale `devel`/`master`/
  `gh-pages` mirrors, `afa`, CI/docs fixes already applied or obsolete, `devenv`,
  `fix_true_false_in_mata_files`), enabling a single fork-wide cleanup.
- `fork/docs-script-examples` was excluded: it is the live head of open PR #1146.
