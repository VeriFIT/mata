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

[`rejected.patch`](rejected.patch) adds an internal `EdgeComponents` helper. NFA epsilon removal uses its SCC decomposition instead of its specialized closure, and `project_out()` composes successor-component closures bottom-up into the flat closure buffers introduced by #1083. It is `git format-patch` output against the base above; apply it with `git am rejected.patch`.

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

Create the base checkout and a patched one with `git am rejected.patch`, then build each with `make release-lib
BUILD_DIR=build/release`. [`probe_closure.cc`](probe_closure.cc) is the exact probe: `projection` builds a two-level
chain of 1000 level-0 states, each `q -0-> n+q -EPSILON-> q+1`, with initial state 0 and final state 999, and times
`project_out({1}, RepeatSymbol)`. `epsilon` builds a 1000-state NFA from 0 to 999 with every `q -EPSILON-> q+1` and
`q -0-> q+1` on even `q`, then times `remove_epsilon()`. Each run executes the operation ten times and prints the total
milliseconds and a checksum. Compile it against each library:

```sh
g++ -std=gnu++23 -O3 -DNDEBUG -Iinclude -Ibuild/release/include -I3rdparty/simlib/include -I3rdparty/cudd/include \
  docs/experiments/5-shared-closure-helper/probe_closure.cc build/release/src/libmata.a -lpthread -o probe
```

Alternate the base and candidate executables five times on one pinned core for each mode and compare medians.
