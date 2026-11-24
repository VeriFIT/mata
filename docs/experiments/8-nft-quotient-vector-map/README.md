---
id: EXP-8
title: Use a state-indexed vector for NFT quotient renaming
status: rejected
date: 2026-11-24
base: 6b98aa37f9a15340bb3d2f149a8255615112fbfc
related: ["#803"]
tags: ["nft", "simulation", "allocation"]
---

# EXP-8: Use a state-indexed vector for NFT quotient renaming

## Question

Can `reduce_size_by_simulation()` replace its temporary `StateRenaming` hash map with a state-indexed vector of reduced-state IDs, avoiding per-state `contains()` and `.at()` calls without changing the quotient or the public renaming?

## What was tried

Scan quotient representatives in increasing state order. Allocate a reduced state only on the first occurrence of a representative and copy its class ID to the rest of that representative's members. Store those IDs in `vector<State>`, use the vector for all subsequent lookups, and materialize the public `StateRenaming` only at the end with `clear()`, `reserve()`, and `emplace()`.

The probe includes the candidate's multi-level regression with `0` and `1` being simulation equivalent while `2` strictly simulates `3`. It checks the exact renaming, levels, initial/final sets, and pruned transition. `quot_proj[q]` is the least state equivalent to `q`, so initialization and class numbering stay identical to the hash-map implementation. Dominance remains `sim_relation.get(s, p)`: `p` simulates `s`.

The candidate is in [change.patch](change.patch). Its implementation is limited to `src/nft/operations.cc`; the regression is in `tests/nft/nft.cc`.

## Method

The standalone [probe.cc](probe.cc) creates two isomorphic 50-state components in a single-level NFT. It requests a renaming into maps pre-seeded with stale entries, clears its allocation counters before calling `reduce()`, and records `operator new` calls and requested bytes. It also checks all 100 state mappings and requires adjacent output fields to be identical.

For whole-call timing, the probe runs 2,000 reductions of the same unchanged input with the renaming supplied. The baseline and candidate used identical GCC `-O3 -DNDEBUG` flags and Release `libmata.a` builds of `6b98aa37f9a1` and the patch respectively. Four alternating and interleaved runs on a single pinned core produced the medians below. No concurrent timings are included.

## Results

| NFT reduction                   |        Old |        New |                         Change |
| ------------------------------- | ---------: | ---------: | -----------------------------: |
| Allocation calls in one call    |      3,245 |      3,243 |                    −2 (−0.06%) |
| Requested bytes in one call     |    247,520 |    246,920 |                  −600 (−0.24%) |
| Median of four 2,000-query runs | 650.261 ms | 648.313 ms | 0.300% faster (speedup 1.003x) |

The output maps and semantic fields are identical for both implementations. The candidate's focused `mata::nft::reduce_size_by_simulation()` test also passes with 71 assertions.

The allocation saving is limited to bucket rehashes versus one vector, not eliminating output-map materialization. For full reductions the measured timing improvement is far below a useful performance gain; it does not meet the adoption threshold. This is not a claim that later or larger workloads cannot differ, only that the proposed optimization is not supported by this A/B evidence.

## Reproducing

Build Release libraries in a pristine checkout at `base` and in a second checkout where `change.patch` has been applied. For each checkout:

```sh
make release-lib BUILD_DIR="$PWD/build" JOBS=2
flags=(-std=gnu++23 -O3 -DNDEBUG)
g++ "${flags[@]}" \
    -Iinclude -Ibuild/include -I3rdparty/simlib/include -I3rdparty/cudd/include \
    /path/to/repo/docs/experiments/8-nft-quotient-vector-map/probe.cc \
    build/src/libmata.a -lpthread -o /tmp/nft-vector-probe
/tmp/nft-vector-probe
```

To reproduce the timing measurements, save one executable per variant and run this on an otherwise idle core:

```sh
for round in 1 2 3 4; do
    taskset --cpu-list 3 /tmp/nft-vector-probe-old bench
    taskset --cpu-list 3 /tmp/nft-vector-probe-new bench
done
```

The intended comparator prints `nft_reduce ns=<time> checksum=<sum>`. Old and new verification printouts must have the same map and semantic fields.

## Conclusion

Do not add the vector in its current form. Its output is correct, but its only demonstrated effects are two fewer allocations, 600 fewer requested bytes, and an insignificant 0.3% median time difference on the tested 100-state NFT. Revisit only when a leading profile assigns substantial time to the renaming hash operations, rather than to the simulation engine or when intermittent rehash collisions become important.

## Side findings

The multi-level regression independently checks least-representative numbering and that target `3` is pruned in favor of the strictly simulating target `2`, without reversing that direction. #1125 already removed the final copy of the public renaming; this candidate does not eliminate the public map's construction or alter the simulation algorithm.
