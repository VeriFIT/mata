//! Finite transducers: Mata's experimental `mata::nft::Nft`.
//!
//! A transducer with *n* levels (tapes) accepts a relation over *n*-tuples of words. Mata encodes one
//! transducer transition as a chain of single-symbol edges through states of levels 0, 1, …, *n* − 1 and
//! back to level 0: the edge leaving a level-*i* state reads the symbol of tape *i*. [`Symbol::EPSILON`]
//! on a tape reads nothing there, and [`DONT_CARE`] reads any one symbol. Only the states of level 0 are
//! the "real" states of the transducer; initial and final states always have level 0. Where an [`Nfa`]
//! turns into a tape ([`Nft::identity`], [`Nft::apply`], [`Nft::restrict`]), a symbol of the automaton
//! equal to [`DONT_CARE`] becomes the wildcard.
//!
//! Mata's transducer interface is experimental and moving fast, so expect breaking changes in this API.
//!
//! ```
//! use mata::nft::Nft;
//! use mata::{Nfa, Symbol};
//!
//! let (a, b) = (Symbol::from(b'a'), Symbol::from(b'b'));
//! // Replace every `a` by `b`, keep every `b`.
//! let mut t = Nft::new(2)?;
//! let q = t.add_state();
//! t.add_initial(q)?;
//! t.add_final(q)?;
//! t.add_transition(q, &[a, b], q)?;
//! t.add_transition(q, &[b, b], q)?;
//!
//! assert!(t.accepts(&[[a, b, a], [b, b, b]]));
//! assert!(!t.accepts(&[[a], [a]]));
//!
//! // The image of a(a|b)* is b+.
//! let input = Nfa::from_regex("a(a|b)*")?;
//! let image = t.apply(&input, 0)?.to_nfa();
//! assert!(image.is_equivalent_to(&Nfa::from_regex("b+")?));
//! # Ok::<(), mata::Error>(())
//! ```

use std::fmt;
use std::str::FromStr;

use mata_sys::cxx::UniquePtr;
use mata_sys::nft::ffi;

pub use crate::nfa::{Dot, States};
pub use crate::transitions::Transitions;

use crate::nfa::{dot, exactly_one};
use crate::types::{Lines, raw_state_count, sorted, states_from_raw, symbols_to_raw};
use crate::{
	Alphabet, InvalidAutomaton, Nfa, NftError, NoSuchState, ParseError, State, Symbol, check,
};

/// The most levels a transducer may have (parsed jumps are unwound into one edge per level).
pub const MAX_LEVELS: usize = mata_sys::nft::MAX_LEVELS;

/// The symbol that matches any one symbol on a tape (Mata's `DONT_CARE`, `u32::MAX - 1`). It does not
/// match [`Symbol::EPSILON`], which reads no symbol.
pub const DONT_CARE: Symbol = Symbol::new(u32::MAX - 1);

/// A finite transducer: Mata's `mata::nft::Nft`.
///
/// See the [module documentation](self) for how transitions are encoded. An `Nft` is always well formed:
/// every state has a level below [`Nft::num_levels`], initial and final states have level 0, and every
/// edge leads to a later level or back to level 0.
///
/// # Thread safety
///
/// `Nft` is `Send` and `Sync`, as [`Nfa`] is.
pub struct Nft {
	/// Never null.
	inner: UniquePtr<ffi::Nft>,
}

// SAFETY: As for `Nfa`: an `Nft` owns its C++ object, the shim never attaches alphabets, and mata-sys links
// only a Mata without the static buffers shared between automata, so Mata keeps no shared state.
unsafe impl Send for Nft {}

// SAFETY: The `&self` methods call `const` Mata functions, which do not mutate the transducer.
unsafe impl Sync for Nft {}

/// Panics with Mata's message if a call that this crate expects to succeed threw.
fn new_nft(result: Result<UniquePtr<ffi::Nft>, mata_sys::cxx::Exception>) -> Nft {
	Nft::from_owned(check(result))
}

/// What [`Nft::compose_on`] does with the synchronized levels.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub enum SyncLevel {
	/// Keep the synchronized level in the result.
	Keep,
	/// Project the synchronized level out of the result.
	#[default]
	ProjectOut,
}

/// Unwraps a call whose arguments were checked already: a failure is a broken precondition, so it panics.
fn expect<T>(result: Result<T, mata_sys::cxx::Exception>) -> T {
	result.unwrap_or_else(|error| panic!("{}", error.what()))
}

fn check_num_levels(num_levels: usize) -> Result<(), NftError> {
	if (1..=MAX_LEVELS).contains(&num_levels) {
		Ok(())
	} else {
		Err(NftError::InvalidNumLevels { num_levels })
	}
}

/// The words as the bridge takes them: all symbols concatenated, and the length of each word.
fn flatten<W: AsRef<[Symbol]>>(words: &[W]) -> (Vec<u32>, Vec<usize>) {
	let symbols = words
		.iter()
		.flat_map(|word| symbols_to_raw(word.as_ref()).iter().copied())
		.collect();
	let lengths = words.iter().map(|word| word.as_ref().len()).collect();
	(symbols, lengths)
}

impl Nft {
	/// Wraps a transducer that the shim built (so it is non-null, well formed and jump-free).
	fn from_owned(inner: UniquePtr<ffi::Nft>) -> Nft {
		assert!(!inner.is_null(), "mata-sys returned a null transducer");
		Nft { inner }
	}

	fn raw(&self) -> &ffi::Nft {
		&self.inner
	}

	fn raw_mut(&mut self) -> std::pin::Pin<&mut ffi::Nft> {
		self.inner.pin_mut()
	}

	fn automaton(&self) -> &mata_sys::ffi::Automaton {
		ffi::nft_automaton(&self.inner)
	}

	/// Checks that `found` (levels, symbols or words) matches the number of levels.
	fn check_arity(&self, found: usize) -> Result<(), NftError> {
		let expected = self.num_levels();
		if found == expected {
			Ok(())
		} else {
			Err(NftError::LevelMismatch { expected, found })
		}
	}

	fn check_level(&self, level: usize) -> Result<(), NftError> {
		let num_levels = self.num_levels();
		if level < num_levels {
			Ok(())
		} else {
			Err(NftError::NoSuchLevel { level, num_levels })
		}
	}

	/// Checks that `levels` are levels in strictly increasing order.
	fn check_levels(&self, levels: &[usize]) -> Result<(), NftError> {
		for &level in levels {
			self.check_level(level)?;
		}
		if levels.windows(2).all(|pair| pair[0] < pair[1]) {
			Ok(())
		} else {
			Err(NftError::LevelsNotIncreasing)
		}
	}

	/// Checks that `state` is a state of level 0, where transitions start and end.
	fn check_level_zero(&self, state: State) -> Result<(), NftError> {
		let level = self.level(state).ok_or_else(|| NoSuchState {
			state,
			num_states: self.num_states(),
		})?;
		if level == 0 {
			Ok(())
		} else {
			Err(NftError::NotLevelZero { state, level })
		}
	}

	// ── Construction ──────────────────────────────────────────────────────────────────────────────

	/// A transducer with `num_levels` levels and no states. Its relation is empty.
	///
	/// # Errors
	///
	/// [`NftError::InvalidNumLevels`] if `num_levels` is 0 or more than [`MAX_LEVELS`].
	pub fn new(num_levels: usize) -> Result<Nft, NftError> {
		check_num_levels(num_levels)?;
		Ok(new_nft(ffi::nft_new(num_levels)))
	}

	/// The identity relation on the language of `nfa`: all tuples (*w*, …, *w*) of `num_levels` copies
	/// of a word *w* of `nfa`.
	///
	/// # Errors
	///
	/// [`NftError::InvalidNumLevels`] if `num_levels` is 0 or more than [`MAX_LEVELS`].
	#[doc(alias = "from_nfa_with_levels_zero")]
	pub fn identity(nfa: &Nfa, num_levels: usize) -> Result<Nft, NftError> {
		check_num_levels(num_levels)?;
		Ok(new_nft(ffi::nft_identity(nfa.as_ffi(), num_levels)))
	}

	/// The transducer accepting only the tuple of `num_levels` empty words.
	///
	/// # Errors
	///
	/// [`NftError::InvalidNumLevels`] if `num_levels` is 0 or more than [`MAX_LEVELS`].
	#[doc(alias = "create_empty_string_nft")]
	pub fn empty_word(num_levels: usize) -> Result<Nft, NftError> {
		check_num_levels(num_levels)?;
		Ok(new_nft(ffi::nft_empty_word(num_levels)))
	}

	/// Parses a transducer in the `.mata` format (`@NFT-explicit`, with `%Levels` and `%LevelsNum`).
	/// Symbols are read as in [`Nfa::parse_mata`], with one alphabet for all levels.
	///
	/// # Errors
	///
	/// [`ParseError`] if the text is not exactly one valid, well-formed NFT section with at most
	/// [`MAX_LEVELS`] levels.
	pub fn parse_mata(text: &str, alphabet: &mut Alphabet) -> Result<Nft, ParseError> {
		exactly_one(Nft::parse_mata_all(text, alphabet)?, "transducer")
	}

	/// Parses every transducer section of a `.mata` text, as [`Nft::parse_mata`].
	///
	/// # Errors
	///
	/// [`ParseError`] if the text is not valid or a section is not a well-formed NFT.
	pub fn parse_mata_all(text: &str, alphabet: &mut Alphabet) -> Result<Vec<Nft>, ParseError> {
		let (names, symbols) = alphabet.raw_parse_names();
		let mut new_names = Vec::new();
		let mut new_symbols = Vec::new();
		let parsed = ffi::nft_parse_mata(
			text,
			false,
			&names,
			&symbols,
			alphabet.raw_next_symbol(),
			&mut new_names,
			&mut new_symbols,
		)
		.map_err(|error| ParseError::new(error.what()))?;
		alphabet.add_parsed_names(new_names, &new_symbols);
		Ok(take_parsed(parsed))
	}

	/// Parses every transducer section of a `.mata` text whose symbols are numbers.
	///
	/// # Errors
	///
	/// [`ParseError`] if the text is not valid, a section is not a well-formed NFT, or a symbol is not a
	/// number.
	pub fn parse_mata_numeric_all(text: &str) -> Result<Vec<Nft>, ParseError> {
		let parsed = ffi::nft_parse_mata(text, true, &[], &[], 0, &mut Vec::new(), &mut Vec::new())
			.map_err(|error| ParseError::new(error.what()))?;
		Ok(take_parsed(parsed))
	}

	/// Takes over a transducer built by other C++ code through [`mata_sys`], dropping its alphabets and
	/// unwinding its jumps.
	///
	/// # Errors
	///
	/// [`InvalidAutomaton`] if `ffi` is null, its transitions are not kept as Mata keeps them (see
	/// [`Nfa::from_ffi`]), or it is not well formed (see [`Nft`]).
	pub fn from_ffi(mut ffi: UniquePtr<ffi::Nft>) -> Result<Nft, InvalidAutomaton> {
		if ffi.is_null() {
			return Err(InvalidAutomaton::new("the transducer is null"));
		}
		ffi::nft_adopt(ffi.pin_mut()).map_err(|error| InvalidAutomaton::new(error.what()))?;
		Ok(Nft { inner: ffi })
	}

	/// The underlying C++ transducer, for passing to other C++ code through [`mata_sys`].
	#[must_use]
	pub fn as_ffi(&self) -> &ffi::Nft {
		self.raw()
	}

	/// Gives up ownership of the underlying C++ transducer.
	#[must_use]
	pub fn into_ffi(self) -> UniquePtr<ffi::Nft> {
		self.inner
	}

	// ── States ────────────────────────────────────────────────────────────────────────────────────

	/// The number of levels (tapes).
	#[must_use]
	pub fn num_levels(&self) -> usize {
		ffi::nft_num_levels(self.raw())
	}

	/// The number of states, of all levels.
	#[must_use]
	pub fn num_states(&self) -> usize {
		mata_sys::ffi::num_states(self.automaton())
	}

	/// The states `0..num_states()`, of all levels. The iterator does not borrow the transducer.
	#[must_use]
	pub fn states(&self) -> States {
		States::new(self.num_states())
	}

	/// The level of `state`, or `None` if it is not a state of this transducer.
	#[must_use]
	pub fn level(&self, state: State) -> Option<usize> {
		(state.index() < self.num_states()).then(|| expect(ffi::nft_level(self.raw(), state.raw())))
	}

	/// Adds a state of level 0 and returns it.
	pub fn add_state(&mut self) -> State {
		State::new(check(ffi::nft_add_state(self.raw_mut(), 0)))
	}

	/// Makes `state` initial.
	///
	/// # Errors
	///
	/// [`NftError::NoSuchState`] if `state` is not a state of this transducer, and
	/// [`NftError::NotLevelZero`] if it is not on level 0.
	pub fn add_initial(&mut self, state: State) -> Result<(), NftError> {
		self.check_level_zero(state)?;
		expect(ffi::nft_add_initial(self.raw_mut(), state.raw()));
		Ok(())
	}

	/// Makes `state` non-initial. Returns whether it was initial (`false` for a state that does not
	/// exist).
	pub fn remove_initial(&mut self, state: State) -> bool {
		ffi::nft_remove_initial(self.raw_mut(), state.raw())
	}

	/// Whether `state` is initial (`false` for a state that does not exist).
	#[must_use]
	pub fn is_initial(&self, state: State) -> bool {
		mata_sys::ffi::is_initial(self.automaton(), state.raw())
	}

	/// The initial states, in no particular order.
	#[must_use]
	pub fn initial_states(&self) -> &[State] {
		states_from_raw(mata_sys::ffi::initial_states(self.automaton()))
	}

	/// Makes `state` final.
	///
	/// # Errors
	///
	/// [`NftError::NoSuchState`] if `state` is not a state of this transducer, and
	/// [`NftError::NotLevelZero`] if it is not on level 0.
	pub fn add_final(&mut self, state: State) -> Result<(), NftError> {
		self.check_level_zero(state)?;
		expect(ffi::nft_add_final(self.raw_mut(), state.raw()));
		Ok(())
	}

	/// Makes `state` non-final. Returns whether it was final (`false` for a state that does not exist).
	pub fn remove_final(&mut self, state: State) -> bool {
		ffi::nft_remove_final(self.raw_mut(), state.raw())
	}

	/// Whether `state` is final (`false` for a state that does not exist).
	#[must_use]
	pub fn is_final(&self, state: State) -> bool {
		mata_sys::ffi::is_final(self.automaton(), state.raw())
	}

	/// The final states, in no particular order.
	#[must_use]
	pub fn final_states(&self) -> &[State] {
		states_from_raw(mata_sys::ffi::final_states(self.automaton()))
	}

	// ── Transitions ───────────────────────────────────────────────────────────────────────────────

	/// Adds the transition from `source` to `target` that reads `symbols[i]` on level `i`, through new
	/// inner states. A symbol may be [`Symbol::EPSILON`] (read nothing on that tape) or [`DONT_CARE`].
	///
	/// # Errors
	///
	/// [`NftError::NoSuchState`] or [`NftError::NotLevelZero`] if `source` or `target` is not a state of
	/// level 0, and [`NftError::LevelMismatch`] if `symbols` does not have one symbol per level.
	pub fn add_transition(
		&mut self,
		source: State,
		symbols: &[Symbol],
		target: State,
	) -> Result<(), NftError> {
		self.check_level_zero(source)?;
		self.check_level_zero(target)?;
		self.check_arity(symbols.len())?;
		expect(ffi::nft_add_transition(
			self.raw_mut(),
			source.raw(),
			symbols_to_raw(symbols),
			target.raw(),
		));
		Ok(())
	}

	/// Adds a path from `source` to `target` that reads `words[i]` on level `i`. Shorter words are padded
	/// with epsilon, so the path has as many transitions as the longest word. [`Symbol::EPSILON`] in a
	/// word reads nothing, so a word of epsilons only counts as empty.
	///
	/// ```
	/// use mata::nft::Nft;
	/// use mata::Symbol;
	///
	/// let mut t = Nft::new(2)?;
	/// let (p, q) = (t.add_state(), t.add_state());
	/// t.add_initial(p)?;
	/// t.add_final(q)?;
	/// t.add_path(p, &[&b"abc".map(Symbol::from)[..], &b"x".map(Symbol::from)[..]], q)?;
	/// assert!(t.accepts(&[&b"abc".map(Symbol::from)[..], &b"x".map(Symbol::from)[..]]));
	/// # Ok::<(), mata::NftError>(())
	/// ```
	///
	/// # Errors
	///
	/// [`NftError::NoSuchState`] or [`NftError::NotLevelZero`] if `source` or `target` is not a state of
	/// level 0, [`NftError::LevelMismatch`] if `words` does not have one word per level, and
	/// [`NftError::EmptyPath`] if all words are empty (or epsilon only).
	#[doc(alias = "insert_word_by_levels")]
	pub fn add_path<W: AsRef<[Symbol]>>(
		&mut self,
		source: State,
		words: &[W],
		target: State,
	) -> Result<(), NftError> {
		self.check_level_zero(source)?;
		self.check_level_zero(target)?;
		self.check_arity(words.len())?;
		if words
			.iter()
			.all(|word| word.as_ref().iter().all(|symbol| symbol.is_epsilon()))
		{
			return Err(NftError::EmptyPath);
		}
		let (symbols, lengths) = flatten(words);
		expect(ffi::nft_insert_words(
			self.raw_mut(),
			source.raw(),
			&symbols,
			&lengths,
			target.raw(),
		));
		Ok(())
	}

	/// The number of single-level edges (each transducer transition is a chain of them). Takes time
	/// linear in the size of the transducer.
	#[must_use]
	pub fn num_edges(&self) -> usize {
		mata_sys::ffi::num_transitions(self.automaton())
	}

	/// The single-level edges, ordered by source, then symbol, then target. The edge leaving a state of
	/// level `i` reads tape `i`, see [`Nft::level`].
	#[must_use]
	pub fn edges(&self) -> Transitions<'_> {
		Transitions::new(self.automaton(), 0..raw_state_count(self.num_states()))
	}

	/// The single-level edges leaving `source`, ordered by symbol, then target. Empty for a state that
	/// does not exist.
	#[must_use]
	pub fn edges_from(&self, source: State) -> Transitions<'_> {
		let num_states = raw_state_count(self.num_states());
		let start = source.raw().min(num_states);
		Transitions::new(
			self.automaton(),
			start..num_states.min(start.saturating_add(1)),
		)
	}

	// ── Properties ────────────────────────────────────────────────────────────────────────────────

	/// Whether the tuple of words, `words[i]` on level `i`, is in the relation. [`Symbol::EPSILON`] in a
	/// word is ignored, and [`DONT_CARE`] matches any symbol. A tuple with another number of words than
	/// levels is not in the relation.
	#[must_use]
	#[doc(alias = "is_in_lang_by_levels")]
	pub fn accepts<W: AsRef<[Symbol]>>(&self, words: &[W]) -> bool {
		if words.len() != self.num_levels() {
			return false;
		}
		let (symbols, lengths) = flatten(words);
		expect(ffi::nft_accepts(self.raw(), &symbols, &lengths))
	}

	/// Whether the relation is empty.
	#[must_use]
	#[doc(alias = "is_empty")]
	pub fn is_lang_empty(&self) -> bool {
		check(mata_sys::ffi::is_lang_empty(self.automaton()))
	}

	/// Whether the relation of `self` is included in that of `other`, decided on the encodings.
	///
	/// `true` is always right; `false` only for transducers without epsilon. Mata compares the interleaved
	/// encodings, so reading (*a*, ε) and then (ε, *b*) differs from reading (*a*, *b*) at once, although both
	/// accept the pair (*a*, *b*). [`DONT_CARE`] counts as any symbol.
	///
	/// # Errors
	///
	/// [`NftError::LevelMismatch`] if the numbers of levels differ.
	#[doc(alias = "is_included")]
	#[doc(alias = "is_subset")]
	pub fn is_included_in(&self, other: &Nft) -> Result<bool, NftError> {
		self.check_arity(other.num_levels())?;
		Ok(check(ffi::nft_is_included(self.raw(), other.raw())))
	}

	/// Whether the relations are equal, decided on the encodings as [`Nft::is_included_in`]: `true` is
	/// always right, `false` only for transducers without epsilon.
	///
	/// # Errors
	///
	/// [`NftError::LevelMismatch`] if the numbers of levels differ.
	#[doc(alias = "are_equivalent")]
	pub fn is_equivalent_to(&self, other: &Nft) -> Result<bool, NftError> {
		self.check_arity(other.num_levels())?;
		Ok(check(ffi::nft_are_equivalent(self.raw(), other.raw())))
	}

	/// Whether the transducer is deterministic on its encoding: one initial state and, from every state,
	/// at most one edge per symbol. [`DONT_CARE`] overlaps every symbol except [`Symbol::EPSILON`], so
	/// it may share a state only with edges that reach the same target.
	#[must_use]
	pub fn is_deterministic(&self) -> bool {
		check(ffi::nft_is_deterministic(self.raw()))
	}

	// ── Operations ────────────────────────────────────────────────────────────────────────────────

	/// Removes the states that are not useful (unreachable from an initial state, or reaching no final
	/// state) and renumbers the remaining ones. The relation does not change.
	pub fn trim(&mut self) {
		check(ffi::nft_trim(self.raw_mut()));
	}

	/// Removes the transitions that read epsilon on every level, adding the transitions they enable, and
	/// trims the result. The relation does not change. Mata keeps an all-epsilon transition whose inner
	/// states it shares with another transition (as [`Nft::determinize`] creates them).
	pub fn remove_epsilon(&mut self) {
		*self = new_nft(ffi::nft_remove_epsilon(self.raw()));
	}

	/// The union of the relations.
	///
	/// # Errors
	///
	/// [`NftError::LevelMismatch`] if the numbers of levels differ.
	#[must_use = "returns a new transducer and leaves `self` unchanged"]
	#[doc(alias = "union_nondet")]
	pub fn union(&self, other: &Nft) -> Result<Nft, NftError> {
		self.check_arity(other.num_levels())?;
		Ok(new_nft(ffi::nft_union(self.raw(), other.raw())))
	}

	/// The intersection of the relations, by the product of the encodings: exact for transducers without
	/// epsilon, and in general a subset (see [`Nft::is_included_in`]).
	///
	/// # Errors
	///
	/// [`NftError::LevelMismatch`] if the numbers of levels differ.
	#[must_use = "returns a new transducer and leaves `self` unchanged"]
	pub fn intersection(&self, other: &Nft) -> Result<Nft, NftError> {
		self.check_arity(other.num_levels())?;
		Ok(new_nft(ffi::nft_intersection(self.raw(), other.raw())))
	}

	/// The concatenation of the relations, tape by tape.
	///
	/// # Errors
	///
	/// [`NftError::LevelMismatch`] if the numbers of levels differ.
	#[must_use = "returns a new transducer and leaves `self` unchanged"]
	#[doc(alias = "concatenate")]
	pub fn concat(&self, other: &Nft) -> Result<Nft, NftError> {
		self.check_arity(other.num_levels())?;
		Ok(new_nft(ffi::nft_concatenate(self.raw(), other.raw())))
	}

	/// The composition: the last level of `self` is matched with level 0 of `other`, and both are
	/// projected out. For two transducers with two levels each, this is the relational composition
	/// {(*u*, *w*) | (*u*, *v*) ∈ `self`, (*v*, *w*) ∈ `other`}.
	///
	/// # Errors
	///
	/// [`NftError::NoLevelLeft`] if both transducers have a single level, and
	/// [`NftError::InvalidNumLevels`] if the result would have more than [`MAX_LEVELS`].
	#[must_use = "returns a new transducer and leaves `self` unchanged"]
	pub fn compose(&self, other: &Nft) -> Result<Nft, NftError> {
		self.compose_on(other, self.num_levels() - 1, 0, SyncLevel::ProjectOut)
	}

	/// The composition synchronizing level `self_level` of `self` with level `other_level` of `other`,
	/// which `sync_level` keeps or projects out. The result has the levels of `self` before `self_level`,
	/// then those of `other` before `other_level`, then the synchronized level (if kept), then the
	/// remaining levels of `self` and of `other`.
	///
	/// Mata can also synchronize several levels at once; this binding does not offer that yet.
	///
	/// # Errors
	///
	/// [`NftError::NoSuchLevel`] if a level does not exist, [`NftError::NoLevelLeft`] if the result
	/// would have no level, and [`NftError::InvalidNumLevels`] if it would have more than [`MAX_LEVELS`].
	#[must_use = "returns a new transducer and leaves `self` unchanged"]
	pub fn compose_on(
		&self,
		other: &Nft,
		self_level: usize,
		other_level: usize,
		sync_level: SyncLevel,
	) -> Result<Nft, NftError> {
		self.check_level(self_level)?;
		other.check_level(other_level)?;
		let removed = match sync_level {
			SyncLevel::Keep => 1,
			SyncLevel::ProjectOut => 2,
		};
		if self.num_levels() + other.num_levels() <= removed {
			return Err(NftError::NoLevelLeft);
		}
		check_num_levels(self.num_levels() + other.num_levels() - removed)?;
		Ok(Nft::from_owned(expect(ffi::nft_compose(
			self.raw(),
			other.raw(),
			self_level,
			other_level,
			sync_level == SyncLevel::ProjectOut,
		))))
	}

	/// Restricts level `level` to the words of `nfa` and projects that level out: for a two-level
	/// transducer and `level` 0, the image of the language of `nfa` (use [`Nft::to_nfa`] on the result).
	///
	/// # Errors
	///
	/// [`NftError::NoSuchLevel`] if `level` does not exist, and [`NftError::NoLevelLeft`] if the
	/// transducer has only one level.
	#[must_use = "returns a new transducer and leaves `self` unchanged"]
	pub fn apply(&self, nfa: &Nfa, level: usize) -> Result<Nft, NftError> {
		self.check_level(level)?;
		if self.num_levels() < 2 {
			return Err(NftError::NoLevelLeft);
		}
		Ok(Nft::from_owned(expect(ffi::nft_apply(
			self.raw(),
			nfa.as_ffi(),
			level,
			true,
		))))
	}

	/// Restricts level `level` to the words of `nfa`, keeping all levels.
	///
	/// # Errors
	///
	/// [`NftError::NoSuchLevel`] if `level` does not exist.
	#[must_use = "returns a new transducer and leaves `self` unchanged"]
	pub fn restrict(&self, nfa: &Nfa, level: usize) -> Result<Nft, NftError> {
		self.check_level(level)?;
		Ok(Nft::from_owned(expect(ffi::nft_apply(
			self.raw(),
			nfa.as_ffi(),
			level,
			false,
		))))
	}

	/// Removes the levels `levels` (strictly increasing).
	///
	/// # Errors
	///
	/// [`NftError::NoSuchLevel`] if a level does not exist, [`NftError::LevelsNotIncreasing`] if the
	/// levels are not strictly increasing, and [`NftError::NoLevelLeft`] if no level would remain.
	#[must_use = "returns a new transducer and leaves `self` unchanged"]
	pub fn project_out(&self, levels: &[usize]) -> Result<Nft, NftError> {
		self.check_levels(levels)?;
		if levels.len() >= self.num_levels() {
			return Err(NftError::NoLevelLeft);
		}
		Ok(Nft::from_owned(expect(ffi::nft_project_out(
			self.raw(),
			levels,
		))))
	}

	/// Keeps only the levels `levels` (strictly increasing, at least one).
	///
	/// # Errors
	///
	/// [`NftError::NoSuchLevel`] if a level does not exist, [`NftError::LevelsNotIncreasing`] if the
	/// levels are not strictly increasing, and [`NftError::NoLevelLeft`] if `levels` is empty.
	#[must_use = "returns a new transducer and leaves `self` unchanged"]
	pub fn project_to(&self, levels: &[usize]) -> Result<Nft, NftError> {
		self.check_levels(levels)?;
		if levels.is_empty() {
			return Err(NftError::NoLevelLeft);
		}
		Ok(Nft::from_owned(expect(ffi::nft_project_to(
			self.raw(),
			levels,
		))))
	}

	/// Reverses the order of the levels: for two levels, the inverse relation.
	#[must_use = "returns a new transducer and leaves `self` unchanged"]
	#[doc(alias = "inverse")]
	pub fn invert_levels(&self) -> Nft {
		new_nft(ffi::nft_invert(self.raw()))
	}

	/// The relation with every word reversed in place: (*w*₀, …, *w*ₙ₋₁) becomes (*w*₀ reversed, …,
	/// *w*ₙ₋₁ reversed).
	#[must_use = "returns a new transducer and leaves `self` unchanged"]
	pub fn reverse(&self) -> Nft {
		new_nft(ffi::nft_reverse(self.raw()))
	}

	/// An equivalent transducer that is deterministic on its encoding: one initial state and, from every
	/// state, at most one edge per symbol, where epsilon counts as a symbol. The relation does not change.
	///
	/// # Complexity
	///
	/// Exponential in the worst case.
	#[must_use = "returns a new transducer and leaves `self` unchanged"]
	pub fn determinize(&self) -> Nft {
		new_nft(ffi::nft_determinize(self.raw()))
	}

	/// The automaton of the interleaved encoding: a word *a*₀*b*₀*a*₁*b*₁… for a two-level transducer.
	/// For a single level, this is the language of the transducer. [`DONT_CARE`] stays a symbol of its
	/// own; see [`Nft::to_nfa_over`].
	#[must_use]
	pub fn to_nfa(&self) -> Nfa {
		Nfa::from_owned(check(ffi::nft_to_nfa(self.raw(), false, &[])))
	}

	/// As [`Nft::to_nfa`], with every [`DONT_CARE`] edge replaced by one edge per symbol of `alphabet`.
	#[must_use]
	pub fn to_nfa_over(&self, alphabet: &Alphabet) -> Nfa {
		let symbols = alphabet.raw_symbols();
		Nfa::from_owned(check(ffi::nft_to_nfa(self.raw(), true, &symbols)))
	}

	// ── More of Mata's transducer operations ──────────────────────────────────────────────────────

	/// Mata's `create_sigma_star_nft(num_levels)`: one state with a [`DONT_CARE`] loop on every level,
	/// relating every tuple of `num_levels` words of equal length.
	///
	/// # Errors
	///
	/// [`NftError::InvalidNumLevels`] if `num_levels` is 0 or more than [`MAX_LEVELS`].
	#[doc(alias = "create_sigma_star_nft")]
	pub fn sigma_star(num_levels: usize) -> Result<Nft, NftError> {
		check_num_levels(num_levels)?;
		Ok(new_nft(ffi::nft_sigma_star(num_levels)))
	}

	/// The transducer whose interleaved encoding (see [`Nft::to_nfa`]) is `nfa`: Mata's
	/// `from_nfa_with_levels_advancing`, which gives the initial states level 0 and every successor of a
	/// level-*i* state level *i* + 1 modulo `num_levels`. States unreachable from an initial state get
	/// level 0.
	///
	/// # Errors
	///
	/// [`NftError::InvalidNumLevels`] if `num_levels` is 0 or more than [`MAX_LEVELS`], and
	/// [`NftError::InconsistentLevels`] if a state would get two levels or a final state a level other
	/// than 0.
	#[doc(alias = "from_nfa_with_levels_advancing")]
	pub fn from_interleaved(nfa: &Nfa, num_levels: usize) -> Result<Nft, NftError> {
		check_num_levels(num_levels)?;
		ffi::nft_from_nfa_with_levels_advancing(nfa.as_ffi(), num_levels)
			.map(Nft::from_owned)
			.map_err(|_| NftError::InconsistentLevels)
	}

	/// Mata's `complement` (the classical algorithm) over the symbols of `alphabet`: determinizes, makes
	/// the transducer complete on every level and swaps the final and non-final states of level 0. A
	/// [`DONT_CARE`] edge is read as one edge per symbol of `alphabet`.
	///
	/// # Errors
	///
	/// [`NftError::EpsilonTransitions`] if the transducer has an epsilon edge, which Mata's complement
	/// does not support.
	///
	/// # Panics
	///
	/// If `alphabet` contains [`DONT_CARE`], which is not a letter.
	///
	/// # Complexity
	///
	/// Determinizes: exponential in the worst case.
	pub fn complement(&self, alphabet: &Alphabet) -> Result<Nft, NftError> {
		if mata_sys::ffi::has_epsilon_transitions(self.automaton()) {
			return Err(NftError::EpsilonTransitions);
		}
		Ok(new_nft(ffi::nft_complement(
			self.raw(),
			&alphabet.raw_symbols(),
		)))
	}

	/// Mata's `Nft::is_complete`: whether every reachable state has an edge under every symbol of
	/// `alphabet`; a [`DONT_CARE`] edge covers them all.
	///
	/// # Errors
	///
	/// [`NftError::NotInAlphabet`] if a reachable state has an edge under a symbol outside `alphabet`.
	///
	/// # Panics
	///
	/// If `alphabet` contains [`DONT_CARE`], which is not a letter.
	pub fn is_complete(&self, alphabet: &Alphabet) -> Result<bool, NftError> {
		assert!(
			!alphabet.contains(DONT_CARE),
			"DONT_CARE is not a letter of an alphabet"
		);
		ffi::nft_is_complete(self.raw(), &alphabet.raw_symbols())
			.map_err(|_| NftError::NotInAlphabet)
	}

	/// The relation concatenated with itself `power` times (Mata's `concatenate_nth_power`); power 0 is
	/// the tuple of empty words.
	#[must_use = "returns a new transducer and leaves `self` unchanged"]
	#[doc(alias = "concatenate_nth_power")]
	pub fn concat_power(&self, power: u32) -> Nft {
		new_nft(ffi::nft_concatenate_nth_power(self.raw(), power))
	}

	/// Mata's `Nft::contains_jump_transitions`: whether an edge skips a level. Always `false` for an
	/// `Nft` of this crate, which unwinds every jump into one edge per level.
	#[must_use]
	#[doc(alias = "contains_jump_transitions")]
	pub fn contains_jumps(&self) -> bool {
		check(ffi::nft_contains_jump_transitions(self.raw()))
	}

	/// Mata's `Nft::unwind_jumps` with its defaults: every jump becomes one edge per level, repeating its
	/// symbol. An `Nft` of this crate has no jumps, so the result is a copy.
	#[must_use = "returns a new transducer and leaves `self` unchanged"]
	pub fn unwind_jumps(&self) -> Nft {
		new_nft(ffi::nft_unwind_jumps(self.raw()))
	}

	/// Mata's `has_epsilon_cycle`: whether a cycle of edges reads epsilon only.
	#[must_use]
	pub fn has_epsilon_cycle(&self) -> bool {
		check(ffi::nft_has_epsilon_cycle(self.raw()))
	}

	/// Structural equality: the same number of states, levels of the states, initial and final states,
	/// and edges. For equality of relations, use [`Nft::is_equivalent_to`].
	#[must_use]
	pub fn is_identical(&self, other: &Nft) -> bool {
		check(ffi::nft_is_identical(self.raw(), other.raw()))
	}

	/// Mata's `Nft::is_in_lang`: whether the interleaved `word` (see [`Nft::to_nfa`]) is accepted, its
	/// symbol *i* read on level *i* modulo the number of levels. With `match_prefix`, accepting a prefix
	/// suffices. `has_epsilon_cycles` `false` promises [`Nft::has_epsilon_cycle`] is `false`, which lets
	/// Mata use a faster search.
	///
	/// # Errors
	///
	/// [`NftError::InterleavedLength`] if the length of `word` is not a multiple of the number of levels,
	/// and [`NftError::EpsilonCycle`] if `has_epsilon_cycles` is `false` but there is one.
	#[doc(alias = "is_in_lang")]
	pub fn accepts_interleaved(
		&self,
		word: &[Symbol],
		match_prefix: bool,
		has_epsilon_cycles: bool,
	) -> Result<bool, NftError> {
		let num_levels = self.num_levels();
		if !word.len().is_multiple_of(num_levels) {
			return Err(NftError::InterleavedLength {
				length: word.len(),
				num_levels,
			});
		}
		if !has_epsilon_cycles && self.has_epsilon_cycle() {
			return Err(NftError::EpsilonCycle);
		}
		Ok(expect(ffi::nft_is_in_lang(
			self.raw(),
			symbols_to_raw(word),
			match_prefix,
			has_epsilon_cycles,
		)))
	}

	/// Mata's `insert_level`: a new level `level`, which reads [`DONT_CARE`] in every transition; the
	/// levels from `level` on move one up. A `level` past the last one appends new levels up to it.
	///
	/// # Errors
	///
	/// [`NftError::InvalidNumLevels`] if the result would have more than [`MAX_LEVELS`] levels.
	pub fn insert_level(&self, level: usize) -> Result<Nft, NftError> {
		check_num_levels(self.num_levels().max(level) + 1)?;
		Ok(new_nft(ffi::nft_insert_level(self.raw(), level)))
	}

	/// Mata's `insert_levels`: the result has a level per entry of `mask`, the existing levels in order at
	/// the `false` entries and a new level, as in [`Nft::insert_level`], at every `true` entry.
	///
	/// # Errors
	///
	/// [`NftError::LevelMismatch`] unless `mask` has one `false` per level, and
	/// [`NftError::InvalidNumLevels`] if it has more than [`MAX_LEVELS`] entries.
	pub fn insert_levels(&self, mask: &[bool]) -> Result<Nft, NftError> {
		self.check_arity(mask.iter().filter(|&&inserted| !inserted).count())?;
		check_num_levels(mask.len())?;
		Ok(new_nft(ffi::nft_insert_levels(self.raw(), mask)))
	}

	/// Mata's `reduce`: a transducer with the same relation and states merged by simulation.
	#[must_use = "returns a new transducer and leaves `self` unchanged"]
	pub fn reduce(&self) -> Nft {
		new_nft(ffi::nft_reduce(self.raw()))
	}

	/// Mata's `revert`: every edge reversed, initial and final states swapped. A state of level *i* gets
	/// level *n* − *i* modulo *n*, so every word is reversed and the levels come in reverse order: level
	/// *k* of the result is level *n* − 1 − *k* of `self`. [`Nft::reverse`] keeps their order.
	#[must_use = "returns a new transducer and leaves `self` unchanged"]
	pub fn revert(&self) -> Nft {
		new_nft(ffi::nft_revert(self.raw()))
	}

	/// Mata's `Nft::is_universal` (antichains): whether the interleaved encoding (see [`Nft::to_nfa`])
	/// accepts every word over `alphabet`. Epsilon edges are removed first (Mata's `remove_epsilon`),
	/// as Mata's check takes no epsilon closure.
	///
	/// # Panics
	///
	/// If `alphabet` contains [`DONT_CARE`], which is not a letter.
	#[must_use]
	pub fn is_universal(&self, alphabet: &Alphabet) -> bool {
		check(ffi::nft_is_universal(self.raw(), &alphabet.raw_symbols()))
	}

	// ── Printing ──────────────────────────────────────────────────────────────────────────────────

	/// Displays the transducer in the `.mata` format with symbol names from `alphabet`, as
	/// [`Nfa::display_with`] does (with the same limits on reading it back).
	pub fn display_with<'a>(&'a self, alphabet: &'a Alphabet) -> DisplayMata<'a> {
		DisplayMata {
			nft: self,
			alphabet,
		}
	}

	/// The transducer's edges in the DOT format of Graphviz; set options on the adapter and print it with
	/// `{}`, as for [`Nfa::dot`].
	pub fn dot(&self) -> Dot<'_, Nft> {
		Dot::new(self)
	}
}

impl dot::Print for Nft {
	fn print_dot(&self, options: &dot::Options<'_>) -> String {
		check(ffi::nft_print_dot(
			self.raw(),
			options.ascii_labels,
			options.intervals,
			options.max_label_len,
			options.names,
			options.symbols,
		))
	}
}

fn take_parsed(mut parsed: UniquePtr<ffi::ParsedNfts>) -> Vec<Nft> {
	(0..ffi::nft_parsed_len(&parsed))
		.map(|index| new_nft(ffi::nft_parsed_take(parsed.pin_mut(), index)))
		.collect()
}

/// A deep copy.
impl Clone for Nft {
	fn clone(&self) -> Nft {
		new_nft(ffi::nft_clone(self.raw()))
	}
}

/// Lists the levels, the level of every state, the sorted initial and final states, and the edges.
impl fmt::Debug for Nft {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		let levels: Vec<usize> = self
			.states()
			.filter_map(|state| self.level(state))
			.collect();
		f.debug_struct("Nft")
			.field("num_levels", &self.num_levels())
			.field("levels", &levels)
			.field("initial", &Lines(sorted(self.initial_states()).into_iter()))
			.field("final", &Lines(sorted(self.final_states()).into_iter()))
			.field("edges", &Lines(self.edges()))
			.finish()
	}
}

/// The `.mata` format (`@NFT-explicit`) with numeric symbols.
impl fmt::Display for Nft {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		f.write_str(&check(ffi::nft_print_mata(self.raw(), &[], &[])))
	}
}

/// Parses one transducer in the `.mata` format whose symbols are numbers.
impl FromStr for Nft {
	type Err = ParseError;

	fn from_str(text: &str) -> Result<Nft, ParseError> {
		exactly_one(Nft::parse_mata_numeric_all(text)?, "transducer")
	}
}

/// Adapter returned by [`Nft::display_with`]; prints the `.mata` format with `{}`.
#[derive(Clone, Copy, Debug)]
#[must_use = "an adapter does nothing until it is formatted"]
pub struct DisplayMata<'a> {
	nft: &'a Nft,
	alphabet: &'a Alphabet,
}

impl fmt::Display for DisplayMata<'_> {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		let (names, symbols) = self.alphabet.raw_names();
		f.write_str(&check(ffi::nft_print_mata(
			self.nft.raw(),
			&names,
			&symbols,
		)))
	}
}
