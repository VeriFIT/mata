# Experiments

Records of experiments, mostly performance and design ideas, that did not make it into the code. They exist so that
nobody spends a week repeating an experiment that has already been run, and so that the conditions under which an
idea becomes worth revisiting are written down.

Before starting a performance or design change, check here whether it has been tried.

## Index

| id                                      | title                                               | status   |
| --------------------------------------- | --------------------------------------------------- | -------- |
| [EXP-1](1-ordvector-flat-set/README.md) | Replace `OrdVector` with `std::flat_set`            | rejected |
| [EXP-1](1-ordvector-flat-set/README.md) | Change `OrdVector` merge, search, and storage       | rejected |
| [EXP-2](2-nft-bdd-automata/README.md)   | Use NFT levels as BDDs on automaton transitions     | rejected |
| [EXP-3][exp-3]                          | Adaptive merging and universality through inclusion | rejected |
| [EXP-5][exp-5]                          | Shared SCC-based epsilon closure                    | rejected |
| [EXP-6][exp-6]                          | Scan for already-contained OrdVector ranges         | rejected |

[exp-3]: 3-antichain-iterator-universality/README.md
[exp-5]: 5-shared-closure-helper/README.md
[exp-6]: 6-ordvector-subset-guard/README.md

## Adding a record

- Take the next free number `N` and a short slug: `N-slug.md`. Refer to the record as `EXP-N` in commits, issues and
  code comments.
- Start with a single file. When the record needs extra material (patches, scripts, full result tables), make it a
  directory `N-slug/` with the record in `README.md` and the material next to it.
- Keep the code that was tried as `git format-patch` output against the recorded base commit, so it stays
  reproducible after the experiment branches are deleted.
- Commit summaries, scripts and small inputs only. Raw measurements are not kept in the repository or referenced from
  it; instead, the record says how to regenerate them (the Reproducing section).
- Discussion belongs to an issue or a GitHub Discussion; link it from the front matter.
- Add the record to the index above.

## Template

```markdown
---
id: EXP-N
title: One line
status: rejected # rejected | inconclusive | superseded | partially adopted (link the PR)
date: YYYY-MM-DD
base: <commit> (<branch>)
related: [] # quoted issues and PRs, e.g. ["#123", "PR #456"]: an unquoted # starts a YAML comment
discussion: # optional link
tags: []
---

# EXP-N: Title

## Question

## What was tried

## Method

## Results

## Reproducing

Commands to rebuild the variants and regenerate the raw measurements behind the results.

## Conclusion

Why it did not land, and what would make it worth revisiting.

## Side findings
```
