//! Query limits stop inside one post, a leaf simulation and a minimization, and report the statistics so far.

use std::time::{Duration, Instant};

use lazymat::{Automaton, Error, Formula, Options, Symbol, Term};

/// The words over {0, 1} with a 1, guessing it. State 0 loops on both and moves to 1 on 1, where both loop.
struct HasAOne;

impl Automaton for HasAOne {
	type State = u32;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u32> {
		[0]
	}

	fn is_final(&self, state: u32) -> bool {
		state == 1
	}

	fn for_each_move(&self, state: u32, mut visit: impl FnMut(&[Symbol], u32)) {
		for symbol in [Symbol::new(0), Symbol::new(1)] {
			visit(&[symbol], state);
		}

		if state == 0 {
			visit(&[Symbol::new(1)], 1);
		}
	}
}

/// The intersection of `copies` leaves of [`HasAOne`], whose first successors on 1 are `2^copies` macrostates.
fn blowup(copies: usize) -> Result<(Formula, Term), Error> {
	let mut formula = Formula::new();
	let mut root = formula.leaf(&HasAOne)?;
	for _ in 1..copies {
		let copy = formula.leaf(&HasAOne)?;
		root = formula.intersection(root, copy)?;
	}

	Ok((formula, root))
}

/// States `0..size`, where `i` moves to `i + 1` on 0 and to `2i` on 1 (modulo `size`), and the states not
/// `rejecting` are final.
struct Doubling {
	size: u32,
	initial: &'static [u32],
	rejecting: &'static [u32],
}

impl Automaton for Doubling {
	type State = u32;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u32> {
		self.initial.iter().copied()
	}

	fn is_final(&self, state: u32) -> bool {
		!self.rejecting.contains(&state)
	}

	fn for_each_move(&self, state: u32, mut visit: impl FnMut(&[Symbol], u32)) {
		visit(&[Symbol::new(0)], (state + 1) % self.size);
		visit(&[Symbol::new(1)], (2 * state) % self.size);
	}
}

/// Starts `starts..2 * starts`, each reading its own bits down to the final state 1, so no start is below another.
struct Bits {
	starts: u32,
}

impl Automaton for Bits {
	type State = u32;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u32> {
		self.starts..2 * self.starts
	}

	fn is_final(&self, state: u32) -> bool {
		state == 1
	}

	fn for_each_move(&self, state: u32, mut visit: impl FnMut(&[Symbol], u32)) {
		if state > 1 {
			visit(&[Symbol::new(state % 2)], state / 2);
		}
	}
}

#[test]
fn a_query_that_answers_holds_at_most_max_states() -> Result<(), Error> {
	let (formula, root) = blowup(16)?;
	for subsumption in [true, false] {
		let options = Options::new().subsumption(subsumption);
		let reference = formula.check(root, &options)?;
		let needed = reference.stats.macrostates;
		assert!(needed > 1000);

		let enough = formula.check(root, &options.clone().max_states(needed))?;
		assert_eq!(enough, reference);

		match formula.check(root, &options.max_states(1000)) {
			// One post holds most of them, so the query stops inside it, a few macrostates past the limit.
			Err(Error::StateLimit {
				max_states: 1000,
				stats,
				..
			}) => assert!((1001..1100).contains(&stats.macrostates), "{stats:?}"),
			other => panic!("{other:?}"),
		}
	}

	Ok(())
}

#[test]
fn a_time_limit_stops_one_large_post() -> Result<(), Error> {
	let (formula, root) = blowup(20)?;
	let limit = Duration::from_millis(50);

	let start = Instant::now();
	match formula.check(root, &Options::new().time_limit(limit)) {
		Err(Error::TimeLimit {
			limit: reported,
			stats,
			..
		}) => {
			assert_eq!(reported, limit);
			assert!(stats.macrostates > 0);
		}
		other => panic!("{other:?}"),
	}

	assert!(start.elapsed() < Duration::from_secs(5));

	Ok(())
}

#[test]
fn a_time_limit_stops_a_leaf_simulation_that_a_later_query_computes() -> Result<(), Error> {
	let mut formula = Formula::new();
	let leaf = formula.leaf(&Doubling {
		size: 4000,
		initial: &[1],
		rejecting: &[0],
	})?;
	let complement = formula.complement(leaf)?;

	// The search compares complement sets of the leaf's states, which needs its simulation.
	let short = Options::new().time_limit(Duration::from_millis(1));
	let start = Instant::now();
	let stopped = formula.check(complement, &short);
	let stopped_after = start.elapsed();
	assert!(
		matches!(stopped, Err(Error::TimeLimit { .. })),
		"{stopped:?}"
	);

	let start = Instant::now();
	let answer = formula.check(complement, &Options::new())?;
	assert!(!answer.is_empty());
	// The first query stopped well before the simulation would have been done.
	assert!(stopped_after < start.elapsed() / 4);

	let long = Options::new().time_limit(Duration::from_secs(3600));
	assert_eq!(formula.check(complement, &long)?, answer);

	Ok(())
}

#[test]
fn a_query_whose_simulation_passes_the_deadline_does_not_answer() -> Result<(), Error> {
	// The second start is compared with the first, which needs the simulation; the third accepts.
	let three_starts = Doubling {
		size: 6000,
		initial: &[0, 2, 1],
		rejecting: &[0, 2],
	};
	let mut formula = Formula::new();
	let leaf = formula.leaf(&three_starts)?;

	let limited = formula.check(leaf, &Options::new().time_limit(Duration::from_millis(1)));
	assert!(
		matches!(limited, Err(Error::TimeLimit { .. })),
		"{limited:?}"
	);
	assert!(!formula.check(leaf, &Options::new())?.is_empty());

	Ok(())
}

#[test]
fn a_time_limit_stops_the_minimization_of_a_large_set() -> Result<(), Error> {
	let mut formula = Formula::new();
	let leaf = formula.leaf(&Bits { starts: 1 << 16 })?;
	let complement = formula.complement(leaf)?;

	// The complement's initial set holds every start, so minimizing it compares every pair of them.
	let limit = Duration::from_millis(50);
	let start = Instant::now();
	match formula.check(complement, &Options::new().time_limit(limit)) {
		Err(Error::TimeLimit {
			limit: reported, ..
		}) => assert_eq!(reported, limit),
		other => panic!("{other:?}"),
	}

	assert!(start.elapsed() < Duration::from_secs(2));

	Ok(())
}
