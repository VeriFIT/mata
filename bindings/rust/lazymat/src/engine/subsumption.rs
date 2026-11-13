//! Subsumption of macrostates, and the antichain that prunes the search.
//!
//! `s1 ⊑ s2` implies that `s1` accepts a subset of what `s2` accepts. Leaves use their simulation, products
//! compare componentwise, and complement sets `S1 ⊑ S2` when every member of `S2` is below a member of `S1`.
//! A macrostate below an explored one cannot reach acceptance first, so the search skips it.

use std::cmp::Reverse;
use std::collections::BTreeMap;
use std::ops::Bound;
use std::time::Instant;

use rustc_hash::{FxHashMap, FxHashSet};
use typed_index_collections::TiVec;

use super::closure::Closures;
use super::deep;
use super::exec::{Exec, LeafRef, Op};
use super::memo::Memo;
use super::store::Store;
use crate::ids::{ExecId, Id, StateId};
use crate::leaf::Leaf;
use crate::simulation::{IDENTITY, Simulation, TimedOut};

/// Comparisons of [`minimize`](Subsumption::minimize) between two reads of the clock.
const CLOCK_COMPARISONS: usize = 4096;

#[derive(Debug)]
pub(crate) struct Subsumption {
	enabled: bool,
	memos: TiVec<ExecId, Memo>,
	closures: Closures,
	/// The query's deadline, for computing leaf simulations and for minimizing.
	deadline: Option<Instant>,
	/// Whether a simulation or a minimization passed the deadline; then leaves without one use the identity.
	late: bool,
}

impl Subsumption {
	pub(crate) fn new(enabled: bool, num_nodes: usize, deadline: Option<Instant>) -> Subsumption {
		Subsumption {
			enabled,
			memos: (0..num_nodes).map(|_| Memo::default()).collect(),
			closures: Closures::new(num_nodes),
			deadline,
			late: false,
		}
	}

	pub(crate) fn is_late(&self) -> bool {
		self.late
	}

	/// The simulation of `leaf`, computed within the deadline; once late, the identity if it has none.
	fn simulation<'l>(&mut self, leaf: &'l Leaf) -> &'l Simulation {
		if !self.late {
			match leaf.simulation(self.deadline) {
				Ok(simulation) => return simulation,
				Err(TimedOut) => self.late = true,
			}
		}

		leaf.computed_simulation().unwrap_or(&IDENTITY)
	}

	// `stores` is a `&TiVec`, not a slice, which saves a register through the recursion and measured faster.
	pub(crate) fn subsumed(
		&mut self,
		exec: &Exec<'_>,
		stores: &TiVec<ExecId, Store>,
		node: ExecId,
		s1: StateId,
		s2: StateId,
	) -> bool {
		if s1 == s2 {
			return true;
		}

		if !self.enabled {
			return false;
		}

		let op = &exec.nodes[node].op;
		if let Op::Leaf(leaf) = op {
			return self.simulation(leaf).le(s1, s2);
		}

		if let Some(child) = op.transparent_child() {
			return self.subsumed(exec, stores, child, s1, s2);
		}

		// A complement over a leaf compares through closures, as fast as a memo lookup, so only what they
		// cannot decide goes to the memo.
		if let Op::Complement(child) = *op
			&& let Op::Leaf(leaf) = &exec.nodes[exec.owner(child)].op
			&& let Some(result) = self.complement_of_leaf(&stores[node], node, leaf, s1, s2)
		{
			return result;
		}

		if let Some(result) = self.memos[node].get(s1, s2) {
			return result;
		}

		let result = deep(|| self.compute(exec, stores, node, s1, s2));
		self.memos[node].insert(s1, s2, result);

		result
	}

	/// `s1 ⊑ s2` for macrostates of the root, which keeps no results, as each pair of them is compared once.
	fn root_subsumed(
		&mut self,
		exec: &Exec<'_>,
		stores: &TiVec<ExecId, Store>,
		s1: StateId,
		s2: StateId,
	) -> bool {
		let root = exec.owner(exec.root);
		if s1 == s2 || matches!(exec.nodes[root].op, Op::Leaf(_)) {
			return self.subsumed(exec, stores, root, s1, s2);
		}

		if let Op::Complement(child) = exec.nodes[root].op
			&& let Op::Leaf(leaf) = &exec.nodes[exec.owner(child)].op
			&& let Some(result) = self.complement_of_leaf(&stores[root], root, leaf, s1, s2)
		{
			return result;
		}

		deep(|| self.compute(exec, stores, root, s1, s2))
	}

	fn compute(
		&mut self,
		exec: &Exec<'_>,
		stores: &TiVec<ExecId, Store>,
		node: ExecId,
		s1: StateId,
		s2: StateId,
	) -> bool {
		let store = &stores[node];
		match exec.nodes[node].op {
			Op::Union(ref branches) => {
				let (smaller, larger) = (store.tagged(s1), store.tagged(s2));

				smaller.branch == larger.branch
					&& self.subsumed(
						exec,
						stores,
						branches[smaller.branch as usize],
						smaller.state,
						larger.state,
					)
			}
			Op::Intersection(a, b) | Op::Sync(a, b, _) | Op::Diagonal(a, b) => {
				let (smaller, larger) = (store.pair(s1), store.pair(s2));

				self.subsumed(exec, stores, a, smaller.lhs, larger.lhs)
					&& self.subsumed(exec, stores, b, smaller.rhs, larger.rhs)
			}
			Op::Complement(child) => {
				let (larger, smaller) = (store.set(s1), store.set(s2));

				// Sound but incomplete, as it misses two members of `smaller` below one member of `larger`.
				smaller.len() <= larger.len()
					&& smaller.iter().all(|&x| {
						larger
							.iter()
							.any(|&y| self.subsumed(exec, stores, child, x, y))
					})
			}
			_ => unreachable!("leaves and transparent nodes are handled above"),
		}
	}

	/// `s1 ⊑ s2` for the complement `node` over `leaf`, by subset or closure; `None` if `s1` gets no closure.
	fn complement_of_leaf(
		&mut self,
		store: &Store,
		node: ExecId,
		leaf: &Leaf,
		s1: StateId,
		s2: StateId,
	) -> Option<bool> {
		let (larger, smaller) = (store.set(s1), store.set(s2));
		if smaller.len() > larger.len() {
			return Some(false);
		}

		let simulation = self.simulation(leaf);
		if simulation.is_identity() {
			return Some(smaller.iter().all(|x| larger.binary_search(x).is_ok()));
		}

		let rows = simulation.downward_rows()?;
		let closure = self.closures.get_or_build(node, store, s1, rows)?;

		Some(smaller.iter().all(|&x| closure.contains(x)))
	}

	/// Sorts `set` and drops the members below another; past the deadline it keeps those it has not compared.
	pub(crate) fn minimize(
		&mut self,
		exec: &Exec<'_>,
		stores: &TiVec<ExecId, Store>,
		node: ExecId,
		set: &mut Vec<StateId>,
	) {
		set.sort_unstable();
		set.dedup();
		if !self.enabled || set.len() < 2 {
			return;
		}

		// In decreasing rank a member can only be below a kept one, and only cover kept ones of its rank.
		let mut ranked: Vec<(i64, StateId)> = set
			.iter()
			.map(|&member| (self.rank(exec, stores, node, member), member))
			.collect();
		ranked.sort_unstable_by_key(|&(rank, member)| (Reverse(rank), member));

		let mut kept: Vec<(i64, StateId)> = Vec::with_capacity(ranked.len());
		// The comparisons since the clock was last read.
		let mut compared = 0;
		for (rank, member) in ranked {
			if compared >= CLOCK_COMPARISONS {
				compared = 0;
				if self
					.deadline
					.is_some_and(|deadline| Instant::now() > deadline)
				{
					self.late = true;
				}
			}

			// Past the deadline the query fails anyway, so the rest stay without comparisons.
			if self.late {
				kept.push((rank, member));

				continue;
			}

			compared += kept.len();
			if kept
				.iter()
				.any(|&(_, other)| self.subsumed(exec, stores, node, member, other))
			{
				continue;
			}

			kept.retain(|&(other_rank, other)| {
				other_rank > rank || !self.subsumed(exec, stores, node, other, member)
			});
			kept.push((rank, member));
		}

		set.clear();
		set.extend(kept.iter().map(|&(_, member)| member));
		set.sort_unstable();
	}

	/// A number that `⊑` never decreases, so `s1 ⊑ s2` implies `rank(s1) <= rank(s2)`.
	fn rank(
		&mut self,
		exec: &Exec<'_>,
		stores: &TiVec<ExecId, Store>,
		node: ExecId,
		state: StateId,
	) -> i64 {
		let op = &exec.nodes[node].op;
		if let Some(child) = op.transparent_child() {
			return self.rank(exec, stores, child, state);
		}

		let store = &stores[node];
		match *op {
			// The states above a state are above every state below it.
			Op::Leaf(ref leaf) => {
				-i64::try_from(self.simulation(leaf).count_above(state)).unwrap_or(i64::MAX)
			}
			Op::Union(ref branches) => {
				let tagged = store.tagged(state);

				self.rank(exec, stores, branches[tagged.branch as usize], tagged.state)
			}
			Op::Intersection(a, b) | Op::Sync(a, b, _) | Op::Diagonal(a, b) => {
				let pair = store.pair(state);

				self.rank(exec, stores, a, pair.lhs) + self.rank(exec, stores, b, pair.rhs)
			}
			// A set is only below sets that are not larger.
			Op::Complement(_) => -i64::try_from(store.set(state).len()).unwrap_or(i64::MAX),
			_ => unreachable!("transparent nodes are handled above"),
		}
	}
}

/// The group and size under which the antichain keeps a root macrostate.
#[derive(Clone, Copy, Debug)]
struct Fingerprint {
	group: StateId,
	size: u32,
}

/// How root macrostates are grouped, so that a new one is compared with few antichain members.
#[derive(Debug)]
enum Shape<'f> {
	All,
	/// A union root, where only macrostates of the same branch compare.
	Branch {
		owner: ExecId,
	},
	/// A complement root, where a set can only be below sets that are not larger.
	Size {
		owner: ExecId,
	},
	/// A product whose right operand is a complement, grouped by the left state and the size of the right set.
	Pair {
		owner: ExecId,
		complement: ExecId,
		/// The left operand if it is a leaf, so that related left states are compared too.
		lhs: Option<LeafRef<'f>>,
	},
	/// Another product whose left operands lead down to a leaf, grouped by the state of that leaf.
	Leftmost {
		/// The products from the root's owner down to the leaf.
		spine: Vec<ExecId>,
		leaf: LeafRef<'f>,
	},
}

/// The products from `owner` down its left operands, and the leaf they end at, if they do.
fn left_spine<'f>(exec: &Exec<'f>, owner: ExecId) -> Option<(Vec<ExecId>, LeafRef<'f>)> {
	let mut spine = Vec::new();
	let mut node = owner;
	loop {
		match &exec.nodes[node].op {
			Op::Intersection(a, _) | Op::Sync(a, _, _) | Op::Diagonal(a, _) => {
				spine.push(node);
				node = exec.owner(*a);
			}
			Op::Leaf(leaf) => return Some((spine, leaf.clone())),
			_ => return None,
		}
	}
}

#[derive(Debug)]
pub(crate) struct Antichain<'f> {
	shape: Shape<'f>,
	alive: FxHashSet<StateId>,
	/// The members by group, then size; no group or size without members.
	buckets: FxHashMap<StateId, BTreeMap<u32, Vec<StateId>>>,
	/// The groups that may hold members above, and below, the macrostate being inserted.
	above: Vec<StateId>,
	below: Vec<StateId>,
}

impl<'f> Antichain<'f> {
	pub(crate) fn new(exec: &Exec<'f>) -> Antichain<'f> {
		let owner = exec.owner(exec.root);
		let shape = match exec.nodes[owner].op {
			Op::Union(_) => Shape::Branch { owner },
			Op::Complement(_) => Shape::Size { owner },
			Op::Intersection(a, b) | Op::Sync(a, b, _) | Op::Diagonal(a, b) => {
				let complement = exec.owner(b);
				if matches!(exec.nodes[complement].op, Op::Complement(_)) {
					let lhs = match &exec.nodes[exec.owner(a)].op {
						Op::Leaf(leaf) => Some(leaf.clone()),
						_ => None,
					};

					Shape::Pair {
						owner,
						complement,
						lhs,
					}
				} else if let Some((spine, leaf)) = left_spine(exec, owner) {
					Shape::Leftmost { spine, leaf }
				} else {
					Shape::All
				}
			}
			_ => Shape::All,
		};

		Antichain {
			shape,
			alive: FxHashSet::default(),
			buckets: FxHashMap::default(),
			above: Vec::new(),
			below: Vec::new(),
		}
	}

	pub(crate) fn is_pruned(&self, state: StateId) -> bool {
		!self.alive.contains(&state)
	}

	/// Unless a member subsumes `state`, adds it and drops the members it subsumes; returns whether it did.
	pub(crate) fn insert(
		&mut self,
		exec: &Exec<'_>,
		stores: &TiVec<ExecId, Store>,
		subsumption: &mut Subsumption,
		state: StateId,
	) -> bool {
		let Fingerprint { group, size } = self.fingerprint(stores, state);
		// Under `All` and `Leftmost` every size is 0.
		let same = (Bound::Included(size), Bound::Included(size));
		let (check, remove) = match self.shape {
			Shape::All | Shape::Branch { .. } | Shape::Leftmost { .. } => (same, same),
			Shape::Size { .. } | Shape::Pair { .. } => (
				(Bound::Unbounded, Bound::Included(size)),
				(Bound::Included(size), Bound::Unbounded),
			),
		};

		self.relate(subsumption, group);
		for &upper in &self.above {
			let Some(sizes) = self.buckets.get(&upper) else {
				continue;
			};

			for (_, members) in sizes.range(check) {
				if members
					.iter()
					.any(|&member| subsumption.root_subsumed(exec, stores, state, member))
				{
					return false;
				}
			}
		}

		for &lower in &self.below {
			let Some(sizes) = self.buckets.get_mut(&lower) else {
				continue;
			};

			let mut emptied = false;
			for (_, members) in sizes.range_mut(remove) {
				let covered = members.extract_if(.., |&mut member| {
					subsumption.root_subsumed(exec, stores, member, state)
				});
				for member in covered {
					self.alive.remove(&member);
				}

				emptied |= members.is_empty();
			}

			if emptied {
				sizes.retain(|_, members| !members.is_empty());
				if sizes.is_empty() {
					self.buckets.remove(&lower);
				}
			}
		}

		self.alive.insert(state);
		self.buckets
			.entry(group)
			.or_default()
			.entry(size)
			.or_default()
			.push(state);

		true
	}

	/// Sets `above` and `below` to the groups that may hold members above and below a macrostate of `group`.
	fn relate(&mut self, subsumption: &mut Subsumption, group: StateId) {
		self.above.clear();
		self.below.clear();

		// Products compare componentwise, so related members are in groups that the leaf relates to `group`.
		// While every member is in `group`, the leaf's simulation, which can take seconds, is not computed.
		let simulation = match &self.shape {
			Shape::Pair {
				lhs: Some(leaf), ..
			}
			| Shape::Leftmost { leaf, .. }
				if self.buckets.keys().any(|&other| other != group) =>
			{
				subsumption.simulation(leaf)
			}
			_ => &IDENTITY,
		};

		// The identity relates `group` only to itself.
		let Some(upward) = simulation.upward(group) else {
			self.above.push(group);
			self.below.push(group);

			return;
		};

		// From the leaf's relation, or from the antichain's groups when those are fewer.
		let groups = || self.buckets.keys().copied();
		if simulation.count_above(group) <= self.buckets.len() {
			self.above.extend(upward);
		} else {
			self.above
				.extend(groups().filter(|&other| simulation.le(group, other)));
		}

		if let Some(downward) = simulation.downward(group) {
			self.below.extend(downward);
			if self.below.len() <= self.buckets.len() {
				return;
			}

			self.below.clear();
		}

		self.below
			.extend(groups().filter(|&other| simulation.le(other, group)));
	}

	fn fingerprint(&self, stores: &TiVec<ExecId, Store>, state: StateId) -> Fingerprint {
		let size = |members: &[StateId]| u32::try_from(members.len()).unwrap_or(u32::MAX);

		match self.shape {
			Shape::All => Fingerprint {
				group: StateId::new(0),
				size: 0,
			},
			Shape::Branch { owner } => Fingerprint {
				group: StateId::new(0),
				size: stores[owner].tagged(state).branch,
			},
			Shape::Size { owner } => Fingerprint {
				group: StateId::new(0),
				size: size(stores[owner].set(state)),
			},
			Shape::Pair {
				owner, complement, ..
			} => {
				let pair = stores[owner].pair(state);

				Fingerprint {
					group: pair.lhs,
					size: size(stores[complement].set(pair.rhs)),
				}
			}
			Shape::Leftmost { ref spine, .. } => Fingerprint {
				group: spine
					.iter()
					.fold(state, |state, &node| stores[node].pair(state).lhs),
				size: 0,
			},
		}
	}
}
