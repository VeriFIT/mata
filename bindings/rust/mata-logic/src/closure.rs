//! The automata that the connectives add.

use mata_lazy::{Automaton, ExplicitAutomaton, Symbol};
use rustc_hash::FxHashMap;

use crate::Padding;
use crate::logic::{BITS, ZERO};

/// The relation without tapes that holds everything if it is true. A step of a relation without tapes reads
/// nothing but still counts, so that a product with a relation of tapes meets words of every length.
pub(crate) struct Constant(pub(crate) bool);

impl Automaton for Constant {
	type State = ();

	fn arity(&self) -> usize {
		0
	}

	fn initial_states(&self) -> impl IntoIterator<Item = ()> {
		[()]
	}

	fn is_final(&self, (): ()) -> bool {
		self.0
	}

	fn for_each_move(&self, (): (), mut visit: impl FnMut(&[Symbol], ())) {
		visit(&[], ());
	}
}

/// Every word over `tapes` (at least one) tapes of bits, the empty word only if not `nonempty`.
pub(crate) struct Encodings {
	pub(crate) tapes: usize,
	pub(crate) nonempty: bool,
}

impl Automaton for Encodings {
	/// Whether a column has been read.
	type State = bool;

	fn arity(&self) -> usize {
		self.tapes
	}

	fn initial_states(&self) -> impl IntoIterator<Item = bool> {
		[false]
	}

	fn is_final(&self, read: bool) -> bool {
		read || !self.nonempty
	}

	fn for_each_move(&self, _: bool, mut visit: impl FnMut(&[Symbol], bool)) {
		visit(&vec![Symbol::ANY; self.tapes], true);
	}
}

/// An explicit automaton closed under padding: a word is accepted if it is accepted once padded.
///
/// With [`Padding::Sign`] a state also holds the last column read, so the moves that read [`ANY`](Symbol::ANY)
/// are split into one per bit.
pub(crate) struct Padded {
	arity: usize,
	initial: Vec<usize>,
	finals: Vec<bool>,
	moves: Vec<Vec<(Box<[Symbol]>, usize)>>,
}

/// A state of [`Padded::signs`]: a state of the explicit automaton and the last column read.
type SignState = (usize, Option<Box<[Symbol]>>);

impl Padded {
	pub(crate) fn new(automaton: &ExplicitAutomaton, padding: Padding) -> Padded {
		match padding {
			Padding::Zero => Padded::zeros(automaton),
			Padding::Sign => Padded::signs(automaton),
		}
	}

	/// Final: a final state is reachable on columns of zeros.
	fn zeros(automaton: &ExplicitAutomaton) -> Padded {
		let num_states = automaton.num_states();
		let zeros = vec![ZERO; automaton.arity()];
		let mut sources = vec![Vec::new(); num_states];
		let mut moves = Vec::with_capacity(num_states);
		for state in 0..num_states {
			let mut out = Vec::with_capacity(automaton.moves(state).len());
			for (tuple, target) in automaton.moves(state) {
				if reads(tuple, &zeros) {
					sources[target].push(state);
				}
				out.push((tuple.into(), target));
			}
			moves.push(out);
		}

		let mut finals: Vec<bool> = (0..num_states)
			.map(|state| automaton.is_final(state))
			.collect();
		let mut stack: Vec<usize> = (0..num_states).filter(|&state| finals[state]).collect();
		while let Some(state) = stack.pop() {
			for &source in &sources[state] {
				if !finals[source] {
					finals[source] = true;
					stack.push(source);
				}
			}
		}

		Padded {
			arity: automaton.arity(),
			initial: automaton.initial_states().collect(),
			finals,
			moves,
		}
	}

	/// States `(state, last column)`, final when a final state is reachable on the last column again.
	fn signs(automaton: &ExplicitAutomaton) -> Padded {
		let arity = automaton.arity();
		// The last column is `None` before the first column, which a word with signs always has.
		let mut numbers: FxHashMap<SignState, usize> = FxHashMap::default();
		let mut pending = Vec::new();
		let mut number = |key: SignState, pending: &mut Vec<_>| {
			let next = numbers.len();
			*numbers.entry(key.clone()).or_insert_with(|| {
				pending.push((next, key));
				next
			})
		};

		let initial: Vec<usize> = automaton
			.initial_states()
			.map(|state| number((state, None), &mut pending))
			.collect();
		let mut finals = Vec::new();
		let mut moves = Vec::new();
		while let Some((id, (state, last))) = pending.pop() {
			if moves.len() <= id {
				moves.resize_with(id + 1, Vec::new);
				finals.resize(id + 1, false);
			}

			finals[id] = last
				.as_deref()
				.is_some_and(|column| reaches_final(automaton, state, column));
			let mut out = Vec::new();
			for (tuple, target) in automaton.moves(state) {
				for column in columns(tuple) {
					let target = number((target, Some(column.clone())), &mut pending);
					out.push((column, target));
				}
			}
			moves[id] = out;
		}

		Padded {
			arity,
			initial,
			finals,
			moves,
		}
	}
}

impl Automaton for Padded {
	type State = usize;

	fn arity(&self) -> usize {
		self.arity
	}

	fn initial_states(&self) -> impl IntoIterator<Item = usize> {
		self.initial.iter().copied()
	}

	fn is_final(&self, state: usize) -> bool {
		self.finals[state]
	}

	fn for_each_move(&self, state: usize, mut visit: impl FnMut(&[Symbol], usize)) {
		for (tuple, target) in &self.moves[state] {
			visit(tuple, *target);
		}
	}
}

/// Whether a move that reads `tuple` reads the column `column` of bits.
fn reads(tuple: &[Symbol], column: &[Symbol]) -> bool {
	tuple
		.iter()
		.zip(column)
		.all(|(&symbol, &bit)| symbol == bit || symbol == Symbol::ANY)
}

/// The columns of bits that `tuple` reads, one per bit of each [`ANY`](Symbol::ANY).
fn columns(tuple: &[Symbol]) -> Vec<Box<[Symbol]>> {
	let mut columns: Vec<Box<[Symbol]>> = vec![tuple.into()];
	for tape in 0..tuple.len() {
		if tuple[tape] == Symbol::ANY {
			columns = columns
				.into_iter()
				.flat_map(|column| {
					BITS.map(|bit| {
						let mut column = column.clone();
						column[tape] = bit;
						column
					})
				})
				.collect();
		}
	}

	columns
}

/// Whether a final state is reachable from `state` on `column` repeated.
fn reaches_final(automaton: &ExplicitAutomaton, state: usize, column: &[Symbol]) -> bool {
	let mut seen = vec![false; automaton.num_states()];
	let mut stack = vec![state];
	seen[state] = true;
	while let Some(state) = stack.pop() {
		if automaton.is_final(state) {
			return true;
		}

		for (tuple, target) in automaton.moves(state) {
			if reads(tuple, column) && !seen[target] {
				seen[target] = true;
				stack.push(target);
			}
		}
	}

	false
}
