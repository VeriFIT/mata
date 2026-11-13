//! Mata automata over {a, b} and formulas with many initial macrostates, shared by `initial` and `products`.

use lazymat::{Error, Formula, Term, Witness};
use mata::{Nfa, Symbol};

use crate::common::nfa;

pub(crate) const A: Symbol = Symbol::new(b'a' as u32);
pub(crate) const B: Symbol = Symbol::new(b'b' as u32);

/// The language `{word}`.
pub(crate) fn just(word: &[Symbol]) -> Nfa {
	let len = u32::try_from(word.len()).expect("a short word");
	let moves: Vec<(u32, Symbol, u32)> = (0..len)
		.zip(word)
		.map(|(state, &symbol)| (state, symbol, state + 1))
		.collect();

	nfa(len + 1, &[0], &[len], &moves)
}

/// The words of length `len` with `letter` at `position`.
fn letter_at(len: u32, position: u32, letter: Symbol) -> Nfa {
	let moves: Vec<(u32, Symbol, u32)> = (0..len)
		.flat_map(|state| [A, B].map(|symbol| (state, symbol, state + 1)))
		.filter(|&(state, symbol, _)| state != position || symbol == letter)
		.collect();

	nfa(len + 1, &[0], &[len], &moves)
}

/// The intersection of `terms` as a balanced tree.
pub(crate) fn intersect(formula: &mut Formula, mut terms: Vec<Term>) -> Result<Term, Error> {
	while terms.len() > 1 {
		terms = terms
			.chunks(2)
			.map(|pair| match *pair {
				[lhs, rhs] => formula.intersection(lhs, rhs),
				[single] => Ok(single),
				_ => unreachable!("chunks of at most 2"),
			})
			.collect::<Result<_, _>>()?;
	}

	Ok(terms[0])
}

/// The words of length `len`, one per initial macrostate, as the intersection of (a there ∪ b there) over the
/// positions.
pub(crate) fn every_word(formula: &mut Formula, len: u32) -> Result<Term, Error> {
	let mut unions = Vec::new();
	for position in 0..len {
		let a_there = formula.leaf(&letter_at(len, position, A))?;
		let b_there = formula.leaf(&letter_at(len, position, B))?;
		unions.push(formula.union(a_there, b_there)?);
	}

	intersect(formula, unions)
}

/// The intersection of `count` unions of {a} and {a, b}, which is the language {a, b} with 2^count initial
/// macrostates.
pub(crate) fn unions_of_a_and_ab(formula: &mut Formula, count: usize) -> Result<Term, Error> {
	let a_or_b = nfa(2, &[0], &[1], &[(0, A, 1), (0, B, 1)]);
	let mut unions = Vec::new();
	for _ in 0..count {
		let a = formula.leaf(&just(&[A]))?;
		let either = formula.leaf(&a_or_b)?;
		unions.push(formula.union(a, either)?);
	}

	intersect(formula, unions)
}

/// The word of a one-tape witness.
pub(crate) fn word_of(witness: &Witness) -> Vec<Symbol> {
	witness
		.tape(0)
		.expect("one tape")
		.into_iter()
		.map(Symbol::from)
		.collect()
}
