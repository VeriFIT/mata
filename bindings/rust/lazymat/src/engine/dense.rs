//! Complement posts over a small child, as the OR of the members' bitset rows, each distinct bitset minimized once.
//! It keeps the members that `minimize` keeps, so the numbering is the general path's.

use std::rc::Rc;

use rustc_hash::FxHashMap;

use super::Engine;
use super::budget::{Budget, Limit};
use super::exec::Op;
use super::post::{Odometer, Post, Table, Target};
use super::store::Store;
use crate::Symbol;
use crate::ids::{ExecId, Id, StateId};

/// The most words of a bitset over the child's states.
const MAX_WORDS: usize = 4;

/// The most child states of a dense complement.
const MAX_STATES: usize = 64 * MAX_WORDS;

/// The most bytes of rows that one complement keeps before it falls back.
const ROWS_LIMIT: usize = 64 << 20;

type Bits = [u64; MAX_WORDS];

/// The bitset machinery of one complement.
#[derive(Debug, Default)]
pub(crate) struct Dense {
	order: ChildOrder,
	/// The child states seen accepting as targets.
	accepting: Bits,
	/// The targets of each child state on every tuple.
	rows: Vec<Option<Row>>,
	rows_bytes: usize,
	/// The union of the members' rows for the post being computed, one bitset per tuple.
	scratch: Vec<Bits>,
	/// The target of each distinct bitset of the post being computed.
	targets: FxHashMap<Bits, Target>,
	/// The kept members of one bitset.
	kept: Vec<StateId>,
}

impl Dense {
	/// Sets `scratch` to the union of `rows` on each of the `num_tuples` tuples; stops past a limit.
	fn union(&mut self, rows: &[Row], num_tuples: usize, budget: &mut Budget) {
		self.scratch.clear();
		self.scratch.resize(num_tuples, [0; MAX_WORDS]);
		for row in rows {
			budget.step(0);
			if budget.passed() {
				return;
			}

			// The common one-word rows, without the inner loop.
			if row.width == 1 {
				self.scratch
					.iter_mut()
					.zip(row.bits.iter())
					.for_each(|(acc, &word)| acc[0] |= word);
			} else {
				self.scratch
					.iter_mut()
					.zip(row.bits.chunks_exact(row.width))
					.flat_map(|(acc, words)| acc.iter_mut().zip(words))
					.for_each(|(acc, &word)| *acc |= word);
			}
		}
	}

	/// Pushes the target of each tuple's bitset in `scratch`, interning each distinct bitset once; stops past a
	/// limit.
	fn push_targets(&mut self, store: &mut Store, budget: &mut Budget, post: &mut Post) {
		self.targets.clear();
		let mut previous: Option<(&Bits, Target)> = None;
		let start = store.len();
		for bits in &self.scratch {
			if budget.passed() {
				break;
			}

			let target = match previous {
				Some((last, target)) if last == bits => target,
				_ => *self.targets.entry(*bits).or_insert_with(|| {
					self.kept.clear();
					self.kept.extend(self.order.kept(bits));
					let state = store.intern_set(&self.kept);
					budget.step(store.len() - start);

					Target {
						state,
						accepting: disjoint(bits, &self.accepting),
					}
				}),
			};

			previous = Some((bits, target));
			post.push_target(target);
		}

		budget.add(store.len() - start);
	}
}

/// The order on the child's states `0..len`, as bitsets.
#[derive(Debug, Default)]
struct ChildOrder {
	/// `drop[x]` holds the states `y` with `x ⊏ y`, or `y ≡ x` and `y < x`.
	drop: Vec<Bits>,
}

impl ChildOrder {
	/// Adds the states up to `len`, comparing each with the earlier ones; `le(x, y)` is `x ⊑ y`.
	fn extend(&mut self, len: usize, mut le: impl FnMut(StateId, StateId) -> bool) {
		for x in self.drop.len()..len {
			self.drop.push([0; MAX_WORDS]);
			for y in 0..x {
				if le(StateId::new(x), StateId::new(y)) {
					set(&mut self.drop[x], y);
				} else if le(StateId::new(y), StateId::new(x)) {
					set(&mut self.drop[y], x);
				}
			}
		}
	}

	/// The members of `bits` that `minimize` keeps, ascending.
	fn kept(&self, bits: &Bits) -> impl Iterator<Item = StateId> {
		members(bits)
			.filter(|&x| disjoint(bits, &self.drop[x]))
			.map(StateId::new)
	}
}

/// The targets of one child state on every tuple, `width` words per tuple.
#[derive(Clone, Debug)]
struct Row {
	width: usize,
	bits: Rc<[u64]>,
}

impl Row {
	fn bytes(&self) -> usize {
		size_of::<Row>() + self.bits.len() * size_of::<u64>()
	}
}

fn set(bits: &mut Bits, x: usize) {
	bits[x / 64] |= 1 << (x % 64);
}

fn disjoint(a: &Bits, b: &Bits) -> bool {
	a.iter().zip(b).all(|(a, b)| a & b == 0)
}

fn members(bits: &Bits) -> impl Iterator<Item = usize> {
	bits.iter().enumerate().flat_map(|(word, &value)| {
		let mut rest = value;

		std::iter::from_fn(move || {
			if rest == 0 {
				return None;
			}

			let bit = rest.trailing_zeros() as usize;
			rest &= rest - 1;

			Some(word * 64 + bit)
		})
	})
}

impl Engine<'_> {
	/// The post of the complement `node` from its `members`; `None` if `node` uses the general path from now on.
	pub(super) fn dense_post(
		&mut self,
		node: ExecId,
		child: ExecId,
		members: &[StateId],
		universes: &[Rc<[Symbol]>],
		num_tuples: usize,
	) -> Result<Option<Post>, Limit> {
		if self.dense[node].is_none() {
			return Ok(None);
		}

		let table = self.tuple_table(node, universes, num_tuples);
		let mut rows = Vec::with_capacity(members.len());
		for &member in members {
			if let Some(row) = self.kept_row(node, member) {
				rows.push(row);

				continue;
			}

			let post = self.post(child, member)?;
			let Some(len) = self.child_len(node, child) else {
				return Ok(None);
			};

			let (row, nanos) =
				self.timed(|engine| engine.build_row(node, &post, &table, num_tuples, len));
			if !self.admit_row(node, member, &row, nanos) {
				return Ok(None);
			}

			rows.push(row);
		}

		let Some(len) = self.child_len(node, child) else {
			return Ok(None);
		};

		let mut post = Post::with_table(self.exec.nodes[node].arity, table);
		let Some(dense) = &mut self.dense[node] else {
			unreachable!("checked above, and a fall back returns")
		};

		dense.order.extend(len, |x, y| {
			self.subsumption
				.subsumed(&self.exec, &self.stores, child, x, y)
		});
		dense.union(&rows, num_tuples, &mut self.budget);
		dense.push_targets(&mut self.stores[node], &mut self.budget, &mut post);

		Ok(Some(post))
	}

	/// The number of states of `child` so far; `None` if it outgrew the bitsets and `node` fell back.
	fn child_len(&mut self, node: ExecId, child: ExecId) -> Option<usize> {
		let owner = self.exec.owner(child);
		let len = match &self.exec.nodes[owner].op {
			Op::Leaf(leaf) => leaf.num_states(),
			_ => self.stores[owner].len(),
		};

		if len > MAX_STATES {
			self.fall_back(node);

			return None;
		}

		Some(len)
	}

	/// The cache budget's key of the rows of `node`, as rows are keyed after the nodes.
	fn row_key(&self, node: ExecId) -> usize {
		self.exec.nodes.len() + node.index()
	}

	/// The kept row of `member`, renewed in the cache budget.
	fn kept_row(&mut self, node: ExecId, member: StateId) -> Option<Row> {
		let dense = self.dense[node].as_ref()?;
		let row = dense.rows.get(member.index())?.clone()?;

		let key = self.row_key(node);
		if let Some(budget) = &mut self.cache_budget {
			budget.touch(key, member);
		}

		Some(row)
	}

	/// The row of a child state from its `post`, over a child of `len` states.
	fn build_row(
		&mut self,
		node: ExecId,
		post: &Post,
		table: &Table,
		num_tuples: usize,
		len: usize,
	) -> Row {
		let width = len.div_ceil(64).max(1);
		let mut bits = vec![0; num_tuples * width];
		let tapes: Vec<usize> = (0..self.exec.nodes[node].arity).collect();
		let mut odometer = Odometer::default();
		let Some(dense) = &mut self.dense[node] else {
			unreachable!("only a dense node builds rows")
		};

		for (tuple, target) in post.iter() {
			if tuple.contains(&Symbol::EPSILON) {
				continue;
			}

			let state = target.state.index();
			if target.accepting {
				set(&mut dense.accepting, state);
			}

			let (word, bit) = (state / 64, 1 << (state % 64));
			table.matching(&tapes, tuple.iter().copied(), &mut odometer, |index| {
				bits[index * width + word] |= bit;
			});
		}

		Row {
			width,
			bits: bits.into(),
		}
	}

	/// Keeps `row` for `member`; `false` if the rows passed [`ROWS_LIMIT`] and the node fell back.
	fn admit_row(&mut self, node: ExecId, member: StateId, row: &Row, nanos: u64) -> bool {
		let bytes = row.bytes();
		let Some(dense) = &mut self.dense[node] else {
			unreachable!("only a dense node keeps rows")
		};

		if dense.rows_bytes + bytes > ROWS_LIMIT {
			self.fall_back(node);

			return false;
		}

		let index = member.index();
		if dense.rows.len() <= index {
			dense.rows.resize(index + 1, None);
		}

		dense.rows[index] = Some(row.clone());
		dense.rows_bytes += bytes;

		let key = self.row_key(node);
		if let Some(budget) = &mut self.cache_budget {
			let dropped = budget.admit(key, member, nanos, bytes);
			self.evict(dropped);
		}

		true
	}

	/// Drops the row of `member` that the cache budget evicted.
	pub(super) fn evict_row(&mut self, node: ExecId, member: StateId) {
		if let Some(dense) = &mut self.dense[node]
			&& let Some(row) = dense.rows.get_mut(member.index()).and_then(Option::take)
		{
			dense.rows_bytes -= row.bytes();
		}
	}

	/// Switches `node` to the general path for good, and frees its rows.
	fn fall_back(&mut self, node: ExecId) {
		let key = self.row_key(node);
		if let Some(dense) = &self.dense[node]
			&& let Some(budget) = &mut self.cache_budget
		{
			for (member, row) in dense.rows.iter().enumerate() {
				if row.is_some() {
					budget.remove(key, StateId::new(member));
				}
			}
		}

		self.dense[node] = None;
	}
}
