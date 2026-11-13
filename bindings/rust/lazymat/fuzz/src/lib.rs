//! Random formulas over small tuple automata. Checks that queries never panic, that subsumption, the antichain and
//! explicit nodes do not change the answer, that a budget only turns an answer into `StateLimit`, that a
//! cache budget does not change the witness, and that a witness is in the relation.

use std::time::Instant;

use arbitrary::{Result, Unstructured};
use lazymat::{Automaton, Error, Formula, Options, Outcome, Symbol, Tape, Term};

const MAX_ARITY: usize = 3;
const MAX_STATES: u32 = 4;
const MAX_MOVES: usize = 6;
const MAX_STEPS: usize = 10;
const MAX_COMPLEMENTS: usize = 3;
const BUDGET: usize = 20_000;

/// A tuple automaton given by its moves.
#[derive(Debug)]
struct Tuples {
	arity: usize,
	initial: Vec<u32>,
	finals: Vec<bool>,
	moves: Vec<(u32, Vec<Symbol>, u32)>,
}

impl Automaton for Tuples {
	type State = u32;

	fn arity(&self) -> usize {
		self.arity
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u32> {
		self.initial.iter().copied()
	}

	fn is_final(&self, state: u32) -> bool {
		self.finals[state as usize]
	}

	fn for_each_move(&self, state: u32, mut visit: impl FnMut(&[Symbol], u32)) {
		for (source, tuple, target) in &self.moves {
			if *source == state {
				visit(tuple, *target);
			}
		}
	}
}

fn symbol(input: &mut Unstructured<'_>) -> Result<Symbol> {
	Ok(match input.int_in_range(0..=5)? {
		0..=2 => Symbol::new(input.int_in_range(0..=2)?),
		3 => Symbol::EPSILON,
		_ => Symbol::ANY,
	})
}

fn automaton(input: &mut Unstructured<'_>, arity: usize) -> Result<Tuples> {
	let num_states = input.int_in_range(1..=MAX_STATES)?;
	let state = |input: &mut Unstructured<'_>| input.int_in_range(0..=num_states - 1);
	let initial = vec![state(input)?];
	let finals = (0..num_states)
		.map(|_| input.arbitrary())
		.collect::<Result<_>>()?;

	let num_moves = input.int_in_range(0..=MAX_MOVES)?;
	let moves = (0..num_moves)
		.map(|_| {
			let source = state(input)?;
			let tuple = (0..arity).map(|_| symbol(input)).collect::<Result<_>>()?;

			Ok((source, tuple, state(input)?))
		})
		.collect::<Result<_>>()?;

	Ok(Tuples {
		arity,
		initial,
		finals,
		moves,
	})
}

/// The word of `steps` as a chain automaton.
fn chain(arity: usize, steps: &[&[Symbol]]) -> Tuples {
	let num_states = u32::try_from(steps.len() + 1).expect("short witnesses");
	let mut finals = vec![false; steps.len() + 1];
	finals[steps.len()] = true;

	Tuples {
		arity,
		initial: vec![0],
		finals,
		moves: (0..num_states - 1)
			.zip(steps)
			.map(|(state, step)| (state, step.to_vec(), state + 1))
			.collect(),
	}
}

/// One of the terms built so far.
fn pick(input: &mut Unstructured<'_>, terms: &[Term]) -> Result<Term> {
	input.choose(terms).copied()
}

/// Builds a formula step by step. Builder errors are part of what is fuzzed, so a failed step is skipped.
fn build(input: &mut Unstructured<'_>) -> Result<(Formula, Vec<Term>)> {
	let mut formula = Formula::new();
	let mut terms = Vec::new();
	let mut complements = 0;

	for _ in 0..input.int_in_range(1..=MAX_STEPS)? {
		let operation = input.int_in_range(0..=8)?;
		// Operations 0 and 1 add a leaf; the others need an operand.
		if operation > 1 && terms.is_empty() {
			continue;
		}

		let built = match operation {
			0 | 1 => {
				let arity = input.int_in_range(1..=MAX_ARITY)?;

				formula.leaf(&automaton(input, arity)?)
			}
			2 => formula.union(pick(input, &terms)?, pick(input, &terms)?),
			3 => formula.intersection(pick(input, &terms)?, pick(input, &terms)?),
			4 if complements < MAX_COMPLEMENTS => {
				let operand = pick(input, &terms)?;
				complements += 1;

				formula.complement(operand)
			}
			5 => formula.identity(pick(input, &terms)?),
			6 => {
				let operand = pick(input, &terms)?;
				let tapes: Vec<usize> = (0..input.int_in_range(0..=MAX_ARITY)?)
					.map(|_| input.int_in_range(0..=MAX_ARITY))
					.collect::<Result<_>>()?;

				formula.project(operand, &tapes)
			}
			7 => {
				let (lhs, rhs) = (pick(input, &terms)?, pick(input, &terms)?);
				let pairs: Vec<(usize, usize)> = (0..input.int_in_range(0..=2)?)
					.map(|_| Ok((input.int_in_range(0..=2)?, input.int_in_range(0..=2)?)))
					.collect::<Result<_>>()?;
				let layout: Vec<Tape> = (0..input.int_in_range(0..=MAX_ARITY)?)
					.map(|_| {
						let index = input.int_in_range(0..=2)?;

						Ok(if input.arbitrary()? {
							Tape::Lhs(index)
						} else {
							Tape::Rhs(index)
						})
					})
					.collect::<Result<_>>()?;

				formula.sync_product(lhs, rhs, &pairs, &layout)
			}
			_ => formula.explicit(pick(input, &terms)?),
		};

		if let Ok(term) = built
			&& formula.arity(term).is_ok_and(|arity| arity <= MAX_ARITY)
		{
			terms.push(term);
		}
	}

	Ok((formula, terms))
}

/// The outcome of a query that may fail only at the state limit.
fn within_limit(result: Result<Outcome, Error>) -> Option<Outcome> {
	match result {
		Ok(outcome) => Some(outcome),
		Err(error) => {
			assert!(matches!(error, Error::StateLimit { .. }), "{error}");

			None
		}
	}
}

/// Runs every check on the formula that `input` describes; `trace` prints each query and its time.
///
/// # Errors
///
/// The error of [`Unstructured`] if `input` does not describe a formula.
///
/// # Panics
///
/// If a check fails.
pub fn check(input: &[u8], trace: bool) -> Result<()> {
	let mut input = Unstructured::new(input);
	let (mut formula, terms) = build(&mut input)?;
	let Some(&root) = terms.last() else {
		return Ok(());
	};

	if trace {
		eprintln!("{formula:#?}\nroot {root:?}");
	}

	let timed = |name: &str, options: &Options| {
		let start = Instant::now();
		let result = formula.check(root, options);
		if trace {
			eprintln!(
				"{name}: {:?} in {:?}",
				result.as_ref().map(Outcome::is_empty),
				start.elapsed()
			);
		}

		result
	};

	let budget = Options::new().max_states(BUDGET);
	let Ok(outcome) = timed("lazy", &budget) else {
		return Ok(());
	};

	let all_explicit = terms
		.iter()
		.fold(budget.clone(), |options, &term| options.explicit(term));
	let tight = Options::new().max_states(input.int_in_range(0..=BUDGET)?);
	for (name, options) in [
		("no subsumption", budget.clone().subsumption(false)),
		("no antichain", budget.clone().antichain(false)),
		("all explicit", all_explicit),
		("tight budget", tight),
	] {
		if let Some(other) = within_limit(timed(name, &options)) {
			assert_eq!(other.is_empty(), outcome.is_empty(), "{name}");
		}
	}

	let small_cache = budget
		.clone()
		.cache_budget(input.int_in_range(0..=1 << 12)?);
	if let Some(other) = within_limit(timed("small cache", &small_cache)) {
		assert_eq!(other.witness, outcome.witness, "small cache");
	}

	if let Some(witness) = &outcome.witness {
		let steps: Vec<&[Symbol]> = witness.steps().collect();
		// A step that pads every tape is silent in a leaf, so its chain would be a shorter word.
		let silent = steps
			.iter()
			.any(|step| step.iter().all(|&symbol| symbol == Symbol::EPSILON));
		if witness.arity() > 0 && !silent {
			let word = formula
				.leaf(&chain(witness.arity(), &steps))
				.expect("a valid chain");
			let inside = formula.intersection(root, word).expect("equal arities");
			if let Some(inside) = within_limit(formula.check(inside, &budget)) {
				assert!(!inside.is_empty(), "witness {witness} not in the relation");
			}
		}
	}

	Ok(())
}
