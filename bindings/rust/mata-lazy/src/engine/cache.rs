//! Which cached posts to drop over [`cache_budget`](crate::Options::cache_budget), by GreedyDual-Size with
//! frequency (Cherkasova, 1998). Priority is the floor plus compute time per byte times reads since cached; the
//! lowest goes and becomes the floor.

use std::cmp::Reverse;
use std::collections::BinaryHeap;
use std::rc::{Rc, Weak};

use rustc_hash::{FxHashMap, FxHashSet};

use super::post::{Post, TapeLabel};
use crate::Symbol;
use crate::ids::StateId;

#[derive(Debug)]
struct SymbolArray {
	reference: Weak<[Symbol]>,
	bytes: usize,
}

/// The allocation's address, not the address of the `Rc` handle (a slice is non-null).
fn array_id(symbols: &Rc<[Symbol]>) -> usize {
	Rc::as_ptr(symbols).cast::<Symbol>() as usize
}

fn array_bytes(len: usize) -> usize {
	2 * size_of::<usize>() + len * size_of::<Symbol>()
}

#[derive(Clone, Copy, Debug)]
struct Entry {
	priority: u64,
	/// Nanoseconds of computing per 1024 bytes.
	density: u64,
	bytes: usize,
	/// Which push of the entry into the queue is current.
	version: u64,
	/// Requests since it was cached, the first included.
	reads: u64,
}

/// The cached entries by node and state.
#[derive(Debug)]
pub(crate) struct CacheBudget {
	limit: usize,
	/// Post handles and targets. Distinct symbol arrays are in `symbol_bytes`.
	bytes: usize,
	symbol_bytes: usize,
	alphabet_arrays: FxHashSet<usize>,
	shared_arrays: FxHashMap<usize, SymbolArray>,
	/// Weak references from evicted posts, reclaimed once no other reference keeps the payload alive.
	retired_arrays: FxHashSet<usize>,
	floor: u64,
	entries: Vec<FxHashMap<StateId, Entry>>,
	/// How many entries are cached.
	live: usize,
	/// `(priority, key, state, version)` of every push, lowest priority first; a replaced push is skipped.
	queue: BinaryHeap<Reverse<(u64, usize, StateId, u64)>>,
	version: u64,
	pub(crate) dropped: usize,
}

impl CacheBudget {
	pub(crate) fn new(limit: usize, num_keys: usize, alphabets: &[Rc<[Symbol]>]) -> CacheBudget {
		CacheBudget {
			limit,
			bytes: 0,
			symbol_bytes: 0,
			alphabet_arrays: alphabets.iter().map(array_id).collect(),
			shared_arrays: FxHashMap::default(),
			retired_arrays: FxHashSet::default(),
			floor: 0,
			entries: vec![FxHashMap::default(); num_keys],
			live: 0,
			queue: BinaryHeap::new(),
			version: 0,
			dropped: 0,
		}
	}

	/// Renews the priority of a cached entry that is read again.
	pub(crate) fn touch(&mut self, key: usize, state: StateId) {
		let Some(entry) = self.entries[key].get_mut(&state) else {
			return;
		};

		entry.reads += 1;
		entry.priority = self
			.floor
			.saturating_add(entry.density.saturating_mul(entry.reads));
		self.version += 1;
		entry.version = self.version;
		self.queue
			.push(Reverse((entry.priority, key, state, entry.version)));
		self.compact();
	}

	/// Post handles plus every distinct symbolic array retained by admitted posts, but not alphabet storage.
	pub(super) fn bytes(&self) -> usize {
		self.bytes.saturating_add(self.symbol_bytes)
	}

	fn charge(&mut self, post: &Rc<Post>) {
		for (labels, _) in post.iter() {
			for label in labels {
				let TapeLabel::Set(symbols) = label else {
					continue;
				};
				if symbols.is_empty() || self.alphabet_arrays.contains(&array_id(symbols)) {
					continue;
				}

				let id = array_id(symbols);
				if self
					.shared_arrays
					.get(&id)
					.is_some_and(|array| array.reference.strong_count() != 0)
				{
					continue;
				}

				let bytes = array_bytes(symbols.len());
				let previous = self.shared_arrays.insert(
					id,
					SymbolArray {
						reference: Rc::downgrade(symbols),
						bytes,
					},
				);
				if let Some(previous) = previous {
					self.symbol_bytes -= previous.bytes;
				}
				self.symbol_bytes += bytes;
			}
		}
	}

	/// Records an entry just cached; returns the cached entries to drop, which may include it.
	pub(crate) fn admit(
		&mut self,
		key: usize,
		state: StateId,
		nanos: u64,
		post: &Rc<Post>,
	) -> Vec<(usize, StateId)> {
		self.reclaim();
		self.charge(post);
		let bytes = post.bytes();
		let density = u128::from(nanos) * 1024 / u128::try_from(bytes.max(1)).unwrap_or(u128::MAX);
		let density = u64::try_from(density).unwrap_or(u64::MAX);
		let priority = self.floor.saturating_add(density);

		self.version += 1;
		let entry = Entry {
			priority,
			density,
			bytes,
			version: self.version,
			reads: 1,
		};
		let replaced = self.entries[key].insert(state, entry);
		if replaced.is_none() {
			self.live += 1;
		}

		self.bytes += bytes;
		self.queue
			.push(Reverse((priority, key, state, self.version)));
		self.compact();

		let mut dropped = Vec::new();
		while self.bytes() > self.limit
			&& let Some(Reverse((priority, key, state, version))) = self.queue.pop()
		{
			if let Some(entry) = self.entries[key].get(&state)
				&& entry.version == version
			{
				self.bytes -= entry.bytes;
				self.entries[key].remove(&state);
				self.live -= 1;
				self.floor = priority;
				dropped.push((key, state));
			}
		}

		self.dropped += dropped.len();

		dropped
	}

	/// Call before dropping an evicted post; the caller then drops it and calls [`reclaim`](Self::reclaim).
	pub(super) fn retire(&mut self, post: &Post) {
		for (labels, _) in post.iter() {
			for label in labels {
				if let TapeLabel::Set(symbols) = label {
					self.retired_arrays.insert(array_id(symbols));
				}
			}
		}
	}

	pub(super) fn reclaim(&mut self) {
		self.retired_arrays.retain(|id| {
			let id = *id;
			let Some(array) = self.shared_arrays.get(&id) else {
				return false;
			};
			if array.reference.strong_count() != 0 {
				return true;
			}

			self.symbol_bytes -= array.bytes;
			self.shared_arrays.remove(&id);
			false
		});
	}

	/// Rebuilds the queue from the entries once most of its pushes are replaced ones.
	fn compact(&mut self) {
		if self.queue.len() > 4 * self.live + 1024 {
			self.queue = self
				.entries
				.iter()
				.enumerate()
				.flat_map(|(key, entries)| {
					entries.iter().map(move |(&state, entry)| {
						Reverse((entry.priority, key, state, entry.version))
					})
				})
				.collect();
		}
	}
}

#[cfg(test)]
mod tests {
	use super::super::post::{Post, TapeLabel, Target};
	use super::CacheBudget;
	use crate::Symbol;
	use crate::ids::{Id, StateId};
	use std::rc::Rc;

	fn symbolic_post(symbols: &Rc<[Symbol]>) -> Rc<Post> {
		let mut post = Post::with_capacity(1, 1);
		post.push_labels(
			&[TapeLabel::Set(Rc::clone(symbols))],
			Target {
				state: StateId::new(0),
				accepting: false,
			},
		);

		Rc::new(post)
	}

	#[test]
	fn reads_without_new_entries_keep_the_queue_small() {
		let mut budget = CacheBudget::new(1 << 20, 1, &[]);
		let state = StateId::new(0);
		budget.admit(0, state, 1000, &Rc::new(Post::new(1)));
		for _ in 0..100_000 {
			budget.touch(0, state);
		}

		assert!(budget.queue.len() <= 4 * budget.live + 1024);
	}

	#[test]
	fn shared_symbol_arrays_are_charged_once_and_reclaimed() {
		let symbols: Rc<[Symbol]> = [Symbol::new(0), Symbol::new(1)].into();
		let mut posts = [
			Some(symbolic_post(&symbols)),
			Some(symbolic_post(&symbols)),
			Some(Rc::new(Post::new(1))),
		];
		drop(symbols);

		let mut budget = CacheBudget::new(usize::MAX, 3, &[]);
		budget.admit(
			0,
			StateId::new(0),
			0,
			posts[0].as_ref().expect("the first post"),
		);
		let post_bytes = posts[0].as_ref().expect("the first post").bytes();
		let first = budget.bytes();
		assert!(first > post_bytes);

		budget.admit(
			1,
			StateId::new(0),
			0,
			posts[1].as_ref().expect("the second post"),
		);
		assert_eq!(budget.bytes(), first + post_bytes);

		budget.limit = 0;
		let dropped = budget.admit(
			2,
			StateId::new(0),
			0,
			posts[2].as_ref().expect("the third post"),
		);
		assert_eq!(dropped.len(), 3);
		for (key, _) in dropped {
			budget.retire(posts[key].take().as_ref().expect("the evicted post"));
		}
		budget.reclaim();
		assert_eq!(budget.bytes(), 0);
	}

	#[test]
	fn alphabet_arrays_keep_their_separate_accounting() {
		let symbols: Rc<[Symbol]> = [Symbol::new(0), Symbol::new(1)].into();
		let post = symbolic_post(&symbols);
		let mut budget = CacheBudget::new(usize::MAX, 1, &[symbols]);

		budget.admit(0, StateId::new(0), 0, &post);

		assert_eq!(budget.bytes(), post.bytes());
	}
}
