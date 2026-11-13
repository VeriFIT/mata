//! A tuple of words in a relation.

use std::fmt;

use crate::Symbol;

/// A tuple of words in a relation, as the steps that read it, one symbol per tape and step.
///
/// The symbols are concrete, or [`EPSILON`](Symbol::EPSILON) where a tape pads a shorter word.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub struct Witness {
	arity: usize,
	symbols: Vec<Symbol>,
}

impl Witness {
	pub(crate) fn new(arity: usize, symbols: Vec<Symbol>) -> Witness {
		debug_assert!(arity == 0 || symbols.len().is_multiple_of(arity));

		Witness { arity, symbols }
	}

	/// The number of tapes.
	#[must_use]
	pub fn arity(&self) -> usize {
		self.arity
	}

	/// The number of steps. Always 0 for arity 0, where a step reads nothing.
	#[must_use]
	pub fn len(&self) -> usize {
		self.symbols.len().checked_div(self.arity).unwrap_or(0)
	}

	/// Whether the witness has no steps (the tuple of empty words).
	#[must_use]
	pub fn is_empty(&self) -> bool {
		self.symbols.is_empty()
	}

	/// The symbols of every step.
	pub fn steps(&self) -> impl ExactSizeIterator<Item = &[Symbol]> + DoubleEndedIterator {
		self.symbols.chunks(self.arity.max(1))
	}

	/// The word on `tape`, without padding, or `None` if `tape` is not below the arity.
	#[must_use]
	pub fn tape(&self, tape: usize) -> Option<Vec<Symbol>> {
		(tape < self.arity).then(|| {
			self.steps()
				.map(|step| step[tape])
				.filter(|&symbol| symbol != Symbol::EPSILON)
				.collect()
		})
	}

	/// Writes the steps, with `name` for the symbols that have one.
	pub(crate) fn write<'n>(
		&self,
		f: &mut fmt::Formatter<'_>,
		name: impl Fn(Symbol) -> Option<&'n str>,
	) -> fmt::Result {
		if self.is_empty() {
			return f.write_str("ε");
		}

		let write_symbol = |f: &mut fmt::Formatter<'_>, symbol: Symbol| match name(symbol) {
			Some(named) => f.write_str(named),
			None => write!(f, "{symbol}"),
		};

		for (index, step) in self.steps().enumerate() {
			if index > 0 {
				f.write_str(" ")?;
			}

			if let [symbol] = *step {
				write_symbol(f, symbol)?;
			} else {
				f.write_str("(")?;
				for (tape, &symbol) in step.iter().enumerate() {
					if tape > 0 {
						f.write_str(", ")?;
					}

					write_symbol(f, symbol)?;
				}

				f.write_str(")")?;
			}
		}

		Ok(())
	}
}

/// Prints the steps separated by spaces, each step of several tapes as a tuple, such as `(97, 98) (97, ε)`, or
/// `ε` for no steps.
impl fmt::Display for Witness {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		self.write(f, |_| None)
	}
}
