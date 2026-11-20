---
id: EXP-3
title: Adaptive existential iterator merging, and universality through inclusion
status: rejected
date: 2026-11-20
base: bc2f271ffb8b0d1bf18f6907e53e16dcf3ad55e0 (devel)
related: ["#752", "#726", "#224"]
discussion: https://github.com/VeriFIT/mata/issues/752
tags: ["performance", "antichains", "iterators"]
---

## Question

`#752` asks whether (c) an adaptive `SynchronizedExistentialIterator`, choosing heap merge instead of a scan, and (d)
implementing universality as antichain inclusion of a one-state Σ* automaton in the input, are faster than the existing
algorithms. Both proposals required measurement, and (c) explicitly required a benefit on Z3-Noodler inputs before
adoption.

## What was tried

- `existential-heap.patch`: an adaptive heap-merge alternative. It retains the existing scan for small position counts
  and estimates the range/entry ratio when choosing the heap for larger inputs.
- `universal-one-state-inclusion.patch`: replaces the dedicated antichain universality search with
  `is_included_antichains(Σ*, aut)`. It clears the counterexample path, whose states would refer to Σ* rather than to
  the input.

The unit tests added by the found work were kept independently: `unify_targets()` is now covered for no current symbol,
one and two synchronized posts, three or more, duplicate and disjoint targets, and targets next to `num_of_states()`.

## Method

Release build with GCC 16.2.0, `-O3 -DNDEBUG`, one pinned CPU, on the same machine as the #726 measurements. Base and
candidate are linked into separate probes and interleaved per instance with three rounds. Per-instance medians are
summed below; dominant outliers therefore dominate the totals intentionally. The 40 ARMC pairs are reported in both
directions. The 60 Z3-Noodler inputs sample the `univ` family. Naive universality validates the answers on 40 separately
generated random NFAs.

| measured operation                             |      baseline |     candidate |  ratio |       slower directions |
| ---------------------------------------------- | ------------: | ------------: | -----: | ----------------------: |
| Antichain inclusion, 240 directions            | 32501.0404 ms | 44388.1584 ms | 1.3657 |               158 / 240 |
| Determinization, 180 evaluations               |     2.6904 ms |     2.7368 ms | 1.0172 |                69 / 180 |
| Universality through inclusion, 40 random NFAs |     0.0875 ms |     0.1313 ms | 1.5006 | not timed per direction |

All answers and shortest-word membership validations agree where checked.

## Reproducing

Start from the recorded base in a separate workspace. The experiment artifacts do not exist at that commit, so copy
them from a checkout of the reviewed revision first. Apply only one candidate patch at a time, then configure a Release
build (and repeat it in an independent baseline workspace):

```sh
review_dir=/path/to/reviewed-revision
cp -R "$review_dir/docs/experiments/3-antichain-iterator-universality" docs/experiments/
git apply docs/experiments/3-antichain-iterator-universality/<candidate.patch>
nix develop --command make release BUILD_DIR=build/release JOBS=4
```

The copied directory supplies both patches, both probe sources, and the list files. The `gitlink` of
`tests-integration/nfa-bench` at the recorded base selects the matching benchmark checkout. Place it below the benchmark
directory as `nfa/benchmarks/nfa`, where the copied list files expect it.

Compile each variant with its workspace as the working directory:

```sh
g++ -std=gnu++23 -O3 -DNDEBUG '-DMATA_BENCH_BUILD_TYPE="Release"' \
  -Iinclude -Ibuild/release/include -Itests-integration/src \
  -I3rdparty/simlib/include -I3rdparty/cudd/include \
  <probe.cc> tests-integration/src/utils/utils.cc build/release/src/libmata.a \
  -lpthread -o <probe-name>
```

Use `probe_iterator.cc` for `probe_c_base` and `probe_c_c`, and `probe_universality.cc` for `probe_univ_base` and
`probe_univ_d`. Copy the list files and the executable of the appropriate build beside them, then run on one core:

```sh
./run_iterator_comparison.sh
./probe_univ_base random 1 40 752 > universality-base.csv
./probe_univ_d random 1 40 752 > universality-candidate.csv
```

To aggregate the inclusion directions, sum columns 4 and 6 for baseline time, and 8 and 10 for candidate time. For
determinization, sum columns 5, 11, and 17 for baseline time, or 8, 14, and 20 for candidate time. Universality outputs
have `instance;answer;milliseconds;repetitions;counterexample-length`.

## Conclusion

Do not adopt either alternative in their tested forms. The heap merge is substantially slower on ARMC inclusion and
slightly slower on the Z3-Noodler determinization sample, which is the acceptance gate named in #752. Inclusion in a
one-state Σ* automaton adds work that the dedicated universality algorithm avoids. Worth revisiting only after a
measured reason for either overhead is identified, or a different cost model substantially changes their tradeoffs.

## Side findings

Automaton-template iterator changes belong separately with that effort; these results concern devel only.
