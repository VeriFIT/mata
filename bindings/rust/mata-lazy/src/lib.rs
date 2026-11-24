//! Lazy emptiness checking of Boolean and relational combinations of automata and transducers.
//!
//! A [`Formula`] is a DAG of operations (union, intersection, complement, identity, projection,
//! synchronized product, composition, images, prefix closure) over leaves. A leaf is any [`Automaton`], and the
//! backends implement it for the automata of other crates. [`materialize`](Formula::materialize) builds the relation
//! of a term as an [`ExplicitAutomaton`] that other crates can copy.
//!
//! | Backend | Leaves |
//! |---|---|
//! | `mata` crate, feature `lazy` (default) | Mata `Nfa` and `Nft`; this crate is re-exported as `mata::lazy`, which also turns an [`ExplicitAutomaton`] into an `Nft` |
//! | feature `regex` | regex-automata DFAs (`Regex`), pure Rust |
//! | feature `rustfst` | rustfst `VectorFst` and `ConstFst` transducers, pure Rust |
//!
//! This crate itself is pure Rust: it builds without a C++ toolchain, with the pure-Rust backends or with your own
//! automata. Use it through `mata::lazy` for Mata leaves. Leaves of different backends mix in one formula, as they
//! share [`Symbol`]s, numbered as in Mata.
//!
//! Building a formula does no automata work. A query ([`is_empty`](Formula::is_empty),
//! [`check`](Formula::check)) explores only the macrostates it needs. Products pair states and complements
//! build subsets on the fly, and simulation-based subsumption prunes macrostates that are below others. A term
//! marked [`explicit`](Formula::explicit) is built as an automaton when a query starts and read like a leaf.
//! A [`Plan`] chooses per query and term between lazy and explicit, and the operand order of products; a
//! [`Policy`] computes one from the [`view`](Formula::view) of a formula. A plan changes the cost of a query,
//! never its answer.
//!
//! ```
//! # #[cfg(feature = "regex")] {
//! use mata_lazy::Formula;
//! use mata_lazy::regex::Regex;
//!
//! let mut formula = Formula::new();
//! let a = formula.leaf(&Regex::new("a(a|b)*")?)?;
//! let b = formula.leaf(&Regex::new("(a|b)*b")?)?;
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
//!   that reach no final state. Only leaves drop such moves. A projection keeps a step whose kept tapes all
//!   read padding, so the projection of {(ε, a)} on tape 0 is one step of padding, and its complement still
//!   holds the empty word.

#![forbid(unsafe_code)]

mod automaton;
mod engine;
mod error;
mod explicit;
mod formula;
mod ids;
mod leaf;
mod options;
mod plan;
mod policy;
#[cfg(feature = "regex")]
pub mod regex;
#[cfg(feature = "rustfst")]
pub mod rustfst;
mod simulation;
mod symbol;
mod view;
mod witness;

pub use automaton::Automaton;
pub use error::Error;
pub use explicit::ExplicitAutomaton;
pub use formula::{Formula, Pad, Tape, Term};
pub use options::{ExplicitOptions, Materialized, Options, Outcome, Stats};
pub use plan::{Annotation, Eval, Order, Plan};
pub use policy::{AllEager, AllLazy, External, Heuristic, Policy};
pub use symbol::Symbol;
pub use view::{LeafInfo, NodeKind, NodeView, PlanView};
pub use witness::Witness;
