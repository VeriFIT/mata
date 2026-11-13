//! Random automata and transducers against Mata's eager operations.

use lazymat::{Error, Formula, Options, Term, Witness};
use mata::nft::Nft;
use mata::{Alphabet, Nfa, State, Symbol};
use rand::rngs::StdRng;
use rand::{RngExt, SeedableRng};

const LETTERS: [u8; 2] = *b"ab";
const SEEDS: u64 = 1000;
const DEPTH: usize = 4;

fn random_letter(rng: &mut StdRng) -> Symbol {
	Symbol::from(LETTERS[rng.random_range(0..LETTERS.len())])
}

fn random_nfa(rng: &mut StdRng, with_epsilon: bool) -> Nfa {
	let num_states = rng.random_range(1..=4u32);
	let mut nfa = Nfa::with_states(num_states as usize);
	nfa.add_initial(State::new(0)).expect("a state of the NFA");

	for state in 0..num_states {
		if rng.random_bool(0.35) {
			nfa.add_final(State::new(state))
				.expect("a state of the NFA");
		}
	}

	for _ in 0..rng.random_range(0..=7) {
		let symbol = if with_epsilon && rng.random_bool(0.15) {
			Symbol::EPSILON
		} else {
			random_letter(rng)
		};

		let source = rng.random_range(0..num_states);
		let target = rng.random_range(0..num_states);
		nfa.add_transition(State::new(source), symbol, State::new(target))
			.expect("a state of the NFA");
	}

	nfa
}

fn random_nft(rng: &mut StdRng) -> Nft {
	let mut nft = Nft::new(2).expect("2 levels");
	let states: Vec<State> = (0..rng.random_range(1..=3))
		.map(|_| nft.add_state())
		.collect();
	nft.add_initial(states[0]).expect("a level-0 state");

	for &state in &states {
		if rng.random_bool(0.4) {
			nft.add_final(state).expect("a level-0 state");
		}
	}

	for _ in 0..rng.random_range(0..=5) {
		let symbols = [random_letter(rng), random_letter(rng)];
		let source = states[rng.random_range(0..states.len())];
		let target = states[rng.random_range(0..states.len())];
		nft.add_transition(source, &symbols, target)
			.expect("a transition of the NFT");
	}

	nft
}

fn sigma() -> Alphabet {
	Alphabet::from_symbols(LETTERS.map(Symbol::from)).expect("letters, not epsilon")
}

fn options() -> Options {
	Options::new().alphabet(LETTERS)
}

fn mata_word(witness: &Witness, tape: usize) -> Vec<Symbol> {
	let word = witness.tape(tape).expect("the tape exists");

	word.into_iter().map(Symbol::from).collect()
}

/// A random Boolean formula, and the same built eagerly by Mata (over epsilon-free copies).
fn boolean(rng: &mut StdRng, formula: &mut Formula, depth: usize) -> Result<(Term, Nfa), Error> {
	if depth == 0 || rng.random_bool(0.25) {
		let nfa = random_nfa(rng, true);
		let mut eager = nfa.clone();
		eager.remove_epsilon();

		return Ok((formula.leaf(&nfa)?, eager));
	}

	match rng.random_range(0..4) {
		0 => {
			let (lhs, lhs_eager) = boolean(rng, formula, depth - 1)?;
			let (rhs, rhs_eager) = boolean(rng, formula, depth - 1)?;

			Ok((formula.union(lhs, rhs)?, lhs_eager.union(&rhs_eager)))
		}
		1 => {
			let (lhs, lhs_eager) = boolean(rng, formula, depth - 1)?;
			let (rhs, rhs_eager) = boolean(rng, formula, depth - 1)?;

			Ok((
				formula.intersection(lhs, rhs)?,
				lhs_eager.intersection(&rhs_eager),
			))
		}
		2 => {
			let (operand, operand_eager) = boolean(rng, formula, depth - 1)?;

			Ok((
				formula.complement(operand)?,
				operand_eager.complement(&sigma()),
			))
		}
		_ => {
			let (operand, operand_eager) = boolean(rng, formula, depth - 1)?;

			Ok((formula.explicit(operand)?, operand_eager))
		}
	}
}

#[test]
fn boolean_combinations_agree_with_mata() -> Result<(), Error> {
	let mut empty = 0;
	for seed in 0..SEEDS {
		let mut rng = StdRng::seed_from_u64(seed);
		let mut formula = Formula::new();
		let (root, eager) = boolean(&mut rng, &mut formula, DEPTH)?;

		let outcome = formula.check(root, &options())?;
		assert_eq!(outcome.is_empty(), eager.is_lang_empty(), "seed {seed}");
		if outcome.is_empty() {
			empty += 1;
		}

		if let Some(witness) = &outcome.witness {
			assert!(
				eager.accepts(&mata_word(witness, 0)),
				"seed {seed}: {witness}"
			);
		}

		let materialized = formula.materialize(root, &options())?;
		assert!(
			materialized.to_nfa_over(&sigma()).is_equivalent_to(&eager),
			"seed {seed}: materialized"
		);
	}

	assert!(
		(SEEDS * 15 / 100..SEEDS * 85 / 100).contains(&empty),
		"{empty} of {SEEDS} empty"
	);

	Ok(())
}

#[test]
fn transducer_operations_agree_with_mata() -> Result<(), Error> {
	let mut queries: u64 = 0;
	let mut empty = 0;
	for seed in 0..SEEDS {
		let mut rng = StdRng::seed_from_u64(seed);
		let lhs = random_nft(&mut rng);
		let rhs = random_nft(&mut rng);
		let language = random_nfa(&mut rng, false);
		let mut formula = Formula::new();
		let lhs_leaf = formula.leaf(&lhs)?;
		let rhs_leaf = formula.leaf(&rhs)?;
		let language_leaf = formula.leaf(&language)?;

		let cases = [
			(
				"intersection",
				formula.intersection(lhs_leaf, rhs_leaf)?,
				lhs.intersection(&rhs)?,
			),
			(
				"union",
				formula.union(lhs_leaf, rhs_leaf)?,
				lhs.union(&rhs)?,
			),
			(
				"compose",
				formula.compose(lhs_leaf, rhs_leaf)?,
				lhs.compose(&rhs)?,
			),
			(
				"post_image",
				formula.post_image(language_leaf, lhs_leaf)?,
				lhs.apply(&language, 0)?,
			),
			(
				"pre_image",
				formula.pre_image(language_leaf, lhs_leaf)?,
				lhs.apply(&language, 1)?,
			),
		];
		for (operation, term, eager) in cases {
			queries += 1;
			let witness = formula.find_word(term)?;
			assert_eq!(
				witness.is_none(),
				eager.is_lang_empty(),
				"seed {seed}: {operation}"
			);
			if witness.is_none() {
				empty += 1;
			}

			if let Some(witness) = witness
				&& witness.arity() == 2
			{
				let words = [mata_word(&witness, 0), mata_word(&witness, 1)];
				assert!(eager.accepts(&words), "seed {seed}: {operation} {witness}");
			}
		}
	}

	assert!(
		(queries * 15 / 100..queries * 85 / 100).contains(&empty),
		"{empty} of {queries} empty"
	);

	Ok(())
}

#[test]
fn transducer_epsilon_is_padding() -> Result<(), Error> {
	// For {(a, ε)}, Mata pads the shorter word, and lock step reads (a, ε) in one step.
	let mut relation = Nft::new(2)?;
	let (source, target) = (relation.add_state(), relation.add_state());
	relation.add_initial(source)?;
	relation.add_final(target)?;
	relation.add_transition(source, &[Symbol::from(b'a'), Symbol::EPSILON], target)?;

	let mut formula = Formula::new();
	let leaf = formula.leaf(&relation)?;
	let witness = formula.find_word(leaf)?.expect("the relation is not empty");

	assert_eq!(
		witness.steps().next().expect("one step"),
		[lazymat::Symbol::from(b'a'), lazymat::Symbol::EPSILON]
	);
	assert_eq!(witness.tape(1), Some(Vec::new()));

	Ok(())
}
