---
id: EXP-4
title: Index residual-after subset candidates with state posting lists
status: rejected
date: 2026-11-23
base: a803d22d11c5d54f12a61c497bebbcb3052d096b (devel)
related: ["#808"]
tags: [nfa, residual-reduction, subset-query, inverted-index]
---

# EXP-4: Residual-after subset-candidate index

## Question

Can `ResidualCoverage::find_covering` avoid its member-stamp setup and linear scan of subset candidates by indexing macrostates by the states they contain? This was investigated as part of #808.

## What was tried

[`rejected.patch`](rejected.patch) replaces the member stamps and linear candidate scan with state-to-macrostate posting lists and generation-stamped per-candidate hit counts. A candidate is a subset when it receives one hit for each of its states. Empty macrostates are enumerated separately, and the collected covering indexes are sorted to preserve the old covering and recursion order.

The patch is `git format-patch` output for the ported commit `b1329134e141830502f26f3a9a80cde289afa720` against the base above; apply it with `git am rejected.patch`. The original donor was `67c3f57933f4db99f9fbb979454faada0626d4ef`, based on `ed639a8a4cd22aa734ddf77bf3c029afe9b89cac`; the port has the same patch.

## Method

Release builds of the base and candidate linked [`harness.cc`](harness.cc), which reads one automaton (a `.mata` file or
a Tabakov–Vardi random NFA) and calls

```cpp
reduce(nfa, nullptr, {{"algorithm", "residual"}, {"type", "after"}, {"direction", "forward"}})
```

under Callgrind with `--toggle-collect='*reduce_residual_after*'`, so parsing and generation are not counted. The
harness prints an FNV-1a hash over the number of states, the initial and final states, and all transitions of the
result. Hashes were identical for all three measured cases and for 310 randomly sampled files across the nfa-bench
corpora.

## Results

Instruction counts (Ir), with ratios candidate/base:

| case                                                                 |     Ir base |   Ir candidate | candidate/base |
| -------------------------------------------------------------------- | ----------: | -------------: | -------------: |
| `automata_inclusion/false-IBakery-4P-BinEnc-BwBad-A-3-lhs.mata`      | 302,100,613 | 17,747,591,634 |          58.7x |
| `regexps/Snort/together/together_aut_1018.mata`                      | 153,176,848 |    512,253,185 |          3.34x |
| Tabakov-Vardi (40 states, 4 symbols, ratio 2.0, density 0.5, seed 1) |  50,243,473 |    552,365,834 |          11.0x |

For a query on macrostate `S`, the candidate walks the complete posting list of every state in `S`. Its work is therefore proportional to `sum over s in S of |{M : s in M}|`, approaching the total macrostate size in the worst case. The old linear scan instead used `all_of` for each candidate and stopped at the first candidate state absent from `S`.

Residual-after reduction determinizes NFAs whose macrostates are large and heavily overlapping, making the posting lists dense and long. Each recursive `try_cover` also repeats an unchanged query, and sorting the covering indexes adds `O(k log k)` work per query. The full posting-list traversal thus defeats the old per-candidate short-circuit instead of reducing work.

## Reproducing

Create base and candidate checkouts at `a803d22d11c5d54f12a61c497bebbcb3052d096b`, apply `rejected.patch` in the
candidate with `git am`, and build both with `just cpp build release`. Link the harness in each checkout (GCC 16.2):

```sh
g++ -std=c++23 -O3 -DNDEBUG \
  -Iinclude -Ibuild/release/g++/include -I3rdparty/simlib/include -I3rdparty/cudd/include \
  docs/experiments/4-residual-after-subset-index/harness.cc build/release/g++/src/libmata.a -o harness
```

With `NFA` set to `benchmarks/nfa-bench/benchmarks/nfa` of
[automata-lib-bench](https://github.com/VeriFIT/automata-lib-bench), run each case under both builds:

```sh
valgrind --tool=callgrind --toggle-collect='*reduce_residual_after*' ./harness \
  file "$NFA/automata_inclusion/false-IBakery-4P-BinEnc-BwBad-A-3-lhs.mata"
valgrind --tool=callgrind --toggle-collect='*reduce_residual_after*' ./harness \
  file "$NFA/regexps/regexps/Snort/together/together_aut_1018.mata"
valgrind --tool=callgrind --toggle-collect='*reduce_residual_after*' ./harness tv 40 4 2.0 0.5 1
```

Ir is the `Collected` total Callgrind prints. The harness lines of base and candidate must agree on `out_states`,
`trans` and `hash`; the recorded hashes are `0dabb3ea4f6abf55`, `249b075ecf1c8f44` and `39662cba67f37602` for the
three cases in table order. Rerun on the base with GCC 16.2 and Valgrind 3.27.1, this harness reproduces those hashes
and collects 68 Ir fewer than the table on each case (the table was measured with an earlier, untidied copy of the
same harness). For the broader identity check, run both harnesses natively on a random sample of 310 files from the
nfa-bench corpora and compare their output lines without the `ms=` field.

## Conclusion

Rejected: identical outputs came with 3.34x to 58.7x more instructions on the residual-heavy cases. Revisit only with size-bucketing and early termination that rejects candidates, including those with `|T| >= |S|`, before counting hits; otherwise dense determinized automata continue to pay the full posting-list length for every repeated query.
