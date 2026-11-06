//! Property tests: every operation is checked against a direct simulation of a small random automaton,
//! on all words of length at most `MAX_LEN` over the alphabet `0..NUM_SYMBOLS`.

use std::collections::BTreeSet;

use mata::{
	Alphabet, CheckAlgorithm, Complementation, Determinization, Direction, LimitExceeded, Limits,
	Minimization, Nfa, Reduction, ResidualConstruction, Side, State, Symbol, Transition, Word,
};
use proptest::prelude::*;
use proptest::test_runner::FileFailurePersistence;

const NUM_SYMBOLS: u32 = 2;
const MAX_STATES: usize = 5;
const MAX_LEN: usize = 5;

/// A small automaton in plain Rust: the oracle that the Mata results are compared against.
#[derive(Clone, Debug)]
struct Model {
	num_states: usize,
	/// `(source, symbol, target)`; `None` is epsilon.
	transitions: Vec<(usize, Option<u32>, usize)>,
	initial: Vec<usize>,
	accepting: Vec<usize>,
}

impl Model {
	fn epsilon_closure(&self, states: &mut BTreeSet<usize>) {
		let mut stack: Vec<usize> = states.iter().copied().collect();
		while let Some(state) = stack.pop() {
			for &(source, symbol, target) in &self.transitions {
				if source == state && symbol.is_none() && states.insert(target) {
					stack.push(target);
				}
			}
		}
	}

	fn accepts(&self, word: &[u32]) -> bool {
		let mut current: BTreeSet<usize> = self.initial.iter().copied().collect();
		self.epsilon_closure(&mut current);
		for &letter in word {
			let mut next: BTreeSet<usize> = self
				.transitions
				.iter()
				.filter(|&&(source, symbol, _)| current.contains(&source) && symbol == Some(letter))
				.map(|&(_, _, target)| target)
				.collect();
			self.epsilon_closure(&mut next);
			current = next;
		}
		self.accepting.iter().any(|state| current.contains(state))
	}

	fn to_nfa(&self) -> Nfa {
		let mut nfa = Nfa::with_states(self.num_states);
		for &index in &self.initial {
			nfa.add_initial(state(index)).unwrap();
		}
		for &index in &self.accepting {
			nfa.add_final(state(index)).unwrap();
		}
		nfa.add_transitions(self.transitions.iter().map(|&(source, symbol, target)| {
			let symbol = symbol.map_or(Symbol::EPSILON, Symbol::new);
			Transition::new(state(source), symbol, state(target))
		}))
		.unwrap();
		nfa
	}
}

fn model(with_epsilon: bool) -> impl Strategy<Value = Model> {
	(1..=MAX_STATES).prop_flat_map(move |n| {
		let symbol = if with_epsilon {
			prop_oneof![4 => (0..NUM_SYMBOLS).prop_map(Some), 1 => Just(None)].boxed()
		} else {
			(0..NUM_SYMBOLS).prop_map(Some).boxed()
		};
		let states: Vec<usize> = (0..n).collect();
		(
			Just(n),
			proptest::collection::vec((0..n, symbol, 0..n), 0..=3 * n),
			proptest::sample::subsequence(states.clone(), 0..=n),
			proptest::sample::subsequence(states, 0..=n),
		)
			.prop_map(|(num_states, transitions, initial, accepting)| Model {
				num_states,
				transitions,
				initial,
				accepting,
			})
	})
}

/// Every word over `0..NUM_SYMBOLS` of length at most `MAX_LEN`.
fn all_words() -> Vec<Vec<u32>> {
	let mut words = vec![vec![]];
	let mut layer = vec![vec![]];
	for _ in 0..MAX_LEN {
		layer = layer
			.iter()
			.flat_map(|word: &Vec<u32>| {
				(0..NUM_SYMBOLS).map(move |letter| {
					let mut longer = word.clone();
					longer.push(letter);
					longer
				})
			})
			.collect();
		words.extend(layer.iter().cloned());
	}
	words
}

fn word(raw: &[u32]) -> Word {
	raw.iter().copied().map(Symbol::new).collect()
}

fn raw(word: &[Symbol]) -> Vec<u32> {
	word.iter().map(|symbol| symbol.value()).collect()
}

fn sigma() -> Alphabet {
	Alphabet::from_symbols((0..NUM_SYMBOLS).map(Symbol::new)).unwrap()
}

/// The state with index `index` of a model.
fn state(index: usize) -> State {
	State::try_from(index).unwrap()
}

/// Asserts that `nfa` accepts exactly the words (up to `MAX_LEN`) for which `expected` holds.
fn assert_language(nfa: &Nfa, expected: impl Fn(&[u32]) -> bool, what: &str) {
	for w in all_words() {
		assert_eq!(
			nfa.accepts(&word(&w)),
			expected(&w),
			"{what} on {w:?}\n{nfa:?}"
		);
	}
}

/// 256 cases, or `PROPTEST_CASES` when set.
fn config() -> ProptestConfig {
	let cases = std::env::var("PROPTEST_CASES")
		.ok()
		.and_then(|cases| cases.parse().ok())
		.unwrap_or(256);
	ProptestConfig {
		// Next to this file, as `<file>.proptest-regressions` (the default looks for `src/lib.rs`).
		failure_persistence: Some(Box::new(FileFailurePersistence::WithSource(
			"proptest-regressions",
		))),
		..ProptestConfig::with_cases(cases)
	}
}

proptest! {
	#![proptest_config(config())]

	#[test]
	fn construction_and_membership(m in model(true)) {
		let nfa = m.to_nfa();
		prop_assert_eq!(nfa.num_states(), m.num_states);
		assert_language(&nfa, |w| m.accepts(w), "membership");

		let expected: BTreeSet<Transition> = m
			.transitions
			.iter()
			.map(|&(source, symbol, target)| {
				Transition::new(state(source), symbol.map_or(Symbol::EPSILON, Symbol::new), state(target))
			})
			.collect();
		let listed: Vec<Transition> = nfa.transitions().collect();
		prop_assert!(listed.windows(2).all(|pair| pair[0] < pair[1]), "transitions are sorted");
		prop_assert_eq!(listed.into_iter().collect::<BTreeSet<_>>(), expected);
		prop_assert_eq!(nfa.num_transitions(), nfa.transitions().count());
		for state in nfa.states() {
			let from: Vec<Transition> = nfa.transitions_from(state).collect();
			prop_assert!(from.iter().all(|t| t.source == state));
			for t in &from {
				prop_assert!(nfa.successors(state, t.symbol).contains(&t.target));
				prop_assert!(nfa.contains_transition(t.source, t.symbol, t.target));
			}
		}
	}

	#[test]
	fn union_intersection_concat(a in model(true), b in model(true)) {
		let (x, y) = (a.to_nfa(), b.to_nfa());
		assert_language(&x.union(&y), |w| a.accepts(w) || b.accepts(w), "union");
		assert_language(&(&x | &y), |w| a.accepts(w) || b.accepts(w), "|");
		assert_language(&x.intersection(&y), |w| a.accepts(w) && b.accepts(w), "intersection");
		assert_language(&(&x & &y), |w| a.accepts(w) && b.accepts(w), "&");
		assert_language(
			&x.concat(&y),
			|w| (0..=w.len()).any(|i| a.accepts(&w[..i]) && b.accepts(&w[i..])),
			"concat",
		);
		let mut z = x.clone();
		z |= &y;
		assert_language(&z, |w| a.accepts(w) || b.accepts(w), "|=");
	}

	#[test]
	fn difference_and_complement(a in model(true), b in model(true)) {
		let (x, y) = (a.to_nfa(), b.to_nfa());
		assert_language(&x.difference(&y), |w| a.accepts(w) && !b.accepts(w), "difference");
		for algorithm in [Complementation::Classical, Complementation::Brzozowski] {
			let complement = x.complement_with(&sigma(), algorithm);
			prop_assert!(complement.is_deterministic());
			prop_assert!(complement.is_complete(&sigma()));
			assert_language(&complement, |w| !a.accepts(w), "complement");
		}
		prop_assert!(x.complement(&sigma()).complement(&sigma()).is_equivalent_to(&x));
	}

	#[test]
	fn determinize_minimize_reduce(m in model(true)) {
		let nfa = m.to_nfa();
		let dfa = nfa.determinize();
		prop_assert!(dfa.is_deterministic());
		assert_language(&dfa, |w| m.accepts(w), "determinize");
		// The same DFA up to the numbering of its states.
		let classical = nfa.determinize_with(Determinization::Classical);
		prop_assert!(classical.is_deterministic());
		prop_assert_eq!(classical.num_states(), dfa.num_states());
		prop_assert_eq!(classical.num_transitions(), dfa.num_transitions());
		assert_language(&classical, |w| m.accepts(w), "determinize classical");

		let brzozowski = nfa.minimize_with(Minimization::Brzozowski);
		let hopcroft = nfa.minimize_with(Minimization::Hopcroft);
		for minimal in [&brzozowski, &hopcroft] {
			prop_assert!(minimal.is_deterministic());
			prop_assert!(minimal.is_minimal());
			assert_language(minimal, |w| m.accepts(w), "minimize");
			prop_assert_eq!(minimal.minimize().num_states(), minimal.num_states());
		}
		if dfa.num_states() > hopcroft.num_states() {
			prop_assert!(!dfa.is_minimal());
		}
		prop_assert_eq!(brzozowski.num_states(), hopcroft.num_states());
		let mut trimmed_dfa = dfa.clone();
		trimmed_dfa.trim();
		// The minimal DFA of the empty language is one state; trimming leaves none.
		prop_assert!(hopcroft.num_states() <= trimmed_dfa.num_states().max(1));

		let reductions = [
			Reduction::Simulation,
			Reduction::Residual { construction: ResidualConstruction::After, direction: Direction::Forward },
			Reduction::Residual { construction: ResidualConstruction::After, direction: Direction::Backward },
			Reduction::Residual { construction: ResidualConstruction::During, direction: Direction::Forward },
			Reduction::Residual { construction: ResidualConstruction::During, direction: Direction::Backward },
		];
		for reduction in reductions {
			let reduced = nfa.reduce_with(reduction);
			assert_language(&reduced, |w| m.accepts(w), &format!("{reduction:?}"));
		}
		prop_assert!(nfa.reduce().num_states() <= nfa.num_states());
	}

	#[test]
	fn limits(a in model(true), b in model(true)) {
		let (x, y) = (a.to_nfa(), b.to_nfa());
		let at = |max_states| Limits::new().max_states(max_states);
		let exceeded = |max_states| Some(LimitExceeded::States { max_states });

		// Without a limit, the bounded operations build what the plain ones build. The DFA and the
		// difference fit exactly their own size.
		let dfa = x.determinize();
		prop_assert!(x.determinize_within(&Limits::new())?.is_identical(&dfa));
		prop_assert!(x.determinize_within(&at(dfa.num_states()))?.is_identical(&dfa));
		prop_assert_eq!(x.determinize_within(&at(dfa.num_states() - 1)).err(), exceeded(dfa.num_states() - 1));

		let difference = x.difference(&y);
		prop_assert!(x.difference_within(&y, &Limits::new())?.is_identical(&difference));
		prop_assert!(x.difference_within(&y, &at(difference.num_states()))?.is_identical(&difference));
		prop_assert_eq!(
			x.difference_within(&y, &at(difference.num_states() - 1)).err(),
			exceeded(difference.num_states() - 1)
		);

		for algorithm in [Minimization::Brzozowski, Minimization::Hopcroft] {
			let minimal = x.minimize_with(algorithm);
			prop_assert!(x.minimize_within(algorithm, &Limits::new())?.is_identical(&minimal));
			prop_assert!(x.minimize_within(algorithm, &at(minimal.num_states() - 1)).is_err());
		}
		// Hopcroft's algorithm determinizes at most once.
		let hopcroft = x.minimize_with(Minimization::Hopcroft);
		let enough = at(dfa.num_states().max(hopcroft.num_states()));
		prop_assert!(x.minimize_within(Minimization::Hopcroft, &enough)?.is_identical(&hopcroft));

		for algorithm in [Complementation::Classical, Complementation::Brzozowski] {
			let complement = x.complement_with(&sigma(), algorithm);
			prop_assert!(x.complement_within(&sigma(), algorithm, &Limits::new())?.is_identical(&complement));
			prop_assert!(x.complement_within(&sigma(), algorithm, &at(complement.num_states() - 1)).is_err());
		}
		// So does the classical complementation, over the symbols that the automaton uses.
		let classical = x.complement_with(&sigma(), Complementation::Classical);
		let enough = at(dfa.num_states().max(classical.num_states()));
		prop_assert!(
			x.complement_within(&sigma(), Complementation::Classical, &enough)?.is_identical(&classical)
		);
	}

	#[test]
	fn reverse_trim_remove_epsilon(m in model(true)) {
		let nfa = m.to_nfa();
		assert_language(&nfa.reverse(), |w| m.accepts(&w.iter().rev().copied().collect::<Vec<_>>()), "reverse");
		prop_assert!(nfa.reverse().reverse().is_equivalent_to(&nfa));

		let mut trimmed = nfa.clone();
		trimmed.trim();
		assert_language(&trimmed, |w| m.accepts(w), "trim");
		prop_assert_eq!(trimmed.useful_states().len(), trimmed.num_states());

		let mut without_epsilon = nfa.clone();
		without_epsilon.remove_epsilon();
		prop_assert!(!without_epsilon.has_epsilon_transitions());
		assert_language(&without_epsilon, |w| m.accepts(w), "remove_epsilon");

		let mut complete = nfa.clone();
		let before = complete.num_states();
		let sink = complete.make_complete(&sigma());
		prop_assert!(complete.is_complete(&sigma()));
		prop_assert_eq!(sink.is_some(), complete.num_states() == before + 1);
		assert_language(&complete, |w| m.accepts(w), "make_complete");
	}

	#[test]
	fn emptiness_and_witnesses(m in model(true)) {
		let nfa = m.to_nfa();
		let accepted = all_words().into_iter().find(|w| m.accepts(w));
		prop_assert_eq!(nfa.is_lang_empty(), nfa.find_accepted_word().is_none());
		if accepted.is_some() {
			prop_assert!(!nfa.is_lang_empty());
		}
		if let Some(w) = nfa.find_accepted_word() {
			prop_assert!(m.accepts(&raw(&w)));
		}
		if let Some(w) = nfa.find_shortest_word() {
			prop_assert!(m.accepts(&raw(&w)));
			if let Some(shortest) = &accepted {
				prop_assert_eq!(w.len(), shortest.len());
			}
		}
		if let Some(run) = nfa.find_accepting_run() {
			prop_assert_eq!(run.path.len(), run.word.len() + 1);
			prop_assert!(nfa.is_initial(run.path[0]));
			prop_assert!(nfa.is_final(*run.path.last().unwrap()));
			for (i, &symbol) in run.word.iter().enumerate() {
				prop_assert!(nfa.contains_transition(run.path[i], symbol, run.path[i + 1]));
			}
		}

		let mut words: Vec<Word> = nfa.words_up_to(3).collect();
		words.sort();
		let mut expected: Vec<Word> =
			all_words().into_iter().filter(|w| w.len() <= 3 && m.accepts(w)).map(|w| word(&w)).collect();
		expected.sort();
		prop_assert_eq!(words, expected);
	}

	#[test]
	fn universality(m in model(true)) {
		let nfa = m.to_nfa();
		let naive = nfa.find_rejected_word_with(&sigma(), CheckAlgorithm::Naive);
		let antichains = nfa.find_rejected_word_with(&sigma(), CheckAlgorithm::Antichains);
		prop_assert_eq!(naive.is_none(), antichains.is_none());
		match antichains {
			Some(w) => prop_assert!(!m.accepts(&raw(&w))),
			None => prop_assert!(all_words().iter().all(|w| m.accepts(w))),
		}
		prop_assert_eq!(nfa.is_universal(&sigma()), nfa.complement(&sigma()).is_lang_empty());
	}

	#[test]
	fn inclusion_and_equivalence(a in model(true), b in model(true)) {
		let (x, y) = (a.to_nfa(), b.to_nfa());
		let naive = x.find_word_not_in_with(&y, CheckAlgorithm::Naive);
		let antichains = x.find_word_not_in_with(&y, CheckAlgorithm::Antichains);
		prop_assert_eq!(naive.is_none(), antichains.is_none());
		for witness in [naive, antichains] {
			match witness {
				Some(w) => prop_assert!(a.accepts(&raw(&w)) && !b.accepts(&raw(&w)), "witness {w:?}"),
				None => prop_assert!(all_words().iter().all(|w| !a.accepts(w) || b.accepts(w))),
			}
		}
		prop_assert!(x.is_included_in(&x.union(&y)));
		prop_assert!(x.intersection(&y).is_included_in(&x));

		match x.find_common_word(&y) {
			Some(w) => prop_assert!(a.accepts(&raw(&w)) && b.accepts(&raw(&w)), "common word {w:?}"),
			None => prop_assert!(all_words().iter().all(|w| !a.accepts(w) || !b.accepts(w))),
		}
		prop_assert_eq!(x.is_disjoint_from(&y), x.intersection(&y).is_lang_empty());

		match x.find_distinguishing_word(&y) {
			Some(distinction) => {
				let w = raw(&distinction.word);
				let left = distinction.accepted_by == Side::Left;
				prop_assert!(a.accepts(&w) == left && b.accepts(&w) != left);
			}
			None => prop_assert!(all_words().iter().all(|w| a.accepts(w) == b.accepts(w))),
		}
		prop_assert!(x.is_equivalent_to(&x.determinize()));
		prop_assert!(x.is_equivalent_to(&x.reduce()));
	}

	#[test]
	fn text_round_trips(m in model(true)) {
		let nfa = m.to_nfa();
		let parsed: Nfa = nfa.to_string().parse()?;
		prop_assert!(parsed.is_equivalent_to(&nfa), "{}", nfa);

		let mut names = Alphabet::from_names(["zero", "one"])?;
		let text = nfa.display_with(&names).to_string();
		prop_assert!(!text.contains(" 0 ") && !text.contains(" 1 "));
		let reparsed = Nfa::parse_mata(&text, &mut names)?;
		prop_assert_eq!(names.len(), 2, "no new names");
		prop_assert!(reparsed.is_equivalent_to(&nfa), "{}", text);

		let dot = nfa.dot().to_string();
		prop_assert!(dot.starts_with("digraph"));
	}

	#[test]
	fn clone_is_deep(m in model(false)) {
		let nfa = m.to_nfa();
		let mut copy = nfa.clone();
		prop_assert!(copy.is_identical(&nfa));
		let extra = copy.add_state();
		copy.add_initial(extra)?;
		copy.add_final(extra)?;
		prop_assert_eq!(nfa.num_states(), m.num_states);
		prop_assert!(copy.accepts(&[]));
		prop_assert_eq!(nfa.accepts(&[]), m.accepts(&[]));
	}
}
