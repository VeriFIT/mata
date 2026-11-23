//! A materialized term, as an explicit automaton that other libraries can copy.

use crate::ids::{Id, StateId};
use crate::leaf::Leaf;
use crate::{Automaton, Symbol};

/// The whole relation of a term, as an automaton with numbered states, built by
/// [`materialize`](crate::Formula::materialize).
///
/// It is trimmed: every state is reachable and reaches a final state. Its moves read one symbol per tape, in lock
/// step, with [`EPSILON`](Symbol::EPSILON) for padding and [`ANY`](Symbol::ANY) where every operand of a step reads
/// any symbol. It is an [`Automaton`] too, so it can be a leaf of another formula.
#[derive(Debug)]
pub struct ExplicitAutomaton(Leaf);

impl ExplicitAutomaton {
	pub(crate) fn new(leaf: Leaf) -> ExplicitAutomaton {
		ExplicitAutomaton(leaf)
	}

	/// The number of tapes.
	#[must_use]
	pub fn arity(&self) -> usize {
		self.0.arity()
	}

	/// The number of states, numbered from 0.
	#[must_use]
	pub fn num_states(&self) -> usize {
		self.0.num_states()
	}

	/// The number of moves.
	#[must_use]
	pub fn num_moves(&self) -> usize {
		self.0.num_moves()
	}

	/// The initial states.
	pub fn initial_states(&self) -> impl ExactSizeIterator<Item = usize> {
		self.0.initial().iter().map(|state| state.index())
	}

	/// Whether `state` is final.
	///
	/// # Panics
	///
	/// If `state` is not below [`num_states`](ExplicitAutomaton::num_states).
	#[must_use]
	pub fn is_final(&self, state: usize) -> bool {
		self.0.is_final(StateId::new(state))
	}

	/// The moves from `state`, as the symbols (one per tape) and the target, sorted by symbols, then target.
	///
	/// # Panics
	///
	/// If `state` is not below [`num_states`](ExplicitAutomaton::num_states).
	pub fn moves(&self, state: usize) -> impl ExactSizeIterator<Item = (&[Symbol], usize)> {
		self.0
			.moves(StateId::new(state))
			.map(|(tuple, target)| (tuple, target.index()))
	}
}

impl Automaton for ExplicitAutomaton {
	type State = usize;

	fn arity(&self) -> usize {
		self.arity()
	}

	fn initial_states(&self) -> impl IntoIterator<Item = usize> {
		self.initial_states()
	}

	fn is_final(&self, state: usize) -> bool {
		self.is_final(state)
	}

	fn for_each_move(&self, state: usize, mut visit: impl FnMut(&[Symbol], usize)) {
		for (tuple, target) in self.moves(state) {
			visit(tuple, target);
		}
	}
}
