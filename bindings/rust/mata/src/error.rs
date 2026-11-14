//! Error types.

use crate::{State, Symbol};

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
	/// An argument that a replace transducer cannot be built from.
	#[error(transparent)]
	Replace(#[from] ReplaceError),
	/// A step of loading in separate steps that its intermediate automaton does not allow.
	#[error(transparent)]
	Intermediate(#[from] IntermediateError),
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
	/// An interleaved word whose length is not a multiple of the number of levels.
	#[error("a word of {length} symbols does not interleave {num_levels} levels")]
	InterleavedLength {
		/// The length of the word.
		length: usize,
		/// The number of levels of the transducer.
		num_levels: usize,
	},
	/// An automaton that does not encode a transducer: a state would get two levels, or a final state a
	/// level other than 0.
	#[error("the automaton does not encode a transducer with that many levels")]
	InconsistentLevels,
	/// A transducer with epsilon edges, which the operation does not support.
	#[error("the operation needs a transducer without epsilon transitions")]
	EpsilonTransitions,
	/// A reachable state has an edge under a symbol outside the alphabet.
	#[error("the transducer reads a symbol outside the alphabet")]
	NotInAlphabet,
	/// A transducer with a cycle of epsilon edges where the caller promised none.
	#[error("the transducer has a cycle of epsilon transitions")]
	EpsilonCycle,
	/// Word lengths that span more configurations of a search than can be indexed.
	#[error("the word lengths span too many configurations")]
	TooManyConfigurations,
}

/// Error from an argument of a [`strings`](crate::strings) transducer construction.
#[derive(Clone, Debug, PartialEq, Eq, thiserror::Error)]
#[non_exhaustive]
pub enum ReplaceError {
	/// The symbol to replace is not in the alphabet.
	#[error("symbol {symbol} is not in the alphabet")]
	SymbolNotInAlphabet {
		/// The symbol that was passed.
		symbol: Symbol,
	},
	/// The alphabet contains a symbol that the construction gives another meaning:
	/// [`DONT_CARE`](crate::nft::DONT_CARE), or the marker of the call.
	#[error("the alphabet contains symbol {symbol}, which the construction reserves")]
	ReservedSymbol {
		/// The reserved symbol.
		symbol: Symbol,
	},
	/// The marker of the call is epsilon or [`DONT_CARE`](crate::nft::DONT_CARE), which read as moves,
	/// not as a symbol that can mark a position.
	#[error("the marker cannot be symbol {marker}, which is epsilon or DONT_CARE")]
	ReservedMarker {
		/// The marker that was passed.
		marker: Symbol,
	},
	/// Invalid regular expression.
	#[error(transparent)]
	Regex(#[from] RegexError),
	/// A number of levels that a transducer cannot have.
	#[error(transparent)]
	Nft(#[from] NftError),
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
/// which requirement failed and the shim's message.
#[derive(Clone, Debug, PartialEq, Eq, thiserror::Error)]
#[error("{message}")]
pub struct UnmetRequirement {
	requirement: Requirement,
	message: String,
}

impl UnmetRequirement {
	pub(crate) fn new(requirement: Requirement, message: impl Into<String>) -> UnmetRequirement {
		UnmetRequirement {
			requirement,
			message: message.into(),
		}
	}

	/// The requirement that failed.
	#[must_use]
	pub fn requirement(&self) -> Requirement {
		self.requirement
	}

	/// What failed, for diagnostics.
	#[must_use]
	pub fn message(&self) -> &str {
		&self.message
	}
}

/// The requirement that an [`UnmetRequirement`] reports.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum Requirement {
	/// An operand must have an initial state and be deterministic.
	Deterministic,
	/// An operand must be complete over the alphabet of the call.
	Complete,
	/// The automaton must read UTF-8 encoded bytes.
	Utf8,
	/// A numeric parameter must be in its range.
	ParameterRange,
	/// Without an alphabet, every symbol must be a Unicode scalar value; with one, every used
	/// symbol's name, or its number if it is unnamed, must be a single character.
	RegexSymbols,
}

/// Error from a step of loading in separate steps on an
/// [`IntermediateAutomaton`](crate::IntermediateAutomaton).
#[derive(Clone, Debug, PartialEq, Eq, thiserror::Error)]
#[non_exhaustive]
pub enum IntermediateError {
	/// [`mintermize`](crate::IntermediateAutomaton::mintermize) takes only NFAs and AFAs with a
	/// bit-vector alphabet.
	#[error("automaton {index} is not an NFA or AFA with a bit-vector alphabet")]
	NotMintermizable {
		/// The position of the automaton in the call, from 0.
		index: usize,
	},
	/// The section is not of the kind the construction builds.
	#[error("the intermediate automaton is not an {expected}")]
	WrongKind {
		/// `"NFA"` or `"NFT"`.
		expected: &'static str,
	},
	/// The section still has a bit-vector alphabet; mintermize it first.
	#[error("the intermediate automaton has a bit-vector alphabet; mintermize it first")]
	NeedsMintermization,
	/// Mata could not parse the text, or construct a transition of the section, or the alphabet has no
	/// symbol left for a new name.
	#[error(transparent)]
	Parse(#[from] ParseError),
}
