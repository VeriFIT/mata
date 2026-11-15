//! The interface to leaves.

use std::hash::Hash;

use crate::Symbol;

/// A finite automaton over tuples of symbols, the interface through which a formula reads its leaves.
///
/// A move reads one [`Symbol`] on every tape at once (lock step). [`EPSILON`](Symbol::EPSILON) is padding, where the
/// tape reads nothing while the other tapes read. [`ANY`](Symbol::ANY) reads any one symbol of the tape's alphabet.
/// A move that pads every tape of an automaton with at least one tape is silent, like an epsilon
/// transition of an NFA.
///
/// [`leaf`](crate::Formula::leaf) copies the part reachable from the initial states, so that part
/// must be finite.
pub trait Automaton {
	/// A state.
	type State: Copy + Eq + Hash;

	/// The number of tapes.
	fn arity(&self) -> usize;

	/// The initial states.
	fn initial_states(&self) -> impl IntoIterator<Item = Self::State>;

	/// Whether `state` is final.
	fn is_final(&self, state: Self::State) -> bool;

	/// Calls `visit` with the symbols (one per tape) and the target of every move from `state`.
	fn for_each_move(&self, state: Self::State, visit: impl FnMut(&[Symbol], Self::State));
}
