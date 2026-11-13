# lazymat

Lazy emptiness checking of Boolean and relational combinations of automata and transducers, on top of [Mata](https://github.com/VeriFIT/mata) and its Rust bindings.

You build a formula (unions, intersections, complements, products, compositions, projections) over automata from Mata, regex-automata or rustfst, and ask whether its relation is empty. lazymat explores only what the answer needs. Complements and products are built on the fly, and subsumption prunes the search. When the relation is not empty, you get a witness.

## Getting started

lazymat is not on crates.io yet, so use it from the repository, together with the `mata` crate.

```toml
[dependencies]
lazymat = { git = "https://github.com/VeriFIT/mata", branch = "devel" }
mata = { git = "https://github.com/VeriFIT/mata", branch = "devel" }
```

```rust
use lazymat::Formula;
use mata::Nfa;

let mut formula = Formula::new();
let a = formula.leaf(&Nfa::from_regex("a(a|b)*b")?)?;
let b = formula.leaf(&Nfa::from_regex("a(a|b)*")?)?;

// a(a|b)*b ⊆ a(a|b)* iff a(a|b)*b ∩ ¬a(a|b)* = ∅.
let not_b = formula.complement(b)?;
let difference = formula.intersection(a, not_b)?;
assert!(formula.is_empty(difference)?);
```

### Requirements

- Rust 1.95 or newer.
- The default backend, Mata, is a C++ library built from source, which needs CMake 3.23 or newer and a C++23 compiler (GCC 14 or newer, or Clang 18 or newer). To link an installed Mata instead, set `MATA_DIR` (see the [`mata-sys` README](../mata-sys/README.md)).
- Without a C++ toolchain, set `default-features = false` and enable a pure-Rust backend (`features = ["regex"]` or `["rustfst"]`), or none if you bring your own automata.

## Backends

| Feature | Leaves |
|---|---|
| `mata` (default) | Mata `Nfa` and `Nft`; also enables `materialize`, which returns a result as a Mata `Nft` |
| `regex` | lazymat's `Regex`, a DFA over bytes |
| `rustfst` | rustfst `VectorFst` and `ConstFst` (arity 2; weights only decide whether a transition exists) |

Leaves of different backends can be mixed in one formula, and your own automata can be leaves too, with or without a backend (below).

## Your own automata

Any automaton can be a leaf. Implement lazymat's `Automaton` for it and pass it to `leaf`. lazymat asks for the number of tapes, the initial states, whether a state is final, and the moves from a state, each with one `Symbol` per tape. It copies the part reachable from the initial states, so that part must be finite.

```rust
use lazymat::{Automaton, Formula, Symbol};

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

A transducer reports one symbol per tape on each move, with the `EPSILON` symbol on a tape that reads nothing. `examples/custom_automaton.rs` adds a transducer that swaps `a` and `b` and asks what the swap does to the counts.

## Usage

| Builder | Relation |
|---|---|
| `leaf`, `leaf_over` | the language of an automaton; `leaf_over` adds symbols to its alphabets |
| `union`, `intersection`, `complement` | Boolean operations on relations of the same arity |
| `identity` | `{(w, w) \| w ∈ L}` |
| `project` | keeps the given tapes, in that order |
| `sync_product`, `compose_on`, `compose`, `post_image`, `pre_image` | products of relations synchronized on tapes |
| `explicit`, `explicit_with` | the same relation, built as an automaton when a query starts |

| Query | Result |
|---|---|
| `is_empty`, `find_word` | whether the relation is empty, or a witness |
| `check` | witness and statistics, with options for extra alphabet symbols, terms made explicit for one query, limits on states and time (their errors carry the statistics so far), a memory budget for cached results, and subsumption and the antichain on or off |
| `materialize` | the whole relation as a Mata `Nft` |

Every builder and query returns a `Result` with lazymat's `Error`, and misuse such as a wrong arity is an error, not a panic. Queries take `&self`, so several threads can query one formula.

A few things are worth knowing.

- **Symbols are numbers**, as in Mata, and leaves of all backends share them. A byte is the symbol of the same number, so a regex and a Mata NFA of one pattern read the same symbols.
- **Tapes move in lock step.** A relation of arity *n* reads one symbol on each tape per step. The `EPSILON` symbol is padding and the `ANY` symbol matches any symbol of the tape's alphabet.
- **A complement is taken over an alphabet**, which holds the symbols the leaves read on the tapes involved, plus those added with `leaf_over` or the query options.
- **The antichain** skips a macrostate of the checked term that is below one found before, which costs a comparison with those found. On large products of deterministic automata the comparisons can cost more than they save; the `antichain(false)` option turns it off and still minimizes complements.
- **Explicit nodes** help when a subterm is small or used by many queries. Such a subterm is built once (and cached in the formula) and gets a better simulation for pruning. Its `max_states` option keeps it lazy when it turns out large.

## Examples

Run them from `bindings/rust`.

```text
cargo run --release -p lazymat --example lazy_inclusion                  # language inclusion of regular expressions
cargo run --release -p lazymat --example model_checking                  # regular model checking with an inductive invariant
cargo run --release -p lazymat --example custom_automaton                # leaves from your own automaton and transducer
cargo run --release -p lazymat --example sat_solver                      # a SAT solver, on a hard sudoku
cargo run --release -p lazymat --all-features --example mixed_backends   # one formula over all three backends
```

## Testing

Run the tests from `bindings/rust`, or `just bindings rust` from the repository root to test every crate of the Rust bindings.

```text
cargo test -p lazymat --all-features                            # everything
cargo test -p lazymat                                           # with the default backend
cargo test -p lazymat --no-default-features --features regex    # without Mata
```

`LAZYMAT_ORACLE_SEEDS=200000 LAZYMAT_ORACLE_DEPTH=5 cargo test -p lazymat --release --all-features --test oracle` runs a larger random campaign against an independent evaluator. `lazymat/fuzz` holds a cargo-fuzz target, which needs nightly and `cargo install cargo-fuzz`.

```text
cd lazymat/fuzz
cargo +nightly fuzz run -O formulas -- -max_total_time=600
cargo run --release --bin replay -- regressions/*      # replays saved inputs, with the time of every query
```

## Limits

- Tapes are read in lock step only; there is no asynchronous alignment of padding as in Mata's own composition.
- Leaves are copied in when added, so they must be finite automata.
- The limits are checked step by step inside the successors of a macrostate and during leaf simulations, and the time limit also during the minimization of a set; one step (one move of a product joined with a very large post) runs to its end first. A leaf simulation cut short by `time_limit` is not kept, so a limit shorter than the simulation fails every time; a query with enough time computes it once for the formula.

## License

MIT, like Mata.
