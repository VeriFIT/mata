---
id: EXP-6
title: Scan for already-contained ranges before merging OrdVector
status: rejected
date: 2026-11-23
base: ff222544b98979dd831c594c8ce5b9f7be738103 (devel)
related: ["#1084", "EXP-1"]
tags: [utils, ord-vector, subset-construction]
---

## Question

Does skipping `OrdVector::insert(std::span<const Key>)`'s merge when the inserted keys are
already a subset improve performance, as requested by #1084?

## What was tried

[`rejected.patch`](rejected.patch) adds `std::ranges::includes(vec_, keys)` between the append
fast path and the allocation and `std::ranges::set_union` of the overlapping-insertion path.
It also adds proper-subset and self-insertion checks to the existing regression tests.

The earlier EXP-1 merge/storage experiments did not measure this exact guard.

## Method

[`probe_subsets.cc`](probe_subsets.cc) was compiled with GCC 16.2, `-std=gnu++23 -O3
-DNDEBUG`, separately against `devel` and the candidate header. On one pinned core, five
old/new interleaved runs measured the median of 2,000 calls in each mode:

- **subset:** an ordered set `{0, ..., 9999}`, with 1,000 already-contained keys `{1000, ..., 1999}`.
- **append-only:** the same set with a disjoint sorted suffix `{10000, 10002, ..., 11998}`, which
  exercises the append fast path rather than the pre-scan.
- Both executable variants also checked 500 seeded cases against a sorted, deduplicated
  `std::vector`, including self-insertions. Every case agreed and exited zero.

## Results

| mode (2,000 calls) |     devel | subset pre-scan | slower/faster |
| ------------------ | --------: | --------------: | ------------: |
| subset             | 27.417 ms |       27.713 ms |        1.011x |
| append-only        | 27.172 ms |       35.666 ms |        1.313x |

## Conclusion

The direct subset pre-scan did not show a worthwhile benefit. The smallest measured subset
case was slightly slower, and the adjacent append-only test slowed materially. This test does
not measure a realistic end-to-end workload dominated by subset insertion, so a future variant
would need both a better algorithm and measurements over such a workload. Adding an extra
linear scan to all overlapping non-subset unions is not justified by these results.

## Reproducing

Read `devel` at the recorded base. Preserve the experiment directory in the base checkout.
First build `probe-old` from the unchanged tree, then apply the candidate with `git am
docs/experiments/6-ordvector-subset-guard/rejected.patch` and build `probe-new`. Keep both
executables:

```sh
g++ -std=gnu++23 -O3 -DNDEBUG \
  -Iinclude -I<configured-build>/include -I3rdparty/simlib/include -I3rdparty/cudd/include \
  docs/experiments/6-ordvector-subset-guard/probe_subsets.cc -o probe-old
g++ -std=gnu++23 -O3 -DNDEBUG \
  -Iinclude -I<configured-build>/include -I3rdparty/simlib/include -I3rdparty/cudd/include \
  docs/experiments/6-ordvector-subset-guard/probe_subsets.cc -o probe-new
```

For five rounds, alternate `taskset --cpu-list 3 ./probe-old <mode>` and
`taskset --cpu-list 3 ./probe-new <mode>`, with `<mode>` set to `subset` or `append-only`.
Compute medians independently for both executables.
