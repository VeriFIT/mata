//! Errors of building and querying a formula.

use std::time::Duration;

use crate::{Stats, Symbol, Tape};

/// Why a builder or a query of a [`Formula`](crate::Formula) failed.
#[derive(Clone, Debug, PartialEq, Eq, thiserror::Error)]
#[non_exhaustive]
pub enum Error {
	/// The term belongs to another formula.
	#[error("the term belongs to another formula")]
	ForeignTerm,
	/// An operand of the wrong arity.
	#[error("{operation} expects arity {expected}, found {found}")]
	Arity {
		/// The builder.
		operation: &'static str,
		/// The arity it needs.
		expected: usize,
		/// The arity it got.
		found: usize,
	},
	/// A tape at or above the arity of its term.
	#[error("tape {tape} is out of range for arity {arity}")]
	TapeOutOfRange {
		/// The tape.
		tape: usize,
		/// The arity of the term.
		arity: usize,
	},
	/// A tape listed twice in a projection.
	#[error("tape {tape} is listed twice")]
	RepeatedTape {
		/// The tape.
		tape: usize,
	},
	/// A tape of a synchronized product at or above the arity of its operand.
	#[error("{tape:?} is out of range for arity {arity}")]
	SyncTapeOutOfRange {
		/// The tape.
		tape: Tape,
		/// The arity of its operand.
		arity: usize,
	},
	/// A tape of a synchronized product that is synchronized twice, or laid out twice (itself or its
	/// synchronized peer).
	#[error("{tape:?} is used twice")]
	RepeatedSyncTape {
		/// The tape.
		tape: Tape,
	},
	/// A move of a leaf that does not read one symbol per tape.
	#[error("a move reads {found} symbols, but the automaton has {arity} tapes")]
	MoveWidth {
		/// The arity of the automaton.
		arity: usize,
		/// The symbols of the move.
		found: usize,
	},
	/// [`EPSILON`](Symbol::EPSILON) or [`ANY`](Symbol::ANY) in an alphabet.
	#[error("the reserved symbol {symbol} cannot be in an alphabet")]
	ReservedSymbol {
		/// The symbol.
		symbol: Symbol,
	},
	/// A complement whose alphabets have more tuples than `usize` can count.
	#[error("a complement over {arity} tapes has too many tuples to enumerate")]
	ComplementTooLarge {
		/// The arity of the complement.
		arity: usize,
	},
	/// Neither one alphabet nor one per tape.
	#[error("{found} alphabets for {arity} tapes")]
	AlphabetCount {
		/// The arity of the automaton.
		arity: usize,
		/// The alphabets given.
		found: usize,
	},
	/// Mata refused a transducer, such as one with 0 or more than
	/// [`MAX_LEVELS`](mata::nft::MAX_LEVELS) levels.
	#[cfg(feature = "mata")]
	#[error(transparent)]
	Mata(#[from] mata::NftError),
	/// The query held more macrostates than [`max_states`](crate::Options::max_states).
	#[error("the query holds more than {max_states} macrostates")]
	#[non_exhaustive]
	StateLimit {
		/// The limit.
		max_states: usize,
		/// What the query did until it stopped.
		stats: Box<Stats>,
	},
	/// The query ran longer than [`time_limit`](crate::Options::time_limit).
	#[error("the query ran longer than {limit:?}")]
	#[non_exhaustive]
	TimeLimit {
		/// The limit.
		limit: Duration,
		/// What the query did until it stopped.
		stats: Box<Stats>,
	},
}
