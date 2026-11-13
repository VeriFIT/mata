//! Macrostates of the nodes that build their own, numbered densely per node.

use indexmap::IndexSet;
use rustc_hash::FxBuildHasher;

use crate::ids::{Id, StateId};

/// A macrostate of a union, a branch and a state of it.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(crate) struct Tagged {
	pub(crate) branch: u32,
	pub(crate) state: StateId,
}

/// A macrostate of a product, a state of each operand.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(crate) struct Pair {
	pub(crate) lhs: StateId,
	pub(crate) rhs: StateId,
}

#[derive(Debug)]
pub(crate) enum Store {
	/// The node numbers no macrostates (a leaf), or shares its child's.
	None,
	Tagged(IndexSet<Tagged, FxBuildHasher>),
	Pair(IndexSet<Pair, FxBuildHasher>),
	/// A complement's sorted, minimized set of states of the child.
	Set(IndexSet<Box<[StateId]>, FxBuildHasher>),
}

impl Store {
	pub(crate) fn len(&self) -> usize {
		match self {
			Store::None => 0,
			Store::Tagged(states) => states.len(),
			Store::Pair(states) => states.len(),
			Store::Set(states) => states.len(),
		}
	}

	pub(crate) fn intern_tagged(&mut self, tagged: Tagged) -> StateId {
		let Store::Tagged(states) = self else {
			unreachable!("not a union node")
		};

		StateId::new(states.insert_full(tagged).0)
	}

	pub(crate) fn tagged(&self, state: StateId) -> Tagged {
		let Store::Tagged(states) = self else {
			unreachable!("not a union node")
		};

		states[state.index()]
	}

	// Called for every move of a product's post, so kept inline in the joins.
	#[inline]
	pub(crate) fn intern_pair(&mut self, pair: Pair) -> StateId {
		let Store::Pair(states) = self else {
			unreachable!("not a product node")
		};

		StateId::new(states.insert_full(pair).0)
	}

	pub(crate) fn pair(&self, state: StateId) -> Pair {
		let Store::Pair(states) = self else {
			unreachable!("not a product node")
		};

		states[state.index()]
	}

	/// Looks `members` up first, so that a known set is not boxed again.
	// Called for every target of a complement's post, so kept inline in the posts.
	#[inline]
	pub(crate) fn intern_set(&mut self, members: &[StateId]) -> StateId {
		let Store::Set(states) = self else {
			unreachable!("not a complement node")
		};

		let index = states
			.get_index_of(members)
			.unwrap_or_else(|| states.insert_full(members.into()).0);

		StateId::new(index)
	}

	pub(crate) fn set(&self, state: StateId) -> &[StateId] {
		let Store::Set(states) = self else {
			unreachable!("not a complement node")
		};

		&states[state.index()]
	}
}
