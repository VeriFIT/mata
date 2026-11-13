//! Mata automata and transducers as leaves, and the export of a materialized term.
//!
//! Mata symbols keep their numbers, so Mata's `EPSILON` is [`EPSILON`](Symbol::EPSILON) and its `DONT_CARE` is
//! [`ANY`](Symbol::ANY), also in an [`Nfa`].

use std::fmt;

use mata::nft::Nft;
use mata::{Alphabet, Nfa, NftError, State};

use crate::ids::{Id, StateId};
use crate::leaf::Leaf;
use crate::{Automaton, Symbol, Witness};

/// Arity 1. Epsilon transitions are silent.
impl Automaton for Nfa {
	type State = State;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = State> {
		Nfa::initial_states(self).iter().copied()
	}

	fn is_final(&self, state: State) -> bool {
		Nfa::is_final(self, state)
	}

	fn for_each_move(&self, state: State, mut visit: impl FnMut(&[Symbol], State)) {
		for transition in self.transitions_from(state) {
			visit(&[transition.symbol.into()], transition.target);
		}
	}
}

/// One tape per level. A transition (a chain of one edge per level) is one move.
impl Automaton for Nft {
	type State = State;

	fn arity(&self) -> usize {
		self.num_levels()
	}

	fn initial_states(&self) -> impl IntoIterator<Item = State> {
		Nft::initial_states(self).iter().copied()
	}

	fn is_final(&self, state: State) -> bool {
		Nft::is_final(self, state)
	}

	fn for_each_move(&self, state: State, mut visit: impl FnMut(&[Symbol], State)) {
		let mut tuple = Vec::with_capacity(self.num_levels());
		walk_levels(self, state, &mut tuple, &mut visit);
	}
}

/// Follows the edges from a state of level `tuple.len()` until they return to level 0. An edge that skips
/// levels reads its symbol on each of them, as Mata's jumps do.
fn walk_levels(
	nft: &Nft,
	state: State,
	tuple: &mut Vec<Symbol>,
	visit: &mut dyn FnMut(&[Symbol], State),
) {
	let levels = nft.num_levels();
	let level = tuple.len();

	for edge in nft.edges_from(state) {
		let next = match nft.level(edge.target) {
			Some(0) => levels,
			Some(next) => next,
			// An `Nft` is well formed, so its edges lead to its states.
			None => continue,
		};

		if next <= level {
			continue;
		}

		tuple.resize(next, edge.symbol.into());
		if next == levels {
			visit(tuple, edge.target);
		} else {
			walk_levels(nft, edge.target, tuple, visit);
		}

		tuple.truncate(level);
	}
}

/// Copies `leaf` into `nft`, an empty transducer with one level per tape of `leaf`.
pub(crate) fn fill(nft: &mut Nft, leaf: &Leaf) -> Result<(), NftError> {
	let states: Vec<State> = (0..leaf.num_states()).map(|_| nft.add_state()).collect();
	for &state in leaf.initial() {
		nft.add_initial(states[state.index()])?;
	}

	let mut symbols = Vec::with_capacity(leaf.arity());
	for (state, &source) in (0..).map(StateId::new).zip(&states) {
		if leaf.is_final(state) {
			nft.add_final(source)?;
		}

		for (tuple, target) in leaf.moves(state) {
			symbols.clear();
			symbols.extend(tuple.iter().copied().map(mata::Symbol::from));
			nft.add_transition(source, &symbols, states[target.index()])?;
		}
	}

	Ok(())
}

impl Witness {
	/// Displays the steps with the names of `alphabet` where it has them, as `Display` does with numbers.
	#[must_use = "an adapter does nothing until it is formatted"]
	pub fn display_with(&self, alphabet: &Alphabet) -> impl fmt::Display {
		fmt::from_fn(move |f| self.write(f, |symbol| alphabet.name(symbol.into())))
	}
}
