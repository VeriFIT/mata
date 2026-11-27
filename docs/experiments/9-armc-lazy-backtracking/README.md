---
id: EXP-9
title: Lazy and early-exit backtracking in ARMC
status: rejected
date: 2026-11-27
base: f62d3bf82b8437710182e48b63a5b44205912d2c (devel)
related: []
tags: [armc, lazy, backtracking, fixpoint]
---

## Question

Can lazy `mata-lazy` pre-images, eager pre-images without minimization, or an inclusion test of the concrete post before
union and abstraction improve the end-to-end finite-length ARMC loop?

## What was tried

[`rejected.patch`](rejected.patch) adds three independent choices to `examples/rmc/armc.matasc`:

- `backtracking=lazy`: nested lazy pre-images intersected with the initial automaton;
- `backtracking=eager-unminimized`: the existing eager pre-images without minimization;
- `early-fixpoint=true`: test `post(reached) ⊆ reached` before union and abstraction.

The reduction experiment landed separately in the recorded base. This patch leaves that policy in place, while the
harness disables it so the rejected choices are isolated.

## Method

Release `mata`, pinned CPU 0, alternating variants, 15 runs per model and 5 s per invocation. Every non-timeout run must
retain the expected verdict. [`harness.py`](harness.py) emits each run as JSON with the phase records. The tested models
are token passing, token copying, Burns, broken Burns, and the translated RTL processor.

## Results

Medians over 15 runs. Early checking and lazy backtracking were also tested on the two token models; lazy backtracking
helped only the small token-copying counterexample and was not generally useful. The Burns and RTL measurements are:

| model         | variant        | timeouts | wall median |
| ------------- | -------------- | -------: | ----------: |
| burns         | base           |     0/15 |   32.425 ms |
| burns         | unminimized    |     0/15 |     2.433 s |
| burns         | lazy           |    15/15 |       > 5 s |
| burns         | early fixpoint |     0/15 |   34.346 ms |
| burns-broken  | base           |     0/15 |   39.179 ms |
| burns-broken  | unminimized    |    15/15 |       > 5 s |
| burns-broken  | lazy           |    15/15 |       > 5 s |
| burns-broken  | early fixpoint |     0/15 |   40.816 ms |
| rtl           | base           |     0/15 |   12.071 ms |
| rtl           | early fixpoint |     0/15 |   11.618 ms |
| token-copying | base           |     0/15 |    6.691 ms |
| token-copying | unminimized    |     0/15 |    6.565 ms |
| token-copying | lazy           |     0/15 |    6.472 ms |
| token-copying | early fixpoint |     0/15 |    6.778 ms |

On Burns, the unminimized backward pass expands intermediate automata severely; it also times out on broken Burns. Lazy
backtracking times out on both Burns variants. Early checking adds one inclusion test per preceding round and saves only
the final collapse, which loses on both Burns models. Lazy backtracking saves about 0.145 ms in median `backtrack` time
on token copying (0.409 ms to 0.264 ms), but only about 0.219 ms in median whole-process time and does not generalize.

The RTL rows were measured with an earlier safe expression that accepted every valid configuration, including the bad
`srh`. With the corrected expression now in [`harness.py`](harness.py), the current `armc.matasc` reaches the same
`safe, length 1` verdict after the same rounds.

## Reproducing

Build the recorded base and apply the patch in a separate checkout:

```sh
git am docs/experiments/9-armc-lazy-backtracking/rejected.patch
just bindings rust build --release
```

Run the variants alternately, one invocation per variant and sample, pinned to CPU 0. Each harness line has `"run": 0`;
the line order of the output file records the executed order:

```sh
model=burns
for sample in $(seq 15); do
  for variant in '--backtracking eager' '--backtracking eager-unminimized' '--backtracking lazy' '--early-fixpoint'; do
    taskset --cpu-list 0 python3 docs/experiments/9-armc-lazy-backtracking/harness.py \
      --mata bindings/rust/target/release/mata --model "$model" $variant --repeats 1
  done
done > "$model.jsonl"
```

Substitute `burns-broken`, `rtl`, or `token-copying` for the other rows. The expected verdicts are `safe, length 1` for
Burns and RTL, and `unsafe, steps: 10, length 1` for broken Burns. Token copying is `unsafe, steps: 1, length 1`.

## Conclusion

Do not make lazy backtracking, unminimized pre-images, or early concrete-post inclusion a default. Lazy backtracking can
be reconsidered only with selective explicit materialization that avoids the Burns blow-up and an end-to-end win over
minimized eager pre-images.

## Side findings

The same measurements motivated retaining eager minimization and testing reduction before abstraction instead. The
latter, with a 32-state threshold, is present in the base above.
