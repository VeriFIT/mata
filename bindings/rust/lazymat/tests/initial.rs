//! Unions, complements, explicit nodes and limits with more initial macrostates than are listed at once.

mod common;
mod words;

use std::time::Duration;

use common::nfa;
use lazymat::{Error, ExplicitOptions, Formula, Options, Term};
use mata::Symbol;
use words::{A, B, every_word, just, unions_of_a_and_ab, word_of};

/// Whether `term` contains `word`.
fn contains(formula: &mut Formula, term: Term, word: &[Symbol]) -> Result<bool, Error> {
	let only = formula.leaf(&just(word))?;
	let both = formula.intersection(term, only)?;
	let outcome = formula.check(both, &Options::new().subsumption(false))?;

	Ok(!outcome.is_empty())
}

/// A word is found without listing 2^40 or 2^70 (more than a `usize` counts) initial macrostates, balanced or a
/// chain.
#[test]
fn intersections_of_many_unions_find_a_witness() -> Result<(), Error> {
	for count in [40, 70] {
		let mut formula = Formula::new();
		let balanced = unions_of_a_and_ab(&mut formula, count)?;
		let mut chain = unions_of_a_and_ab(&mut formula, 1)?;
		for _ in 1..count {
			let union = unions_of_a_and_ab(&mut formula, 1)?;
			chain = formula.intersection(chain, union)?;
		}

		for root in [balanced, chain] {
			for subsumption in [true, false] {
				let outcome = formula.check(root, &Options::new().subsumption(subsumption))?;
				let word = word_of(&outcome.witness.expect("the language is {a, b}"));
				assert!(word == [A] || word == [B], "{word:?}");
			}
		}
	}

	Ok(())
}

/// A union of products with 2^17 initial macrostates each reaches the branch that holds the word.
#[test]
fn a_union_of_large_products_reaches_every_branch() -> Result<(), Error> {
	let a_last: Vec<Symbol> = [B; 16].into_iter().chain([A]).collect();
	let b_last: Vec<Symbol> = [A; 16].into_iter().chain([B]).collect();
	let mut formula = Formula::new();
	let all = every_word(&mut formula, 17)?;
	let short = formula.leaf(&just(&[A, B]))?;
	let ends_a = formula.leaf(&just(&a_last))?;
	let ends_b = formula.leaf(&just(&b_last))?;
	let none = formula.intersection(all, short)?;
	let a_branch = formula.intersection(all, ends_a)?;
	let b_branch = formula.intersection(all, ends_b)?;
	let plain = Options::new().subsumption(false);

	let root = formula.union(none, b_branch)?;
	let witness = formula
		.check(root, &plain)?
		.witness
		.expect("the word ending in b");
	assert_eq!(word_of(&witness), b_last);

	let empty = formula.union(none, none)?;
	assert!(formula.check(empty, &plain)?.is_empty());

	let both = formula.union(a_branch, b_branch)?;
	let witness = formula.check(both, &plain)?.witness.expect("either word");
	let word = word_of(&witness);
	assert!(word == a_last || word == b_last, "{word:?}");

	Ok(())
}

/// The complement of a lazy node with 2^17 initial macrostates holds exactly the words not of length 17.
#[test]
fn a_complement_of_a_large_lazy_node_is_exact() -> Result<(), Error> {
	let mut formula = Formula::new();
	let all = every_word(&mut formula, 17)?;
	let lazy = formula.explicit_with(all, ExplicitOptions::new().max_states(10))?;
	let root = formula.complement(lazy)?;

	let outcome = formula.check(root, &Options::new().subsumption(false))?;
	assert!(outcome.stats.explicit[0].lazy);
	let witness = outcome.witness.expect("the empty word, for one");
	assert_ne!(witness.len(), 17);

	for (word, inside) in [
		(vec![A; 17], false),
		(vec![B; 17], false),
		(vec![A; 16], true),
		(vec![], true),
	] {
		assert_eq!(contains(&mut formula, root, &word)?, inside, "{word:?}");
	}

	Ok(())
}

/// The state and time limits stop the initial set of a complement over 2^40 initial macrostates.
#[test]
fn limits_stop_a_complement_of_many_initial_macrostates() -> Result<(), Error> {
	let mut formula = Formula::new();
	let unions = unions_of_a_and_ab(&mut formula, 40)?;
	let lazy = formula.explicit_with(unions, ExplicitOptions::new().max_states(10))?;
	let root = formula.complement(lazy)?;
	let plain = Options::new().subsumption(false);

	let stopped = formula.check(root, &plain.clone().max_states(200_000));
	assert!(
		matches!(
			stopped,
			Err(Error::StateLimit {
				max_states: 200_000,
				..
			})
		),
		"{stopped:?}"
	);

	let limit = Duration::from_millis(300);
	let stopped = formula.check(root, &plain.time_limit(limit));
	assert!(
		matches!(stopped, Err(Error::TimeLimit { limit: reported, .. }) if reported == limit),
		"{stopped:?}"
	);

	Ok(())
}

/// An empty operand of a product leaves it no initial macrostate, whatever the other operand has.
#[test]
fn an_empty_operand_hides_a_large_one() -> Result<(), Error> {
	let mut formula = Formula::new();
	let unions = unions_of_a_and_ab(&mut formula, 40)?;
	let empty = formula.leaf(&nfa(1, &[0], &[], &[]))?;
	let nothing = formula.intersection(empty, unions)?;
	let b = formula.leaf(&just(&[B]))?;
	let root = formula.union(nothing, b)?;

	let witness = formula.find_word(root)?.expect("the word b");
	assert_eq!(word_of(&witness), [B]);
	assert!(formula.is_empty(nothing)?);

	let negated = formula.complement(nothing)?;
	assert!(!formula.is_empty(negated)?);

	Ok(())
}

/// An explicit node over 2^17 initial macrostates, without a budget, is built with the same language.
#[test]
fn an_explicit_node_over_many_initial_macrostates_is_exact() -> Result<(), Error> {
	let mut formula = Formula::new();
	let all = every_word(&mut formula, 17)?;
	let explicit = formula.explicit(all)?;

	let outcome = formula.check(explicit, &Options::new().subsumption(false))?;
	assert!(!outcome.stats.explicit[0].lazy);
	assert_eq!(outcome.witness.map(|witness| witness.len()), Some(17));

	let alternating = [B, A].repeat(8).into_iter().chain([B]).collect();
	for (word, inside) in [
		(vec![A; 17], true),
		(alternating, true),
		(vec![A; 16], false),
	] {
		assert_eq!(contains(&mut formula, explicit, &word)?, inside, "{word:?}");
	}

	Ok(())
}

/// A witness through an initial macrostate that the search reached from an earlier one is a word of the relation.
#[test]
fn initial_macrostates_reached_from_others_give_true_witnesses() -> Result<(), Error> {
	const K: u32 = 300;

	// Every state initial, i -a-> i + 1, and the last reads b into the final state.
	let initial: Vec<u32> = (0..K).collect();
	let mut moves: Vec<(u32, Symbol, u32)> =
		(0..K - 1).map(|state| (state, A, state + 1)).collect();
	moves.push((K - 1, B, K));
	let chain = nfa(K + 1, &initial, &[K], &moves);

	let mut formula = Formula::new();
	let left = formula.leaf(&chain)?;
	let right = formula.leaf(&chain)?;
	let wider = formula.leaf(&nfa(K + 2, &initial, &[K], &moves))?;
	let pair = formula.intersection(left, right)?;
	let triple = formula.intersection(pair, wider)?;

	for term in [pair, triple] {
		for subsumption in [true, false] {
			let outcome = formula.check(term, &Options::new().subsumption(subsumption))?;
			let word = word_of(&outcome.witness.expect("a^i b for each i below K"));
			let (last, prefix) = word.split_last().expect("at least b");
			assert_eq!(*last, B);
			assert!(prefix.len() < K as usize && prefix.iter().all(|&letter| letter == A));
			assert!(contains(&mut formula, term, &word)?);
		}
	}

	Ok(())
}

/// Queries over 2^17 initial macrostates from several threads give the sequential answer.
#[test]
fn parallel_queries_over_many_initial_macrostates_agree() -> Result<(), Error> {
	const THREADS: usize = 4;

	let mut formula = Formula::new();
	let all = every_word(&mut formula, 17)?;
	let all_b = formula.leaf(&just(&[B; 17]))?;
	let root = formula.intersection(all, all_b)?;
	let explicit = formula.explicit_with(all, ExplicitOptions::new().max_states(1000))?;
	let lazy = formula.intersection(explicit, all_b)?;
	let expected = formula
		.check(root, &Options::new().subsumption(false))?
		.witness;

	std::thread::scope(|scope| {
		let formula = &formula;
		let handles: Vec<_> = (0..THREADS)
			.map(|index| {
				scope.spawn(move || {
					let term = if index % 2 == 0 { root } else { lazy };
					let options = Options::new().subsumption(false).cache_budget(index * 4096);

					formula.check(term, &options).map(|outcome| outcome.witness)
				})
			})
			.collect();

		for handle in handles {
			let witness = handle.join().expect("a query does not panic");
			assert_eq!(witness, Ok(expected.clone()));
		}
	});

	Ok(())
}
