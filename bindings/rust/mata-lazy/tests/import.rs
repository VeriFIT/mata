//! Importing a leaf removes its silent moves without holding the moves that many closures share once per closure.

use std::alloc::{GlobalAlloc, Layout, System};
use std::sync::atomic::{AtomicUsize, Ordering};

use mata_lazy::{Automaton, Error, Formula, Options, Symbol};

/// The system allocator, counting the bytes allocated and their peak.
struct Counting;

static ALLOCATED: AtomicUsize = AtomicUsize::new(0);
static PEAK: AtomicUsize = AtomicUsize::new(0);

#[global_allocator]
static COUNTING: Counting = Counting;

// SAFETY: forwards every call to `System` and only counts.
unsafe impl GlobalAlloc for Counting {
	unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
		// SAFETY: the caller keeps the contract of `alloc`, which is the same for `System`.
		let pointer = unsafe { System.alloc(layout) };
		if !pointer.is_null() {
			let allocated = ALLOCATED.fetch_add(layout.size(), Ordering::Relaxed) + layout.size();
			PEAK.fetch_max(allocated, Ordering::Relaxed);
		}

		pointer
	}

	unsafe fn dealloc(&self, pointer: *mut u8, layout: Layout) {
		// SAFETY: `alloc` above took `pointer` from `System` with `layout`.
		unsafe { System.dealloc(pointer, layout) };
		ALLOCATED.fetch_sub(layout.size(), Ordering::Relaxed);
	}
}

/// The peak of the bytes that `run` allocates and keeps at once.
fn peak_bytes<T>(run: impl FnOnce() -> T) -> (T, usize) {
	let before = ALLOCATED.load(Ordering::Relaxed);
	PEAK.store(before, Ordering::Relaxed);
	let result = run();

	(result, PEAK.load(Ordering::Relaxed) - before)
}

/// States `0..length`, all initial, where `i` moves silently to `i + 1` and on `a` to the final state `length`.
struct SharedMove {
	length: u32,
	/// The calls of `for_each_move` so far.
	calls: AtomicUsize,
}

impl SharedMove {
	fn new(length: u32) -> SharedMove {
		SharedMove {
			length,
			calls: AtomicUsize::new(0),
		}
	}
}

impl Automaton for SharedMove {
	type State = u32;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u32> {
		0..self.length
	}

	fn is_final(&self, state: u32) -> bool {
		state == self.length
	}

	fn for_each_move(&self, state: u32, mut visit: impl FnMut(&[Symbol], u32)) {
		self.calls.fetch_add(1, Ordering::Relaxed);
		if state < self.length {
			visit(&[Symbol::from(b'a')], self.length);
		}

		if state + 1 < self.length {
			visit(&[Symbol::EPSILON], state + 1);
		}
	}
}

/// State `i` inherits the move of every later state, `length^2 / 2` moves (2 million here) unless each closure keeps
/// one copy of it.
#[test]
fn a_move_that_many_closures_share_is_held_once_per_closure() -> Result<(), Error> {
	let length = 2000;
	let mut formula = Formula::new();
	let (leaf, peak) = peak_bytes(|| formula.leaf(&SharedMove::new(length)));
	assert!(peak < 1 << 22, "importing took {peak} bytes");

	let explicit = formula.materialize(leaf?, &Options::new())?;
	assert_eq!(explicit.num_moves(), length as usize);

	Ok(())
}

/// Every state is a source that silently reaches all later ones; walking each closure to its end would read the moves
/// of a state `length^2 / 2` times (2 million here) instead of once.
#[test]
fn a_chain_of_silent_moves_reads_each_state_once() -> Result<(), Error> {
	let length = 2000;
	let automaton = SharedMove::new(length);
	let mut formula = Formula::new();
	let leaf = formula.leaf(&automaton)?;
	assert_eq!(automaton.calls.load(Ordering::Relaxed), length as usize + 1);

	let explicit = formula.materialize(leaf, &Options::new())?;
	assert_eq!(explicit.num_moves(), length as usize);

	Ok(())
}

/// States `0..3`, all initial, on a silent cycle; `i` reads `b'a' + i` to the final state 3.
struct SilentCycle;

impl Automaton for SilentCycle {
	type State = u8;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u8> {
		0..3
	}

	fn is_final(&self, state: u8) -> bool {
		state == 3
	}

	fn for_each_move(&self, state: u8, mut visit: impl FnMut(&[Symbol], u8)) {
		if state < 3 {
			visit(&[Symbol::from(b'a' + state)], 3);
			visit(&[Symbol::EPSILON], (state + 1) % 3);
		}
	}
}

/// A source that the walk of another one reaches while that walk is still open gets the moves of the whole cycle.
#[test]
fn every_state_of_a_silent_cycle_reads_the_moves_of_all_of_it() -> Result<(), Error> {
	let mut formula = Formula::new();
	let leaf = formula.leaf(&SilentCycle)?;
	let explicit = formula.materialize(leaf, &Options::new())?;
	assert_eq!(explicit.num_moves(), 9);

	Ok(())
}

/// One move, on [`ANY`](Symbol::ANY), from the initial state 0 to the final state 1.
struct Wildcard;

impl Automaton for Wildcard {
	type State = u8;

	fn arity(&self) -> usize {
		1
	}

	fn initial_states(&self) -> impl IntoIterator<Item = u8> {
		[0]
	}

	fn is_final(&self, state: u8) -> bool {
		state == 1
	}

	fn for_each_move(&self, state: u8, mut visit: impl FnMut(&[Symbol], u8)) {
		if state == 0 {
			visit(&[Symbol::ANY], 1);
		}
	}
}

/// The wildcard of a materialized term still reads the symbols of its tape once the term is a leaf again.
#[test]
fn a_materialized_wildcard_reads_its_alphabet_when_imported_again() -> Result<(), Error> {
	let mut formula = Formula::new();
	let leaf = formula.leaf_over(&Wildcard, [*b"a"])?;
	assert!(!formula.is_empty(leaf)?);
	let explicit = formula.materialize(leaf, &Options::new())?;

	let mut again = Formula::new();
	let leaf = again.leaf(&explicit)?;
	let witness = again.find_word(leaf)?.expect("the leaf reads `a`");
	assert_eq!(witness.tape(0), Some(vec![Symbol::from(b'a')]));

	Ok(())
}

/// An explicit node over a wildcard, built by one query and taken from the cache by the next.
#[test]
fn a_cached_explicit_wildcard_reads_its_alphabet() -> Result<(), Error> {
	let mut formula = Formula::new();
	let leaf = formula.leaf_over(&Wildcard, [*b"a"])?;
	let explicit = formula.explicit(leaf)?;
	assert!(!formula.is_empty(explicit)?);
	assert!(!formula.is_empty(explicit)?);
	let not = formula.complement(explicit)?;
	assert!(!formula.is_empty(not)?);

	Ok(())
}
