//! Small Mata automata and transducers shared by `port` and `regressions`.

use mata::nft::Nft;
use mata::{Nfa, State, Symbol};

use crate::common::nfa;

/// A transducer with level-0 states `0..num_states` and one transition per `(source, symbols, target)`.
pub(crate) fn nft(
	levels: usize,
	num_states: u32,
	initial: &[u32],
	finals: &[u32],
	moves: &[(u32, &[Symbol], u32)],
) -> Nft {
	let mut nft = Nft::new(levels).expect("1 to MAX_LEVELS levels");
	let states: Vec<State> = (0..num_states).map(|_| nft.add_state()).collect();
	for &state in initial {
		nft.add_initial(states[state as usize])
			.expect("a level-0 state");
	}

	for &state in finals {
		nft.add_final(states[state as usize])
			.expect("a level-0 state");
	}

	for &(source, symbols, target) in moves {
		nft.add_transition(states[source as usize], symbols, states[target as usize])
			.expect("a transition of the NFT");
	}

	nft
}

/// The language `{symbol}`.
pub(crate) fn single(symbol: Symbol) -> Nfa {
	nfa(2, &[0], &[1], &[(0, symbol, 1)])
}

/// The language `symbol*`.
pub(crate) fn star(symbol: Symbol) -> Nfa {
	nfa(1, &[0], &[0], &[(0, symbol, 0)])
}

/// The relation that holds only the tuple `symbols`, one symbol per tape.
pub(crate) fn tuple(symbols: &[Symbol]) -> Nft {
	nft(symbols.len(), 2, &[0], &[1], &[(0, symbols, 1)])
}
