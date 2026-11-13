//! Leaves from regex-automata, rustfst and Mata in one formula.

use std::error::Error;

use lazymat::regex::Regex;
use lazymat::{Formula, Options, Symbol, Term};
use mata::nft::Nft;
use mata::{Nfa, State};
use rand::rngs::StdRng;
use rand::{RngExt, SeedableRng};
use rustfst::fst_impls::VectorFst;
use rustfst::fst_traits::MutableFst;
use rustfst::semirings::{Semiring, TropicalWeight};
use rustfst::{EPS_LABEL, StateId, Tr};

const PATTERNS: [&str; 8] = [
	"a(b|c)*",
	"(ab)*a?",
	"[a-c]{2,3}",
	"a*b*",
	"(a|b)*abb",
	"",
	"c+|(ab)+",
	"((a|b)(a|b))*",
];
const SEEDS: u64 = 500;

fn over_abc() -> Options {
	Options::new().alphabet(*b"abc")
}

/// `lhs ∩ ¬rhs`, which is empty iff `lhs ⊆ rhs`.
fn difference(formula: &mut Formula, lhs: Term, rhs: Term) -> Result<Term, lazymat::Error> {
	let not_rhs = formula.complement(rhs)?;

	formula.intersection(lhs, not_rhs)
}

#[test]
fn regex_automata_and_mata_agree_on_patterns() -> Result<(), Box<dyn Error>> {
	for pattern in PATTERNS {
		let mut formula = Formula::new();
		let dfa = formula.leaf(&Regex::new(pattern)?)?;
		let nfa = formula.leaf(&Nfa::from_regex(pattern)?)?;

		for (lhs, rhs) in [(dfa, nfa), (nfa, dfa)] {
			let lhs_only = difference(&mut formula, lhs, rhs)?;
			assert!(
				formula.check(lhs_only, &over_abc())?.is_empty(),
				"{pattern}"
			);
		}
	}

	Ok(())
}

#[test]
fn different_patterns_are_told_apart() -> Result<(), Box<dyn Error>> {
	let starts_a = Nfa::from_regex("a(b|c)*")?;
	let starts_a_ends_b = Nfa::from_regex("a(b|c)*b")?;
	let mut formula = Formula::new();
	let dfa = formula.leaf(&Regex::new("a(b|c)*")?)?;
	let nfa = formula.leaf(&starts_a_ends_b)?;
	let dfa_only = difference(&mut formula, dfa, nfa)?;

	let witness = formula
		.check(dfa_only, &over_abc())?
		.witness
		.expect("the languages differ");
	let word: Vec<mata::Symbol> = witness
		.tape(0)
		.expect("one tape")
		.into_iter()
		.map(Into::into)
		.collect();

	assert!(starts_a.accepts(&word));
	assert!(!starts_a_ends_b.accepts(&word));

	Ok(())
}

#[test]
fn one_formula_over_three_backends() -> Result<(), Box<dyn Error>> {
	// A rustfst transducer that replaces a by b and keeps c.
	let mut replace = VectorFst::<TropicalWeight>::new();
	let state = replace.add_state();
	replace.set_start(state)?;
	replace.set_final(state, TropicalWeight::one())?;
	for (input, output) in [(b'a', b'b'), (b'c', b'c')] {
		let tr = Tr::new(input.into(), output.into(), TropicalWeight::one(), state);
		replace.add_tr(state, tr)?;
	}

	// A Mata NFA of the words with no two c in a row.
	let [a, b, c] = b"abc".map(mata::Symbol::from);
	let mut no_cc = Nfa::with_states(2);
	let (free, after_c) = (State::new(0), State::new(1));
	no_cc.add_initial(free)?;
	no_cc.add_final(free)?;
	no_cc.add_final(after_c)?;
	for state in [free, after_c] {
		no_cc.add_transition(state, a, free)?;
		no_cc.add_transition(state, b, free)?;
	}

	no_cc.add_transition(free, c, after_c)?;

	let mut formula = Formula::new();
	let input = formula.leaf(&Regex::new("(ac)*")?)?;
	let relation = formula.leaf(&replace)?;
	let filter = formula.leaf(&no_cc)?;
	let image = formula.post_image(input, relation)?;
	let only_bc = formula.leaf(&Regex::new("(bc)*")?)?;

	// The image of (ac)* is (bc)*, and it has no cc.
	let outside = difference(&mut formula, image, only_bc)?;
	let with_cc = difference(&mut formula, image, filter)?;
	assert!(formula.check(outside, &over_abc())?.is_empty());
	assert!(formula.check(with_cc, &over_abc())?.is_empty());

	Ok(())
}

/// The same random transducer over the labels 1 and 2 for rustfst and Mata, with absent zero-weight moves in rustfst.
fn random_pair(rng: &mut StdRng) -> (VectorFst<TropicalWeight>, Nft) {
	let mut fst = VectorFst::<TropicalWeight>::new();
	let mut nft = Nft::new(2).expect("2 levels");
	let num_states: usize = rng.random_range(1..=3);
	let fst_states: Vec<StateId> = (0..num_states).map(|_| fst.add_state()).collect();
	let nft_states: Vec<State> = (0..num_states).map(|_| nft.add_state()).collect();
	fst.set_start(fst_states[0]).expect("a state of the FST");
	nft.add_initial(nft_states[0]).expect("a level-0 state");

	for (&fst_state, &nft_state) in fst_states.iter().zip(&nft_states) {
		if rng.random_bool(0.4) {
			fst.set_final(fst_state, TropicalWeight::new(0.5))
				.expect("a state of the FST");
			nft.add_final(nft_state).expect("a level-0 state");
		}
	}

	for _ in 0..rng.random_range(0..=6) {
		let source = rng.random_range(0..num_states);
		let target = rng.random_range(0..num_states);
		let input = rng.random_range(1..=2u32);
		let output = rng.random_range(1..=2u32);
		let weight = if rng.random_bool(0.2) {
			TropicalWeight::zero()
		} else {
			TropicalWeight::new(1.0)
		};

		let tr = Tr::new(input, output, weight, fst_states[target]);
		fst.add_tr(fst_states[source], tr)
			.expect("a state of the FST");
		if !weight.is_zero() {
			let symbols = [mata::Symbol::new(input), mata::Symbol::new(output)];
			nft.add_transition(nft_states[source], &symbols, nft_states[target])
				.expect("a transition of the NFT");
		}
	}

	(fst, nft)
}

#[test]
fn rustfst_and_mata_transducers_agree() -> Result<(), Box<dyn Error>> {
	let options = Options::new().alphabet([1u32, 2]);
	let mut non_empty = 0;
	for seed in 0..SEEDS {
		let mut rng = StdRng::seed_from_u64(seed);
		let (fst, nft) = random_pair(&mut rng);
		let mut formula = Formula::new();
		let fst_leaf = formula.leaf(&fst)?;
		let nft_leaf = formula.leaf(&nft)?;

		if !formula.is_empty(fst_leaf)? {
			non_empty += 1;
		}

		for (lhs, rhs) in [(fst_leaf, nft_leaf), (nft_leaf, fst_leaf)] {
			let lhs_only = difference(&mut formula, lhs, rhs)?;
			assert!(formula.check(lhs_only, &options)?.is_empty(), "seed {seed}");
		}
	}

	assert!(
		(SEEDS / 5..SEEDS * 9 / 10).contains(&non_empty),
		"{non_empty} of {SEEDS} non-empty"
	);

	Ok(())
}

#[test]
fn rustfst_epsilon_is_padding() -> Result<(), Box<dyn Error>> {
	let mut fst = VectorFst::<TropicalWeight>::new();
	let (source, target) = (fst.add_state(), fst.add_state());
	fst.set_start(source)?;
	fst.set_final(target, TropicalWeight::one())?;
	let tr = Tr::new(u32::from(b'a'), EPS_LABEL, TropicalWeight::one(), target);
	fst.add_tr(source, tr)?;

	let mut formula = Formula::new();
	let leaf = formula.leaf(&fst)?;
	let witness = formula.find_word(leaf)?.expect("the relation is not empty");

	assert_eq!(
		witness.steps().next().expect("one step"),
		[Symbol::from(b'a'), Symbol::EPSILON]
	);

	Ok(())
}

#[test]
fn materialized_terms_go_back_to_mata() -> Result<(), Box<dyn Error>> {
	let ends_b_nfa = Nfa::from_regex("(a|b)*b")?;
	let ends_abb_nfa = Nfa::from_regex("(a|b)*abb")?;
	let mut formula = Formula::new();
	let ends_abb = formula.leaf(&Regex::new("(a|b)*abb")?)?;
	let ends_b = formula.leaf(&ends_b_nfa)?;
	let ends_b_only = difference(&mut formula, ends_b, ends_abb)?;

	let materialized = formula.materialize(ends_b_only, &Options::new())?;
	let alphabet = mata::Alphabet::from_symbols(b"ab".map(mata::Symbol::from))?;
	let nfa = materialized.to_nfa_over(&alphabet);
	let expected = ends_b_nfa.difference(&ends_abb_nfa);
	assert!(nfa.is_equivalent_to(&expected));

	let minimal = nfa.minimize();
	let mut again = Formula::new();
	let leaf = again.leaf(&minimal)?;
	let original = again.leaf(&expected)?;
	let extra = difference(&mut again, leaf, original)?;
	assert!(again.is_empty(extra)?);

	Ok(())
}
