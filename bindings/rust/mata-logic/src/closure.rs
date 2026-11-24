//! The automata that the connectives add.

use mata_lazy::{Automaton, Symbol};

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
