//! Lazy emptiness checking of Boolean and relational combinations of automata and transducers: the
//! [`mata-lazy`](https://github.com/VeriFIT/mata/tree/devel/bindings/rust/mata-lazy) crate, with Mata's [`Nfa`]s
//! and [`Nft`]s as leaves.
//!
//! Everything of `mata-lazy` is here. This module adds the [`Automaton`] implementations of [`Nfa`] (arity 1) and
//! [`Nft`] (one tape per level), the conversion of an [`ExplicitAutomaton`] into an [`Nft`], and the conversions
//! between Mata's [`Symbol`](crate::Symbol) and this module's [`Symbol`], which keep the number. Mata's
//! `EPSILON` is [`EPSILON`](Symbol::EPSILON) and its `DONT_CARE` is [`ANY`](Symbol::ANY), also in an [`Nfa`].
//!
//! ```
//! use mata::Nfa;
//! use mata::lazy::Formula;
//!
//! let mut formula = Formula::new();
//! let a = formula.leaf(&Nfa::from_regex("a(a|b)*")?)?;
//! let b = formula.leaf(&Nfa::from_regex("(a|b)*b")?)?;
//! // L(a) ⊆ L(b) iff L(a) ∩ ¬L(b) = ∅.
//! let not_b = formula.complement(b)?;
//! let difference = formula.intersection(a, not_b)?;
//! let witness = formula.find_word(difference)?.expect("`a` is not included");
//! assert_eq!(witness.tape(0), Some(vec![b'a'.into()]));
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```

pub use mata_lazy::*;

use crate::nft::Nft;
use crate::{Nfa, NftError, State};

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

/// One level per tape, trimmed as the automaton is. [`ANY`](Symbol::ANY) stays as
/// [`DONT_CARE`](crate::nft::DONT_CARE), and [`to_nfa_over`](Nft::to_nfa_over) expands it for arity 1.
///
/// Mata reads padding asynchronously, so for a relation that is not length-preserving the Mata relation drops it:
/// `(a, ε)` then `(ε, b)` is the pair `(a, b)` there. [`leaf_over`](Formula::leaf_over) reads the transducer back
/// in lock step, given the alphabets.
///
/// # Errors
///
/// If Mata refuses the number of levels: 0 or more than [`MAX_LEVELS`](crate::nft::MAX_LEVELS).
impl TryFrom<&ExplicitAutomaton> for Nft {
	type Error = NftError;

	fn try_from(automaton: &ExplicitAutomaton) -> Result<Nft, NftError> {
		let mut nft = Nft::new(automaton.arity())?;
		let states: Vec<State> = (0..automaton.num_states())
			.map(|_| nft.add_state())
			.collect();
		for state in automaton.initial_states() {
			nft.add_initial(states[state])?;
		}

		let mut symbols = Vec::with_capacity(automaton.arity());
		for (state, &source) in states.iter().enumerate() {
			if automaton.is_final(state) {
				nft.add_final(source)?;
			}

			for (tuple, target) in automaton.moves(state) {
				symbols.clear();
				symbols.extend(tuple.iter().copied().map(crate::Symbol::from));
				nft.add_transition(source, &symbols, states[target])?;
			}
		}

		Ok(nft)
	}
}

impl From<crate::Symbol> for Symbol {
	fn from(symbol: crate::Symbol) -> Symbol {
		Symbol::from(symbol.value())
	}
}

impl From<Symbol> for crate::Symbol {
	fn from(symbol: Symbol) -> crate::Symbol {
		crate::Symbol::new(symbol.into())
	}
}
