//! String-solving operations: Mata's `mata::applications::strings`, with its `seg_nfa` and `replace`
//! namespaces.
//!
//! The word queries work on the language: they remove epsilon transitions from a copy first, as the
//! language checks of [`Nfa`] do, so epsilon never counts as a letter. The replace constructions build
//! two-level transducers (input on level 0, output on level 1) over the symbols of an [`Alphabet`].
//!
//! ```
//! use mata::strings::{self, ReplaceMode};
//! use mata::{Alphabet, Nfa, Symbol};
//!
//! let (a, b) = (Symbol::from(b'a'), Symbol::from(b'b'));
//! let nfa = Nfa::from_regex("ab|ba|b")?;
//! assert_eq!(strings::shortest_words(&nfa), vec![vec![b]]);
//! assert_eq!(strings::longest_words(&nfa), Some(vec![vec![a, b], vec![b, a]]));
//!
//! // Every a becomes b.
//! let alphabet = Alphabet::from_symbols([a, b])?;
//! let nft = strings::replace_reluctant_single_symbol(a, &[b], &alphabet, ReplaceMode::All)?;
//! assert!(nft.accepts(&[vec![a, b, a], vec![b, b, b]]));
//! # Ok::<(), mata::Error>(())
//! ```

use mata_sys::cxx::UniquePtr;
use mata_sys::ffi;
use mata_sys::nft::ffi as nft_ffi;

use crate::nft::{DONT_CARE, Nft};
use crate::types::{symbols_to_raw, word_from_raw};
use crate::{Alphabet, Nfa, NftError, RegexError, ReplaceError, Symbol, Word, check};

/// The default begin marker of [`replace_reluctant_regex`] (Mata's `replace::BEGIN_MARKER`).
pub const BEGIN_MARKER: Symbol = Symbol::new(mata_sys::nft::BEGIN_MARKER);

/// The default end marker of [`replace_reluctant_literal`] (Mata's `replace::END_MARKER`).
pub const END_MARKER: Symbol = Symbol::new(mata_sys::nft::END_MARKER);

/// How many occurrences a replace transducer replaces (Mata's `replace::ReplaceMode`).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub enum ReplaceMode {
	/// Only the first (leftmost) occurrence.
	Single,
	/// All occurrences, from left to right.
	#[default]
	All,
}

impl ReplaceMode {
	fn single(self) -> bool {
		self == ReplaceMode::Single
	}
}

/// Splits words that the bridge returned concatenated.
fn words_from_raw(symbols: Vec<u32>, lengths: &[usize]) -> Vec<Word> {
	let mut symbols = word_from_raw(symbols).into_iter();
	lengths
		.iter()
		.map(|&length| symbols.by_ref().take(length).collect())
		.collect()
}

/// The shortest accepted words, ascending; none if the language is empty.
#[must_use]
#[doc(alias = "get_shortest_words")]
pub fn shortest_words(nfa: &Nfa) -> Vec<Word> {
	let mut symbols = Vec::new();
	let mut lengths = Vec::new();
	check(ffi::strings_shortest_words(
		nfa.as_ffi(),
		&mut symbols,
		&mut lengths,
	));
	words_from_raw(symbols, &lengths)
}

/// The longest accepted words, ascending (none if the language is empty), or `None` if the language is
/// infinite.
#[must_use]
#[doc(alias = "get_longest_words")]
pub fn longest_words(nfa: &Nfa) -> Option<Vec<Word>> {
	let mut symbols = Vec::new();
	let mut lengths = Vec::new();
	check(ffi::strings_longest_words(
		nfa.as_ffi(),
		&mut symbols,
		&mut lengths,
	))
	.then(|| words_from_raw(symbols, &lengths))
}

/// The symbols that are accepted words of length one, ascending.
#[must_use]
#[doc(alias = "get_accepted_symbols")]
pub fn accepted_symbols(nfa: &Nfa) -> Vec<Symbol> {
	word_from_raw(check(ffi::strings_accepted_symbols(nfa.as_ffi())))
}

/// The lengths of the accepted words as pairs (*offset*, *period*), ascending: the language has a word of
/// length *n* iff *n* = *offset* + *k* · *period* for some pair and some *k* ≥ 0. A period of 0 stands
/// for the single length *offset*. Empty if the language is empty.
#[must_use]
#[doc(alias = "get_word_lengths")]
pub fn word_lengths(nfa: &Nfa) -> Vec<(usize, usize)> {
	check(ffi::strings_word_lengths(nfa.as_ffi()))
		.as_chunks::<2>()
		.0
		.iter()
		.map(|pair| (pair[0], pair[1]))
		.collect()
}

/// Whether the language is exactly {ε}.
#[must_use]
pub fn is_lang_eps(nfa: &Nfa) -> bool {
	check(ffi::strings_is_lang_eps(nfa.as_ffi()))
}

/// The segments of a segment automaton, from the one of its initial states to the one of its final
/// states, each trimmed (Mata's `seg_nfa::Segmentation::get_segments`).
///
/// A segment automaton is split into segments by its transitions over the symbols `epsilons`, which form
/// a chain: every such transition leads from one segment to the next, none lies on a cycle, initial states
/// are in the first segment and final states in the last. The segments of another automaton are
/// whatever Mata's breadth-first search yields; Mata does not check the shape.
#[must_use]
#[doc(alias = "get_segments")]
pub fn segments(nfa: &Nfa, epsilons: &[Symbol]) -> Vec<Nfa> {
	let mut parsed = check(ffi::strings_segments(
		nfa.as_ffi(),
		symbols_to_raw(epsilons),
	));
	(0..ffi::parsed_len(&parsed))
		.map(|index| Nfa::from_owned(check(ffi::parsed_take(parsed.pin_mut(), index))))
		.collect()
}

/// The automaton without the transitions that leave final states, so it accepts only the words none of
/// whose proper prefixes it accepts (Mata's `replace::reluctant_nfa`).
#[must_use]
pub fn reluctant_nfa(nfa: &Nfa) -> Nfa {
	Nfa::from_owned(check(ffi::strings_reluctant_nfa(nfa.as_ffi())))
}

/// One word per level, with `lengths[i]` symbols on level `i`, whose tuple is in the relation of `nft`,
/// or `None` if there is none (Mata's `get_words_of_lengths`).
///
/// # Errors
///
/// [`NftError::LevelMismatch`] if `lengths` does not have one length per level, and
/// [`NftError::TooManyConfigurations`] if the lengths span more search configurations than can be indexed.
#[doc(alias = "get_words_of_lengths")]
pub fn words_of_lengths(nft: &Nft, lengths: &[usize]) -> Result<Option<Vec<Word>>, NftError> {
	if lengths.len() != nft.num_levels() {
		return Err(NftError::LevelMismatch {
			expected: nft.num_levels(),
			found: lengths.len(),
		});
	}
	let lengths = lengths
		.iter()
		.map(|&length| u32::try_from(length).map_err(|_| NftError::TooManyConfigurations))
		.collect::<Result<Vec<_>, _>>()?;
	let mut symbols = Vec::new();
	let mut word_lengths = Vec::new();
	// With one length per level and no jumps, the only failure left is the configuration count.
	let found =
		nft_ffi::strings_words_of_lengths(nft.as_ffi(), &lengths, &mut symbols, &mut word_lengths)
			.map_err(|_| NftError::TooManyConfigurations)?;
	Ok(found.then(|| words_from_raw(symbols, &word_lengths)))
}

/// The symbols of `alphabet` as the bridge takes them, rejecting those the construction reserves.
fn replace_symbols(alphabet: &Alphabet, marker: Option<Symbol>) -> Result<Vec<u32>, ReplaceError> {
	for symbol in [Some(DONT_CARE), marker].into_iter().flatten() {
		if alphabet.contains(symbol) {
			return Err(ReplaceError::ReservedSymbol { symbol });
		}
	}
	Ok(alphabet.raw_symbols())
}

fn require_symbol(alphabet: &Alphabet, symbol: Symbol) -> Result<(), ReplaceError> {
	if alphabet.contains(symbol) {
		Ok(())
	} else {
		Err(ReplaceError::SymbolNotInAlphabet { symbol })
	}
}

/// Unwraps a construction whose arguments were checked already.
fn built(result: Result<UniquePtr<nft_ffi::Nft>, mata_sys::cxx::Exception>) -> Nft {
	Nft::from_owned(check(result))
}

/// The identity relation on all words over `alphabet`, with `num_levels` levels (Mata's
/// `replace::create_identity`).
///
/// # Errors
///
/// [`ReplaceError::Nft`] if `num_levels` is 0 or more than [`MAX_LEVELS`](crate::nft::MAX_LEVELS), and
/// [`ReplaceError::ReservedSymbol`] if `alphabet` contains [`DONT_CARE`].
pub fn create_identity(alphabet: &Alphabet, num_levels: usize) -> Result<Nft, ReplaceError> {
	if num_levels == 0 || num_levels > crate::nft::MAX_LEVELS {
		return Err(NftError::InvalidNumLevels { num_levels }.into());
	}
	let symbols = replace_symbols(alphabet, None)?;
	Ok(built(nft_ffi::strings_create_identity(
		&symbols, num_levels,
	)))
}

/// The two-level identity over `alphabet` that writes `replacement` for `from_symbol`: every occurrence
/// with [`ReplaceMode::All`], the first one with [`ReplaceMode::Single`] (Mata's
/// `replace::create_identity_with_single_symbol_replace`; a one-symbol `replacement` is its `Symbol`
/// overload).
///
/// # Errors
///
/// [`ReplaceError::SymbolNotInAlphabet`] if `from_symbol` is not in `alphabet`, and
/// [`ReplaceError::ReservedSymbol`] if `alphabet` contains [`DONT_CARE`].
pub fn create_identity_with_single_symbol_replace(
	alphabet: &Alphabet,
	from_symbol: Symbol,
	replacement: &[Symbol],
	mode: ReplaceMode,
) -> Result<Nft, ReplaceError> {
	let symbols = replace_symbols(alphabet, None)?;
	require_symbol(alphabet, from_symbol)?;
	Ok(built(nft_ffi::strings_identity_with_replace(
		&symbols,
		from_symbol.value(),
		symbols_to_raw(replacement),
		mode.single(),
	)))
}

/// The reluctant leftmost replace of the RE2 pattern `regex` by `replacement` over `alphabet`: each
/// occurrence is the shortest leftmost match (Mata's `replace::replace_reluctant_regex`). The pattern
/// reads bytes, as [`Nfa::from_regex`] does. `begin_marker` is a symbol the construction uses internally,
/// [`BEGIN_MARKER`] by default.
///
/// # Errors
///
/// [`ReplaceError::Regex`] if the pattern is invalid, and [`ReplaceError::ReservedSymbol`] if `alphabet`
/// contains [`DONT_CARE`] or `begin_marker`.
pub fn replace_reluctant_regex(
	regex: &str,
	replacement: &[Symbol],
	alphabet: &Alphabet,
	mode: ReplaceMode,
	begin_marker: Symbol,
) -> Result<Nft, ReplaceError> {
	let symbols = replace_symbols(alphabet, Some(begin_marker))?;
	nft_ffi::strings_replace_regex(
		regex,
		symbols_to_raw(replacement),
		&symbols,
		mode.single(),
		begin_marker.value(),
	)
	.map(Nft::from_owned)
	.map_err(|error| RegexError::new(error.what()).into())
}

/// [`replace_reluctant_regex`] with the language of `regex` as the part to replace. Mata needs a
/// deterministic automaton; `regex` is made epsilon-free and deterministic first, as the pattern form does.
///
/// # Errors
///
/// [`ReplaceError::ReservedSymbol`] if `alphabet` contains [`DONT_CARE`] or `begin_marker`.
pub fn replace_reluctant_regex_nfa(
	regex: &Nfa,
	replacement: &[Symbol],
	alphabet: &Alphabet,
	mode: ReplaceMode,
	begin_marker: Symbol,
) -> Result<Nft, ReplaceError> {
	let symbols = replace_symbols(alphabet, Some(begin_marker))?;
	Ok(built(nft_ffi::strings_replace_regex_nfa(
		regex.as_ffi(),
		symbols_to_raw(replacement),
		&symbols,
		mode.single(),
		begin_marker.value(),
	)))
}

/// The reluctant leftmost replace of the word `literal` by `replacement` over `alphabet` (Mata's
/// `replace::replace_reluctant_literal`). `end_marker` is a symbol the construction uses internally,
/// [`END_MARKER`] by default.
///
/// # Errors
///
/// [`ReplaceError::ReservedSymbol`] if `alphabet` contains [`DONT_CARE`] or `end_marker`.
pub fn replace_reluctant_literal(
	literal: &[Symbol],
	replacement: &[Symbol],
	alphabet: &Alphabet,
	mode: ReplaceMode,
	end_marker: Symbol,
) -> Result<Nft, ReplaceError> {
	let symbols = replace_symbols(alphabet, Some(end_marker))?;
	Ok(built(nft_ffi::strings_replace_literal(
		symbols_to_raw(literal),
		symbols_to_raw(replacement),
		&symbols,
		mode.single(),
		end_marker.value(),
	)))
}

/// The reluctant leftmost replace of `from_symbol` by `replacement` over `alphabet` (Mata's
/// `replace::replace_reluctant_single_symbol`; a one-symbol `replacement` is its `Symbol` overload).
///
/// # Errors
///
/// [`ReplaceError::SymbolNotInAlphabet`] if `from_symbol` is not in `alphabet`, and
/// [`ReplaceError::ReservedSymbol`] if `alphabet` contains [`DONT_CARE`].
pub fn replace_reluctant_single_symbol(
	from_symbol: Symbol,
	replacement: &[Symbol],
	alphabet: &Alphabet,
	mode: ReplaceMode,
) -> Result<Nft, ReplaceError> {
	let symbols = replace_symbols(alphabet, None)?;
	require_symbol(alphabet, from_symbol)?;
	Ok(built(nft_ffi::strings_replace_symbol(
		from_symbol.value(),
		symbols_to_raw(replacement),
		&symbols,
		mode.single(),
	)))
}
