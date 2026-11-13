//! Lazy emptiness checking of Boolean and relational combinations of automata and transducers.
//!
//! A [`Formula`] is a DAG of operations (union, intersection, complement, identity, projection,
//! synchronized product, composition, images) over leaves. A leaf is any [`Automaton`], and the backends
//! implement it for the automata of other crates.
//!
//! | Feature | Leaves |
//! |---|---|
//! | `mata` (default) | Mata `Nfa` and `Nft` (Mata's Rust bindings); also enables `materialize` |
//! | `regex` | regex-automata DFAs (`Regex`), pure Rust |
//! | `rustfst` | rustfst `VectorFst` and `ConstFst` transducers, pure Rust |
//!
//! The default build compiles Mata, a C++ library, or links an installed one (the README says what that
//! needs). `default-features = false` builds in pure Rust, with the other backends or with your own automata.
//! Leaves of different backends mix in one formula, as they share [`Symbol`]s, numbered as in Mata.
//!
//! Building a formula does no automata work. A query ([`is_empty`](Formula::is_empty),
//! [`check`](Formula::check)) explores only the macrostates it needs. Products pair states and complements
//! build subsets on the fly, and simulation-based subsumption prunes macrostates that are below others. A term
//! marked [`explicit`](Formula::explicit) is built as an automaton when a query starts and read like a leaf.
//!
//! ```
//! # #[cfg(feature = "mata")] {
//! use lazymat::Formula;
//! use mata::Nfa;
//!
//! let mut formula = Formula::new();
//! let a = formula.leaf(&Nfa::from_regex("a(a|b)*")?)?;
//! let b = formula.leaf(&Nfa::from_regex("(a|b)*b")?)?;
//! // L(a) ⊆ L(b) iff L(a) ∩ ¬L(b) = ∅.
//! let not_b = formula.complement(b)?;
//! let difference = formula.intersection(a, not_b)?;
//! let witness = formula.find_word(difference)?.expect("`a` is not included");
//! assert_eq!(witness.tape(0), Some(vec![b'a'.into()]));
//! # }
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! # Semantics
//!
//! - **Lock step.** A relation of arity *n* is a language over *n*-tuples of symbols, and one step reads one
//!   symbol on every tape. [`EPSILON`](Symbol::EPSILON) pads a tape and matches only padding.
//!   [`ANY`](Symbol::ANY) reads any symbol of the tape's alphabet, the same one on both tapes of an identity.
//!   This is exact for length-preserving relations and for relations whose padding lines up, as in regular
//!   model checking. Mata and rustfst align padding asynchronously, so their own operations can give other
//!   relations on transducers that pad.
//! - **Symbols.** A byte is the symbol of the same number, so a regex and a Mata NFA of one pattern read the
//!   same symbols. A rustfst label `n` is the symbol `n`, and label 0 is padding.
//! - **Alphabets.** Tapes that must read the same symbols share an alphabet, such as both operands of an
//!   intersection, the synchronized tapes of a product and both tapes of an identity. It holds every concrete
//!   symbol that a leaf reads or declares ([`leaf_over`](Formula::leaf_over)) on these tapes, and for the
//!   checked term also the symbols of the query ([`alphabet`](Options::alphabet)). The same complement can
//!   therefore mean different things in different queries.
//! - **Complement** is taken over the tuples of concrete symbols of the alphabets, so it holds no padding and
//!   no wildcards. A double complement gives the term back only when the term has no padded words.
//! - **Leaves.** [`leaf`](Formula::leaf) copies the part of the automaton reachable from its initial states,
//!   without silent moves (moves that pad every tape, such as epsilon transitions) and without the states
//!   that reach no final state.

#![forbid(unsafe_code)]

mod automaton;
mod engine;
mod error;
mod formula;
mod ids;
mod leaf;
#[cfg(feature = "mata")]
mod mata;
mod options;
#[cfg(feature = "regex")]
pub mod regex;
#[cfg(feature = "rustfst")]
pub mod rustfst;
mod simulation;
mod symbol;
mod witness;

pub use automaton::Automaton;
pub use error::Error;
pub use formula::{Formula, Tape, Term};
pub use options::{ExplicitOptions, Materialized, Options, Outcome, Stats};
pub use symbol::Symbol;
pub use witness::Witness;
