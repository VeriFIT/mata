#![doc = include_str!("../README.md")]
#![cfg_attr(docsrs, feature(doc_cfg))]

pub mod alphabet;
mod error;
mod limits;
pub mod nfa;
pub mod nft;
mod options;
#[cfg(feature = "serde")]
mod serde_impl;
mod transitions;
mod types;

pub use alphabet::Alphabet;
pub use error::{
	AlphabetError, Error, InvalidAutomaton, LimitExceeded, NftError, NoSuchState, ParseError,
	RegexError,
};
pub use limits::Limits;
pub use nfa::Nfa;
pub use options::{
	CheckAlgorithm, Complementation, Determinization, Direction, Minimization, Reduction,
	ResidualConstruction,
};
pub use types::{Distinction, Run, Side, State, Symbol, Transition, Word, word_from_bytes};

/// The native layer: the `libmata` build and the cxx bridge this crate uses.
pub use mata_sys as sys;

/// Unwraps a bridge call that throws only when something is badly wrong (such as running out of memory).
fn check<T>(result: Result<T, mata_sys::cxx::Exception>) -> T {
	result.unwrap_or_else(|error| panic!("Mata failed: {}", error.what()))
}
