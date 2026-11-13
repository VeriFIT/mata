//! Leaves from [regex-automata](https://docs.rs/regex-automata) DFAs.
//!
//! A DFA reads bytes, and a byte is the symbol of the same number, as in Mata's `from_regex`. So a regex-automata
//! leaf and a Mata leaf of the same pattern read the same symbols.
//!
//! ```
//! use lazymat::Formula;
//! use lazymat::regex::Regex;
//!
//! let mut formula = Formula::new();
//! let ab = formula.leaf(&Regex::new("(ab)*")?)?;
//! let a_or_b = formula.leaf(&Regex::new("[ab]*")?)?;
//! let not_a_or_b = formula.complement(a_or_b)?;
//! let difference = formula.intersection(ab, not_a_or_b)?;
//! assert!(formula.is_empty(difference)?);
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```

use std::str::FromStr;

use regex_automata::dfa::{Automaton as _, StartError, StartKind, dense};
use regex_automata::util::primitives::StateID;
use regex_automata::util::start;
use regex_automata::util::syntax;
use regex_automata::{Anchored, MatchKind};
use rustc_hash::{FxHashMap, FxHashSet};

use crate::{Automaton, Symbol};

type Dfa = dense::DFA<Vec<u32>>;

/// Why a [`Regex`] could not be built.
#[derive(Debug, thiserror::Error)]
#[non_exhaustive]
pub enum Error {
	/// regex-automata could not build the DFA, for example for a syntax error.
	#[error(transparent)]
	Build(#[from] Box<dense::BuildError>),
	/// The DFA has no anchored start state.
	#[error(transparent)]
	Start(#[from] StartError),
}

/// The language of a regular expression, as a dense DFA that must match the whole word.
#[derive(Clone, Debug)]
pub struct Regex {
	dfa: Dfa,
	start: StateID,
	/// The states that lead to a match. The others include the states that only report a match one byte
	/// late, which would otherwise put every byte into the alphabet.
	live: FxHashSet<StateID>,
}

impl Regex {
	/// The DFA of `pattern`, over bytes, without Unicode classes (`.` is any byte but `\n`).
	///
	/// # Errors
	///
	/// [`Build`](Error::Build) if regex-automata cannot build the DFA, for example for a syntax error.
	pub fn new(pattern: &str) -> Result<Regex, Error> {
		let dfa = dense::Builder::new()
			.configure(
				dense::Config::new()
					.start_kind(StartKind::Anchored)
					.match_kind(MatchKind::All),
			)
			.syntax(syntax::Config::new().unicode(false).utf8(false))
			.build(pattern)
			.map_err(Box::new)?;

		Regex::from_dfa(dfa)
	}

	/// A DFA built elsewhere. It must report all matches ([`All`](MatchKind::All)); otherwise words that a longer
	/// match passes through are lost.
	///
	/// # Errors
	///
	/// [`Start`](Error::Start) if the DFA does not support anchored searches.
	pub fn from_dfa(dfa: dense::DFA<Vec<u32>>) -> Result<Regex, Error> {
		let start = dfa.start_state(&start::Config::new().anchored(Anchored::Yes))?;
		let live = live_states(&dfa, start);

		Ok(Regex { dfa, start, live })
	}
}

/// The moves of `state` that do not lead to a dead or quit state.
fn targets(dfa: &Dfa, state: StateID) -> impl Iterator<Item = (u8, StateID)> {
	(u8::MIN..=u8::MAX)
		.map(move |byte| (byte, dfa.next_state(state, byte)))
		.filter(|&(_, target)| !dfa.is_dead_state(target) && !dfa.is_quit_state(target))
}

/// A DFA reports a match one byte late, so a state accepts if the end of input leads to a match.
fn accepts(dfa: &Dfa, state: StateID) -> bool {
	dfa.is_match_state(dfa.next_eoi_state(state))
}

/// The states reachable from `start` that reach an accepting state.
fn live_states(dfa: &Dfa, start: StateID) -> FxHashSet<StateID> {
	let mut predecessors: FxHashMap<StateID, Vec<StateID>> = FxHashMap::default();
	let mut reached = FxHashSet::from_iter([start]);
	let mut stack = vec![start];
	while let Some(state) = stack.pop() {
		for (_, target) in targets(dfa, state) {
			predecessors.entry(target).or_default().push(state);
			if reached.insert(target) {
				stack.push(target);
			}
		}
	}

	let mut live: FxHashSet<StateID> = reached
		.into_iter()
		.filter(|&state| accepts(dfa, state))
		.collect();
	let mut stack: Vec<StateID> = live.iter().copied().collect();
	while let Some(state) = stack.pop() {
		for &source in predecessors.get(&state).into_iter().flatten() {
			if live.insert(source) {
				stack.push(source);
			}
		}
	}

	live
}

impl Automaton for Regex {
	type State = StateID;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = StateID> {
		[self.start]
	}

	fn is_final(&self, state: StateID) -> bool {
		accepts(&self.dfa, state)
	}

	fn for_each_move(&self, state: StateID, mut visit: impl FnMut(&[Symbol], StateID)) {
		for (byte, target) in targets(&self.dfa, state) {
			if self.live.contains(&target) {
				visit(&[Symbol::from(byte)], target);
			}
		}
	}
}

impl FromStr for Regex {
	type Err = Error;

	fn from_str(pattern: &str) -> Result<Regex, Error> {
		Regex::new(pattern)
	}
}
