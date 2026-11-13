//! Downward closures of the sets of complements over a leaf with a simulation.
//!
//! For complement sets `S ⊑ T` when every member of `T` is below a member of `S`,
//! that is, when `T` lies in the closure of `S`.
//! A closure is a bitset, the union of its members' rows of the transposed simulation,
//! so a comparison tests one bit per member of `T`.

use typed_index_collections::TiVec;

use super::store::Store;
use crate::ids::{ExecId, Id, StateId};
use crate::simulation::DownwardRows;

/// The most bytes that the closures of one query allocate, words and windows together.
const CLOSURES_LIMIT: usize = 64 << 20;

/// A closure's `len` words from `start` in the shared vector, which are the words from `offset` on of its bitset.
#[derive(Clone, Copy, Debug)]
struct Window {
	offset: u32,
	start: u32,
	len: u32,
}

/// A set of leaf states as the words of a bitset from word `offset` on, all words outside them zero.
#[derive(Clone, Copy, Debug)]
pub(crate) struct Closure<'a> {
	offset: usize,
	words: &'a [u64],
}

impl Closure<'_> {
	pub(crate) fn contains(self, state: StateId) -> bool {
		let index = state.index();

		(index / 64)
			.checked_sub(self.offset)
			.and_then(|word| self.words.get(word))
			.is_some_and(|&word| word >> (index % 64) & 1 != 0)
	}
}

#[derive(Debug)]
pub(crate) struct Closures {
	/// The words of every closure, one after another.
	words: Vec<u64>,
	/// Where the closure of each macrostate of each node is, once built.
	windows: TiVec<ExecId, Vec<Option<Window>>>,
	/// The bytes that `words` and `windows` allocate.
	bytes: usize,
	/// Working space for building a closure.
	scratch: Vec<u64>,
}

impl Closures {
	pub(crate) fn new(num_nodes: usize) -> Closures {
		Closures {
			words: Vec::new(),
			windows: vec![Vec::new(); num_nodes].into(),
			bytes: 0,
			scratch: Vec::new(),
		}
	}

	/// The closure of `state` of the complement `node`, built once; `None` if building it passes the limit.
	pub(crate) fn get_or_build(
		&mut self,
		node: ExecId,
		store: &Store,
		state: StateId,
		rows: DownwardRows<'_>,
	) -> Option<Closure<'_>> {
		let index = state.index();
		if let Some(&Some(window)) = self.windows[node].get(index) {
			return Some(self.view(window));
		}

		let set = store.set(state);
		self.scratch.clear();
		if let Some((&first, rest)) = set.split_first() {
			self.scratch.extend_from_slice(rows.row(first));
			for &member in rest {
				self.scratch
					.iter_mut()
					.zip(rows.row(member))
					.for_each(|(acc, &word)| *acc |= word);
			}
		}

		// `x ⊑ x` holds without the simulation, so the closure does not rely on the rows' diagonal.
		for &member in set {
			let member = member.index();
			self.scratch[member / 64] |= 1 << (member % 64);
		}

		let offset = self.scratch.iter().position(|&word| word != 0).unwrap_or(0);
		let end = self
			.scratch
			.iter()
			.rposition(|&word| word != 0)
			.map_or(offset, |last| last + 1);

		let (words_len, windows_len) = (self.words.len() + end - offset, index + 1);
		let more = room(&self.words, words_len) + room(&self.windows[node], windows_len);
		if self.bytes + more > CLOSURES_LIMIT {
			return None;
		}

		self.bytes += grow(&mut self.words, words_len) + grow(&mut self.windows[node], windows_len);
		let window = Window {
			offset: within_limit(offset),
			start: within_limit(self.words.len()),
			len: within_limit(end - offset),
		};
		self.words.extend_from_slice(&self.scratch[offset..end]);

		let windows = &mut self.windows[node];
		if windows.len() <= index {
			windows.resize(index + 1, None);
		}

		windows[index] = Some(window);

		Some(self.view(window))
	}

	fn view(&self, window: Window) -> Closure<'_> {
		let start = window.start as usize;

		Closure {
			offset: window.offset as usize,
			words: &self.words[start..start + window.len as usize],
		}
	}
}

/// The bytes that `vec` adds to hold `len` items, its capacity doubling as needed.
fn room<T>(vec: &Vec<T>, len: usize) -> usize {
	let capacity = vec.capacity();
	if len <= capacity {
		return 0;
	}

	(len.max(2 * capacity) - capacity) * size_of::<T>()
}

/// Makes `vec` hold `len` items as [`room`] says, and returns the bytes it added.
fn grow<T>(vec: &mut Vec<T>, len: usize) -> usize {
	let capacity = vec.capacity();
	if len > capacity {
		vec.reserve_exact(len.max(2 * capacity) - vec.len());
	}

	(vec.capacity() - capacity) * size_of::<T>()
}

/// A word index or count of the closures, which stay within [`CLOSURES_LIMIT`] bytes.
fn within_limit(value: usize) -> u32 {
	u32::try_from(value).expect("the closures stay within CLOSURES_LIMIT")
}

#[cfg(test)]
mod tests {
	use indexmap::IndexSet;
	use rand::rngs::StdRng;
	use rand::{RngExt, SeedableRng};

	use super::{CLOSURES_LIMIT, Closures};
	use crate::Symbol;
	use crate::engine::store::Store;
	use crate::ids::{ExecId, Id, StateId};
	use crate::leaf::{Leaf, Raw};
	use crate::simulation::Simulation;

	/// A random leaf of up to `size` states (fewer after trimming) over `letters` letters.
	fn random_leaf(rng: &mut StdRng, size: usize, letters: u32, moves: usize, finals: f64) -> Leaf {
		let mut raw = Raw::new(1);
		raw.initial.push(StateId::new(0));
		for _ in 0..size {
			raw.finals.push(rng.random_bool(finals));
		}

		for source in 0..size {
			for _ in 0..rng.random_range(0..=moves) {
				// Mostly forward, so that states differ and simulate each other in part.
				let target = if rng.random_bool(0.8) {
					rng.random_range(source..size.min(source + 8))
				} else {
					rng.random_range(0..size)
				};

				let letter = Symbol::new(rng.random_range(0..letters));
				raw.push(StateId::new(source), &[letter], StateId::new(target));
			}
		}

		Leaf::build(&raw, Vec::new())
	}

	/// A sorted set of states in a window of the leaf's states.
	fn random_set(rng: &mut StdRng, size: usize) -> Vec<StateId> {
		if size == 0 {
			return Vec::new();
		}

		let low = rng.random_range(0..size);
		let width = rng.random_range(1..400);
		let high = rng.random_range(low..size.min(low + width));
		let count = rng.random_range(0..=8);
		let mut set: Vec<StateId> = (0..count)
			.map(|_| StateId::new(rng.random_range(low..=high)))
			.collect();
		set.sort_unstable();
		set.dedup();

		set
	}

	/// The order of `subsumed` over a leaf, which is `x == y` or the simulation.
	fn below_some(simulation: &Simulation, set: &[StateId], x: StateId) -> bool {
		set.iter().any(|&y| x == y || simulation.le(x, y))
	}

	#[test]
	fn closures_hold_the_states_below_a_member() {
		let mut checked = 0_usize;
		let mut nontrivial = 0_usize;
		for seed in 0..300 {
			let mut rng = StdRng::seed_from_u64(seed);
			let sizes = [1, 2, 5, 63, 64, 65, 127, 128, 129, 200, 500, 1000, 3000];
			let size = sizes[rng.random_range(0..sizes.len())];
			let letters = rng.random_range(1..=3);
			let moves = rng.random_range(0..=3);
			let finals = [0.1, 0.5, 0.9][rng.random_range(0..3)];
			let leaf = random_leaf(&mut rng, size, letters, moves, finals);
			let size = leaf.num_states();
			let simulation = leaf.simulation(None).expect("no deadline");
			let Some(rows) = simulation.downward_rows() else {
				continue;
			};

			// A row holds exactly the states below its state.
			if size <= 600 {
				for p in 0..size {
					let row = rows.row(StateId::new(p));
					for q in 0..size {
						let bit = row[q / 64] >> (q % 64) & 1 != 0;
						assert_eq!(
							bit,
							simulation.le(StateId::new(q), StateId::new(p)),
							"seed {seed}"
						);
						nontrivial += usize::from(bit && p != q);
					}
				}
			}

			let mut store = Store::Set(IndexSet::default());
			let mut closures = Closures::new(1);
			let sets: Vec<StateId> = (0..60)
				.map(|_| store.intern_set(&random_set(&mut rng, size)))
				.collect();
			for &set in &sets {
				let closure = closures
					.get_or_build(ExecId::new(0), &store, set, rows)
					.expect("below the limit");
				// Every state, and states past the leaf (outside every window).
				for x in (0..size + 130).map(StateId::new) {
					let expected = x.index() < size && below_some(simulation, store.set(set), x);
					assert_eq!(closure.contains(x), expected, "seed {seed}: {x:?}");
					checked += 1;
				}
			}

			// Asked again, a closure decides the nested loop's relation.
			for _ in 0..400 {
				let (s, t) = (
					sets[rng.random_range(0..sets.len())],
					sets[rng.random_range(0..sets.len())],
				);
				let closure = closures
					.get_or_build(ExecId::new(0), &store, s, rows)
					.expect("built");
				let fast = store.set(t).iter().all(|&x| closure.contains(x));
				let slow = store
					.set(t)
					.iter()
					.all(|&x| below_some(simulation, store.set(s), x));
				assert_eq!(fast, slow, "seed {seed}");
			}
		}

		assert!(
			checked > 1_000_000 && nontrivial > 10_000,
			"{checked} {nontrivial}"
		);
	}

	/// The limit holds for the closures of all nodes together; past it, closures are refused to every node,
	/// and the built ones stay.
	#[test]
	fn closures_stop_at_the_limit_of_the_query() {
		// Every state final with a loop on its own letter, and the last state with all loops. Each state is below
		// only itself and the last one, so a set with the last state has a closure of all words.
		let size = 1 << 14;
		let top = StateId::new(size - 1);
		let mut raw = Raw::new(1);
		raw.initial.push(StateId::new(0));
		raw.finals = vec![true; size];
		for state in (0..size).map(StateId::new) {
			let letter = Symbol::new(u32::from(state));
			raw.push(state, &[letter], state);
			raw.push(top, &[letter], top);
		}

		let leaf = Leaf::build(&raw, Vec::new());
		let simulation = leaf.simulation(None).expect("no deadline");
		let rows = simulation.downward_rows().expect("a relation of few pairs");

		let mut store = Store::Set(IndexSet::default());
		let sets: Vec<StateId> = (0..size - 1)
			.flat_map(|x| [vec![x, size - 1], vec![x, (x + 1) % (size - 1), size - 1]])
			.map(|mut set| {
				set.sort_unstable();
				set.dedup();
				let set: Vec<StateId> = set.into_iter().map(StateId::new).collect();

				store.intern_set(&set)
			})
			.collect();

		let mut closures = Closures::new(2);
		let built: Vec<bool> = sets
			.iter()
			.enumerate()
			.map(|(index, &set)| {
				let built = closures
					.get_or_build(ExecId::new(index % 2), &store, set, rows)
					.is_some();
				assert!(closures.bytes <= CLOSURES_LIMIT, "{} bytes", closures.bytes);

				built
			})
			.collect();

		let refused: Vec<usize> = (0..sets.len()).filter(|&index| !built[index]).collect();
		assert!(
			refused.iter().any(|index| index % 2 == 0)
				&& refused.iter().any(|index| index % 2 == 1)
		);
		assert!(refused[0] > 1000, "refused from {}", refused[0]);
		for (index, &set) in sets.iter().enumerate().filter(|&(index, _)| built[index]) {
			let closure = closures
				.get_or_build(ExecId::new(index % 2), &store, set, rows)
				.expect("built before the limit");
			assert!(closure.contains(StateId::new(0)) && closure.contains(top));
		}
	}
}
