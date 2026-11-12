//! Error types.

use crate::State;

/// Any error of this crate, for code that propagates several kinds with `?`.
#[derive(Clone, Debug, PartialEq, Eq, thiserror::Error)]
#[non_exhaustive]
pub enum Error {
	/// Invalid `.mata` input.
	#[error(transparent)]
	Parse(#[from] ParseError),
	/// Invalid regular expression.
	#[error(transparent)]
	Regex(#[from] RegexError),
	/// A state argument is not a state of the automaton.
	#[error(transparent)]
	NoSuchState(#[from] NoSuchState),
	/// An operation stopped at one of its [`Limits`](crate::Limits).
	#[error(transparent)]
	LimitExceeded(#[from] LimitExceeded),
	/// A symbol or name that an [`Alphabet`](crate::Alphabet) cannot take.
	#[error(transparent)]
	Alphabet(#[from] AlphabetError),
	/// A transducer argument that does not fit the transducer.
	#[error(transparent)]
	Nft(#[from] NftError),
	/// An automaton from other C++ code that breaks Mata's invariants.
	#[error(transparent)]
	InvalidAutomaton(#[from] InvalidAutomaton),
	/// An operand or argument that does not meet a requirement of the operation.
	#[error(transparent)]
	UnmetRequirement(#[from] UnmetRequirement),
}

/// Error from parsing the `.mata` format. It carries Mata's message.
#[derive(Clone, Debug, PartialEq, Eq, thiserror::Error)]
#[error("invalid .mata input: {message}")]
pub struct ParseError {
	message: String,
}

impl ParseError {
	pub(crate) fn new(message: impl Into<String>) -> ParseError {
		ParseError {
			message: message.into(),
		}
	}

	/// The parser's message.
	#[must_use]
	pub fn message(&self) -> &str {
		&self.message
	}
}

/// Error from parsing a regular expression. It carries RE2's message.
#[derive(Clone, Debug, PartialEq, Eq, thiserror::Error)]
#[error("invalid regular expression: {message}")]
pub struct RegexError {
	message: String,
}

impl RegexError {
	pub(crate) fn new(message: impl Into<String>) -> RegexError {
		RegexError {
			message: message.into(),
		}
	}

	/// RE2's message.
	#[must_use]
	pub fn message(&self) -> &str {
		&self.message
	}
}

/// A state argument that is not a state of the automaton.
#[derive(Clone, Copy, Debug, PartialEq, Eq, thiserror::Error)]
#[error("state {state} does not exist (the automaton has {num_states} states)")]
#[non_exhaustive]
pub struct NoSuchState {
	/// The state that was passed.
	pub state: State,
	/// The number of states of the automaton.
	pub num_states: usize,
}

/// Error from an operation that stopped at one of its [`Limits`](crate::Limits).
#[derive(Clone, Debug, PartialEq, Eq, thiserror::Error)]
#[non_exhaustive]
pub enum LimitExceeded {
	/// A deterministic automaton would have more than `max_states` states
	/// ([`Limits::max_states`](crate::Limits::max_states)).
	#[error("the deterministic automaton would have more than {max_states} states")]
	States {
		/// The limit.
		max_states: usize,
	},
}

/// Error from adding a symbol or name to an [`Alphabet`](crate::Alphabet).
#[derive(Clone, Debug, PartialEq, Eq, thiserror::Error)]
#[non_exhaustive]
pub enum AlphabetError {
	/// [`Symbol::EPSILON`](crate::Symbol::EPSILON), which is never a member.
	#[error("epsilon is not a member of any alphabet")]
	Epsilon,
	/// The name `<dntcr>`, which Mata reserves.
	#[error("the name <dntcr> is reserved")]
	ReservedName,
	/// A name that consists only of decimal digits. Printing would write it as a symbol number,
	/// so a round trip through the `.mata` format would resolve it to the symbol with that number.
	#[error("the name {name} is only digits")]
	NumericName {
		/// The rejected name.
		name: String,
	},
	/// No symbol below `u32::MAX - 1` (the value of [`DONT_CARE`](crate::nft::DONT_CARE)) is left for a
	/// new name.
	#[error("the alphabet has no symbol left")]
	Full,
}

/// Error from a transducer argument that does not fit the transducer.
#[derive(Clone, Copy, Debug, PartialEq, Eq, thiserror::Error)]
#[non_exhaustive]
pub enum NftError {
	/// A state argument is not a state of the transducer.
	#[error(transparent)]
	NoSuchState(#[from] NoSuchState),
	/// A state where a transition starts or ends, or an initial or final state, is not on level 0.
	#[error("state {state} has level {level}, not 0")]
	NotLevelZero {
		/// The state that was passed.
		state: State,
		/// Its level.
		level: usize,
	},
	/// A number of levels that a transducer cannot have: 0, or more than
	/// [`MAX_LEVELS`](crate::nft::MAX_LEVELS).
	#[error("a transducer needs 1 to {max} levels, not {num_levels}", max = crate::nft::MAX_LEVELS)]
	InvalidNumLevels {
		/// The number that was passed.
		num_levels: usize,
	},
	/// Another transducer, or the symbols or words of a transition, have another number of levels.
	#[error("expected {expected} levels, found {found}")]
	LevelMismatch {
		/// The number of levels of the transducer.
		expected: usize,
		/// The number of levels that was passed.
		found: usize,
	},
	/// A level argument is not a level of the transducer.
	#[error("level {level} does not exist (the transducer has {num_levels} levels)")]
	NoSuchLevel {
		/// The level that was passed.
		level: usize,
		/// The number of levels of the transducer.
		num_levels: usize,
	},
	/// Levels that are not in strictly increasing order.
	#[error("the levels are not in strictly increasing order")]
	LevelsNotIncreasing,
	/// The result would have no level.
	#[error("the result would have no level")]
	NoLevelLeft,
	/// A path whose words are all empty.
	#[error("at least one word of a path must be nonempty")]
	EmptyPath,
}

/// An automaton from other C++ code that breaks Mata's invariants. It carries the shim's message.
#[derive(Clone, Debug, PartialEq, Eq, thiserror::Error)]
#[error("invalid automaton: {message}")]
pub struct InvalidAutomaton {
	message: String,
}

impl InvalidAutomaton {
	pub(crate) fn new(message: impl Into<String>) -> InvalidAutomaton {
		InvalidAutomaton {
			message: message.into(),
		}
	}

	/// The shim's message.
	#[must_use]
	pub fn message(&self) -> &str {
		&self.message
	}
}

/// An operand or argument that does not meet a requirement of the operation, such as a deterministic
/// and complete automaton for [`Nfa::union_det_complete`](crate::Nfa::union_det_complete). It carries
/// the shim's message.
#[derive(Clone, Debug, PartialEq, Eq, thiserror::Error)]
#[error("{message}")]
pub struct UnmetRequirement {
	message: String,
}

impl UnmetRequirement {
	pub(crate) fn new(message: impl Into<String>) -> UnmetRequirement {
		UnmetRequirement {
			message: message.into(),
		}
	}

	/// The shim's message.
	#[must_use]
	pub fn message(&self) -> &str {
		&self.message
	}
}
