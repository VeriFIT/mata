---
id: EXP-2
title: Use NFT levels as BDDs on automaton transitions
status: rejected
date: 2026-11-19
base: 53021908d58bf600054a1bf2d902944e9cc45a27 (measured devel)
related: []
tags: [nft, bdd, symbolic-automata, performance]
---

# EXP-2: NFT levels as BDDs on automaton transitions

## Question

Can an NFT represent an automaton with BDDs on its transitions, using one level per BDD variable, with performance
competitive with dedicated BDD libraries? This repeats the question after the NFT performance improvements through
Mata `53021908`, comparing both with BDD libraries and with Mata `v1.32.45-dev.25`.

## What was tried

Four implementations, all in [automata-lib-bench](https://github.com/VeriFIT/automata-lib-bench):

- **`mata-bdd`**: a symbolic automaton over n Boolean variables is an n-level NFT. Level-0 states are ordinary automaton
  states. A transition's guard is a BDD: at the level of its tested variable, a node reads 0 or 1 and jumps over
  don't-care levels using `JumpMode::AppendDontCares`. Intersection and inclusion consume the levels and jumps directly.
  Complement and determinization first unwind jumps over 0 and 1; this is timed separately as `unwind`.
- **`cudd`**: explicit automaton states with a CUDD BDD for the guard of each pair of states. Products conjoin guards;
  determinization splits the guards of a macrostate into target regions; inclusion uses the antichain algorithm. This
  is the primary baseline.
- **`mona`**: MONA 1.4-18's DFA library. An input NFA is determinized when loaded, projecting variables used to select
  targets. That often dominates its whole-process runtime, but is outside `operation`.
- **`mata`**: the ordinary explicit-NFA representation, parsing and mintermizing the same inputs into Mata.

All symbolic implementations start from the same parsed formulas with the same variable order, and encode one binary
symbol per BDD edge. NFT-BDD and MONA use the harness `bench::Robdd` representation; CUDD builds its BDDs directly from
those formulas. The comparison therefore measures representation and automaton operations, not interoperable
Mata parsing. It does not claim that an NFT is a semantically complete replacement for a BDD package.

The code is preserved in [`patches/`](patches/), against the following bases:

| repository                          | base                                       | final revision                                                          |
| ----------------------------------- | ------------------------------------------ | ----------------------------------------------------------------------- |
| `harnesses/automata-program-parser` | `cf992bbbbf30b80654520c02fecaf0c0dd8b4c20` | `74c879589583713aebf417189bf5367b72dd5893`                              |
| `harnesses/pycobench`               | `644801e08845215bbd1ee694cd311373c8976c48` | `ea4f7576dd02e43f78ba4c72477bce98bfa8d800`                              |
| `automata-lib-bench`                | `3ae78a8c8f`                               | `de495a25620f6d0c82d0bf38866a2b89333e0911` (bookmark `bdd-transitions`) |

These are the original experimental tips. Their histories are retained alongside subsequent review fixes; the result
tables are not remeasured on those fixes.

## Method

- TACAS'24 instances with `@NFA-bits` alphabets (6–16 variables), nine families and **1,803 input tuples**, using
  nfa-bench `c65ba0e272fc7145199425396adb112129474e45`. These cover parametric Boolean combinations, intersection,
  union, difference, arithmetic-formula complement, triple/variadic intersection, email-filter inclusion, and armc
  inclusion.
- Release builds, **60 s timeout, 8 GB memout**, `Release` mode, pinned workers, and `--repeat 3` with the minimum of
  completed operation times. `operation` sums the operations of the program and excludes parsing and construction;
  `runtime` was also recorded but is not the metric summarized here.
- The primary run used six workers. The old/new run used six workers for `armc-incl`, `b-smt`, `email-filter`,
  `lia-symbolic` and `param-inter`, then two for `param-inter-variadic`, `param-union`, `param-union-variadic` and
  `param-diff` to avoid exhausting the host. Every comparison within a family has identical settings for both builds;
  no cross-cohort ratio is computed.
- The first families of the primary run used pycobench `644801e0`; the last and historical runs used `ea4f7576`, which
  preserves operation metrics after truncation. The preserved final harness is what the reproduction commands use.
- [`tools/analyse.py`](tools/analyse.py) reads one final result matrix per family, or the newest repeats matrix if no
  merged matrix exists. It accounts for extra leading columns when a command has multiple automaton operands.
- Each ratio is the geometric mean of `build / baseline` **on that pair's shared instances with positive operation
  times for both builds**. Aggregate
  ratios pool those per-instance ratios; populations can therefore differ between columns. Missing and failed runs
  are not imputed. This is a descriptive capped benchmark, not a statistically controlled A/A experiment.

The raw measurements are not committed; the commands below regenerate them.

## Results

Full pairwise ratios, solved counts, failures and disagreement counts are in [`results.md`](results.md).

| family               | NFT-BDD / CUDD, n | NFT-BDD / CUDD | NFT-BDD September 17, n | NFT-BDD / September 17 |
| -------------------- | ----------------- | -------------- | ----------------------- | ---------------------- |
| armc-incl            | 135               | 9.33×          | 135                     | 0.25×                  |
| b-smt                | 384               | 8.06×          | 384                     | 0.52×                  |
| email-filter         | 500               | 11.91×         | 500                     | 0.33×                  |
| lia-symbolic         | 169               | 1.61×          | 169                     | 0.44×                  |
| param-inter          | 80                | 22.09×         | 56                      | 0.51×                  |
| param-inter-variadic | 28                | 13.44×         | 28                      | 0.60×                  |
| param-union          | 80                | 5.95×          | 80                      | 0.21×                  |
| param-union-variadic | 187               | 3.04×          | 187                     | 0.31×                  |
| **all shared pairs** | **1,563**         | **7.24×**      | **1,539**               | **0.37×**              |

All 80 `param-diff` instances failed for every build (timeout, memory, or error), so the family has no operation-time
ratio. Against MONA directly, the aggregate NFT-BDD ratio is **4.56× on 1,207 shared instances**.

NFT-BDD completed **1,564/1,803** primary instances and **1,563/1,803** in the second cohort; the September revision
completed **1,539/1,803**. Only symbolic complement approaches CUDD. Products and inclusions over this benchmark are
not close to dedicated BDD performance, even though they improved materially since September.

The analysis found no primary answer disagreement on completed pairs, including the 500 email-filter comparisons
between CUDD, NFT-BDD, and explicit-minterm Mata. In the historical cohort there
are **399/500** email-filter answer differences: `old=false`, `new=true`. The current answers match CUDD, MONA (where
it completed), and explicit-minterm Mata, so this is not merely a performance-only change. No source-level bisection
of those older answers was done here. The benchmark is a differential check of measured operations, not proof of
language equivalence.

## Reproducing

Let `$record` be this directory's absolute path. Start from a fresh automata-lib-bench checkout and initialize its
submodules at the base pins listed above. Apply the harness changes before advancing the superproject's gitlinks:

```sh
git submodule update --init --recursive
git -C harnesses/automata-program-parser am "$record/patches/01-automata-program-parser.patch"
git -C harnesses/pycobench am "$record/patches/02-pycobench.patch"
git am "$record/patches/03-automata-lib-bench.patch"
# Match the superproject's gitlinks to the submodule commits just replayed.
git add harnesses/automata-program-parser harnesses/pycobench
git commit --message "bench: Record replayed submodule revisions"
git status --short
just setup-python
```

`git am` creates new submodule commit IDs when the local committer identity or date differs from the original. The
follow-up commit records those replayed submodule HEADs in the superproject, so its gitlinks match the actual
submodules and the final `git status --short` produces no output. These replayed IDs, including the superproject's,
need not equal the original experiment revisions in the table; the original measurements and the review-fix history
are preserved in the patch series.

From the automata-lib-bench root, with the Mata source at `../mata`, reproduce the four-library comparison with these
explicit Mata pins. The shared lock prevents other measurements from competing for workers.

```sh
nix develop --command flock /tmp/mata-alb-bench.lock ./scripts/bench.py suites/bdd.toml \
  --fetch-from ../mata --repeat 3 --jobs 6 --output-dir bdd-dedicated \
  cudd:3.0.0 mata-bdd:53021908d58bf600054a1bf2d902944e9cc45a27=devel \
  mona:1.4-18 mata:53021908d58bf600054a1bf2d902944e9cc45a27=devel
```

For the historical comparison, `v1.32.45-dev.25` resolves to `f874bae84d6e167d4b0f947eec9e96270f5c725d` here:
replace that revision by the full hash if the tag is unavailable. Run these two commands:

```sh
nix develop --command flock /tmp/mata-alb-bench.lock ./scripts/bench.py suites/bdd.toml \
  --fetch-from ../mata --only armc-incl,b-smt,email-filter,lia-symbolic,param-inter \
  --repeat 3 --jobs 6 --output-dir bdd-historical mata-bdd:v1.32.45-dev.25=old \
  mata-bdd:53021908d58bf600054a1bf2d902944e9cc45a27=new

nix develop --command flock /tmp/mata-alb-bench.lock ./scripts/bench.py suites/bdd.toml \
  --fetch-from ../mata --only param-inter-variadic,param-union,param-union-variadic,param-diff \
  --repeat 3 --jobs 2 --output-dir bdd-historical mata-bdd:v1.32.45-dev.25=old \
  mata-bdd:53021908d58bf600054a1bf2d902944e9cc45a27=new
```

Regenerate the result tables (or substitute `mona-1.4-18` as the baseline for a direct MONA comparison):

```sh
python3 "$record/tools/analyse.py" cudd-3.0.0 results/data/bdd-dedicated
python3 "$record/tools/analyse.py" mata-bdd-old results/data/bdd-historical
```

## Conclusion

Rejected as a claim of generally competitive performance on this benchmark. The NFT representation is usable for the
measured symbolic-automaton operations and has improved by a factor of about **2.7** over September on shared
instances, but it remains **7.24× CUDD** and **4.56× MONA** overall. Complement is the one near-competitive operation at
**1.61× CUDD**. Do not claim that the general NFT implementation can replace dedicated BDD automata libraries for
performance-sensitive symbolic products and inclusions without new evidence.

Worth revisiting after an optimization specifically targets the heavy intersection/inclusion and trim workloads. The
important question is whether the level encoding can avoid its automaton-state representation costs or reuse computed
BDD relations; this experiment did not measure a candidate implementation of those changes.

## Side findings

- On the measured Mata revision, `mata::nft::complement()` of an automaton with no initial states accepts only the empty
  word rather than the full alphabet language. The shared alphabet used for explicit-minterm `mata` can also omit a
  minterm with no originating transition. Check `disagreements.csv` before treating complement results as correct.
- pycobench's metric-order handling and truncation parser needed fixes, preserved in
  [`02-pycobench.patch`](patches/02-pycobench.patch); the latter has three passing regression tests.
