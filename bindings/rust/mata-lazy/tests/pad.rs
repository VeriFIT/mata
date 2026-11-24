//! Pad closures against brute force over random automata of two tapes of bits.

use mata_lazy::{Automaton, Formula, Pad, Symbol, Term};
use rand::rngs::StdRng;
use rand::{RngExt, SeedableRng};

const SEEDS: u64 = 200;
const BITS: [Symbol; 2] = [Symbol::new(0), Symbol::new(1)];

/// An automaton over `arity` tapes, with its moves listed.
#[derive(Clone)]
struct Moves {
	arity: usize,
	initial: Vec<u32>,
	finals: Vec<bool>,
	edges: Vec<(u32, Vec<Symbol>, u32)>,
}

impl Automaton for Moves {
	type State = u32;

	fn arity(&self) -> usize {
		self.arity
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u32> {
		self.initial.clone()
	}

	fn is_final(&self, state: u32) -> bool {
		self.finals[state as usize]
	}

	fn for_each_move(&self, state: u32, mut visit: impl FnMut(&[Symbol], u32)) {
		for (source, tuple, target) in &self.edges {
			if *source == state {
				visit(tuple, *target);
			}
		}
	}

	fn alphabet(&self, _tape: usize) -> impl IntoIterator<Item = Symbol> {
		BITS
	}
}

impl Moves {
	fn random(rng: &mut StdRng, arity: usize) -> Moves {
		let states = rng.random_range(1..=4_u32);
		let moves = (0..rng.random_range(0..=8))
			.map(|_| {
				let tuple = (0..arity)
					.map(|_| match rng.random_range(0..5) {
						0 => Symbol::ANY,
						bit => BITS[bit % 2],
					})
					.collect();

				(
					rng.random_range(0..states),
					tuple,
					rng.random_range(0..states),
				)
			})
			.collect();

		Moves {
			arity,
			initial: vec![0],
			finals: (0..states).map(|_| rng.random_bool(0.3)).collect(),
			edges: moves,
		}
	}

	/// The word, as steps.
	fn word(arity: usize, steps: &[Vec<Symbol>]) -> Moves {
		let len = u32::try_from(steps.len()).expect("a short word");
		Moves {
			arity,
			initial: vec![0],
			finals: (0..=len).map(|state| state == len).collect(),
			edges: (0..len)
				.map(|state| (state, steps[state as usize].clone(), state + 1))
				.collect(),
		}
	}

	fn accepts(&self, steps: &[Vec<Symbol>]) -> bool {
		let mut current: Vec<u32> = self.initial.clone();
		for step in steps {
			let mut next = Vec::new();
			for (source, tuple, target) in &self.edges {
				let reads = tuple.iter().zip(step).all(|(&read, &letter)| {
					read == letter || read == Symbol::ANY || letter == Symbol::ANY
				});
				if current.contains(source) && reads && !next.contains(target) {
					next.push(*target);
				}
			}

			current = next;
		}

		current.iter().any(|&state| self.finals[state as usize])
	}

	/// Whether some word of the other tapes joins `steps` of `tape` in a word of the automaton.
	fn accepts_on(&self, steps: &[Vec<Symbol>], tape: usize) -> bool {
		let mut current: Vec<u32> = self.initial.clone();
		for step in steps {
			let mut next = Vec::new();
			for (source, tuple, target) in &self.edges {
				let reads = tuple[tape] == step[0] || tuple[tape] == Symbol::ANY;
				if current.contains(source) && reads && !next.contains(target) {
					next.push(*target);
				}
			}

			current = next;
		}

		current.iter().any(|&state| self.finals[state as usize])
	}

	/// `steps·pad^k` for some `k`, which needs no more padding steps than there are subsets of states.
	fn accepts_padded(&self, steps: &[Vec<Symbol>], pad: &Pad) -> bool {
		let pad = match pad {
			Pad::Step(step) => Some(step.to_vec()),
			Pad::Last => steps.last().cloned(),
		};
		let mut padded = steps.to_vec();
		for _ in 0..=16 {
			if self.accepts(&padded) {
				return true;
			}

			let Some(step) = &pad else { return false };
			padded.push(step.clone());
		}

		false
	}
}

/// Every word over `arity` tapes of bits up to `len` steps.
fn words(arity: usize, len: usize) -> Vec<Vec<Vec<Symbol>>> {
	let columns: Vec<Vec<Symbol>> = (0..1_usize << arity)
		.map(|bits| (0..arity).map(|tape| BITS[(bits >> tape) & 1]).collect())
		.collect();
	let mut words = vec![Vec::new()];
	let mut last = vec![Vec::new()];
	for _ in 0..len {
		last = last
			.iter()
			.flat_map(|word: &Vec<Vec<Symbol>>| {
				columns.iter().map(move |column| {
					let mut word = word.clone();
					word.push(column.clone());

					word
				})
			})
			.collect();
		words.extend(last.iter().cloned());
	}

	words
}

fn holds(formula: &mut Formula, term: Term, arity: usize, word: &[Vec<Symbol>]) -> bool {
	let word = formula.leaf(&Moves::word(arity, word)).expect("a word");
	let both = formula.intersection(term, word).expect("same arity");

	!formula.is_empty(both).expect("no limits")
}

fn pads(rng: &mut StdRng, arity: usize) -> Pad {
	if rng.random_bool(0.5) {
		return Pad::Last;
	}

	Pad::Step(
		(0..arity)
			.map(|_| match rng.random_range(0..3) {
				0 => Symbol::ANY,
				bit => BITS[bit % 2],
			})
			.collect(),
	)
}

#[test]
fn closures_and_their_complements_hold_the_padded_words() {
	for seed in 0..SEEDS {
		let mut rng = StdRng::seed_from_u64(seed);
		let automaton = Moves::random(&mut rng, 2);
		let pad = pads(&mut rng, 2);
		let mut formula = Formula::new();
		let leaf = formula.leaf(&automaton).expect("a leaf");
		let closed = formula.pad_closure(leaf, pad.clone()).expect("two tapes");
		let open = formula.complement(closed).expect("a term");

		for word in words(2, 3) {
			let expected = automaton.accepts_padded(&word, &pad);
			assert_eq!(
				holds(&mut formula, closed, 2, &word),
				expected,
				"seed {seed}, {word:?}"
			);
			assert_eq!(
				holds(&mut formula, open, 2, &word),
				!expected,
				"seed {seed}, {word:?}"
			);
		}
	}
}

/// The case of logics over numbers: a variable dropped by a projection may need longer words than the kept one.
#[test]
fn a_closed_projection_holds_the_words_too_short_for_the_dropped_tape() {
	for seed in 0..SEEDS {
		let mut rng = StdRng::seed_from_u64(seed);
		let automaton = Moves::random(&mut rng, 2);
		let mut formula = Formula::new();
		let leaf = formula.leaf(&automaton).expect("a leaf");
		let kept = formula.project(leaf, &[0]).expect("tape 0");
		let closed = formula
			.pad_closure(kept, Pad::Step([BITS[0]].into()))
			.expect("one tape");

		for word in words(1, 4) {
			// Some word of the dropped tape, then zeros on the kept one.
			let expected = (0..=8).any(|zeros| {
				let mut padded = word.clone();
				padded.extend(std::iter::repeat_n(vec![BITS[0]], zeros));

				automaton.accepts_on(&padded, 0)
			});
			assert_eq!(
				holds(&mut formula, closed, 1, &word),
				expected,
				"seed {seed}, {word:?}"
			);
		}
	}
}

#[test]
fn a_step_of_the_wrong_width_is_rejected() {
	let mut formula = Formula::new();
	let leaf = formula
		.leaf(&Moves::word(2, &[vec![BITS[0], BITS[1]]]))
		.expect("a word");

	assert!(
		formula
			.pad_closure(leaf, Pad::Step([BITS[0]].into()))
			.is_err()
	);
}
