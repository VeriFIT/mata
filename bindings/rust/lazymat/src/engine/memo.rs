//! The results of `s1 ⊑ s2` that one node has computed, two bits per pair (known, result).
//!
//! Macrostates are numbered densely. While both are below [`DIRECTORY_MAX`], the bits are in blocks of 32 × 32
//! pairs found by index in a directory. Other pairs are compared sparsely, so they are in blocks of 4 × 4 pairs
//! in a hash map, and nothing moves when a node passes the bound.

use rustc_hash::FxHashMap;

use crate::ids::{Id, StateId};

/// Below this many macrostates, the directory of blocks takes at most 32 MiB.
const DIRECTORY_MAX: usize = 1 << 16;

/// The side of a directory block.
const BLOCK: usize = 32;
/// Words of a block, two bits for each of its pairs.
const BLOCK_WORDS: usize = BLOCK * BLOCK * 2 / 64;

/// The side of a hash map block; it divides [`DIRECTORY_MAX`], so no such block holds a directory pair.
const FAR_BLOCK: usize = 4;

type Block = [u64; BLOCK_WORDS];

/// Where the bits of a pair are in the directory, as its block's row and column and the word and shift in it.
#[derive(Clone, Copy, Debug)]
struct Place {
	row: usize,
	col: usize,
	word: usize,
	shift: usize,
}

impl Place {
	fn of(s1: StateId, s2: StateId) -> Place {
		let (s1, s2) = (s1.index(), s2.index());
		let bit = ((s1 % BLOCK) * BLOCK + s2 % BLOCK) * 2;

		Place {
			row: s1 / BLOCK,
			col: s2 / BLOCK,
			word: bit / 64,
			shift: bit % 64,
		}
	}
}

/// The block of a pair in the hash map, keyed by its first pair, and the shift of the pair's bits in it.
fn far_place(s1: StateId, s2: StateId) -> ((StateId, StateId), usize) {
	let (s1, s2) = (s1.index(), s2.index());
	let first = |state: usize| StateId::new(state - state % FAR_BLOCK);
	let shift = ((s1 % FAR_BLOCK) * FAR_BLOCK + s2 % FAR_BLOCK) * 2;

	((first(s1), first(s2)), shift)
}

/// The two bits of a result.
fn bits(result: bool) -> u8 {
	1 | u8::from(result) << 1
}

/// The result in the two bits at the bottom of `bits`, if known.
fn result(bits: u64) -> Option<bool> {
	(bits & 1 != 0).then_some(bits & 2 != 0)
}

#[derive(Debug, Default)]
pub(crate) struct Memo {
	/// Blocks per row and per column of the directory.
	span: usize,
	directory: Vec<Option<Box<Block>>>,
	/// The blocks of pairs with a macrostate from [`DIRECTORY_MAX`] on.
	far: FxHashMap<(StateId, StateId), u32>,
}

impl Memo {
	// Called for every comparison of a node, so kept inline in `subsumed`.
	#[inline]
	pub(crate) fn get(&self, s1: StateId, s2: StateId) -> Option<bool> {
		let place = Place::of(s1, s2);
		if place.row < self.span && place.col < self.span {
			let block = self.directory[place.row * self.span + place.col].as_deref()?;

			return result(block[place.word] >> place.shift);
		}

		// Outside the directory's span, a pair is in the hash map if a macrostate is from `DIRECTORY_MAX` on, else
		// unknown.
		let (key, shift) = far_place(s1, s2);

		result(u64::from(*self.far.get(&key)? >> shift))
	}

	// Called for every comparison computed, so kept inline in `subsumed` too.
	#[inline]
	pub(crate) fn insert(&mut self, s1: StateId, s2: StateId, result: bool) {
		if s1.max(s2).index() >= DIRECTORY_MAX {
			let (key, shift) = far_place(s1, s2);
			*self.far.entry(key).or_default() |= u32::from(bits(result)) << shift;

			return;
		}

		let place = Place::of(s1, s2);
		let span = place.row.max(place.col) + 1;
		if span > self.span {
			self.grow(span);
		}

		let block = self.directory[place.row * self.span + place.col]
			.get_or_insert_with(|| Box::new([0; BLOCK_WORDS]));
		block[place.word] |= u64::from(bits(result)) << place.shift;
	}

	/// Makes room for `span` blocks per row and column, more than now, moving the blocks to their new places.
	fn grow(&mut self, span: usize) {
		let span = span.max(self.span * 2).min(DIRECTORY_MAX / BLOCK);
		let mut directory = vec![None; span * span];
		for (index, block) in self.directory.drain(..).enumerate() {
			let (row, col) = (index / self.span, index % self.span);
			directory[row * span + col] = block;
		}

		self.span = span;
		self.directory = directory;
	}
}

#[cfg(test)]
mod tests {
	use std::collections::HashMap;

	use rand::rngs::StdRng;
	use rand::{RngExt, SeedableRng};

	use super::{DIRECTORY_MAX, Memo};
	use crate::ids::{Id, StateId};

	/// The largest macrostate number (below 2^31).
	const TOP: u32 = (1 << 31) - 1;

	/// The macrostate numbered `number`.
	fn id(number: u32) -> StateId {
		StateId::new(number as usize)
	}

	/// The number of the first macrostate outside the directory.
	fn bound() -> u32 {
		u32::try_from(DIRECTORY_MAX).expect("below 2^31")
	}

	/// A random state number, biased to block edges, the bound and the top; `counter` runs up across the bound,
	/// as the macrostates of a growing node do.
	fn state(rng: &mut StdRng, counter: &mut u32) -> u32 {
		let near = |rng: &mut StdRng, at: i64| -> u32 {
			let value = at + rng.random_range(-2_i64..=2);

			u32::try_from(value.clamp(0, i64::from(TOP))).expect("in range")
		};

		let bound = bound();
		let block = rng.random_range(0_i64..2049);
		let far_block = rng.random_range(0_i64..64);
		let any_block = rng.random_range(0_i64..1 << 20);

		match rng.random_range(0..10) {
			0 => rng.random_range(0..64),
			1 => near(rng, 32 * block),
			2 => near(rng, i64::from(bound)),
			3 => rng.random_range(0..bound),
			4 => rng.random_range(bound..bound * 4),
			5 => near(rng, i64::from(bound) + 4 * far_block),
			6 => rng.random_range(TOP - 64..=TOP),
			7 => near(rng, 4 * any_block),
			_ => {
				*counter = (*counter + rng.random_range(1..400)).min(TOP);

				*counter
			}
		}
	}

	/// Every pair of `reference` is known with its result; its neighbours and its transpose only if inserted.
	fn check_all(memo: &Memo, reference: &HashMap<(u32, u32), bool>) {
		for (&(s1, s2), &result) in reference {
			assert_eq!(memo.get(id(s1), id(s2)), Some(result), "({s1}, {s2})");
			for (n1, n2) in [
				(s1, s2.wrapping_add(1)),
				(s1.wrapping_add(1), s2),
				(s1, s2.wrapping_sub(1)),
				(s1.wrapping_sub(1), s2),
				(s2, s1),
			] {
				if n1 <= TOP && n2 <= TOP {
					let expected = reference.get(&(n1, n2)).copied();
					assert_eq!(memo.get(id(n1), id(n2)), expected, "({n1}, {n2})");
				}
			}
		}
	}

	#[test]
	fn random_pairs_read_back_what_was_inserted() {
		for seed in 0..40 {
			let mut rng = StdRng::seed_from_u64(seed);
			let mut memo = Memo::default();
			let mut reference = HashMap::new();
			let mut counter = rng.random_range(0..bound());
			for _ in 0..20_000 {
				let (s1, s2) = (state(&mut rng, &mut counter), state(&mut rng, &mut counter));
				// As in the search, a pair is inserted once, when it is unknown.
				if rng.random_bool(0.5) && !reference.contains_key(&(s1, s2)) {
					assert_eq!(memo.get(id(s1), id(s2)), None, "seed {seed}: ({s1}, {s2})");
					let result = rng.random_bool(0.5);
					memo.insert(id(s1), id(s2), result);
					reference.insert((s1, s2), result);
				} else {
					let expected = reference.get(&(s1, s2)).copied();
					assert_eq!(
						memo.get(id(s1), id(s2)),
						expected,
						"seed {seed}: ({s1}, {s2})"
					);
				}
			}

			check_all(&memo, &reference);
		}
	}

	#[test]
	fn pairs_at_block_edges_and_the_bound_read_back() {
		let bound = bound();
		let edges: Vec<u32> = [
			0, 1, 3, 4, 5, 31, 32, 33, 63, 64, 65, 32_767, 32_768, 32_769,
		]
		.into_iter()
		.chain(bound - 4..=bound + 5)
		.chain([2 * bound - 1, 2 * bound, TOP - 4, TOP - 3, TOP])
		.collect();

		// Two insertion orders, and a third of the pairs left out, so that blocks are shared and grow between.
		for order in 0..2 {
			for skip in 0..2 {
				let mut memo = Memo::default();
				let mut reference = HashMap::new();
				let mut pairs: Vec<(u32, u32)> = edges
					.iter()
					.flat_map(|&s1| edges.iter().map(move |&s2| (s1, s2)))
					.collect();
				if order == 1 {
					pairs.reverse();
				}

				for (index, &(s1, s2)) in (0_u32..).zip(&pairs) {
					if (index + skip) % 3 == 0 {
						continue;
					}

					let result = (s1 ^ s2 ^ index).is_multiple_of(2);
					memo.insert(id(s1), id(s2), result);
					reference.insert((s1, s2), result);
				}

				for &(s1, s2) in &pairs {
					let expected = reference.get(&(s1, s2)).copied();
					assert_eq!(memo.get(id(s1), id(s2)), expected, "({s1}, {s2})");
				}

				check_all(&memo, &reference);
			}
		}
	}

	/// A node passing the bound adds one block to the hash map and moves nothing.
	#[test]
	fn passing_the_bound_moves_nothing() {
		let mut memo = Memo::default();
		for s1 in (0..bound()).step_by(997) {
			for s2 in (0..bound()).step_by(1009) {
				memo.insert(id(s1), id(s2), (s1 + s2) % 3 == 0);
			}
		}

		let blocks = memo.directory.iter().flatten().count();
		let directory = memo.directory.as_ptr();

		memo.insert(id(bound()), id(5), true);
		assert_eq!(memo.far.len(), 1);
		assert_eq!(memo.directory.iter().flatten().count(), blocks);
		assert_eq!(memo.directory.as_ptr(), directory);
		assert_eq!(memo.get(id(bound()), id(5)), Some(true));
		assert_eq!(memo.get(id(997), id(1009)), Some(false));
	}

	/// Full blocks of the hash map and of the directory, and windows across the bound.
	#[test]
	fn full_blocks_keep_every_pair() {
		let bound = bound();
		for (first1, first2, side) in [
			(bound, bound, 4),
			(bound - 4, bound, 8),
			(0, bound, 8),
			(TOP - 3, 0, 4),
			(0, 0, 32),
			(bound - 32, bound - 32, 32),
		] {
			for pattern in 0..4 {
				let mut memo = Memo::default();
				let mut reference = HashMap::new();
				for i in 0..side {
					for j in 0..side {
						if (i * 7 + j * 3 + pattern) % 5 != 0 {
							let result = (i + j + pattern) % 2 == 0;
							memo.insert(id(first1 + i), id(first2 + j), result);
							reference.insert((first1 + i, first2 + j), result);
						}
					}
				}

				for i in 0..side {
					for j in 0..side {
						let (s1, s2) = (first1 + i, first2 + j);
						let expected = reference.get(&(s1, s2)).copied();
						assert_eq!(memo.get(id(s1), id(s2)), expected, "({s1}, {s2})");
					}
				}
			}
		}
	}
}
