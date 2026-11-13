//! What the antichain of the search prunes and costs, and that turning it off keeps every answer.

use std::time::Duration;

use lazymat::{Automaton, Error, Formula, Options, Outcome, Symbol, Term};
use rand::rngs::StdRng;
use rand::{RngExt, SeedableRng};

const SEEDS: u64 = 300;

/// A one-tape automaton given by its moves, with initial state 0.
struct Moves {
	finals: &'static [u32],
	moves: &'static [(u32, u8, u32)],
}

impl Automaton for Moves {
	type State = u32;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u32> {
		[0]
	}

	fn is_final(&self, state: u32) -> bool {
		self.finals.contains(&state)
	}

	fn for_each_move(&self, state: u32, mut visit: impl FnMut(&[Symbol], u32)) {
		for &(source, letter, target) in self.moves {
			if source == state {
				visit(&[Symbol::from(letter)], target);
			}
		}
	}
}

/// {aa, ab, ba}, where the state after `b` accepts less than the one after `a`.
const TWO_LETTERS: Moves = Moves {
	finals: &[3],
	moves: &[
		(0, b'a', 1),
		(0, b'b', 2),
		(1, b'a', 3),
		(1, b'b', 3),
		(2, b'a', 3),
	],
};

/// The words of length 3 over {a, b}, through different but equivalent states after `a` and after `b`.
const THREE_LETTERS: Moves = Moves {
	finals: &[4],
	moves: &[
		(0, b'a', 1),
		(0, b'b', 2),
		(1, b'a', 3),
		(1, b'b', 3),
		(2, b'a', 3),
		(2, b'b', 3),
		(3, b'a', 4),
		(3, b'b', 4),
	],
};

/// A variable and the value that satisfies it.
type Literal = (u32, bool);

/// The assignments of `variables` variables, one symbol 0 or 1 each, that satisfy `literals`.
struct Clause<'c> {
	variables: u32,
	literals: &'c [Literal],
}

impl Automaton for Clause<'_> {
	/// The variables read, and whether a literal holds.
	type State = (u32, bool);

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = (u32, bool)> {
		[(0, false)]
	}

	fn is_final(&self, (read, satisfied): (u32, bool)) -> bool {
		read == self.variables && satisfied
	}

	fn for_each_move(
		&self,
		(read, satisfied): (u32, bool),
		mut visit: impl FnMut(&[Symbol], (u32, bool)),
	) {
		if read == self.variables {
			return;
		}

		for value in [false, true] {
			let holds = self.literals.contains(&(read, value));
			visit(
				&[Symbol::from(u8::from(value))],
				(read + 1, satisfied || holds),
			);
		}
	}
}

/// The words of `variables` bits whose 1s at the positions `residue` modulo `modulus` are odd in number iff `odd`.
struct Parity {
	variables: u32,
	modulus: u32,
	residue: u32,
	odd: bool,
}

impl Automaton for Parity {
	/// The symbols read, and whether an odd number of the counted ones are 1.
	type State = (u32, bool);

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = (u32, bool)> {
		[(0, false)]
	}

	fn is_final(&self, (read, odd): (u32, bool)) -> bool {
		read == self.variables && odd == self.odd
	}

	fn for_each_move(
		&self,
		(read, odd): (u32, bool),
		mut visit: impl FnMut(&[Symbol], (u32, bool)),
	) {
		if read == self.variables {
			return;
		}

		let counted = read % self.modulus == self.residue;
		for value in [false, true] {
			visit(
				&[Symbol::from(u8::from(value))],
				(read + 1, odd != (counted && value)),
			);
		}
	}
}

/// States `0..states`, with `a` to the next, `b` to the double and `c` to the final `states`. Each simulates every
/// other.
struct Dense {
	states: u32,
}

impl Automaton for Dense {
	type State = u32;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u32> {
		[0]
	}

	fn is_final(&self, state: u32) -> bool {
		state == self.states
	}

	fn for_each_move(&self, state: u32, mut visit: impl FnMut(&[Symbol], u32)) {
		if state == self.states {
			return;
		}

		visit(&[Symbol::from(b'c')], self.states);
		visit(&[Symbol::from(b'a')], (state + 1) % self.states);
		visit(&[Symbol::from(b'b')], state * 2 % self.states);
	}
}

/// {c}.
const ONE_C: Moves = Moves {
	finals: &[1],
	moves: &[(0, b'c', 1)],
};

/// The intersection of `terms` as a balanced tree, so that the leftmost leaf is several products deep.
fn balanced(formula: &mut Formula, terms: &[Term]) -> Result<Term, Error> {
	if let [only] = terms {
		return Ok(*only);
	}

	let (left, right) = terms.split_at(terms.len() / 2);
	let left = balanced(formula, left)?;
	let right = balanced(formula, right)?;

	formula.intersection(left, right)
}

fn satisfies(cnf: &[Vec<Literal>], assignment: &[bool]) -> bool {
	cnf.iter().all(|clause| {
		clause
			.iter()
			.any(|&(variable, value)| assignment[variable as usize] == value)
	})
}

fn random_cnf(rng: &mut StdRng, variables: u32) -> Vec<Vec<Literal>> {
	let clauses = rng.random_range(1..=6 * variables);

	(0..clauses)
		.map(|_| {
			(0..rng.random_range(1..=3))
				.map(|_| (rng.random_range(0..variables), rng.random_bool(0.5)))
				.collect()
		})
		.collect()
}

/// Checks the answer of `outcome` for `cnf`, and its witness.
fn assert_answers(outcome: &Outcome, cnf: &[Vec<Literal>], satisfiable: bool, mode: &str) {
	assert_eq!(outcome.is_empty(), !satisfiable, "{mode}: {cnf:?}");
	if let Some(witness) = &outcome.witness {
		let assignment: Vec<bool> = witness
			.tape(0)
			.expect("one tape")
			.into_iter()
			.map(|symbol| symbol == Symbol::from(1_u8))
			.collect();
		assert!(satisfies(cnf, &assignment), "{mode}: {cnf:?} by {witness}");
	}
}

#[test]
fn a_macrostate_below_another_through_the_leftmost_leaf_is_skipped() -> Result<(), Error> {
	let mut formula = Formula::new();
	let two = formula.leaf(&TWO_LETTERS)?;
	let threes = [
		formula.leaf(&THREE_LETTERS)?,
		formula.leaf(&THREE_LETTERS)?,
		formula.leaf(&THREE_LETTERS)?,
	];
	let flat = formula.intersection(two, threes[0])?;
	let deep = balanced(&mut formula, &[two, threes[0], threes[1], threes[2]])?;

	// After `b` every leaf is below its state after `a`, and the leftmost leaf is in another state.
	for root in [flat, deep] {
		let with = formula.check(root, &Options::new())?;
		let without = formula.check(root, &Options::new().antichain(false))?;
		assert!(with.is_empty() && without.is_empty());
		assert!(with.stats.explored < without.stats.explored);
		assert_eq!(without.stats.pruned, 0);
	}

	Ok(())
}

#[test]
fn products_of_clauses_agree_with_brute_force_with_and_without_the_antichain() -> Result<(), Error>
{
	let mut rng = StdRng::seed_from_u64(1);
	for _ in 0..SEEDS {
		let variables = rng.random_range(1..=8);
		let cnf = random_cnf(&mut rng, variables);
		let satisfiable = (0..1_u32 << variables).any(|bits| {
			let assignment: Vec<bool> = (0..variables)
				.map(|variable| bits >> variable & 1 == 1)
				.collect();

			satisfies(&cnf, &assignment)
		});

		let mut formula = Formula::new();
		let clauses = cnf
			.iter()
			.map(|literals| {
				formula.leaf(&Clause {
					variables,
					literals,
				})
			})
			.collect::<Result<Vec<Term>, Error>>()?;
		let root = balanced(&mut formula, &clauses)?;
		let not_root = formula.complement(root)?;
		let meet = formula.intersection(root, not_root)?;

		let with = formula.check(root, &Options::new())?;
		let without = formula.check(root, &Options::new().antichain(false))?;
		let plain = formula.check(root, &Options::new().subsumption(false))?;
		assert_answers(&with, &cnf, satisfiable, "antichain");
		assert_answers(&without, &cnf, satisfiable, "no antichain");
		assert_answers(&plain, &cnf, satisfiable, "no subsumption");
		assert_eq!(without.stats.pruned, 0);
		assert_eq!(plain.stats.pruned, 0);
		if !satisfiable {
			assert!(with.stats.explored <= without.stats.explored);
		}

		for options in [Options::new(), Options::new().antichain(false)] {
			assert!(formula.check(meet, &options)?.is_empty(), "{cnf:?}");
		}
	}

	Ok(())
}

#[test]
fn an_antichain_that_prunes_nothing_costs_little() -> Result<(), Error> {
	const VARIABLES: u32 = 500;
	const MODULUS: u32 = 6;

	// Incomparable states, and the last automaton contradicts the first, so the search visits every macrostate.
	let mut formula = Formula::new();
	let mut parities = (0..MODULUS)
		.map(|residue| {
			formula.leaf(&Parity {
				variables: VARIABLES,
				modulus: MODULUS,
				residue,
				odd: residue.is_multiple_of(2),
			})
		})
		.collect::<Result<Vec<Term>, Error>>()?;
	parities.push(formula.leaf(&Parity {
		variables: VARIABLES,
		modulus: MODULUS,
		residue: 0,
		odd: false,
	})?);
	let root = balanced(&mut formula, &parities)?;

	let limited = Options::new().time_limit(Duration::from_secs(10));
	let with = formula.check(root, &limited)?;
	let without = formula.check(root, &limited.clone().antichain(false))?;
	assert!(with.is_empty() && without.is_empty());
	assert_eq!(with.stats.pruned, 0);
	assert_eq!(with.stats.explored, without.stats.explored);
	assert!(with.stats.explored > 20_000);

	Ok(())
}

#[test]
fn a_query_of_one_step_does_not_wait_for_the_simulation_of_a_large_leaf() -> Result<(), Error> {
	// The leftmost leaf's simulation takes seconds; the query finds `c` from its first macrostate.
	let mut formula = Formula::new();
	let dense = formula.leaf(&Dense { states: 12_000 })?;
	let c = formula.leaf(&ONE_C)?;
	let other_c = formula.leaf(&ONE_C)?;
	let both_c = formula.intersection(c, other_c)?;
	let root = formula.intersection(dense, both_c)?;

	let outcome = formula.check(root, &Options::new().time_limit(Duration::from_secs(1)))?;
	assert!(!outcome.is_empty());
	assert_eq!(outcome.stats.explored, 1);

	Ok(())
}
