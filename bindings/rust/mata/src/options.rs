//! Algorithm choices of the operations that Mata implements in more than one way.
//!
//! Each enum's default is the algorithm Mata uses by default.

use mata_sys::ffi;

/// How [`Nfa::determinize_with`](crate::Nfa::determinize_with) determinizes. Both give the same DFA up to
/// the numbering of its states.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum Determinization {
	/// The "Four Russians" subset construction: the post of each part of a macrostate inside a block of
	/// states is computed once and kept in a table. Usually much faster on large automata.
	#[default]
	FourRussians,
	/// The classical subset construction.
	Classical,
}

/// How [`Nfa::minimize_with`](crate::Nfa::minimize_with) minimizes. Both give the minimal (trimmed) DFA.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum Minimization {
	/// Brzozowski's algorithm: determinize the reversal twice. Works on any automaton, but can blow up
	/// exponentially even on a DFA.
	Brzozowski,
	/// Hopcroft's partition refinement. The automaton is determinized and trimmed first when needed
	/// (Mata's `"auto"`).
	#[default]
	Hopcroft,
}

/// How [`Nfa::complement_with`](crate::Nfa::complement_with) complements.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum Complementation {
	/// Determinize, complete with a sink state, and swap final and non-final states.
	#[default]
	Classical,
	/// Minimize with Brzozowski's algorithm, complete, and swap final and non-final states.
	Brzozowski,
}

/// How [`Nfa::reduce_with`](crate::Nfa::reduce_with) shrinks an automaton. The result keeps the language
/// and may stay nondeterministic.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum Reduction {
	/// Merge states related by forward simulation.
	#[default]
	Simulation,
	/// Build the residual automaton (the canonical RFSA).
	Residual {
		/// When the residual states are computed.
		construction: ResidualConstruction,
		/// Forward or backward residuals.
		direction: Direction,
	},
}

/// When a residual reduction computes the residual states.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum ResidualConstruction {
	/// After a full determinization (Mata's `"after"`).
	#[default]
	After,
	/// During the determinization (Mata's `"with"`).
	During,
}

/// Direction of a residual reduction.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub enum Direction {
	/// Residuals of the automaton.
	#[default]
	Forward,
	/// Residuals of the reversed automaton.
	Backward,
}

/// Algorithm of the universality, inclusion and equivalence checks.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub enum CheckAlgorithm {
	/// Complement one side and search the product for an accepted word.
	Naive,
	/// Search the subset construction lazily, pruned by antichains. Usually much faster.
	#[default]
	Antichains,
}

impl From<Determinization> for ffi::Determinization {
	fn from(algorithm: Determinization) -> ffi::Determinization {
		match algorithm {
			Determinization::FourRussians => ffi::Determinization::FourRussians,
			Determinization::Classical => ffi::Determinization::Classical,
		}
	}
}

impl From<Minimization> for ffi::Minimization {
	fn from(algorithm: Minimization) -> ffi::Minimization {
		match algorithm {
			Minimization::Brzozowski => ffi::Minimization::Brzozowski,
			Minimization::Hopcroft => ffi::Minimization::Hopcroft,
		}
	}
}

impl From<Complementation> for ffi::Complementation {
	fn from(algorithm: Complementation) -> ffi::Complementation {
		match algorithm {
			Complementation::Classical => ffi::Complementation::Classical,
			Complementation::Brzozowski => ffi::Complementation::Brzozowski,
		}
	}
}

impl From<ResidualConstruction> for ffi::ResidualConstruction {
	fn from(construction: ResidualConstruction) -> ffi::ResidualConstruction {
		match construction {
			ResidualConstruction::After => ffi::ResidualConstruction::After,
			ResidualConstruction::During => ffi::ResidualConstruction::With,
		}
	}
}

impl From<Direction> for ffi::Direction {
	fn from(direction: Direction) -> ffi::Direction {
		match direction {
			Direction::Forward => ffi::Direction::Forward,
			Direction::Backward => ffi::Direction::Backward,
		}
	}
}

impl From<CheckAlgorithm> for ffi::CheckAlgorithm {
	fn from(algorithm: CheckAlgorithm) -> ffi::CheckAlgorithm {
		match algorithm {
			CheckAlgorithm::Naive => ffi::CheckAlgorithm::Naive,
			CheckAlgorithm::Antichains => ffi::CheckAlgorithm::Antichains,
		}
	}
}
