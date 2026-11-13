//! Subsumption of complement sets on SAT-shaped formulas against brute force, on large leaves, and on roots past
//! 2^16 macrostates.

use lazymat::{Automaton, Error, Formula, Options, Symbol, Term};
use rand::rngs::StdRng;
use rand::{RngExt, SeedableRng};

const FALSE: Symbol = Symbol::new(0);
const TRUE: Symbol = Symbol::new(1);
const VALUES: [Symbol; 2] = [FALSE, TRUE];

const VARIABLES: u32 = 16;
const SEEDS: u64 = 40;

/// A variable and the value that satisfies it.
#[derive(Clone, Copy, Debug)]
struct Literal {
	variable: u32,
	value: bool,
}

/// The words of `length` values, one value per variable.
struct Words {
	length: u32,
}

impl Automaton for Words {
	/// The position.
	type State = u32;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u32> {
		[0]
	}

	fn is_final(&self, position: u32) -> bool {
		position == self.length
	}

	fn for_each_move(&self, position: u32, mut visit: impl FnMut(&[Symbol], u32)) {
		if position < self.length {
			for value in VALUES {
				visit(&[value], position + 1);
			}
		}
	}
}

/// The assignments that falsify a clause (literals sorted by variable); with `bare`, also every word of another
/// length.
struct Falsifier<'c> {
	length: u32,
	clauses: &'c [Vec<Literal>],
	bare: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
enum Falsify {
	/// No clause chosen yet.
	Start { position: u32 },
	/// A clause chosen, its literals before `next` false.
	Clause {
		clause: usize,
		next: usize,
		position: u32,
	},
	/// A clause is false.
	Falsified { position: u32 },
	/// More than `length` symbols read.
	Longer,
}

impl Falsifier<'_> {
	/// The state at `position` after the literals of `clause` before `next` were false.
	fn after(&self, clause: usize, next: usize, position: u32) -> Falsify {
		if next == self.clauses[clause].len() {
			Falsify::Falsified { position }
		} else {
			Falsify::Clause {
				clause,
				next,
				position,
			}
		}
	}
}

impl Automaton for Falsifier<'_> {
	type State = Falsify;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = Falsify> {
		[Falsify::Start { position: 0 }]
	}

	fn is_final(&self, state: Falsify) -> bool {
		match state {
			Falsify::Falsified { position } => position == self.length,
			Falsify::Start { position } => self.bare && position < self.length,
			Falsify::Longer => true,
			Falsify::Clause { .. } => false,
		}
	}

	fn for_each_move(&self, state: Falsify, mut visit: impl FnMut(&[Symbol], Falsify)) {
		match state {
			Falsify::Start { position } if position < self.length => {
				for value in VALUES {
					visit(
						&[value],
						Falsify::Start {
							position: position + 1,
						},
					);
				}

				for (clause, literals) in self.clauses.iter().enumerate() {
					if literals[0].variable == position {
						visit(
							&[false_value(literals[0])],
							self.after(clause, 1, position + 1),
						);
					}
				}
			}
			Falsify::Clause {
				clause,
				next,
				position,
			} => {
				let literal = self.clauses[clause][next];
				if literal.variable == position {
					visit(
						&[false_value(literal)],
						self.after(clause, next + 1, position + 1),
					);
				} else {
					for value in VALUES {
						visit(
							&[value],
							Falsify::Clause {
								clause,
								next,
								position: position + 1,
							},
						);
					}
				}
			}
			Falsify::Falsified { position } if position < self.length => {
				for value in VALUES {
					visit(
						&[value],
						Falsify::Falsified {
							position: position + 1,
						},
					);
				}
			}
			Falsify::Start { .. } | Falsify::Longer if self.bare => {
				for value in VALUES {
					visit(&[value], Falsify::Longer);
				}
			}
			_ => {}
		}
	}
}

fn false_value(literal: Literal) -> Symbol {
	if literal.value { FALSE } else { TRUE }
}

/// A random 3-CNF over [`VARIABLES`] variables, around the ratio where half of them are satisfiable.
fn random_cnf(rng: &mut StdRng) -> Vec<Vec<Literal>> {
	let num_clauses = rng.random_range(50..=85);

	(0..num_clauses)
		.map(|_| {
			let mut variables: Vec<u32> = Vec::new();
			while variables.len() < 3 {
				let variable = rng.random_range(0..VARIABLES);
				if !variables.contains(&variable) {
					variables.push(variable);
				}
			}

			variables.sort_unstable();

			variables
				.into_iter()
				.map(|variable| Literal {
					variable,
					value: rng.random_bool(0.5),
				})
				.collect()
		})
		.collect()
}

fn satisfies(clauses: &[Vec<Literal>], assignment: &[bool]) -> bool {
	clauses.iter().all(|literals| {
		literals
			.iter()
			.any(|literal| assignment[literal.variable as usize] == literal.value)
	})
}

fn satisfiable(clauses: &[Vec<Literal>]) -> bool {
	(0_u32..1 << VARIABLES).any(|bits| {
		let assignment: Vec<bool> = (0..VARIABLES)
			.map(|variable| bits >> variable & 1 == 1)
			.collect();

		satisfies(clauses, &assignment)
	})
}

/// Checks over random 3-CNFs that the term `build` makes of each holds exactly its satisfying assignments.
fn assert_satisfying_assignments(
	options: &[Options],
	mut build: impl FnMut(&mut Formula, &[Vec<Literal>]) -> Result<Term, Error>,
) -> Result<(), Error> {
	let mut formula = Formula::new();
	let mut satisfiable_count = 0;
	for seed in 0..SEEDS {
		let clauses = random_cnf(&mut StdRng::seed_from_u64(seed));
		let expected = satisfiable(&clauses);
		satisfiable_count += u64::from(expected);
		let root = build(&mut formula, &clauses)?;

		for options in options {
			let outcome = formula.check(root, options)?;
			assert_eq!(
				outcome.witness.is_some(),
				expected,
				"seed {seed}, {options:?}"
			);
			if let Some(witness) = outcome.witness {
				let word = witness.tape(0).expect("one tape");
				let assignment: Vec<bool> = word.iter().map(|&symbol| symbol == TRUE).collect();
				assert_eq!(assignment.len(), VARIABLES as usize, "seed {seed}");
				assert!(satisfies(&clauses, &assignment), "seed {seed}");
			}
		}
	}

	assert!(satisfiable_count > 0 && satisfiable_count < SEEDS);

	Ok(())
}

/// A complement over a leaf under a product, whose sets compare through the leaf simulation.
#[test]
fn complements_of_falsifiers_hold_exactly_the_satisfying_assignments() -> Result<(), Error> {
	let options = [Options::new(), Options::new().subsumption(false)];

	assert_satisfying_assignments(&options, |formula, clauses| {
		let words = formula.leaf(&Words { length: VARIABLES })?;
		let falsifier = formula.leaf(&Falsifier {
			length: VARIABLES,
			clauses,
			bare: false,
		})?;
		let not_falsified = formula.complement(falsifier)?;

		formula.intersection(words, not_falsified)
	})
}

/// A complement over a leaf as the root, whose sets compare through closures in the antichain too.
#[test]
fn bare_complements_of_falsifiers_hold_exactly_the_satisfying_assignments() -> Result<(), Error> {
	let options = [
		Options::new(),
		Options::new().antichain(false),
		Options::new().subsumption(false),
	];

	assert_satisfying_assignments(&options, |formula, clauses| {
		let falsifier = formula.leaf(&Falsifier {
			length: VARIABLES,
			clauses,
			bare: true,
		})?;

		formula.complement(falsifier)
	})
}

/// The words with two `FALSE`s in a row.
struct TwoFalse;

impl Automaton for TwoFalse {
	/// How many `FALSE`s in a row were read, up to 2.
	type State = u8;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u8> {
		[0]
	}

	fn is_final(&self, state: u8) -> bool {
		state == 2
	}

	fn for_each_move(&self, state: u8, mut visit: impl FnMut(&[Symbol], u8)) {
		match state {
			0 => {
				visit(&[FALSE], 0);
				visit(&[TRUE], 0);
				visit(&[FALSE], 1);
			}
			1 => visit(&[FALSE], 2),
			_ => {
				visit(&[FALSE], 2);
				visit(&[TRUE], 2);
			}
		}
	}
}

/// The memo of a node changes layout at 65,536 macrostates; the search compares macrostates on both sides.
#[test]
fn a_root_past_65536_macrostates_keeps_its_answer() -> Result<(), Error> {
	const LENGTH: u32 = 40_000;

	let mut formula = Formula::new();
	let words = formula.leaf(&Words { length: LENGTH })?;
	let two_false = formula.leaf(&TwoFalse)?;
	let no_two_false = formula.complement(two_false)?;
	let root = formula.intersection(words, no_two_false)?;

	for subsumption in [true, false] {
		let outcome = formula.check(root, &Options::new().subsumption(subsumption))?;
		assert!(outcome.stats.macrostates > 1 << 16);
		let word = outcome
			.witness
			.expect("a word without two FALSEs in a row")
			.tape(0)
			.expect("one tape");
		assert_eq!(word.len(), LENGTH as usize);
		assert!(word.windows(2).all(|pair| pair != [FALSE, FALSE]));
	}

	Ok(())
}

/// The words with `TRUE` `n`-th from the end, and `FALSE^padding` through a chain of `padding` more states.
struct NthFromEnd {
	n: u32,
	padding: u32,
}

impl Automaton for NthFromEnd {
	/// States `0..=n` count the symbols read since the guessed `TRUE`, and those above `n` chain into the final state
	/// `n`.
	type State = u32;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u32> {
		let mut initial = vec![0];
		if self.padding > 0 {
			initial.push(self.n + 1);
		}

		initial
	}

	fn is_final(&self, state: u32) -> bool {
		state == self.n
	}

	fn for_each_move(&self, state: u32, mut visit: impl FnMut(&[Symbol], u32)) {
		match state {
			0 => {
				visit(&[FALSE], 0);
				visit(&[TRUE], 0);
				visit(&[TRUE], 1);
			}
			state if state < self.n => {
				visit(&[FALSE], state + 1);
				visit(&[TRUE], state + 1);
			}
			state if state == self.n => {}
			state if state == self.n + self.padding => visit(&[FALSE], self.n),
			state => visit(&[FALSE], state + 1),
		}
	}
}

/// The words of `length` symbols whose symbol at `position` is `symbol`.
struct Fixed {
	length: u32,
	position: u32,
	symbol: Symbol,
}

impl Automaton for Fixed {
	/// The position.
	type State = u32;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u32> {
		[0]
	}

	fn is_final(&self, position: u32) -> bool {
		position == self.length
	}

	fn for_each_move(&self, position: u32, mut visit: impl FnMut(&[Symbol], u32)) {
		if position == self.position {
			visit(&[self.symbol], position + 1);
		} else if position < self.length {
			for value in VALUES {
				visit(&[value], position + 1);
			}
		}
	}
}

/// Over a leaf of more than 32768 states the simulation is the identity, and complement sets compare as subsets.
#[test]
fn complements_over_leaves_without_a_simulation_keep_their_answers() -> Result<(), Error> {
	const LENGTH: u32 = 40;
	const N: u32 = 10;

	for padding in [0, 33_000] {
		let mut formula = Formula::new();
		let nth = formula.leaf(&NthFromEnd { n: N, padding })?;
		let not_nth = formula.complement(nth)?;
		for (symbol, empty) in [(TRUE, true), (FALSE, false)] {
			let fixed = formula.leaf(&Fixed {
				length: LENGTH,
				position: LENGTH - N,
				symbol,
			})?;
			let root = formula.intersection(fixed, not_nth)?;

			let mut explored = Vec::new();
			for subsumption in [true, false] {
				let outcome = formula.check(root, &Options::new().subsumption(subsumption))?;
				assert_eq!(
					outcome.is_empty(),
					empty,
					"padding {padding}, subsumption {subsumption}"
				);
				explored.push(outcome.stats.explored);
				if let Some(witness) = outcome.witness {
					let word = witness.tape(0).expect("one tape");
					assert_eq!(word.len(), LENGTH as usize);
					assert_eq!(word[(LENGTH - N) as usize], FALSE);
				}
			}

			// Subsets prune most of the exploration of the empty root.
			if empty {
				assert!(
					explored[0] < explored[1],
					"padding {padding}: explored {explored:?}"
				);
			}
		}
	}

	Ok(())
}
