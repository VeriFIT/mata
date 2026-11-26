# mata-lazy

Lazy emptiness checking of Boolean and relational combinations of automata and transducers, the engine behind
`mata::lazy` in the Rust bindings of [Mata](https://github.com/VeriFIT/mata).

You build a formula (unions, intersections, complements, products, compositions, projections) over automata from
Mata, regex-automata or rustfst, and ask whether its relation is empty. mata-lazy explores only what the answer
needs. Complements and products are built on the fly, and subsumption prunes the search. When the relation is not
empty, you get a witness.

## Getting started

With Mata's automata, use the engine through the `mata` crate, which re-exports it as `mata::lazy` (the `lazy`
feature, on by default). Neither crate is on crates.io yet, so use them from the repository.

```toml
[dependencies]
mata = { git = "https://github.com/VeriFIT/mata", branch = "devel" }
```

```rust
use mata::Nfa;
use mata::lazy::Formula;

let mut formula = Formula::new();
let a = formula.leaf(&Nfa::from_regex("a(a|b)*b")?)?;
let b = formula.leaf(&Nfa::from_regex("a(a|b)*")?)?;

// a(a|b)*b ⊆ a(a|b)* iff a(a|b)*b ∩ ¬a(a|b)* = ∅.
let not_b = formula.complement(b)?;
let difference = formula.intersection(a, not_b)?;
assert!(formula.is_empty(difference)?);
```

Without Mata, depend on `mata-lazy` alone: it is pure Rust and needs no C++ toolchain.

```toml
[dependencies]
mata-lazy = { git = "https://github.com/VeriFIT/mata", branch = "devel", features = ["regex"] }
```

### Requirements

- Rust 1.95 or newer.
- Through `mata`: Mata is a C++ library built from source, which needs CMake 3.23 or newer and a C++23 compiler
  (GCC 14 or newer, or Clang 18 or newer). To link an installed Mata instead, set `MATA_DIR` (see the
  [`mata-sys` README](../mata-sys/README.md)).

## Backends

- **The `mata` crate, as `mata::lazy`:** Mata `Nfa` and `Nft`.
  `Nft::try_from(&formula.materialize(term, &options)?)` turns a result into a Mata `Nft`.
- **Feature `regex`:** mata-lazy's `Regex`, a DFA over bytes.
- **Feature `rustfst`:** rustfst `VectorFst` and `ConstFst` (arity 2; weights only decide whether a transition
  exists).

Leaves of different backends can be mixed in one formula, and your own automata can be leaves too, with or without
a backend (below).

## Your own automata

Any automaton can be a leaf. Implement mata-lazy's `Automaton` for it and pass it to `leaf`. mata-lazy asks for the
number of tapes, the initial states, whether a state is final, and the moves from a state, each with one `Symbol`
per tape. It copies the part reachable from the initial states, so that part must be finite.

```rust
use mata_lazy::{Automaton, Formula, Symbol};

/// Words over {a, b} with a multiple of 3 `a`s.
struct ThreeAs;

impl Automaton for ThreeAs {
    /// The number of `a`s read so far, modulo 3.
    type State = u32;

    fn arity(&self) -> usize {
        1
    }

    fn initial_states(&self) -> impl IntoIterator<Item = u32> {
        [0]
    }

    fn is_final(&self, state: u32) -> bool {
        state == 0
    }

    fn for_each_move(&self, state: u32, mut visit: impl FnMut(&[Symbol], u32)) {
        visit(&[Symbol::from(b'a')], (state + 1) % 3);
        visit(&[Symbol::from(b'b')], state);
    }
}

let mut formula = Formula::new();
let three_as = formula.leaf(&ThreeAs)?;
assert!(!formula.is_empty(three_as)?);
```

A transducer reports one symbol per tape on each move, with the `EPSILON` symbol on a tape that reads nothing.
`examples/custom_automaton.rs` adds a transducer that swaps `a` and `b` and asks what the swap does to the counts.

## Usage

Builders:

- `leaf`, `leaf_over`: the language of an automaton; `leaf_over` adds symbols to its alphabets.
- `union`, `intersection`, `complement`: Boolean operations on relations of the same arity.
- `identity`: `{(w, w) | w ∈ L}`.
- `project`: keeps the given tapes, in that order.
- `pad_closure`: `{w | w·pᵏ ∈ L for some k}`, with `p` a fixed step or the word's last step; logics over numbers
  need it after a projection.
- `sync_product`, `compose_on`, `compose`, `post_image`, `pre_image`: products of relations synchronized on tapes.
- `explicit`, `explicit_with`: the same relation, built as an automaton when a query starts.
- `prefixes`: the prefix closure, the words that some word of the relation extends, counted in lock steps.

Queries:

- `is_empty`, `find_word`: whether the relation is empty, or a witness.
- `check`: a witness and statistics, with options for extra alphabet symbols, terms made explicit for one query,
  a plan (below), limits on states and time (their errors carry the statistics so far), a memory budget for cached
  results, subsumption and the antichain on or off, a search bounded to words of at most `max_length` steps (the
  outcome is then `bounded` when the search stopped there without a witness), and a breadth-first search (`shortest`)
  whose witness has the fewest steps. The statistics also count macrostates and computed successors per term.
- `materialize`: the whole relation as an `ExplicitAutomaton`, which other libraries copy (into a Mata `Nft` with
  `Nft::try_from`) and which can be a leaf again.

Every builder and query returns a `Result` with mata-lazy's `Error`, and misuse such as a wrong arity is an error,
not a panic. Queries take `&self`, so several threads can query one formula.

A few things are worth knowing.

- **Symbols are numbers**, as in Mata, and leaves of all backends share them. A byte is the symbol of the same
  number, so a regex and a Mata NFA of one pattern read the same symbols.
- **Tapes move in lock step.** A relation of arity _n_ reads one symbol on each tape per step. The `EPSILON` symbol
  is padding and the `ANY` symbol matches any symbol of the tape's alphabet.
- **A complement is taken over an alphabet**, which holds the symbols the leaves read on the tapes involved, plus
  those added with `leaf_over` or the query options. Its moves read sets of symbols per tape, split only where the
  moves of the complemented term differ, so a complement over many tapes costs no more moves than that term has.
- **The antichain** skips a macrostate of the checked term that is below one found before, which costs a
  comparison with those found. On large products of deterministic automata the comparisons can cost more than they
  save; the `antichain(false)` option turns it off and still minimizes complements.
- **Explicit nodes** help when a subterm is small or used by many queries. Such a subterm is built once (and cached
  in the formula) and gets a better simulation for pruning. Its `max_states` option keeps it lazy when it turns out
  large.

## Plans and policies

A query can run the same formula with different per-term choices, given as a `Plan` in `Options::plan`. An
`Annotation` makes a term lazy or eager (`Eval::Lazy`, `Eval::Eager` with an optional budget, like
`explicit_with`) and fixes the operand order of an intersection or a synchronized product (`Order::AsWritten`,
`Order::Swapped`; by default the engine puts the operand with fewer macrostates left in an intersection). A plan
changes the cost of a query, never its answer: only a limit of the query can turn an answer into an error, and the
witness can differ. Plans name terms of one formula; an order on a term that is not a product, or a term of another
formula, makes the query fail.

`Formula::view(term)` gives the terms below `term`, children first, with their kind, arity, operands, fan-out and
the size of each leaf. A `Policy` turns that view into a plan:

- `AllLazy`: every operation lazy, even the formula's explicit nodes.
- `AllEager`: every operation built explicitly, children first, the eager baseline.
- `Heuristic`: bounds the macrostates of each term from the leaf sizes (products multiply, unions add, a
  complement is exponential) and builds explicitly the largest terms below the root that stay under a bound. For
  `(A ∩ B) ∩ C` with small `A` and `B`, it builds `A ∩ B` eagerly and runs the outer intersection lazily.
- `External`: a plan from outside, such as one that a person or a tool wrote after reading the view.

With the `serde` feature, `PlanView::to_json` and `Stats` serialize to JSON, with terms written as their `Term::id`,
and `Formula::plan_from_json` reads a plan back, rejecting it as a whole when it names a missing term, a term
twice, or an order of a term that is not a product:

```json
{
  "annotations": [
    { "term": 3, "eval": { "eager": { "max_states": 1000 } } },
    { "term": 4, "eval": "lazy", "order": "swapped" }
  ]
}
```

`Heuristic` is opt-in: no plan keeps the default behaviour, and the heuristic has no benchmarked cost rule yet.

## Examples

Run them from `bindings/rust`. The ones with Mata leaves live in the `mata` crate.

```text
cargo run --release -p mata --example lazy_inclusion             # inclusion of regular expressions
cargo run --release -p mata --example lazy_model_checking        # regular model checking, inductive invariant
cargo run --release -p mata-lazy --example custom_automaton      # your own automaton and transducer
cargo run --release -p mata --example lazy_sat_solver            # a SAT solver, on a hard sudoku
cargo run --release -p mata --example lazy_mixed_backends        # one formula over all three backends
```

## Testing

Run the tests from `bindings/rust`, or `just bindings rust` from the repository root to test every crate of the
Rust bindings.

```text
cargo test -p mata-lazy --all-features      # the engine, without Mata
cargo test -p mata --test lazy              # with Mata leaves, against Mata's eager operations
```

`MATA_LAZY_ORACLE_SEEDS=200000 MATA_LAZY_ORACLE_DEPTH=5 cargo test -p mata --release --test lazy -- oracle` runs a
larger random campaign against an independent evaluator. `mata-lazy/fuzz` holds a cargo-fuzz target, which needs
nightly and `cargo install cargo-fuzz`.

```text
cd mata-lazy/fuzz
cargo +nightly fuzz run -O formulas -- -max_total_time=600
cargo run --release --bin replay -- regressions/*      # replays saved inputs, with the time of every query
```

## Limits

- Tapes are read in lock step only; there is no asynchronous alignment of padding as in Mata's own composition.
- Leaves are copied in when added, so they must be finite automata.
- The limits are checked step by step inside the successors of a macrostate and during leaf simulations, and the
  time limit also during the minimization of a set; one step (one move of a product joined with a very large post)
  runs to its end first. A leaf simulation cut short by `time_limit` is not kept, so a limit shorter than the
  simulation fails every time; a query with enough time computes it once for the formula.
- The engine recurses over the formula and grows its stack with `stacker` when the formula is deep. On WebAssembly
  (Emscripten), wasm frames also use the JavaScript engine's stack, which `stacker` cannot grow: a chain of 20 000
  nested intersections overflows it even under `node --stack-size=4000`, the runner the workspace sets. Keep
  formulas shallow there, for example by building long chains of unions and intersections as balanced trees.

## License

MIT, like Mata.
