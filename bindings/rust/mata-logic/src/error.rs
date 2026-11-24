//! Errors of building and deciding formulas.

use crate::Var;

/// Why building or deciding a formula failed.
#[derive(Clone, Debug, PartialEq, Eq, thiserror::Error)]
#[non_exhaustive]
pub enum Error {
	/// The variable was not created by this logic.
	#[error("variable {var} is not a variable of this logic")]
	ForeignVar {
		/// The variable.
		var: Var,
	},
	/// A variable given twice.
	#[error("variable {var} is given twice")]
	RepeatedVar {
		/// The variable.
		var: Var,
	},
	/// An atom whose automaton does not have one tape per variable.
	#[error("an automaton of arity {arity} for {vars} variables")]
	AtomArity {
		/// The arity of the automaton.
		arity: usize,
		/// The number of variables.
		vars: usize,
	},
	/// Text that is not a formula of a front-end, such as [`Ws1s::parse`](crate::ws1s::Ws1s::parse).
	#[error("at byte {offset}: {message}")]
	Parse {
		/// The byte offset of the error in the text.
		offset: usize,
		/// What is wrong.
		message: String,
	},
	/// An error of the formula, including the limits of a query.
	#[error(transparent)]
	Lazy(#[from] mata_lazy::Error),
}
