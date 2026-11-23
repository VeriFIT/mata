---
id: EXP-7
title: Build composition posts with push_back and sort_and_rmdupl
status: rejected
date: 2026-11-23
base: a803d22d11c5d54f12a61c497bebbcb3052d096b (devel)
related: ["#817"]
tags: [nft, composition, symbol-post, sorting]
---

## Question

Can fast composition avoid quadratic element movement when a newly created `SymbolPost` revisits existing pair states
by appending targets unsorted and sorting once, as proposed for the small-post part of #817's adaptive outgoing-post
builder?

## What was tried

[`rejected.patch`](rejected.patch) changes the eight target-collection sites in `compose_fast_with()` from one
`SymbolPost::insert(State)` per target to `push_back()` followed by one `targets.sort_and_rmdupl()` before
`insert_symbol_post_to_delta()`. The sites are both paths in `combine_targets`, the three paths in `copy_transition`,
both posts in `model_waiting`, and `process_fast_epsilon_transitions`.

`insert()` appends an increasing target but otherwise binary-searches and shifts the vector tail. Newly created pair
states arrive in increasing order, whereas revisiting `k` existing pairs can incur `O(k^2)` element moves. Appending
and sorting preserves the sorted duplicate-free target set, and every sort happens before any reader sees the post.

The patch is `git format-patch` output for the ported commit `afc72ac6157af54a7aed21d740ee14aa808944a0` against
the base above; apply it with `git am rejected.patch`. The original donor was
`344cc5bdf01477e7bc9a2b09a0ef8c37e3d9a0fb`, based on `58fc4a257e49ec2f78f25156948261c1d40dd9f9`.

## Method

Release builds of the base and candidate ran `mata::nft::compose(lhs, rhs, 1, 0, project_out, NoJump, Auto)` under
Callgrind with `--collect-atstart=no --toggle-collect='run_compose*'`, so Ir covers only the compose call. The corpus
was a rank-stratified sample of automata-lib-bench `suites/nft.toml` inputs: 12 evenly spaced by output size plus the
largest below an 11M-transition cap from each group, giving 55 files, plus five large random two-level pairs with up
to 23M output transitions.

Output identity compared the states, transitions, and an FNV hash of levels, initial/final states, and delta. All 6063
corpus outputs (2,879 `pair`, 2,580 `construct_replace`, 168 `sync_1_0`, and 436 `sync_0_0`) and all random cases
were byte-identical between variants.

## Results

Aggregate instruction counts, with ratios candidate/base:

| group                           |  cases |             Ir base |        Ir candidate | candidate/base |
| ------------------------------- | -----: | ------------------: | ------------------: | -------------: |
| `composition/pair`              |     14 |      42,153,794,029 |      43,506,278,298 |         1.0321 |
| `composition/construct_replace` |     14 |       4,746,629,486 |       4,822,778,175 |         1.0160 |
| `length_formula/sync_1_0`       |     14 |      30,932,182,907 |      30,822,847,211 |         0.9965 |
| `length_formula/sync_0_0`       |     13 |      21,902,163,771 |      20,802,546,028 |         0.9498 |
| random                          |      5 |      39,742,255,487 |      38,996,024,053 |         0.9812 |
| **total**                       | **60** | **139,477,025,680** | **138,950,473,765** |     **0.9962** |

The three largest `pair` cases all regressed:

| abbreviated case                                                      | output transitions | candidate/base |
| --------------------------------------------------------------------- | -----------------: | -------------: |
| `non-incremental-QF_SLIA-202303…e-escapeString-smt2_reverse_331.mata` |          4,252,702 |         1.0318 |
| `non-incremental-QF_SLIA-202303…lUnescape-escapeString-smt2_335.mata` |         10,477,174 |         1.0316 |
| `non-incremental-QF_SLIA-202303…e-escapeString-smt2_reverse_335.mata` |         10,489,746 |         1.0327 |

Nearly every target in these cases is a freshly created pair state, so `insert()` already appends and the additional
`is_sorted` check is pure cost. These three cases contribute 42.1G base instructions, dominating the aggregate result.

The largest wins were instead on length-formula and dense-random shapes: 0.8790x on the best `sync_0_0` case
(10,278,622 output transitions), and 0.8925x and 0.9027x on the two dense-random cases. They did not offset
regressions on the largest realistic composition inputs or the 1.0160x `construct_replace` regression.

## Reproducing

Create base and candidate checkouts at `a803d22d11c5d54f12a61c497bebbcb3052d096b`, apply `rejected.patch` in the
candidate with `git am`, and build both with `just cpp build release`. Use the rank-stratified sampling and Callgrind
command above on the four automata-lib-bench groups. Generate the five random cases with parameters `300,4,3,100,1`;
`1000,3,4,50,2`; `500,6,2,200,4`; `300,8,2,0,5`; and `150,8,2,0,6`. Sum Ir by group and overall, then compare native
output hashes over the complete corpora and all random cases.

## Conclusion

Rejected as a uniform replacement: the aggregate 0.9962x hides a 1.0321x regression on `pair`, including 1.0316x to
1.0327x on its three largest inputs, and a 1.0160x regression on `construct_replace`, against wins concentrated on
length-formula and dense-random shapes. Revisit as part of #817's adaptive builder: append and sort for small posts,
but use a generation-stamped set for high-duplication posts and retain a path that does not penalize monotonically
created fresh pair states.

## Side findings

An earlier storage experiment replaced `StatePost::push_back` and `StatePost::insert` more broadly by concatenating and
sorting entire posts. It was also rejected after mixed direct solver results and slower direct composition/apply
comparisons. That whole-post rewrite is not the variant measured here: EXP-7 changes only freshly created, block-local
`SymbolPost`s at eight fast-composition sites, and its measurements must not be attributed to the broader rewrite.
