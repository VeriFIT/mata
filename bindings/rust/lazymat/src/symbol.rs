//! Symbols, numbered as in Mata.

use std::fmt;

/// A symbol, a number that all leaves of a formula share, as Mata numbers them. Two values are reserved.
///
/// A byte is the symbol of the same number, as in Mata's `from_regex`.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Symbol(u32);

impl Symbol {
	/// Padding, where the tape reads nothing while the other tapes read (Mata's `EPSILON`, the largest `u32`).
	pub const EPSILON: Symbol = Symbol(u32::MAX);
	/// Any one symbol of the tape's alphabet (Mata's `DONT_CARE`, the `u32` below `EPSILON`).
	pub const ANY: Symbol = Symbol(u32::MAX - 1);

	/// The symbol with number `value`.
	#[must_use]
	pub const fn new(value: u32) -> Symbol {
		Symbol(value)
	}

	/// The number of this symbol.
	#[must_use]
	pub const fn value(self) -> u32 {
		self.0
	}

	/// Neither padding nor the wildcard.
	#[must_use]
	pub const fn is_concrete(self) -> bool {
		self.0 < Symbol::ANY.0
	}
}

/// The symbol that both symbols read, if they agree. [`ANY`](Symbol::ANY) takes the other symbol.
pub(crate) fn merge(lhs: Symbol, rhs: Symbol) -> Option<Symbol> {
	if lhs == rhs {
		return Some(lhs);
	}

	match (lhs, rhs) {
		(Symbol::EPSILON, _) | (_, Symbol::EPSILON) => None,
		(Symbol::ANY, other) | (other, Symbol::ANY) => Some(other),
		_ => None,
	}
}

impl From<u8> for Symbol {
	fn from(byte: u8) -> Symbol {
		Symbol(byte.into())
	}
}

impl From<u32> for Symbol {
	fn from(value: u32) -> Symbol {
		Symbol(value)
	}
}

impl From<Symbol> for u32 {
	fn from(symbol: Symbol) -> u32 {
		symbol.0
	}
}

#[cfg(feature = "mata")]
impl From<mata::Symbol> for Symbol {
	fn from(symbol: mata::Symbol) -> Symbol {
		Symbol(symbol.value())
	}
}

#[cfg(feature = "mata")]
impl From<Symbol> for mata::Symbol {
	fn from(symbol: Symbol) -> mata::Symbol {
		mata::Symbol::new(symbol.0)
	}
}

/// Prints the number, `ε` for [`EPSILON`](Symbol::EPSILON) or `*` for [`ANY`](Symbol::ANY).
impl fmt::Display for Symbol {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		match *self {
			Symbol::EPSILON => f.write_str("ε"),
			Symbol::ANY => f.write_str("*"),
			Symbol(value) => write!(f, "{value}"),
		}
	}
}
