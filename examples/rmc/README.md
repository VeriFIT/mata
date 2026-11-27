# Regular model checking

[`armc.matasc`](armc.matasc) is abstract regular model checking (Bouajjani, Habermehl, Vojnar, CAV 2004) as a Mata
Script. A configuration of a parameterised system is a word with one letter per process, the transition relation a
length-preserving two-level transducer. By default, the script over-approximates the reachable configurations with
`nfa:collapse-by-length`, checks them against the bad configurations lazily, decides with a backward search whether a
bad configuration it meets is really reachable, and refines a spurious one by a larger length. With
`--param 'abstraction=predicates'`, it instead uses `nfa:collapse-by-predicates`, retains the intersections found by
backward counterexample analysis as new predicates, and stops with `unknown, abstraction cycling` if refinement adds
no new predicate language. By default (`predicate-seed=adaptive`), the initial predicate is the bad-language automaton;
all its states contribute fingerprints, and the first spurious counterexample adds the transducer's domain and range.
Change `predicate-seed` to `none` to start without predicates, `actions` to use the transducer domain and range, or
`bad-actions` to use all of them. `predicate-granularity=languages` compares only whole predicate languages instead of
all state residuals; this is faster on broken Burns but slower on safe Burns and is therefore not the default. With
`verification-direction=backward`, the script starts from the original bad
configurations, uses the inverted transition relation, and tests reachability of the original initial configurations.

| System                                     | Letters             | Result                      |
| ------------------------------------------ | ------------------- | --------------------------- |
| [`token-passing.mata`](token-passing.mata) | `t` token, `n` none | safe, length 1              |
| [`token-copying.mata`](token-copying.mata) | as above            | unsafe, steps: 1, length 1  |
| [`burns.mata`](burns.mata)                 | `0`–`5`, see below  | safe, length 1              |
| [`burns-broken.mata`](burns-broken.mata)   | as above            | unsafe, steps: 10, length 1 |
| [`rtl.mata`](rtl.mata)                     | RTL stages, below   | safe, length 1              |

Run, from the repository root:

```sh
mata script run --param 'initial-regex=tn*' --param 'bad-regex=n*tn*' --param 'symbols=n t' \
  examples/rmc/armc.matasc examples/rmc/token-passing.mata
mata script run --param 'initial-regex=tn*' --param 'bad-regex=n*tn*' --param 'symbols=n t' \
  --param 'abstraction=predicates' examples/rmc/armc.matasc examples/rmc/token-passing.mata
mata script run --param 'initial-regex=0*' --param 'bad-regex=[0-4]*(5[0-4]*)?' --param 'symbols=0 1 2 3 4 5' \
  examples/rmc/armc.matasc examples/rmc/burns.mata
```

## Burns' mutual exclusion

Each process runs Burns' one-bit algorithm. One process moves per step, and a scan of the other flags is atomic. The
letters are the program counters: `0` idle, `1` scan the left processes before raising the flag, `2` raise it, `3`
scan the left processes again, `4` wait for the right processes, `5` critical. The flag is up in `3`, `4` and `5`.

| From | To  | Condition                                |
| ---- | --- | ---------------------------------------- |
| `0`  | `1` | always                                   |
| `1`  | `0` | some process on the left has its flag up |
| `1`  | `2` | no process on the left has its flag up   |
| `2`  | `3` | always                                   |
| `3`  | `0` | some process on the left has its flag up |
| `3`  | `4` | no process on the left has its flag up   |
| `4`  | `5` | no process on the right has its flag up  |
| `5`  | `0` | always                                   |

Initially every process is idle (`0*`); a configuration with two processes in `5` is bad. `burns-broken.mata` drops
the condition of `4` to `5`.

## RTL processor

`rtl.mata` translates the unary ARTMC model in `rtl_artmc_exprm-bak.ml`. `s` is the initial leaf, represented here by
an identity letter; `i`, `f`, `d`, `e`, `r` and `o` are the processor stages, and `h` is the original model's `eh`
exception-handler letter. The seven level-zero states track the source transducer state. Each of its 47 rules is a
level-zero/level-one pair, so every step preserves the word length; this includes all 24 identity rules and 23 changing
rules. The safe expression used by the runner is the complement, over valid configurations, of the source bad automaton
`s[ifdeor]*r[ifdro]*h[ifdero]*`. The observed verdict is `safe, length 1`: at abstraction length zero the first round
is spurious, and length one closes after rounds 0 through 6 without a concrete `r`-then-`h` trace.

## Instrumentation and repeatable benchmarks

Instrumentation is off by default, leaving the verdict-only output unchanged. Add `--param 'instrument=true'` to
`mata script run` to print `armc.nN.initial-collapse`, per-round `post`, `collapse`, `inclusion` and `round` timings,
and `pre-I` timings during concrete backward checks. `N` is the abstraction length and `R` in `armc.nN.rR` is the round.
Each `armc.nN.rR.bad.*` group reports the seven `lazy:stats` counters for that round's bad check. All labels are unique
within a run, and the final verdict is unchanged. The whole concrete counterexample decision also has a `backtrack`
timer. By default, a union with at least 32 states is simulation-reduced before bounded-length abstraction; set
`reduce-before-collapse=false` to disable this, or change `reduction-min-states`. The 32-state threshold avoids the
measured reduction overhead on small token and RTL unions while preserving the larger Burns improvements.

[`benchmark.py`](benchmark.py) supplies the model parameters, enables instrumentation, and writes one JSON object per
run to standard output. It uses only Python's standard library and accepts any installed `mata` executable:

```sh
python3 examples/rmc/benchmark.py --model rtl --runs 3
python3 examples/rmc/benchmark.py --model burns-broken --runs 3 \
  --mata bindings/rust/target/debug/mata
```

Each JSON line contains the model, zero-based run number, process status, wall-clock seconds, verdict, standard error,
and a `records` object with all phase timings and lazy counters. Available models are `token-passing`, `token-copying`,
`burns`, `burns-broken` and `rtl`; `rtl` and one run are the defaults.

## Ranked tree model checking

`ta-token.cc` evaluates bottom-up tree automata with nullary rules and final root states (no initial-state set). `@TA-explicit`
uses rules `a -> q` and `f(q1,q2) -> q`; `%Final` declares the accepting root states. Structure-preserving `@TT-explicit`
transducers use `t -> n : q1` and `nn(q1,q0) -> tt : q2`. The driver composes concrete posts, computes predicate or
finite-height abstractions, checks inclusion, and backtracks bad intersections. It preserves concrete posts for real
versus spurious counterexample checks and detects predicate-refinement cycling.

```sh
build/release/g++/examples/rmc-ta-token examples/rmc
```

| Model         | Predicates                                  | Finite height                |
| ------------- | ------------------------------------------- | ---------------------------- |
| token-passing | `safe, predicate refinements 0`             | `safe, height 0`             |
| token-copying | `unsafe, steps: 1, predicate refinements 0` | `unsafe, steps: 1, height 0` |
| percolate     | `safe, predicate refinements 0`             | `safe, height 1`             |

[`benchmark_ta.py`](benchmark_ta.py) repeats all six runs and emits model, mode, verdict, process status, and post,
collapse, inclusion, and backtrack timing as JSONL. The default executable and directory are the ones above; override
them with `--mata` and `--directory`, and set the count with `--runs N`.

## An inductive invariant

[`inductive-invariant.matasc`](inductive-invariant.matasc) is the proof rule of regular abstraction frameworks
(_Computing Inductive Invariants of Regular Abstraction Frameworks_, CONCUR 2024) on the example of
`bindings/rust/mata/examples/lazy_model_checking.rs`, as one lazy formula with no loop. It prints
`no reachable 0: true`.
