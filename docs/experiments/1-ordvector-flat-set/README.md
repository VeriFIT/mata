---
id: EXP-1
title: Replace OrdVector with std::flat_set, or change its merge, search and storage
status: rejected
date: 2026-11-18
base: 53021908 (devel)
related: [PR #1067]
tags: [containers, performance, allocator]
---

# EXP-1: OrdVector vs `std::flat_set`, and alternative merge, search and storage

## Question

C++23 brings `std::flat_set`, which is a sorted, duplicate-free vector like `mata::utils::OrdVector`. Would
replacing `OrdVector` with it, or adopting its algorithms, make Mata faster? And would anything else in
`OrdVector` (merge, search, storage) pay off?

## What was tried

Each variant is one patch against `53021908` in [`patches/`](patches/).

| variant           | change                                                                                                    |
| ----------------- | --------------------------------------------------------------------------------------------------------- |
| `flatset-merge`   | `OrdVector::insert(span)` as libstdc++'s `flat_set` does it: append, `std::inplace_merge`, `std::unique`. |
| `backmerge`       | `OrdVector::insert(span)` merges back to front inside the grown vector; no temporary buffer.              |
| `append-fastpath` | single-key `insert` appends keys above the maximum without a search.                                      |
| `branchless-lb`   | branchless lower bound in `OrdVector` and `SmallOrdVector` lookups and inserts.                           |
| `delta-pool`      | reference: the existing `MATA_DELTA_ALLOCATOR=pool` made the build default.                               |
| `stateset-pool`   | `delta-pool` + `StateSet` allocating from the same pool (the storage experiment).                         |

`std::flat_set` itself was not dropped in for `OrdVector`: its elements are immutable and its container is reachable
only through `extract()`/`replace()`, while Mata mutates `SymbolPost`s in place inside a `StatePost`, appends unsorted
elements and sorts afterwards, and uses `reserve`, `to_vector`, `rename` and `filter_indexes`. It also needs GCC 15 or
libc++ 21, above Mata's minimum of GCC 14 and Clang 18. It was measured directly in the microbenchmark instead.

A small-buffer `StateSet` was not built: small inline sets already exist where sets are small and numerous
(`SymbolPost::targets` is a `SmallOrdVector`, and determinization keeps its macrostates in its own `StateSetTable`).
`stateset-pool` makes `StateSet` allocations nearly free, which bounds what inline storage could save.

## Method

- **Profile:** `perf record` of a `RelWithDebInfo` build of `devel` over 40 sampled instances per experiment,
  samples attributed to the source file of the innermost inlined frame ([`tools/shares.sh`](tools/shares.sh)).
- **Microbenchmark:** [`tools/micro.cc`](tools/micro.cc), `OrdVector` vs `std::flat_set` vs the merge and search
  variants on `uint32_t` sets, `g++ 16.2 -O3 -DNDEBUG`, pinned to one core, minimum of 5.
- **End to end:** the TACAS'24 Mata experiments through the automata-lib-bench `mata` interpreter builds
  ([`tools/run.py`](tools/run.py), [`tools/analyse.py`](tools/analyse.py)). All builds ran on each instance in
  shuffled order, 3 times, pinned to one core; the minimum counts. Timeout 30 s, memory limit 7 GB (heavy
  experiments) or 3 GB. Metric: operation time, i.e. `overall - mataparsing - mataminterm`. A byte copy of the
  `devel` binary (`devel-aa`) ran as an A/A control. 7,823 instances measured on every build, 188k process runs.
- **Correctness:** every variant passed the full C++ test suite, and no instance gave a result different from
  `devel`.
- `nfa-bench` at `1390e4ba0`, since the commit automata-lib-bench pinned was not on origin. The raw measurements
  (JSONL) are not committed; they stayed in the local automata-lib-bench checkout (`results/data/ordvector-exp/`).

## Results

Full tables in [`results.md`](results.md). Total operation time against `devel`:

| experiment           | devel s | A/A   | flatset-merge | backmerge | append-fastpath | branchless-lb | delta-pool | stateset-pool |
| -------------------- | ------- | ----- | ------------- | --------- | --------------- | ------------- | ---------- | ------------- |
| armc-incl            | 8.61    | 0.990 | 0.987         | 0.986     | 0.982           | 0.982         | 0.983      | 0.975         |
| noodler-concat       | 10.83   | 1.002 | 0.999         | 1.000     | 1.006           | 1.003         | 0.959      | 0.954         |
| param-inter          | 14.79   | 1.015 | 0.991         | 1.001     | 0.998           | 1.004         | 0.831      | 0.827         |
| param-inter-variadic | 36.85   | 0.972 | 0.945         | 0.974     | 0.983           | 0.963         | 0.830      | 0.829         |
| param-union-variadic | 15.33   | 0.997 | 1.000         | 0.990     | 1.001           | 0.995         | 1.004      | 0.991         |
| regexps-det-min      | 17.90   | 1.025 | 1.015         | 0.976     | 1.042           | 0.976         | 0.988      | 0.994         |
| email-filter         | 0.73    | 1.001 | 1.000         | 0.988     | 1.001           | 0.999         | 0.901      | 0.919         |

- **Noise floor:** the identical binary differs by up to ±3% per experiment. All four `OrdVector` variants stay
  within it everywhere.
- **Profile:** code from `ord-vector.hh` is at most 0.06% of samples in any experiment. The hot set code is
  `SmallOrdVector` (up to 7%) and determinization's own `StateSetTable`.
- **Microbenchmark** (ns per operation): `std::flat_set` and the `flatset-merge` strategy are 1.25–1.7× slower than
  `OrdVector`'s `set_union` into a fresh buffer; merging back to front loses up to 5× when a small set takes in a
  large one. The append fast path is 4–6× faster for ascending keys; the branchless search is 2.3× faster on 65,536
  elements and slightly slower on 8–64.
- **Storage:** `stateset-pool` matches `delta-pool` (all-instance geomean 0.971 vs 0.975), so `StateSet` allocations
  cost nothing measurable.

## Conclusion

Rejected. `std::flat_set` is the same data structure as `OrdVector`, its range insert is slower, and its interface
cannot express how Mata uses `OrdVector`. None of the alternative algorithms moves end-to-end time, because
`OrdVector`'s own operations are not on the hot paths: the hot sets are `SmallOrdVector` and dedicated structures.

Worth revisiting only if a profile shows `ord-vector.hh` (or `std::lower_bound`/`std::set_union` called from it)
taking a real share of time. If so, the append fast path and the branchless search are the candidates, not
`std::flat_set`.

## Side findings

- `OrdVector::to_vector_mut()` was `const` while returning a mutable reference, and `count()` scanned linearly; both
  fixed in PR #1067.
- The existing pool allocator (`MATA_DELTA_ALLOCATOR=pool`) is 17% faster in total on `param-inter` and
  `param-inter-variadic` and 10% on `email-filter`, but 4.7% slower by geomean on `noodler-inter`'s ~1 ms instances.
  Whether to make it the default is a separate decision.
- With the default `std` backend, the allocator dispatch (`backend()` in `delta-allocator`) took 4.7% of samples in
  the `param-inter` profile.
