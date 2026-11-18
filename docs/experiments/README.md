# Experiments

Records of experiments, mostly performance and design ideas, that did not make it into the code. They exist so that
nobody spends a week repeating an experiment that has already been run, and so that the conditions under which an
idea becomes worth revisiting are written down.

Before starting a performance or design change, check here whether it has been tried.

## Index

| id                                      | title                                                                             | status   |
| --------------------------------------- | --------------------------------------------------------------------------------- | -------- |
| [EXP-1](1-ordvector-flat-set/README.md) | Replace `OrdVector` with `std::flat_set`, or change its merge, search and storage | rejected |

## Adding a record

- Take the next free number `N` and a short slug: `N-slug.md`. Refer to the record as `EXP-N` in commits, issues and
  code comments.
- Start with a single file. When the record needs extra material (patches, scripts, full result tables), make it a
  directory `N-slug/` with the record in `README.md` and the material next to it.
- Keep the code that was tried as `git format-patch` output against the recorded base commit, so it stays
  reproducible after the experiment branches are deleted.
- Commit summaries, scripts and small inputs only. Large raw data stays outside the repository; say where it is.
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
related: [] # issues, PRs
discussion: # optional link
tags: []
---

# EXP-N: Title

## Question

## What was tried

## Method

## Results

## Conclusion

Why it did not land, and what would make it worth revisiting.

## Side findings
```
