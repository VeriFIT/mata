//! Numbers of formula nodes, leaves, execution nodes and states.

use std::fmt;

/// A number of one kind of item, counted from 0.
pub(crate) trait Id: Copy {
	fn new(index: usize) -> Self;

	fn index(self) -> usize;
}

/// An [`Id`] over `usize`, printed as its number.
macro_rules! usize_id {
	($(#[$doc:meta])* $name:ident) => {
		$(#[$doc])*
		#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
		#[repr(transparent)]
		pub(crate) struct $name(usize);

		impl Id for $name {
			#[inline]
			fn new(index: usize) -> $name {
				$name(index)
			}

			#[inline]
			fn index(self) -> usize {
				self.0
			}
		}

		// For indexing a `TiVec`.
		impl From<usize> for $name {
			#[inline]
			fn from(index: usize) -> $name {
				$name(index)
			}
		}

		impl From<$name> for usize {
			#[inline]
			fn from(id: $name) -> usize {
				id.0
			}
		}

		impl fmt::Debug for $name {
			fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
				fmt::Debug::fmt(&self.0, f)
			}
		}
	};
}

usize_id! {
	/// A node of a [`Formula`](crate::Formula).
	NodeId
}

usize_id! {
	/// A leaf of a formula, in the order of [`leaf`](crate::Formula::leaf) calls.
	LeafId
}

usize_id! {
	/// A node of the execution DAG of one query.
	ExecId
}

/// The top bit of a `u32`, which no [`StateId`] uses.
const TOP_BIT: u32 = 1 << 31;

/// A state of a leaf, or a macrostate of an execution node, numbered densely from 0, below 2^31, as a post
/// keeps whether a target accepts in the top bit.
#[derive(Clone, Copy, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[repr(transparent)]
pub(crate) struct StateId(u32);

impl Id for StateId {
	#[inline]
	fn new(index: usize) -> StateId {
		u32::try_from(index)
			.ok()
			.filter(|&state| state < TOP_BIT)
			.map(StateId)
			.expect("fewer than 2^31 states")
	}

	#[inline]
	fn index(self) -> usize {
		self.0 as usize
	}
}

impl StateId {
	/// A number that no state has, for a slot that holds no state yet.
	pub(crate) const NONE: StateId = StateId(u32::MAX);

	/// The state with `accepting` in the top bit, as a post keeps a target.
	#[inline]
	pub(crate) fn pack(self, accepting: bool) -> u32 {
		self.0 | if accepting { TOP_BIT } else { 0 }
	}

	/// The state of a target packed by [`pack`](StateId::pack).
	#[inline]
	pub(crate) fn unpack(packed: u32) -> StateId {
		StateId(packed & !TOP_BIT)
	}

	/// Whether a target packed by [`pack`](StateId::pack) accepts.
	#[inline]
	pub(crate) fn unpack_accepting(packed: u32) -> bool {
		packed & TOP_BIT != 0
	}
}

impl From<StateId> for u32 {
	#[inline]
	fn from(state: StateId) -> u32 {
		state.0
	}
}

impl From<StateId> for u64 {
	#[inline]
	fn from(state: StateId) -> u64 {
		u64::from(state.0)
	}
}

impl fmt::Debug for StateId {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		fmt::Debug::fmt(&self.0, f)
	}
}
