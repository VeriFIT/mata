//! Length-bounded and breadth-first searches, against an eager evaluator of random formulas over {a, b}.

use std::collections::VecDeque;

use mata_lazy::{Automaton, Error, Formula, Options, Symbol, Term};
use rand::rngs::StdRng;
use rand::{RngExt, SeedableRng};

const LETTERS: [u8; 2] = *b"ab";

/// An NFA over {a, b}, with letters as indices into [`LETTERS`].
#[derive(Clone, Debug)]
struct Nfa {
	initial: Vec<usize>,
	finals: Vec<bool>,
	/// The targets of every state, per letter.
	moves: Vec<[Vec<usize>; 2]>,
}

impl Automaton for Nfa {
	type State = usize;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = usize> {
		self.initial.clone()
	}

	fn is_final(&self, state: usize) -> bool {
		self.finals[state]
	}

	fn for_each_move(&self, state: usize, mut visit: impl FnMut(&[Symbol], usize)) {
		for (letter, targets) in LETTERS.iter().zip(&self.moves[state]) {
			for &target in targets {
				visit(&[Symbol::from(*letter)], target);
			}
		}
	}
}

impl Nfa {
	fn random(rng: &mut StdRng) -> Nfa {
		let states = rng.random_range(1..=4);
		let mut initial = vec![0];
		if states > 1 && rng.random_bool(0.2) {
			initial.push(rng.random_range(1..states));
		}

		Nfa {
			initial,
			finals: (0..states).map(|_| rng.random_bool(0.35)).collect(),
			moves: (0..states)
				.map(|_| {
					[(); 2].map(|()| {
						(0..rng.random_range(0..=2))
							.map(|_| rng.random_range(0..states))
							.collect()
					})
				})
				.collect(),
		}
	}

	fn union(&self, other: &Nfa) -> Nfa {
		let offset = self.finals.len();
		let shift = |states: &[usize]| states.iter().map(|&state| state + offset).collect();

		Nfa {
			initial: self
				.initial
				.iter()
				.copied()
				.chain(shift(&other.initial))
				.collect(),
			finals: self.finals.iter().chain(&other.finals).copied().collect(),
			moves: self
				.moves
				.iter()
				.cloned()
				.chain(
					other
						.moves
						.iter()
						.map(|moves| moves.clone().map(|targets| shift(&targets))),
				)
				.collect(),
		}
	}

	fn intersection(&self, other: &Nfa) -> Nfa {
		let width = other.finals.len();
		let pairs = |lhs: &[usize], rhs: &[usize]| -> Vec<usize> {
			lhs.iter()
				.flat_map(|&left| rhs.iter().map(move |&right| left * width + right))
				.collect()
		};

		Nfa {
			initial: pairs(&self.initial, &other.initial),
			finals: (0..self.finals.len() * width)
				.map(|pair| self.finals[pair / width] && other.finals[pair % width])
				.collect(),
			moves: (0..self.finals.len() * width)
				.map(|pair| {
					let (left, right) = (&self.moves[pair / width], &other.moves[pair % width]);

					[0, 1].map(|letter| pairs(&left[letter], &right[letter]))
				})
				.collect(),
		}
	}

	/// The complement over {a, b}, by the subset construction.
	fn complement(&self) -> Nfa {
		let start = normalized(self.initial.clone());
		let mut subsets = vec![start];
		let mut complement = Nfa {
			initial: vec![0],
			finals: Vec::new(),
			moves: Vec::new(),
		};
		let mut next = 0;
		while next < subsets.len() {
			let subset = subsets[next].clone();
			complement
				.finals
				.push(!subset.iter().any(|&state| self.finals[state]));
			let moves = [0, 1].map(|letter| {
				let successor = normalized(
					subset
						.iter()
						.flat_map(|&state| self.moves[state][letter].iter().copied())
						.collect(),
				);
				let index = subsets
					.iter()
					.position(|known| *known == successor)
					.unwrap_or_else(|| {
						subsets.push(successor);
						subsets.len() - 1
					});

				vec![index]
			});
			complement.moves.push(moves);
			next += 1;
		}

		complement
	}

	fn accepts(&self, word: &[usize]) -> bool {
		let mut states = normalized(self.initial.clone());
		for &letter in word {
			states = normalized(
				states
					.iter()
					.flat_map(|&state| self.moves[state][letter].iter().copied())
					.collect(),
			);
		}

		states.iter().any(|&state| self.finals[state])
	}

	/// The length of the shortest accepted word, by breadth-first search.
	fn shortest(&self) -> Option<usize> {
		let mut distances = vec![None; self.finals.len()];
		let mut queue = VecDeque::new();
		for &state in &self.initial {
			if distances[state].is_none() {
				distances[state] = Some(0);
				queue.push_back(state);
			}
		}

		while let Some(state) = queue.pop_front() {
			let distance = distances[state]?;
			if self.finals[state] {
				return Some(distance);
			}

			for &target in self.moves[state].iter().flatten() {
				if distances[target].is_none() {
					distances[target] = Some(distance + 1);
					queue.push_back(target);
				}
			}
		}

		None
	}
}

fn normalized(mut states: Vec<usize>) -> Vec<usize> {
	states.sort_unstable();
	states.dedup();

	states
}

fn leaf(formula: &mut Formula, nfa: &Nfa) -> Result<Term, Error> {
	formula.leaf_over(nfa, [LETTERS])
}

/// A random formula of the given depth, and its language computed eagerly.
fn generate(formula: &mut Formula, rng: &mut StdRng, depth: usize) -> Result<(Term, Nfa), Error> {
	if depth == 0 || rng.random_bool(0.2) {
		let nfa = Nfa::random(rng);

		return Ok((leaf(formula, &nfa)?, nfa));
	}

	Ok(match rng.random_range(0..6) {
		0 => {
			let ((a, ea), (b, eb)) = (
				generate(formula, rng, depth - 1)?,
				generate(formula, rng, depth - 1)?,
			);
			(formula.union(a, b)?, ea.union(&eb))
		}
		1 => {
			let ((a, ea), (b, eb)) = (
				generate(formula, rng, depth - 1)?,
				generate(formula, rng, depth - 1)?,
			);
			(formula.intersection(a, b)?, ea.intersection(&eb))
		}
		2 | 3 => {
			let (a, ea) = generate(formula, rng, depth - 1)?;
			(formula.complement(a)?, ea.complement())
		}
		_ => {
			let (a, ea) = generate(formula, rng, depth - 1)?;
			(formula.explicit(a)?, ea)
		}
	})
}

/// The search configurations that must all give the same answers.
fn configurations() -> [Options; 3] {
	[
		Options::new(),
		Options::new().antichain(false),
		Options::new().subsumption(false),
	]
}

/// The letters of the only tape of a witness.
fn letters(word: &[Symbol]) -> Vec<usize> {
	word.iter()
		.map(|&symbol| {
			LETTERS
				.iter()
				.position(|&letter| Symbol::from(letter) == symbol)
				.expect("a letter of {a, b}")
		})
		.collect()
}

/// An NFA with initial state 0 and the given final states and moves.
fn nfa(states: usize, finals: &[usize], moves: &[(usize, u8, usize)]) -> Nfa {
	let mut nfa = Nfa {
		initial: vec![0],
		finals: (0..states).map(|state| finals.contains(&state)).collect(),
		moves: vec![[Vec::new(), Vec::new()]; states],
	};
	for &(source, letter, target) in moves {
		nfa.moves[source][usize::from(letter - b'a')].push(target);
	}

	nfa
}

/// The length of the witness of every configuration, searched as `order` says within `max_length` steps.
fn witness_lengths(
	nfa: &Nfa,
	max_length: usize,
	breadth_first: bool,
) -> Result<Vec<Option<usize>>, Error> {
	let mut formula = Formula::new();
	let term = leaf(&mut formula, nfa)?;

	configurations()
		.into_iter()
		.map(|options| {
			let options = options.max_length(max_length).shortest(breadth_first);

			Ok(formula
				.check(term, &options)?
				.witness
				.map(|witness| witness.len()))
		})
		.collect()
}

#[test]
fn a_bounded_search_keeps_a_macrostate_below_one_reached_in_more_steps() -> Result<(), Error> {
	// `aa` reaches 1 then 5; depth first, the search first goes `bb` to 3, which simulates 1, at the bound.
	let nfa = nfa(
		6,
		&[5],
		&[
			(0, b'a', 1),
			(0, b'b', 2),
			(1, b'a', 5),
			(2, b'b', 3),
			(3, b'a', 5),
			(3, b'b', 5),
		],
	);
	assert_eq!(nfa.shortest(), Some(2));
	assert_eq!(witness_lengths(&nfa, 2, false)?, [Some(2); 3]);

	Ok(())
}

#[test]
fn a_breadth_first_search_keeps_a_macrostate_below_one_reached_in_more_steps() -> Result<(), Error>
{
	// `bb` reaches 1 then 5; breadth first, `aa` reaches 3, which simulates 1, before 1 is expanded.
	let nfa = nfa(
		6,
		&[5],
		&[
			(0, b'a', 2),
			(0, b'b', 1),
			(2, b'a', 3),
			(1, b'b', 5),
			(3, b'a', 5),
			(3, b'b', 5),
		],
	);
	assert_eq!(nfa.shortest(), Some(2));
	for max_length in [2, 5] {
		assert_eq!(witness_lengths(&nfa, max_length, true)?, [Some(2); 3]);
	}

	Ok(())
}

#[test]
fn a_bounded_search_expands_a_macrostate_again_when_reached_in_fewer_steps() -> Result<(), Error> {
	// Depth first, `bbb` reaches 3 before `aa` does; only from `aa` is the final state within 4 steps.
	let nfa = nfa(
		7,
		&[5],
		&[
			(0, b'a', 1),
			(0, b'b', 2),
			(2, b'b', 4),
			(4, b'b', 3),
			(1, b'a', 3),
			(3, b'a', 6),
			(6, b'a', 5),
		],
	);
	assert_eq!(nfa.shortest(), Some(4));
	assert_eq!(witness_lengths(&nfa, 4, false)?, [Some(4); 3]);

	Ok(())
}

#[test]
fn breadth_first_witnesses_have_the_fewest_steps() -> Result<(), Error> {
	for seed in 0..400 {
		let mut rng = StdRng::seed_from_u64(seed);
		let mut formula = Formula::new();
		let (term, eager) = generate(&mut formula, &mut rng, 3)?;
		let shortest = eager.shortest();
		for options in configurations() {
			let options = options.shortest(true);
			let outcome = formula.check(term, &options)?;
			let witness = outcome
				.witness
				.as_ref()
				.map(|witness| letters(&witness.tape(0).unwrap()));
			assert_eq!(
				witness.as_ref().map(Vec::len),
				shortest,
				"seed {seed}, {options:?}"
			);
			assert!(
				witness.is_none_or(|word| eager.accepts(&word)),
				"seed {seed}, {options:?}"
			);
			assert!(!outcome.bounded, "seed {seed}");
		}
	}

	Ok(())
}

#[test]
fn bounded_searches_answer_for_the_words_within_the_bound() -> Result<(), Error> {
	for seed in 0..250 {
		let mut rng = StdRng::seed_from_u64(seed);
		let mut formula = Formula::new();
		let (term, eager) = generate(&mut formula, &mut rng, 3)?;
		let shortest = eager.shortest();
		for max_length in 0..=5 {
			for breadth_first in [false, true] {
				for options in configurations() {
					let options = options.max_length(max_length).shortest(breadth_first);
					let outcome = formula.check(term, &options)?;
					let context = format!("seed {seed}, {options:?}, shortest word {shortest:?}");
					if let Some(witness) = &outcome.witness {
						let word = letters(&witness.tape(0).unwrap());
						assert!(eager.accepts(&word), "{context}");
						assert!(word.len() <= max_length, "{context}");
						if breadth_first {
							assert_eq!(Some(word.len()), shortest, "{context}");
						}
					} else {
						// No word within the bound, and emptiness claimed only for an empty relation.
						assert!(
							shortest.is_none_or(|length| length > max_length),
							"{context}"
						);
						assert!(!outcome.is_empty() || shortest.is_none(), "{context}");
					}
				}
			}
		}
	}

	Ok(())
}
