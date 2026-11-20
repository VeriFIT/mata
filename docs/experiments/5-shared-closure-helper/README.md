---
id: EXP-5
title: Share an SCC-based epsilon closure between NFA epsilon removal and NFT project_out
status: rejected
date: 2026-11-20
base: ba4ac400d0294eb9433ddbba39e2e63f959c4d9f (devel)
related: ["#240", "#753"]
tags: [nfa, nft, epsilon-closure, strongly-connected-components]
---

# EXP-5: Shared SCC-based epsilon closure

## Question

koniksedy's follow-up to #753 asks whether NFT `project_out()` and NFA `remove_epsilon()` can use one closure helper with an edge predicate, without assuming an NFT level order.

## What was tried

[`rejected.patch`](rejected.patch) adds an internal `EdgeComponents` helper. NFA epsilon removal uses its SCC decomposition instead of its specialized closure, and `project_out()` composes successor-component closures bottom-up into the flat closure buffers introduced by #1083. It is applied against the base above with `git apply rejected.patch`.

## Method

A Release library was built for both variants. One pinned core, five interleaved runs, ten repetitions per measurement:

- `project_out({1})` on a two-level, 1000-state chain with 1999 input transitions.
- NFA `remove_epsilon()` on a 1000-state forward chain with 999 epsilon and 500 ordinary transitions.

The outputs had the same state and transition counts in both variants.

## Results

| operation (10 repetitions) |    devel | candidate |
| -------------------------- | -------: | --------: |
| projection                 | 2.930 ms |  3.315 ms |
| NFA epsilon removal        | 4.227 ms |  6.812 ms |

The candidate was 1.131x slower on projection and 1.612x slower on epsilon removal (ratios slower/faster). Revisit if a shared helper can avoid component bookkeeping on the acyclic common case while retaining the specialized fast paths.

## Reproducing

Build the base and patched workspaces in Release with `make release BUILD_DIR=build/release`. Link the same standalone probe against each `build/release*/src/libmata.a`; its sources are derivable from the operations listed in Method. Alternate the executables five times on one pinned core.
