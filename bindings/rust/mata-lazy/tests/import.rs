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
	let (leaf, peak) = peak_bytes(|| formula.leaf(&SharedMove { length }));
	assert!(peak < 1 << 22, "importing took {peak} bytes");

	let explicit = formula.materialize(leaf?, &Options::new())?;
	assert_eq!(explicit.num_moves(), length as usize);

	Ok(())
}
