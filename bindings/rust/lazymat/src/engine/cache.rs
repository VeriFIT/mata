//! Which cached posts to drop over [`cache_budget`](crate::Options::cache_budget), by GreedyDual-Size with
//! frequency (Cherkasova, 1998). Priority is the floor plus compute time per byte times reads since cached; the
//! lowest goes and becomes the floor.

use std::cmp::Reverse;
use std::collections::BinaryHeap;

use rustc_hash::FxHashMap;

use crate::ids::StateId;

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

/// The cached entries by key and state; a key is a node, or the rows of a dense complement.
#[derive(Debug)]
pub(crate) struct CacheBudget {
	limit: usize,
	bytes: usize,
	floor: u64,
	entries: Vec<FxHashMap<StateId, Entry>>,
	/// `(priority, key, state, version)` of every push, lowest priority first; a replaced push is skipped.
	queue: BinaryHeap<Reverse<(u64, usize, StateId, u64)>>,
	version: u64,
	pub(crate) dropped: usize,
}

impl CacheBudget {
	pub(crate) fn new(limit: usize, num_keys: usize) -> CacheBudget {
		CacheBudget {
			limit,
			bytes: 0,
			floor: 0,
			entries: vec![FxHashMap::default(); num_keys],
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
	}

	/// Records an entry just cached; returns the cached entries to drop, which may include it.
	pub(crate) fn admit(
		&mut self,
		key: usize,
		state: StateId,
		nanos: u64,
		bytes: usize,
	) -> Vec<(usize, StateId)> {
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
		self.entries[key].insert(state, entry);
		self.bytes += bytes;
		self.queue
			.push(Reverse((priority, key, state, self.version)));
		self.compact();

		let mut dropped = Vec::new();
		while self.bytes > self.limit
			&& let Some(Reverse((priority, key, state, version))) = self.queue.pop()
		{
			if let Some(entry) = self.entries[key].get(&state)
				&& entry.version == version
			{
				self.bytes -= entry.bytes;
				self.entries[key].remove(&state);
				self.floor = priority;
				dropped.push((key, state));
			}
		}

		self.dropped += dropped.len();

		dropped
	}

	/// Forgets an entry that its owner dropped by itself.
	pub(crate) fn remove(&mut self, key: usize, state: StateId) {
		if let Some(entry) = self.entries[key].remove(&state) {
			self.bytes -= entry.bytes;
		}
	}

	/// Rebuilds the queue from the entries once most of its pushes are replaced ones.
	fn compact(&mut self) {
		let live: usize = self.entries.iter().map(FxHashMap::len).sum();
		if self.queue.len() > 4 * live + 1024 {
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
