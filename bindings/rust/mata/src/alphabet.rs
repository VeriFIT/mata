//! A set of symbols with optional names, and its iterators.

use std::collections::{BTreeMap, HashMap, btree_map};
use std::fmt;
use std::iter::FusedIterator;

use crate::{AlphabetError, Symbol, Word};

/// The name that maps to [`Symbol::EPSILON`] in the `.mata` format.
const EPSILON_NAME: &str = "<eps>";
/// Mata reserves this name for the "don't care" symbol of transducers.
const DONT_CARE_NAME: &str = "<dntcr>";
/// The symbol that transducers read as any one symbol (`mata::nft::DONT_CARE`), never given to a new name.
const DONT_CARE_VALUE: Symbol = Symbol::new(u32::MAX - 1);

/// Whether printing would write `name` as the number of a symbol.
fn is_numeric(name: &str) -> bool {
	!name.is_empty() && name.bytes().all(|byte| byte.is_ascii_digit())
}

/// A finite set of symbols, each with an optional name.
///
/// An alphabet is a plain Rust value: automata do not keep one. Operations that need an alphabet, such as
/// [`Nfa::complement`](crate::Nfa::complement) or [`Nfa::is_universal`](crate::Nfa::is_universal), borrow
/// it for one call, and [`Nfa::parse_mata`](crate::Nfa::parse_mata) adds the symbol names it reads.
///
/// [`Symbol::EPSILON`] is never a member. The name `<eps>` stands for it, and `<dntcr>` is reserved.
/// [`insert_name`](Alphabet::insert_name) rejects names that consist only of decimal digits: printing
/// would write such a name as the number of a symbol, so a round trip through the `.mata` format
/// would resolve it to that symbol. [`insert_named`](Alphabet::insert_named) binds them.
///
/// ```
/// use mata::{Alphabet, Symbol};
///
/// let mut sigma = Alphabet::from_names(["a", "b"])?;
/// assert_eq!(sigma.symbol("b"), Some(Symbol::new(1)));
/// let c = sigma.insert_name("c")?;
/// assert_eq!(sigma.name(c), Some("c"));
/// assert_eq!(sigma.len(), 3);
/// # Ok::<(), mata::AlphabetError>(())
/// ```
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Alphabet {
	/// Every symbol, with its name if it has one.
	symbols: BTreeMap<Symbol, Option<String>>,
	/// The symbol of every name.
	by_name: HashMap<String, Symbol>,
}

impl Alphabet {
	/// An empty alphabet.
	#[must_use]
	pub fn new() -> Alphabet {
		Alphabet::default()
	}

	/// An alphabet of unnamed symbols.
	///
	/// # Errors
	///
	/// [`AlphabetError::Epsilon`] if a symbol is [`Symbol::EPSILON`].
	pub fn from_symbols(
		symbols: impl IntoIterator<Item = Symbol>,
	) -> Result<Alphabet, AlphabetError> {
		let mut alphabet = Alphabet::new();
		for symbol in symbols {
			alphabet.insert(symbol)?;
		}
		Ok(alphabet)
	}

	/// An alphabet where the `i`-th distinct name gets symbol `i`. `<eps>` stands for
	/// [`Symbol::EPSILON`] and is skipped, so it takes no number.
	///
	/// # Errors
	///
	/// [`AlphabetError::ReservedName`] if a name is `<dntcr>`.
	pub fn from_names<I>(names: I) -> Result<Alphabet, AlphabetError>
	where
		I: IntoIterator,
		I::Item: AsRef<str>,
	{
		let mut alphabet = Alphabet::new();
		for name in names {
			alphabet.insert_name(name.as_ref())?;
		}
		Ok(alphabet)
	}

	/// The number of symbols.
	#[must_use]
	pub fn len(&self) -> usize {
		self.symbols.len()
	}

	/// Whether there are no symbols.
	#[must_use]
	pub fn is_empty(&self) -> bool {
		self.symbols.is_empty()
	}

	/// Whether `symbol` is in the alphabet.
	#[must_use]
	pub fn contains(&self, symbol: Symbol) -> bool {
		self.symbols.contains_key(&symbol)
	}

	/// Adds an unnamed symbol. Returns `false` if it was already present. `u32::MAX - 1` is an ordinary
	/// symbol in an [`Nfa`](crate::Nfa) but [`DONT_CARE`](crate::nft::DONT_CARE), any symbol, in a
	/// transducer.
	///
	/// # Errors
	///
	/// [`AlphabetError::Epsilon`] if `symbol` is [`Symbol::EPSILON`].
	pub fn insert(&mut self, symbol: Symbol) -> Result<bool, AlphabetError> {
		if symbol.is_epsilon() {
			return Err(AlphabetError::Epsilon);
		}
		match self.symbols.entry(symbol) {
			btree_map::Entry::Vacant(entry) => {
				entry.insert(None);
				Ok(true)
			}
			btree_map::Entry::Occupied(_) => Ok(false),
		}
	}

	/// The symbol named `name`, adding a new symbol (one above the largest) if the name is new, as Mata's
	/// on-the-fly alphabet does. `<eps>` gives [`Symbol::EPSILON`] and adds nothing.
	///
	/// Names are printed as they are, so these names do not survive a round trip through the `.mata`
	/// format: names with whitespace or `#`, names in quotes (the parser drops the quotes), names that
	/// start with `%` or `@`, and the empty name.
	///
	/// # Errors
	///
	/// [`AlphabetError::ReservedName`] if `name` is `<dntcr>`,
	/// [`AlphabetError::NumericName`] if `name` is new and consists only of decimal digits (printing would
	/// write it as the number of another symbol, changing the language on the next parse), and
	/// [`AlphabetError::Full`] if the name is new and no symbol below `u32::MAX - 1` (the value of
	/// [`DONT_CARE`](crate::nft::DONT_CARE) in transducers) is left.
	pub fn insert_name(&mut self, name: &str) -> Result<Symbol, AlphabetError> {
		if name == EPSILON_NAME {
			return Ok(Symbol::EPSILON);
		}
		let symbol = self
			.insert_new_name(name)?
			.unwrap_or_else(|| self.next_symbol());
		if symbol >= DONT_CARE_VALUE {
			return Err(AlphabetError::Full);
		}
		self.add_named(name.to_owned(), symbol);
		Ok(symbol)
	}

	/// Binds `name` to exactly `symbol`, as Mata's `OnTheFlyAlphabet::add_new_symbol(name, value)` does.
	/// Nothing changes if the name or the symbol is already in the alphabet; the first binding is kept.
	///
	/// Unlike [`insert_name`](Alphabet::insert_name), the symbol is given instead of the next free one, and a
	/// name that consists only of decimal digits is accepted: the `"bytes"` script alphabet names the byte
	/// symbols `"0"`…`"9"`.
	/// Such a name is safe, because Mata's parser resolves a token to a name before it reads it as a number.
	///
	/// # Errors
	///
	/// [`AlphabetError::Epsilon`] if `symbol` is [`Symbol::EPSILON`] or `name` is `<eps>`,
	/// [`AlphabetError::ReservedName`] if `name` is `<dntcr>`, and
	/// [`AlphabetError::Full`] if `symbol` is [`DONT_CARE`](crate::nft::DONT_CARE).
	pub fn insert_named(&mut self, name: &str, symbol: Symbol) -> Result<(), AlphabetError> {
		if symbol.is_epsilon() || name == EPSILON_NAME {
			return Err(AlphabetError::Epsilon);
		}
		if name == DONT_CARE_NAME {
			return Err(AlphabetError::ReservedName);
		}
		if symbol >= DONT_CARE_VALUE {
			return Err(AlphabetError::Full);
		}
		if self.by_name.contains_key(name) || self.symbols.contains_key(&symbol) {
			return Ok(());
		}
		self.add_named(name.to_owned(), symbol);
		Ok(())
	}

	/// The checks that every way of naming a symbol shares: the reserved names, and a name that
	/// printing would write as the number of a symbol. Returns `Some` for a known name, numeric or not:
	/// [`insert_named`](Alphabet::insert_named) may have bound one.
	fn insert_new_name(&self, name: &str) -> Result<Option<Symbol>, AlphabetError> {
		if name == DONT_CARE_NAME {
			return Err(AlphabetError::ReservedName);
		}
		if let Some(&symbol) = self.by_name.get(name) {
			return Ok(Some(symbol));
		}
		if is_numeric(name) {
			return Err(AlphabetError::NumericName {
				name: name.to_owned(),
			});
		}
		Ok(None)
	}

	/// The symbol named `name`, if any. `<eps>` gives [`Symbol::EPSILON`].
	#[must_use]
	pub fn symbol(&self, name: &str) -> Option<Symbol> {
		if name == EPSILON_NAME {
			Some(Symbol::EPSILON)
		} else {
			self.by_name.get(name).copied()
		}
	}

	/// The name of `symbol`, if it has one.
	#[must_use]
	pub fn name(&self, symbol: Symbol) -> Option<&str> {
		self.symbols.get(&symbol)?.as_deref()
	}

	/// The symbols in ascending order.
	#[must_use]
	pub fn symbols(&self) -> Symbols<'_> {
		Symbols {
			inner: self.symbols.keys(),
		}
	}

	/// `(symbol, name)` pairs in ascending order of symbols.
	#[must_use]
	pub fn iter(&self) -> Iter<'_> {
		Iter {
			inner: self.symbols.iter(),
		}
	}

	/// Translates names into a word. `None` if a name is not in the alphabet.
	///
	/// ```
	/// use mata::Alphabet;
	///
	/// let sigma = Alphabet::from_names(["a", "b"])?;
	/// let word = sigma.encode(["b", "a", "b"]).unwrap();
	/// assert_eq!(sigma.display_word(&word).to_string(), "b a b");
	/// assert_eq!(sigma.encode(["c"]), None);
	/// # Ok::<(), mata::AlphabetError>(())
	/// ```
	#[must_use]
	pub fn encode<'n>(&self, names: impl IntoIterator<Item = &'n str>) -> Option<Word> {
		names.into_iter().map(|name| self.symbol(name)).collect()
	}

	/// Displays `word` with names where the alphabet has them and numbers otherwise, separated by spaces.
	/// The empty word displays as `ε`.
	pub fn display_word<'a>(&'a self, word: &'a [Symbol]) -> DisplayWord<'a> {
		DisplayWord {
			alphabet: self,
			word,
		}
	}

	/// The symbol after the largest one, or 0 for an empty alphabet.
	fn next_symbol(&self) -> Symbol {
		self.symbols
			.last_key_value()
			.map_or(Symbol::new(0), |(last, _)| {
				Symbol::new(last.value().saturating_add(1))
			})
	}

	fn add_named(&mut self, name: String, symbol: Symbol) {
		self.by_name.insert(name.clone(), symbol);
		self.symbols.insert(symbol, Some(name));
	}

	/// The symbols, ascending, as the bridge takes them.
	pub(crate) fn raw_symbols(&self) -> Vec<u32> {
		self.symbols.keys().map(|symbol| symbol.value()).collect()
	}

	/// The named symbols as parallel lists of names and symbols, as the bridge takes them.
	pub(crate) fn raw_names(&self) -> (Vec<String>, Vec<u32>) {
		self.symbols
			.iter()
			.filter_map(|(symbol, name)| Some((name.clone()?, symbol.value())))
			.unzip()
	}

	/// The names that the parser knows: every name, and the number of every unnamed symbol whose number is
	/// not a name, as printing writes it.
	pub(crate) fn raw_parse_names(&self) -> (Vec<String>, Vec<u32>) {
		self.symbols
			.iter()
			.filter_map(|(symbol, name)| {
				if let Some(name) = name {
					return Some((name.clone(), symbol.value()));
				}
				let number = symbol.value().to_string();
				(!self.by_name.contains_key(&number)).then_some((number, symbol.value()))
			})
			.unzip()
	}

	/// The symbol that the parser gives to the first new name.
	pub(crate) fn raw_next_symbol(&self) -> u32 {
		self.next_symbol().value()
	}

	/// Records the names that the parser added. Skips names that printing would write as the number
	/// of a symbol: the shim only mints such names for a numeric token that already names a symbol,
	/// where the parser has resolved it to that symbol.
	pub(crate) fn add_parsed_names(&mut self, names: Vec<String>, symbols: &[u32]) {
		for (name, &symbol) in names.into_iter().zip(symbols) {
			if is_numeric(&name) {
				continue;
			}
			self.add_named(name, Symbol::new(symbol));
		}
	}
}

impl<'a> IntoIterator for &'a Alphabet {
	type Item = (Symbol, Option<&'a str>);
	type IntoIter = Iter<'a>;

	fn into_iter(self) -> Iter<'a> {
		self.iter()
	}
}

/// Iterator returned by [`Alphabet::symbols`].
#[derive(Clone, Debug)]
pub struct Symbols<'a> {
	inner: btree_map::Keys<'a, Symbol, Option<String>>,
}

impl Iterator for Symbols<'_> {
	type Item = Symbol;

	fn next(&mut self) -> Option<Symbol> {
		self.inner.next().copied()
	}

	fn size_hint(&self) -> (usize, Option<usize>) {
		self.inner.size_hint()
	}
}

impl DoubleEndedIterator for Symbols<'_> {
	fn next_back(&mut self) -> Option<Symbol> {
		self.inner.next_back().copied()
	}
}

impl ExactSizeIterator for Symbols<'_> {}

impl FusedIterator for Symbols<'_> {}

/// Iterator returned by [`Alphabet::iter`].
#[derive(Clone, Debug)]
pub struct Iter<'a> {
	inner: btree_map::Iter<'a, Symbol, Option<String>>,
}

impl<'a> Iterator for Iter<'a> {
	type Item = (Symbol, Option<&'a str>);

	fn next(&mut self) -> Option<Self::Item> {
		self.inner
			.next()
			.map(|(&symbol, name)| (symbol, name.as_deref()))
	}

	fn size_hint(&self) -> (usize, Option<usize>) {
		self.inner.size_hint()
	}
}

impl DoubleEndedIterator for Iter<'_> {
	fn next_back(&mut self) -> Option<Self::Item> {
		self.inner
			.next_back()
			.map(|(&symbol, name)| (symbol, name.as_deref()))
	}
}

impl ExactSizeIterator for Iter<'_> {}

impl FusedIterator for Iter<'_> {}

/// Adapter returned by [`Alphabet::display_word`]; prints the word with `{}`.
#[derive(Clone, Copy, Debug)]
#[must_use = "an adapter does nothing until it is formatted"]
pub struct DisplayWord<'a> {
	alphabet: &'a Alphabet,
	word: &'a [Symbol],
}

impl fmt::Display for DisplayWord<'_> {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		if self.word.is_empty() {
			return f.write_str("ε");
		}
		for (i, &symbol) in self.word.iter().enumerate() {
			if i > 0 {
				f.write_str(" ")?;
			}
			match self.alphabet.name(symbol) {
				Some(name) => f.write_str(name)?,
				None => write!(f, "{symbol}")?,
			}
		}
		Ok(())
	}
}

#[cfg(feature = "serde")]
mod serde_impl {
	use serde::{Deserialize, Deserializer, Serialize, Serializer};

	use super::Alphabet;
	use crate::Symbol;

	/// An alphabet serializes as a list of `(symbol, name)` pairs in ascending order of symbols.
	impl Serialize for Alphabet {
		fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
			serializer.collect_seq(self.iter())
		}
	}

	impl<'de> Deserialize<'de> for Alphabet {
		fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Alphabet, D::Error> {
			let pairs = Vec::<(Symbol, Option<String>)>::deserialize(deserializer)?;
			let mut alphabet = Alphabet::new();
			for (symbol, name) in pairs {
				if symbol.is_epsilon() {
					return Err(serde::de::Error::custom(
						"epsilon is not a member of any alphabet",
					));
				}
				if alphabet.contains(symbol) {
					return Err(serde::de::Error::custom(format!(
						"symbol {symbol} appears twice"
					)));
				}
				match name {
					Some(name) if name == super::EPSILON_NAME || name == super::DONT_CARE_NAME => {
						return Err(serde::de::Error::custom(format!(
							"the name {name} is reserved"
						)));
					}
					// Numeric names are accepted, as `insert_named` binds them.
					Some(name) if alphabet.by_name.contains_key(&name) => {
						return Err(serde::de::Error::custom(format!(
							"name {name:?} appears twice"
						)));
					}
					Some(name) => alphabet.add_named(name, symbol),
					None => {
						alphabet.symbols.insert(symbol, None);
					}
				}
			}
			Ok(alphabet)
		}
	}
}
