//! The atoms of WS1S as automata over the bits of their arguments.
//!
//! A first-order argument is a position, encoded as a singleton set: its tape reads [`ONE`] exactly once. Every
//! atom accepts only words where its first-order arguments are singletons, and keeps accepting when zero columns
//! follow, as [`Padding::Zero`](crate::Padding::Zero) needs.

use mata_lazy::{Automaton, Symbol};

use crate::{ONE, ZERO};

/// How a value compares to a bound.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum Cmp {
	Eq,
	Le,
	Ge,
}

impl Cmp {
	fn holds(self, value: i64, bound: i64) -> bool {
		match self {
			Cmp::Eq => value == bound,
			Cmp::Le => value <= bound,
			Cmp::Ge => value >= bound,
		}
	}
}

/// An atom; `x`, `y` are first-order arguments and `X`, `Y` second-order ones, in this order.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum Atom {
	/// `x` is a position.
	Singleton,
	/// `x cmp k`.
	Position(Cmp, i64),
	/// `y − x cmp d`.
	Distance(Cmp, i64),
	/// `x + k ∈ X`.
	Member(u32),
	/// `k ∈ X`.
	Contains(u32),
	/// `X ⊆ Y`.
	Subset,
	/// `X = Y`.
	Equal,
	/// `X = ∅`.
	Empty,
}

impl Atom {
	/// The number of arguments.
	pub(crate) fn arguments(self) -> usize {
		match self {
			Atom::Singleton | Atom::Position(..) | Atom::Contains(_) | Atom::Empty => 1,
			Atom::Distance(..) | Atom::Member(_) | Atom::Subset | Atom::Equal => 2,
		}
	}

	fn initial(self) -> State {
		match self {
			Atom::Contains(k) => State::Wait(k),
			_ => State::Start(0),
		}
	}

	fn is_final(self, state: State) -> bool {
		match self {
			Atom::Subset | Atom::Equal | Atom::Empty => true,
			_ => state == State::Done,
		}
	}

	/// The state after reading `bits`, one per argument, or `None` if the word is rejected.
	fn step(self, state: State, bits: [bool; 2]) -> Option<State> {
		let [first, second] = bits;
		match (self, state) {
			(Atom::Subset, _) => (!first || second).then_some(state),
			(Atom::Equal, _) => (first == second).then_some(state),
			(Atom::Empty, _) => (!first).then_some(state),
			// After the position, nothing but zeros on the first-order arguments.
			(Atom::Singleton | Atom::Position(..) | Atom::Distance(..), State::Done) => {
				(!first && !second).then_some(State::Done)
			}
			(Atom::Member(_), State::Done | State::Wait(_)) if first => None,
			(Atom::Contains(_) | Atom::Member(_), State::Done) => Some(State::Done),
			(Atom::Singleton, _) => Some(if first { State::Done } else { state }),
			(Atom::Position(cmp, k), State::Start(n)) => {
				if first {
					cmp.holds(i64::from(n), k).then_some(State::Done)
				} else {
					// Positions from `saturation` on compare to `k` alike.
					let saturation = u32::try_from((k + 1).max(0)).unwrap_or(u32::MAX);
					Some(State::Start(n.saturating_add(1).min(saturation)))
				}
			}
			(Atom::Distance(cmp, d), State::Start(_)) => match (first, second) {
				(false, false) => Some(state),
				(true, true) => cmp.holds(0, d).then_some(State::Done),
				(x_first, _) => Some(State::Between { x_first, n: 0 }),
			},
			(Atom::Distance(cmp, d), State::Between { x_first, n }) => {
				let (earlier, later) = if x_first {
					(first, second)
				} else {
					(second, first)
				};
				if earlier {
					return None;
				}

				if later {
					let distance = i64::from(n) + 1;
					let difference = if x_first { distance } else { -distance };
					return cmp.holds(difference, d).then_some(State::Done);
				}

				// Distances above |d| + 1 compare to `d` alike.
				let saturation = u32::try_from(d.unsigned_abs() + 1).unwrap_or(u32::MAX);
				Some(State::Between {
					x_first,
					n: n.saturating_add(1).min(saturation),
				})
			}
			(Atom::Member(k), State::Start(_)) => Some(match (first, k) {
				(false, _) => state,
				(true, 0) => return second.then_some(State::Done),
				(true, k) => State::Wait(k - 1),
			}),
			(Atom::Member(_) | Atom::Contains(_), State::Wait(0)) => {
				let member = if matches!(self, Atom::Contains(_)) {
					first
				} else {
					second
				};
				member.then_some(State::Done)
			}
			(Atom::Member(_) | Atom::Contains(_), State::Wait(r)) => Some(State::Wait(r - 1)),
			(_, _) => unreachable!("{self:?} never reaches {state:?}"),
		}
	}
}

/// A state of an [`Atom`].
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub(crate) enum State {
	/// No position read yet, after `n` columns (saturated).
	Start(u32),
	/// One of the two positions read, `n` columns ago (saturated); `x_first` if it was `x`.
	Between { x_first: bool, n: u32 },
	/// `r` more columns before the column whose position must be a member.
	Wait(u32),
	/// The atom holds; the positions have been read.
	Done,
}

/// An [`Atom`] whose argument `i` is read on tape `tapes[i]`. Two arguments may share a tape, as in `x < x`.
pub(crate) struct Leaf {
	pub(crate) atom: Atom,
	pub(crate) tapes: [usize; 2],
	pub(crate) arity: usize,
}

impl Automaton for Leaf {
	type State = State;

	fn arity(&self) -> usize {
		self.arity
	}

	fn initial_states(&self) -> impl IntoIterator<Item = State> {
		[self.atom.initial()]
	}

	fn is_final(&self, state: State) -> bool {
		self.atom.is_final(state)
	}

	fn for_each_move(&self, state: State, mut visit: impl FnMut(&[Symbol], State)) {
		let mut symbols = vec![ZERO; self.arity];
		for column in 0..1_usize << self.arity {
			let bit = |tape: usize| column >> tape & 1 == 1;
			for (tape, symbol) in symbols.iter_mut().enumerate() {
				*symbol = if bit(tape) { ONE } else { ZERO };
			}

			let second = self.atom.arguments() == 2 && bit(self.tapes[1]);
			if let Some(next) = self.atom.step(state, [bit(self.tapes[0]), second]) {
				visit(&symbols, next);
			}
		}
	}
}
