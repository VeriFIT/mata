//! Explicit nodes give the same answers, and their budgets, cache across queries and statistics work.

use lazymat::{Error, ExplicitOptions, Formula, Options, Symbol, Term, Witness};
use mata::{Nfa, State};

/// Words over {a, b} whose `n`-th letter from the end is `a`.
fn nth_from_end(n: u32) -> Nfa {
	let [a, b] = b"ab".map(mata::Symbol::from);
	let mut automaton = Nfa::with_states(n as usize + 1);
	automaton
		.add_initial(State::new(0))
		.expect("a state of the NFA");
	automaton
		.add_final(State::new(n))
		.expect("a state of the NFA");

	for (source, symbol, target) in [(0, a, 0), (0, b, 0), (0, a, 1)] {
		automaton
			.add_transition(State::new(source), symbol, State::new(target))
			.expect("a state of the NFA");
	}

	for state in 1..n {
		for symbol in [a, b] {
			automaton
				.add_transition(State::new(state), symbol, State::new(state + 1))
				.expect("a state of the NFA");
		}
	}

	automaton
}

struct Setup {
	formula: Formula,
	/// The words whose 2nd letter from the end is a.
	second: Term,
	/// The words whose 2nd and 3rd letters from the end are a.
	both: Term,
	/// The words whose 2nd letter from the end is a, and the 3rd is not (or is missing).
	root: Term,
}

fn setup() -> Result<Setup, Error> {
	let mut formula = Formula::new();
	let third = formula.leaf(&nth_from_end(3))?;
	let second = formula.leaf(&nth_from_end(2))?;
	let both = formula.intersection(third, second)?;
	let not_both = formula.complement(both)?;
	let root = formula.intersection(not_both, second)?;

	Ok(Setup {
		formula,
		second,
		both,
		root,
	})
}

fn in_root(witness: &Witness) -> bool {
	let word = witness.tape(0).expect("one tape");
	let from_end = |n: usize| word.len().checked_sub(n).map(|index| word[index]);
	let a = Symbol::from(b'a');

	from_end(2) == Some(a) && from_end(3) != Some(a)
}

#[test]
fn explicit_wrapper_keeps_the_answer() -> Result<(), Error> {
	let Setup {
		mut formula,
		second,
		both,
		root,
	} = setup()?;
	let witness = formula.find_word(root)?.expect("root is not empty");
	assert!(in_root(&witness));

	let explicit = formula.explicit(both)?;
	let not_explicit = formula.complement(explicit)?;
	let root = formula.intersection(not_explicit, second)?;
	let outcome = formula.check(root, &Options::new())?;
	let witness = outcome.witness.as_ref().expect("root is not empty");
	assert!(in_root(witness));

	let [materialized] = outcome.stats.explicit.as_slice() else {
		panic!("one explicit node: {:?}", outcome.stats.explicit)
	};

	assert_eq!(materialized.term, explicit);
	assert!(!materialized.cached);
	assert!(!materialized.lazy);
	assert!(materialized.states > 0);

	Ok(())
}

#[test]
fn explicit_nodes_are_reused_by_later_queries() -> Result<(), Error> {
	let Setup {
		formula,
		both,
		root,
		..
	} = setup()?;
	let options = Options::new().explicit(both);

	let first = formula.check(root, &options)?;
	let second = formula.check(root, &options)?;
	assert!(!first.stats.explicit[0].cached);
	assert!(second.stats.explicit[0].cached);
	assert_eq!(first.is_empty(), second.is_empty());

	// Another alphabet gives another complement, so the cache does not apply.
	let wider = formula.check(root, &options.clone().alphabet(*b"c"))?;
	assert!(!wider.stats.explicit[0].cached);

	formula.clear_cache();
	assert!(!formula.check(root, &options)?.stats.explicit[0].cached);

	Ok(())
}

#[test]
fn explicit_nodes_over_budget_stay_lazy() -> Result<(), Error> {
	let Setup {
		mut formula,
		second,
		both,
		..
	} = setup()?;
	let small = formula.explicit_with(both, ExplicitOptions::new().max_states(2))?;
	let not_small = formula.complement(small)?;
	let root = formula.intersection(not_small, second)?;

	let outcome = formula.check(root, &Options::new())?;
	assert!(outcome.stats.explicit[0].lazy);
	assert!(in_root(&outcome.witness.expect("root is not empty")));

	Ok(())
}

#[test]
fn explicit_complement_and_nested_explicit_nodes() -> Result<(), Error> {
	let Setup {
		mut formula,
		second,
		both,
		root,
	} = setup()?;
	let expected = formula.is_empty(root)?;

	let inner = formula.explicit(both)?;
	let not_inner = formula.complement(inner)?;
	let outer = formula.explicit(not_inner)?;
	let nested = formula.intersection(outer, second)?;

	let outcome = formula.check(nested, &Options::new())?;
	assert_eq!(outcome.is_empty(), expected);
	assert_eq!(outcome.stats.explicit.len(), 2);

	Ok(())
}

#[test]
fn limits_stop_the_query() -> Result<(), Error> {
	let Setup { formula, root, .. } = setup()?;

	let stopped = formula.check(root, &Options::new().max_states(3));
	assert!(
		matches!(&stopped, Err(Error::StateLimit { max_states: 3, stats, .. }) if stats.macrostates > 3),
		"{stopped:?}"
	);
	formula.check(root, &Options::new().max_states(10_000))?;

	Ok(())
}

#[test]
fn queries_run_in_parallel_on_one_formula() -> Result<(), Error> {
	const THREADS: usize = 4;

	fn assert_send_sync<T: Send + Sync>(_: &T) {}

	let Setup {
		formula,
		both,
		root,
		..
	} = setup()?;
	assert_send_sync(&formula);
	let expected = formula.is_empty(root)?;

	std::thread::scope(|scope| {
		let formula = &formula;
		let handles: Vec<_> = (0..THREADS)
			.map(|index| {
				scope.spawn(move || {
					let options = if index % 2 == 0 {
						Options::new().explicit(both)
					} else {
						Options::new()
					};

					formula
						.check(root, &options)
						.map(|outcome| outcome.is_empty())
				})
			})
			.collect();

		for handle in handles {
			let answer = handle.join().expect("a query does not panic");
			assert_eq!(answer, Ok(expected));
		}
	});

	Ok(())
}
