//! The NFA builder of the tests over Mata leaves.

use mata::{Nfa, State, Symbol};

/// An NFA with states `0..num_states` and one transition per `(source, symbol, target)`.
pub(crate) fn nfa(
	num_states: u32,
	initial: &[u32],
	finals: &[u32],
	moves: &[(u32, Symbol, u32)],
) -> Nfa {
	let mut nfa = Nfa::with_states(num_states as usize);
	for &state in initial {
		nfa.add_initial(State::new(state))
			.expect("a state of the NFA");
	}

	for &state in finals {
		nfa.add_final(State::new(state))
			.expect("a state of the NFA");
	}

	for &(source, symbol, target) in moves {
		nfa.add_transition(State::new(source), symbol, State::new(target))
			.expect("a state of the NFA");
	}

	nfa
}
