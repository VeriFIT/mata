//! The initial macrostates of every node, listed when there are at most [`INITIAL_BATCH`], else numbered on demand.
//!
//! Intersected unions multiply their initial macrostates. Both ways number them in the same order (products left
//! major, unions branch after branch), and each one numbered on demand counts against the query's limits.

use std::ops::Range;
use std::rc::Rc;

use super::budget::Limit;
use super::exec::Op;
use super::post::Target;
use super::store::Pair;
use super::{Engine, deep};
use crate::ids::{ExecId, StateId};

/// The most initial macrostates that a node lists at once, and the size of a batch of them.
pub(super) const INITIAL_BATCH: usize = 1 << 16;

/// The batches of `indices`, each at most `size` long.
pub(super) fn batches(
	indices: Range<usize>,
	size: usize,
) -> impl DoubleEndedIterator<Item = Range<usize>> {
	let end = indices.end;

	indices
		.step_by(size)
		.map(move |start| start..end.min(start.saturating_add(size)))
}

impl Engine<'_> {
	/// How many initial macrostates `node` has, saturating at the largest `usize`, more than a query can visit.
	pub(crate) fn initial_count(&mut self, node: ExecId) -> usize {
		if let Some(count) = self.initial_counts[node] {
			return count;
		}

		let count = deep(|| match self.exec.nodes[node].op.clone() {
			Op::Leaf(leaf) => leaf.initial().len(),
			Op::Union(branches) => branches
				.iter()
				.map(|&branch| self.initial_count(branch))
				.fold(0, usize::saturating_add),
			Op::Intersection(a, b) | Op::Sync(a, b, _) | Op::Diagonal(a, b) => {
				self.initial_count(a).saturating_mul(self.initial_count(b))
			}
			Op::Complement(_) => 1,
			Op::Identity(child) | Op::Project(child, _) | Op::Transparent(child) => {
				self.initial_count(child)
			}
			Op::Explicit { .. } => unreachable!("explicit nodes are replaced before the search"),
		});
		self.initial_counts[node] = Some(count);

		count
	}

	/// Whether the initial macrostates of `node` are listed at once rather than numbered on demand.
	pub(crate) fn is_listed(&mut self, node: ExecId) -> bool {
		self.initial_count(node) <= INITIAL_BATCH
	}

	/// The initial macrostate of `node` at `index`, below its count, or the limit that the query passed.
	pub(crate) fn initial_at(&mut self, node: ExecId, index: usize) -> Result<Target, Limit> {
		if self.is_listed(node) {
			let target = self.initial(node)?[index];
			self.stopped()?;

			return Ok(target);
		}

		self.budget.step(0);
		let start = self.stores[node].len();
		let target = deep(|| match self.exec.nodes[node].op.clone() {
			Op::Leaf(leaf) => Ok(Target::of_leaf(&leaf, leaf.initial()[index])),
			Op::Union(branches) => {
				let starts = self.branch_starts(node, &branches);
				let branch = starts.partition_point(|&branch_start| branch_start <= index) - 1;
				let target = self.initial_at(branches[branch], index - starts[branch])?;
				let branch = u32::try_from(branch).expect("fewer than 2^32 branches");

				Ok(self.tagged_target(node, branch, target))
			}
			Op::Intersection(a, b) | Op::Sync(a, b, _) | Op::Diagonal(a, b) => {
				let right_count = self.initial_count(b);
				let lhs = self.initial_at(a, index / right_count)?;
				let rhs = self.initial_at(b, index % right_count)?;

				Ok(self.pair_target(node, lhs, rhs))
			}
			Op::Complement(_) => unreachable!("a complement has one initial macrostate"),
			Op::Identity(child) | Op::Project(child, _) | Op::Transparent(child) => {
				self.initial_at(child, index)
			}
			Op::Explicit { .. } => unreachable!("explicit nodes are replaced before the search"),
		})?;
		self.budget.add(self.stores[node].len() - start);
		self.stopped()?;

		Ok(target)
	}

	/// Where the initial macrostates of each branch of the union `node` start in its own, computed once.
	fn branch_starts(&mut self, node: ExecId, branches: &[ExecId]) -> Rc<[usize]> {
		if let Some(starts) = &self.union_starts[node] {
			return Rc::clone(starts);
		}

		let mut start: usize = 0;
		let mut starts = Vec::with_capacity(branches.len());
		for &branch in branches {
			starts.push(start);
			start = start.saturating_add(self.initial_count(branch));
		}

		let starts: Rc<[usize]> = starts.into();
		self.union_starts[node] = Some(Rc::clone(&starts));

		starts
	}

	/// Whether an initial macrostate of `node` accepts, decided without numbering them.
	pub(super) fn has_accepting_initial(&mut self, node: ExecId) -> bool {
		if let Some(known) = self.accepting_initials[node] {
			return known;
		}

		let known = deep(|| match self.exec.nodes[node].op.clone() {
			Op::Leaf(leaf) => leaf.initial().iter().any(|&state| leaf.is_final(state)),
			Op::Union(branches) => branches
				.iter()
				.any(|&branch| self.has_accepting_initial(branch)),
			Op::Intersection(a, b) | Op::Sync(a, b, _) | Op::Diagonal(a, b) => {
				self.has_accepting_initial(a) && self.has_accepting_initial(b)
			}
			Op::Complement(child) => !self.has_accepting_initial(child),
			Op::Identity(child) | Op::Project(child, _) | Op::Transparent(child) => {
				self.has_accepting_initial(child)
			}
			Op::Explicit { .. } => unreachable!("explicit nodes are replaced before the search"),
		});
		self.accepting_initials[node] = Some(known);

		known
	}

	/// The batches in which the search takes the root's initial macrostates. Above [`INITIAL_BATCH`], one at a time
	/// (a batch would cost the antichain every pair), each segment from its last so that it prunes earlier ones below
	/// it.
	pub(super) fn search_order(
		&mut self,
		root: ExecId,
	) -> impl Iterator<Item = Range<usize>> + use<> {
		let count = self.initial_count(root);
		let listed = self.is_listed(root);
		let owner = self.exec.owner(root);
		let starts: Rc<[usize]> =
			if !listed && let Op::Union(branches) = self.exec.nodes[owner].op.clone() {
				self.branch_starts(owner, &branches)
			} else {
				Rc::new([0])
			};
		let ends = starts.iter().skip(1).copied().chain([count]);
		let segments: Vec<Range<usize>> = starts
			.iter()
			.copied()
			.zip(ends)
			.map(|(start, end)| start..end)
			.collect();
		let size = if listed { INITIAL_BATCH } else { 1 };

		segments
			.into_iter()
			.flat_map(move |segment| batches(segment, size).rev())
	}

	/// Every initial macrostate of `node`, which has at most [`INITIAL_BATCH`], or the limit that the query passed.
	fn initial(&mut self, node: ExecId) -> Result<Rc<[Target]>, Limit> {
		if let Some(initial) = &self.initials[node] {
			return Ok(Rc::clone(initial));
		}

		let start = self.stores[node].len();
		let initial = deep(|| self.list_initial(node));
		self.budget.add(self.stores[node].len() - start);

		// Past a limit they may be partial, so they are neither kept nor used.
		let initial = initial?;
		self.stopped()?;
		self.initials[node] = Some(Rc::clone(&initial));

		Ok(initial)
	}

	fn list_initial(&mut self, node: ExecId) -> Result<Rc<[Target]>, Limit> {
		let initial: Rc<[Target]> = match self.exec.nodes[node].op.clone() {
			Op::Leaf(leaf) => leaf
				.initial()
				.iter()
				.map(|&state| Target::of_leaf(&leaf, state))
				.collect(),
			Op::Union(branches) => {
				let mut initial = Vec::new();
				for (branch, &child) in (0_u32..).zip(branches.iter()) {
					for &target in self.initial(child)?.iter() {
						initial.push(self.tagged_target(node, branch, target));
					}
				}

				initial.into()
			}
			Op::Intersection(a, b) | Op::Sync(a, b, _) | Op::Diagonal(a, b) => {
				// With no initial macrostate on one side, the other side may have too many to list.
				if self.initial_count(node) == 0 && !(self.is_listed(a) && self.is_listed(b)) {
					return Ok(Rc::new([]));
				}

				let (left, right) = (self.initial(a)?, self.initial(b)?);
				let start = self.stores[node].len();
				let mut initial = Vec::with_capacity(left.len() * right.len());
				for &lhs in left.iter() {
					self.budget.step(self.stores[node].len() - start);
					if self.budget.passed() {
						break;
					}

					for &rhs in right.iter() {
						initial.push(self.pair_target(node, lhs, rhs));
					}
				}

				initial.into()
			}
			Op::Complement(child) => {
				let mut members: Vec<StateId> = Vec::new();
				let mut accepting = true;
				// Every initial macrostate of the child, numbered on demand when there are many.
				for index in 0..self.initial_count(child) {
					let member = self.initial_at(child, index)?;
					accepting &= !member.accepting;
					members.push(member.state);
				}

				self.subsumption
					.minimize(&self.exec, &self.stores, child, &mut members);
				let state = self.stores[node].intern_set(&members);

				Rc::new([Target { state, accepting }])
			}
			Op::Identity(child) | Op::Project(child, _) | Op::Transparent(child) => {
				self.initial(child)?
			}
			Op::Explicit { .. } => unreachable!("explicit nodes are replaced before the search"),
		};

		Ok(initial)
	}

	fn pair_target(&mut self, node: ExecId, lhs: Target, rhs: Target) -> Target {
		let state = self.stores[node].intern_pair(Pair {
			lhs: lhs.state,
			rhs: rhs.state,
		});

		Target {
			state,
			accepting: lhs.accepting && rhs.accepting,
		}
	}
}
