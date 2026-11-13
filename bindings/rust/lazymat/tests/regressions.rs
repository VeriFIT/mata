//! Semantic corner cases and inputs that once went wrong or did not scale.

mod automata;
mod common;

use automata::{nft, single, star, tuple};
use common::nfa;
use lazymat::{Error, Formula, Options};
use mata::Symbol;
use mata::nft::DONT_CARE;

const A: Symbol = Symbol::new(b'a' as u32);
const B: Symbol = Symbol::new(b'b' as u32);

/// Epsilon transitions of an NFA are silent.
#[test]
fn epsilon_transitions_are_silent() -> Result<(), Error> {
	let mut formula = Formula::new();
	let eps_a = formula.leaf(&nfa(3, &[0], &[2], &[(0, Symbol::EPSILON, 1), (1, A, 2)]))?;
	let just_a = formula.leaf(&single(A))?;
	let both = formula.intersection(eps_a, just_a)?;
	let not_eps_a = formula.complement(eps_a)?;
	let rest = formula.intersection(not_eps_a, just_a)?;

	assert!(!formula.is_empty(both)?);
	assert!(formula.is_empty(rest)?);

	Ok(())
}

/// A complement matches wildcards against every symbol and never contains wildcards itself.
#[test]
fn complement_reads_dont_care_as_any_symbol() -> Result<(), Error> {
	let mut formula = Formula::new();
	let wild = formula.leaf(&tuple(&[DONT_CARE, B]))?;
	let concrete = formula.leaf(&tuple(&[A, B]))?;
	let not_wild = formula.complement(wild)?;
	let inside = formula.intersection(not_wild, concrete)?;
	assert!(formula.is_empty(inside)?);

	let any = formula.leaf(&single(DONT_CARE))?;
	let not_any = formula.complement(any)?;
	let contradiction = formula.intersection(any, not_any)?;
	let options = Options::new().alphabet([A, B]);
	assert!(formula.check(contradiction, &options)?.is_empty());

	Ok(())
}

/// The identity of a wildcard reads the same symbol on both tapes.
#[test]
fn identity_of_a_wildcard_is_diagonal() -> Result<(), Error> {
	let mut formula = Formula::new();
	let any = formula.leaf(&single(DONT_CARE))?;
	let diagonal = formula.identity(any)?;
	let a_to_b = formula.leaf(&tuple(&[A, B]))?;
	let a_to_a = formula.leaf(&tuple(&[A, A]))?;
	let off = formula.intersection(diagonal, a_to_b)?;
	let on = formula.intersection(diagonal, a_to_a)?;

	assert!(formula.is_empty(off)?);
	assert!(!formula.is_empty(on)?);

	Ok(())
}

/// An identity or projection above a pruned product.
#[test]
fn projected_root_over_a_complemented_composition() -> Result<(), Error> {
	let mut formula = Formula::new();
	let language = formula.leaf(&single(A))?;
	let first = formula.leaf(&tuple(&[A, A]))?;
	let second = formula.leaf(&tuple(&[A, A]))?;
	let composed = formula.compose(first, second)?;
	let not_composed = formula.complement(composed)?;
	let id = formula.identity(language)?;
	let both = formula.intersection(id, not_composed)?;
	let projected = formula.project(both, &[0])?;

	assert!(formula.is_empty(projected)?);

	let wider = Options::new().alphabet(*b"ab");
	assert!(formula.check(projected, &wider)?.is_empty());

	Ok(())
}

/// Arities above 255.
#[test]
fn wide_transducers() -> Result<(), Error> {
	const ARITY: usize = 300;

	let mut formula = Formula::new();
	let leaf = formula.leaf(&tuple(&[A; ARITY]))?;
	let not_leaf = formula.complement(leaf)?;
	let none = formula.intersection(leaf, not_leaf)?;

	assert_eq!(formula.arity(leaf)?, ARITY);
	let witness = formula.find_word(leaf)?.expect("the relation is not empty");
	assert_eq!(witness.len(), 1);
	assert!(formula.is_empty(none)?);

	Ok(())
}

/// Deep formulas, and union chains (one union node, so linearly many macrostates).
#[test]
fn long_chains() -> Result<(), Error> {
	const LONG_CHAIN: u32 = 2000;
	const SHORT_CHAIN: u32 = 150;

	let mut formula = Formula::new();
	let mut unions = formula.leaf(&single(Symbol::new(0)))?;
	let mut short_unions = unions;
	let mut intersections = formula.leaf(&star(A))?;
	for index in 1..LONG_CHAIN {
		let leaf = formula.leaf(&single(Symbol::new(index)))?;
		unions = formula.union(unions, leaf)?;
		if index < SHORT_CHAIN {
			short_unions = formula.union(short_unions, leaf)?;
		}

		let a_star = formula.leaf(&star(A))?;
		intersections = formula.intersection(intersections, a_star)?;
	}

	let outcome = formula.check(unions, &Options::new())?;
	assert!(!outcome.is_empty());
	assert!(outcome.stats.macrostates < 2 * LONG_CHAIN as usize);

	let witness = formula
		.find_word(intersections)?
		.expect("every a* holds the empty word");
	assert!(witness.is_empty());

	let not_short = formula.complement(short_unions)?;
	let none = formula.intersection(short_unions, not_short)?;
	assert!(formula.is_empty(none)?);

	Ok(())
}

/// Double complement drops padded words, as a complement has none.
#[test]
fn double_complement_of_padded_relation() -> Result<(), Error> {
	let mut formula = Formula::new();
	let leaf = formula.leaf(&tuple(&[A, Symbol::EPSILON]))?;
	let not_leaf = formula.complement(leaf)?;
	let twice = formula.complement(not_leaf)?;

	assert!(!formula.is_empty(leaf)?);
	assert!(formula.is_empty(twice)?);

	Ok(())
}

/// Mata's all-epsilon transitions are silent in a transducer too.
#[test]
fn all_epsilon_transducer_moves_are_silent() -> Result<(), Error> {
	let silent = [Symbol::EPSILON, Symbol::EPSILON];
	let mut formula = Formula::new();
	let leaf = formula.leaf(&nft(2, 3, &[0], &[2], &[(0, &[A, B], 1), (1, &silent, 2)]))?;
	let exact = formula.leaf(&tuple(&[A, B]))?;
	let both = formula.intersection(leaf, exact)?;

	assert!(!formula.is_empty(both)?);

	Ok(())
}
