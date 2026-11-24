//! Decision procedures for logics whose models are encoded as words, on top of [`mata_lazy`].
//!
//! A [`Logic`] builds formulas out of atoms, which are automata over bits, with `∧`, `∨`, `¬`, `∃` and `∀`, and
//! decides them lazily: a formula is a [`mata_lazy::Formula`] term with one tape per free variable, and a query
//! explores only what its answer needs. The encoding, and so the logic, is chosen by the atoms and the
//! [`Padding`]: natural numbers or finite sets (WS1S) pad with zeros, integers in two's complement (Presburger
//! arithmetic, LIA) repeat the sign.
//!
//! This crate is pure Rust. `mata` re-exports it as `mata::logic`.

mod closure;
mod error;
mod logic;
mod model;

pub use error::Error;
pub use logic::{Logic, ONE, Padding, Prop, Var, ZERO};
pub use model::Model;
