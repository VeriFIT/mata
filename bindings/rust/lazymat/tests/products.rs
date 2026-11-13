//! Products of nondeterministic operands with many initial macrostates.

mod common;
mod words;

use std::time::Duration;

use common::nfa;
use lazymat::{Error, ExplicitOptions, Formula, Options};
use mata::{Nfa, Symbol};
use words::{A, B, every_word, intersect, just, unions_of_a_and_ab, word_of};

const X: Symbol = Symbol::new(b'x' as u32);
const Y: Symbol = Symbol::new(b'y' as u32);

/// The state limit stops a query that would visit 2^40 initial macrostates.
#[test]
fn the_state_limit_stops_many_initial_macrostates() -> Result<(), Error> {
	let mut formula = Formula::new();
	let unions = unions_of_a_and_ab(&mut formula, 40)?;
	let bb = formula.leaf(&just(&[B, B]))?;
	let root = formula.intersection(unions, bb)?;

	let options = Options::new().subsumption(false).max_states(100_000);
	let stopped = formula.check(root, &options);
	assert!(
		matches!(
			stopped,
			Err(Error::StateLimit {
				max_states: 100_000,
				..
			})
		),
		"{stopped:?}"
	);

	Ok(())
}

/// Each word of length 17 is found through its own one of the 2^17 initial macrostates, numbered on demand.
#[test]
fn every_initial_macrostate_of_a_large_product_is_visited() -> Result<(), Error> {
	const LEN: u32 = 17;

	let alternating = (0..LEN)
		.map(|position| if position % 3 == 0 { A } else { B })
		.collect();
	let mut formula = Formula::new();
	let all = every_word(&mut formula, LEN)?;
	// The root antichain would compare each initial macrostate with all others.
	let plain = Options::new().subsumption(false);

	for word in [vec![A; LEN as usize], vec![B; LEN as usize], alternating] {
		let only = formula.leaf(&just(&word))?;
		let root = formula.intersection(all, only)?;
		let witness = formula.check(root, &plain)?.witness.expect("the word");
		assert_eq!(word_of(&witness), word);
	}

	let short = formula.leaf(&just(&[A, B]))?;
	let none = formula.intersection(all, short)?;
	assert!(formula.check(none, &plain)?.is_empty());

	Ok(())
}

/// An explicit node stops at its budget while numbering its initial macrostates.
#[test]
fn an_explicit_node_with_many_initial_macrostates_stays_lazy() -> Result<(), Error> {
	let mut formula = Formula::new();
	let all = every_word(&mut formula, 17)?;
	let explicit = formula.explicit_with(all, ExplicitOptions::new().max_states(1000))?;

	let outcome = formula.check(explicit, &Options::new())?;
	assert!(!outcome.is_empty());
	assert!(outcome.stats.explicit[0].lazy);

	Ok(())
}

/// Starts `0..count` with `height` steps up on `a`, `b` to the same step of the previous start, `end` at the top.
fn staircase(count: u32, height: u32, end: Symbol) -> Nfa {
	let state = |start: u32, step: u32| start * (height + 1) + step;
	let last = count * (height + 1);
	let mut moves = Vec::new();
	for start in 0..count {
		for step in 0..=height {
			let (symbol, target) = if step < height {
				(A, state(start, step + 1))
			} else {
				(end, last)
			};

			moves.push((state(start, step), symbol, target));
			if start > 0 {
				moves.push((state(start, step), B, state(start - 1, step)));
			}
		}
	}

	let starts: Vec<u32> = (0..count).map(|start| state(start, 0)).collect();

	nfa(last + 1, &starts, &[last], &moves)
}

/// 67,600 initial macrostates, more than one batch, all below the last one, so the root antichain prunes them.
#[test]
fn the_antichain_prunes_many_initial_macrostates() -> Result<(), Error> {
	const STARTS: u32 = 260;

	let mut formula = Formula::new();
	let x = formula.leaf(&staircase(STARTS, 2, X))?;
	let y = formula.leaf(&staircase(STARTS, 2, Y))?;
	let none = formula.intersection(x, y)?;

	let outcome = formula.check(none, &Options::new())?;
	assert!(outcome.is_empty());
	assert!(outcome.stats.explored < 1000, "{:?}", outcome.stats);

	Ok(())
}

/// Start `i` of `count` reads its own letter, then `end`, so no start is below another.
fn fan(count: u32, end: Symbol) -> Nfa {
	let (middle, last) = (count, count + 1);
	let starts: Vec<u32> = (0..count).collect();
	let mut moves: Vec<(u32, Symbol, u32)> = starts
		.iter()
		.map(|&start| (start, Symbol::new(10 + start), middle))
		.collect();
	moves.push((middle, end, last));

	nfa(count + 2, &starts, &[last], &moves)
}

/// Under a union with a*, the empty word answers before any of the 300² initial macrostates of two fans.
#[test]
fn an_accepting_initial_macrostate_beside_many_answers_at_once() -> Result<(), Error> {
	let mut formula = Formula::new();
	let a_star = formula.leaf(&nfa(1, &[0], &[0], &[(0, A, 0)]))?;
	let x = formula.leaf(&fan(300, X))?;
	let y = formula.leaf(&fan(300, Y))?;
	let none = formula.intersection(x, y)?;
	let root = formula.union(a_star, none)?;

	let outcome = formula.check(root, &Options::new().time_limit(Duration::from_secs(60)))?;
	assert_eq!(outcome.witness.map(|witness| witness.len()), Some(0));

	Ok(())
}

/// A union takes its one-word leaf before an empty intersection with 2^40 initial macrostates.
#[test]
fn a_union_root_takes_its_branches_in_order() -> Result<(), Error> {
	let mut formula = Formula::new();
	let unions = unions_of_a_and_ab(&mut formula, 40)?;
	let x = formula.leaf(&just(&[X]))?;
	let none = formula.intersection(unions, x)?;
	let y = formula.leaf(&just(&[Y]))?;
	let root = formula.union(y, none)?;

	let options = Options::new()
		.subsumption(false)
		.time_limit(Duration::from_secs(60));
	let witness = formula.check(root, &options)?.witness.expect("the word y");
	assert_eq!(word_of(&witness), [Y]);

	Ok(())
}

/// 17 unions of {ε, a} and {aa}, where the first of 2^17 initial macrostates accepts and is found without numbering
/// the others.
#[test]
fn an_accepting_initial_macrostate_is_found_without_numbering() -> Result<(), Error> {
	let mut formula = Formula::new();
	let mut unions = Vec::new();
	for _ in 0..17 {
		let empty_or_a = formula.leaf(&nfa(2, &[0], &[0, 1], &[(0, A, 1)]))?;
		let aa = formula.leaf(&just(&[A, A]))?;
		unions.push(formula.union(empty_or_a, aa)?);
	}

	let root = intersect(&mut formula, unions)?;

	let outcome = formula.check(root, &Options::new().subsumption(false))?;
	assert_eq!(outcome.witness.map(|witness| witness.len()), Some(0));

	Ok(())
}
