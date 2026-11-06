//! The automaton type, its iterators and its display adapters.

use std::fmt;
use std::iter::FusedIterator;
use std::marker::PhantomData;
use std::ops::{BitAnd, BitOr, BitOrAssign, Range, Sub};
use std::str::FromStr;

use mata_sys::cxx::UniquePtr;
use mata_sys::ffi;

pub use crate::transitions::Transitions;
use crate::types::{
	Lines, raw_state_count, sorted, states_from_raw, states_from_vec, symbols_to_raw, word_from_raw,
};
use crate::{
	Alphabet, CheckAlgorithm, Complementation, Determinization, Direction, Distinction,
	InvalidAutomaton, LimitExceeded, Limits, Minimization, NoSuchState, ParseError, Reduction,
	RegexError, ResidualConstruction, Run, Side, State, Symbol, Transition, Word, check,
};

/// A nondeterministic finite automaton with epsilon transitions: Mata's `mata::nfa::Nfa`.
///
/// The states are `0..num_states()`. Transitions are labelled with [`Symbol`]s; [`Symbol::EPSILON`]
/// labels epsilon transitions. An automaton holds no alphabet: operations that need one borrow an
/// [`Alphabet`].
///
/// Determinization, complementation, minimization, difference and the language checks remove epsilon
/// transitions from a copy first, so they work on the language.
///
/// # Thread safety
///
/// `Nfa` is `Send` and `Sync`: distinct automata share no state, and `&Nfa` methods do not mutate.
///
/// # Examples
///
/// ```
/// use mata::{Nfa, Symbol};
///
/// // (a|b)*b
/// let (a, b) = (Symbol::from(b'a'), Symbol::from(b'b'));
/// let mut nfa = Nfa::new();
/// let q0 = nfa.add_state();
/// let q1 = nfa.add_state();
/// nfa.add_initial(q0)?;
/// nfa.add_final(q1)?;
/// nfa.add_transition(q0, a, q0)?;
/// nfa.add_transition(q0, b, q0)?;
/// nfa.add_transition(q0, b, q1)?;
///
/// assert!(nfa.accepts(&[a, b]));
/// assert!(!nfa.accepts(&[b, a]));
/// assert!(nfa.is_equivalent_to(&Nfa::from_regex("(a|b)*b")?));
/// # Ok::<(), mata::Error>(())
/// ```
pub struct Nfa {
	/// Never null.
	inner: UniquePtr<ffi::Nfa>,
}

// SAFETY: An `Nfa` owns its C++ object and never carries an alphabet, and mata-sys links only a Mata without
// the static buffers that automata shared before upstream #868, so it shares no state with other automata.
unsafe impl Send for Nfa {}

// SAFETY: The `&self` methods call `const` Mata functions, which do not mutate the automaton (Mata has no
// `mutable` members or caches) and, given the conditions above, no shared state either.
unsafe impl Sync for Nfa {}

/// Panics with Mata's message if a call that this crate expects to succeed threw.
fn new_nfa(result: Result<UniquePtr<ffi::Nfa>, mata_sys::cxx::Exception>) -> Nfa {
	Nfa::from_owned(check(result))
}

/// Calls a `_within` function of the bridge, which returns null when a limit stopped it.
fn new_nfa_within(
	limits: &Limits,
	call: impl FnOnce(&ffi::Limits) -> Result<UniquePtr<ffi::Nfa>, mata_sys::cxx::Exception>,
) -> Result<Nfa, LimitExceeded> {
	let limits = ffi::Limits::from(limits);
	let inner = check(call(&limits));
	if inner.is_null() {
		return Err(LimitExceeded::States {
			max_states: limits.max_states,
		});
	}
	Ok(Nfa::from_owned(inner))
}

impl Nfa {
	/// Wraps an automaton that the shim built (so it is non-null and has no alphabet).
	pub(crate) fn from_owned(inner: UniquePtr<ffi::Nfa>) -> Nfa {
		assert!(!inner.is_null(), "mata-sys returned a null automaton");
		Nfa { inner }
	}

	fn raw(&self) -> &ffi::Nfa {
		&self.inner
	}

	fn automaton(&self) -> &ffi::Automaton {
		ffi::nfa_automaton(&self.inner)
	}

	fn raw_mut(&mut self) -> std::pin::Pin<&mut ffi::Nfa> {
		self.inner.pin_mut()
	}

	// ── Construction ──────────────────────────────────────────────────────────────────────────────

	/// An automaton with no states. Its language is empty.
	#[must_use]
	pub fn new() -> Nfa {
		Nfa::with_states(0)
	}

	/// An automaton with the states `0..num_states` and nothing else.
	///
	/// # Panics
	///
	/// If `num_states` is `u32::MAX` or more (see [`State`]), or the memory for the states cannot be
	/// allocated.
	#[must_use]
	pub fn with_states(num_states: usize) -> Nfa {
		new_nfa(ffi::new_nfa(num_states))
	}

	/// [`Nfa::with_states`], reporting Mata's message if the automaton cannot be created.
	#[cfg(feature = "serde")]
	pub(crate) fn try_with_states(num_states: usize) -> Result<Nfa, String> {
		ffi::new_nfa(num_states)
			.map(Nfa::from_owned)
			.map_err(|error| format!("cannot create {num_states} states: {}", error.what()))
	}

	/// The automaton accepting only the empty word.
	#[must_use]
	#[doc(alias = "create_empty_string_nfa")]
	pub fn empty_word() -> Nfa {
		new_nfa(ffi::empty_word_nfa())
	}

	/// The automaton accepting only `word`.
	///
	/// ```
	/// use mata::{Nfa, Symbol};
	///
	/// let word = b"ab".map(Symbol::from);
	/// let nfa = Nfa::from_word(&word);
	/// assert!(nfa.accepts(&word));
	/// assert_eq!(nfa.num_states(), 3);
	/// ```
	#[must_use]
	#[doc(alias = "create_single_word_nfa")]
	pub fn from_word(word: &[Symbol]) -> Nfa {
		new_nfa(ffi::word_nfa(symbols_to_raw(word)))
	}

	/// The one-state automaton accepting every word over `alphabet`, Σ*.
	#[must_use]
	#[doc(alias = "universal")]
	#[doc(alias = "create_sigma_star_nfa")]
	pub fn sigma_star(alphabet: &Alphabet) -> Nfa {
		new_nfa(ffi::sigma_star_nfa(&alphabet.raw_symbols()))
	}

	/// Builds an automaton from a regular expression in RE2 syntax, over bytes: symbol `b` is byte `b`.
	/// RE2 reads the pattern as Latin-1, so a non-ASCII literal is the sequence of its UTF-8 bytes, while a
	/// class or `.` matches single bytes (`[é]` is one of the two bytes of `é`, and `.` is any byte but
	/// `\n`). The result has no epsilon transitions and is trimmed and reduced by simulation. Anchors (`^`,
	/// `$`, `\b`) are ignored.
	///
	/// # Errors
	///
	/// [`RegexError`] if `pattern` is not valid RE2 syntax or is too large to compile.
	///
	/// # Complexity
	///
	/// The reduction by simulation takes time and memory quadratic in the size of the pattern. When memory
	/// runs out during it, Mata's simulation code aborts the process.
	///
	/// ```
	/// use mata::{Nfa, Symbol};
	///
	/// let nfa = Nfa::from_regex("a(b|c)*")?;
	/// assert!(nfa.accepts(&b"abcb".map(Symbol::from)));
	/// assert!(Nfa::from_regex("a(").is_err());
	/// # Ok::<(), mata::RegexError>(())
	/// ```
	#[doc(alias = "create_from_regex")]
	pub fn from_regex(pattern: &str) -> Result<Nfa, RegexError> {
		ffi::regex_nfa(pattern)
			.map(Nfa::from_owned)
			.map_err(|error| RegexError::new(error.what()))
	}

	/// Parses an automaton in the `.mata` format. Symbol names are looked up in `alphabet`, and names
	/// not in it yet are added (see [`Alphabet::insert_name`]), so explicit automata parsed with the same
	/// alphabet agree on their symbols. An unnamed symbol of `alphabet` is read from its number, as
	/// [`Nfa::display_with`] prints it. `<eps>` is [`Symbol::EPSILON`].
	///
	/// The text must hold exactly one automaton section: `@NFA-explicit`, or `@NFA-bits`, whose
	/// transition formulas are turned into one symbol per minterm. The minterms of one call are named `0`,
	/// `1`, … and mean nothing outside it, so parse bit-vector automata that are to be compared together,
	/// with [`Nfa::parse_mata_all`]. States are numbered in the order they first appear, and the largest
	/// state number sets the size of the automaton (bound it for untrusted input).
	///
	/// # Errors
	///
	/// [`ParseError`] if the text is not exactly one valid NFA section, or if `alphabet` has no symbol
	/// left below `u32::MAX - 1` for a new name.
	///
	/// ```
	/// use mata::{Alphabet, Nfa};
	///
	/// let mut sigma = Alphabet::new();
	/// let nfa = Nfa::parse_mata(
	///     "@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 a q1\nq1 b q1\n",
	///     &mut sigma,
	/// )?;
	/// let ab = sigma.encode(["a", "b"]).unwrap();
	/// assert!(nfa.accepts(&ab));
	/// # Ok::<(), mata::ParseError>(())
	/// ```
	#[doc(alias = "parse_from_mata")]
	#[doc(alias = "construct")]
	pub fn parse_mata(text: &str, alphabet: &mut Alphabet) -> Result<Nfa, ParseError> {
		exactly_one(Nfa::parse_mata_all(text, alphabet)?, "automaton")
	}

	/// Parses every automaton section of a `.mata` text, as [`Nfa::parse_mata`]. Bit-vector sections are
	/// mintermized together, so their symbols agree.
	///
	/// # Errors
	///
	/// [`ParseError`] if the text is not valid or holds a section other than an NFA.
	pub fn parse_mata_all(text: &str, alphabet: &mut Alphabet) -> Result<Vec<Nfa>, ParseError> {
		let (names, symbols) = alphabet.raw_parse_names();
		let mut new_names = Vec::new();
		let mut new_symbols = Vec::new();
		let parsed = ffi::parse_mata(
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

	/// Parses every automaton section of a `.mata` text whose symbols are numbers, as [`str::parse`]
	/// does for one automaton.
	///
	/// # Errors
	///
	/// [`ParseError`] if the text is not valid, holds a section other than an NFA, or a symbol that is
	/// not a number.
	pub fn parse_mata_numeric_all(text: &str) -> Result<Vec<Nfa>, ParseError> {
		let parsed = ffi::parse_mata(text, true, &[], &[], 0, &mut Vec::new(), &mut Vec::new())
			.map_err(|error| ParseError::new(error.what()))?;
		Ok(take_parsed(parsed))
	}

	/// Takes over an automaton built by other C++ code through [`mata_sys`], dropping its alphabet.
	///
	/// # Errors
	///
	/// [`InvalidAutomaton`] if `ffi` is null, or its transitions are not kept as Mata keeps them (sorted
	/// by symbol, with sorted, unique targets that are states of the automaton).
	pub fn from_ffi(mut ffi: UniquePtr<ffi::Nfa>) -> Result<Nfa, InvalidAutomaton> {
		if ffi.is_null() {
			return Err(InvalidAutomaton::new("the automaton is null"));
		}
		ffi::adopt(ffi.pin_mut()).map_err(|error| InvalidAutomaton::new(error.what()))?;
		Ok(Nfa { inner: ffi })
	}

	/// The underlying C++ automaton, for passing to other C++ code through [`mata_sys`].
	#[must_use]
	pub fn as_ffi(&self) -> &ffi::Nfa {
		self.raw()
	}

	/// Gives up ownership of the underlying C++ automaton.
	#[must_use]
	pub fn into_ffi(self) -> UniquePtr<ffi::Nfa> {
		self.inner
	}

	// ── States ────────────────────────────────────────────────────────────────────────────────────

	/// The number of states.
	#[must_use]
	pub fn num_states(&self) -> usize {
		ffi::num_states(self.automaton())
	}

	/// The states `0..num_states()`. The iterator does not borrow the automaton.
	#[must_use]
	pub fn states(&self) -> States {
		States::new(self.num_states())
	}

	/// Adds a state and returns it.
	pub fn add_state(&mut self) -> State {
		State::new(check(ffi::add_state(self.raw_mut())))
	}

	/// Makes `state` initial.
	///
	/// # Errors
	///
	/// [`NoSuchState`] if `state` is not a state of this automaton.
	pub fn add_initial(&mut self, state: State) -> Result<(), NoSuchState> {
		self.check_state(state)?;
		check(ffi::add_initial(self.raw_mut(), state.raw()));
		Ok(())
	}

	/// Makes `state` non-initial. Returns whether it was initial (`false` for a state that does not
	/// exist).
	pub fn remove_initial(&mut self, state: State) -> bool {
		ffi::remove_initial(self.raw_mut(), state.raw())
	}

	/// Whether `state` is initial (`false` for a state that does not exist).
	#[must_use]
	pub fn is_initial(&self, state: State) -> bool {
		ffi::is_initial(self.automaton(), state.raw())
	}

	/// The initial states, in no particular order.
	#[must_use]
	pub fn initial_states(&self) -> &[State] {
		states_from_raw(ffi::initial_states(self.automaton()))
	}

	/// Makes `state` final.
	///
	/// # Errors
	///
	/// [`NoSuchState`] if `state` is not a state of this automaton.
	pub fn add_final(&mut self, state: State) -> Result<(), NoSuchState> {
		self.check_state(state)?;
		check(ffi::add_final(self.raw_mut(), state.raw()));
		Ok(())
	}

	/// Makes `state` non-final. Returns whether it was final (`false` for a state that does not exist).
	pub fn remove_final(&mut self, state: State) -> bool {
		ffi::remove_final(self.raw_mut(), state.raw())
	}

	/// Whether `state` is final (`false` for a state that does not exist).
	#[must_use]
	pub fn is_final(&self, state: State) -> bool {
		ffi::is_final(self.automaton(), state.raw())
	}

	/// The final states, in no particular order.
	#[must_use]
	pub fn final_states(&self) -> &[State] {
		states_from_raw(ffi::final_states(self.automaton()))
	}

	/// The states reachable from an initial state, ascending.
	#[must_use]
	pub fn reachable_states(&self) -> Vec<State> {
		states_from_vec(check(ffi::reachable_states(self.automaton())))
	}

	/// The useful states (reachable from an initial state and reaching a final state), ascending.
	#[must_use]
	pub fn useful_states(&self) -> Vec<State> {
		states_from_vec(check(ffi::useful_states(self.automaton())))
	}

	fn check_state(&self, state: State) -> Result<(), NoSuchState> {
		let num_states = self.num_states();
		if state.index() < num_states {
			Ok(())
		} else {
			Err(NoSuchState { state, num_states })
		}
	}

	// ── Transitions ───────────────────────────────────────────────────────────────────────────────

	/// The number of transitions. Takes time linear in the size of the automaton.
	#[must_use]
	pub fn num_transitions(&self) -> usize {
		ffi::num_transitions(self.automaton())
	}

	/// Adds the transition `source --symbol--> target`.
	///
	/// # Errors
	///
	/// [`NoSuchState`] if `source` or `target` is not a state of this automaton.
	pub fn add_transition(
		&mut self,
		source: State,
		symbol: Symbol,
		target: State,
	) -> Result<(), NoSuchState> {
		self.check_state(source)?;
		self.check_state(target)?;
		check(ffi::add_transition(
			self.raw_mut(),
			source.raw(),
			symbol.value(),
			target.raw(),
		));
		Ok(())
	}

	/// Adds the transitions in order.
	///
	/// # Errors
	///
	/// [`NoSuchState`] at the first transition whose source or target is not a state of this automaton.
	/// The transitions before it stay added.
	pub fn add_transitions(
		&mut self,
		transitions: impl IntoIterator<Item = Transition>,
	) -> Result<(), NoSuchState> {
		for Transition {
			source,
			symbol,
			target,
		} in transitions
		{
			self.add_transition(source, symbol, target)?;
		}
		Ok(())
	}

	/// Removes the transition `source --symbol--> target`. Returns whether it was present (`false` for a
	/// state that does not exist).
	pub fn remove_transition(&mut self, source: State, symbol: Symbol, target: State) -> bool {
		check(ffi::remove_transition(
			self.raw_mut(),
			source.raw(),
			symbol.value(),
			target.raw(),
		))
	}

	/// Whether the transition `source --symbol--> target` is present.
	#[must_use]
	pub fn contains_transition(&self, source: State, symbol: Symbol, target: State) -> bool {
		ffi::contains_transition(self.automaton(), source.raw(), symbol.value(), target.raw())
	}

	/// The targets of the transitions from `source` under `symbol`, ascending. Empty for a state that
	/// does not exist.
	#[must_use]
	pub fn successors(&self, source: State, symbol: Symbol) -> &[State] {
		states_from_raw(ffi::successors(
			self.automaton(),
			source.raw(),
			symbol.value(),
		))
	}

	/// All transitions, ordered by source, then symbol, then target.
	///
	/// The iterator borrows the automaton, so the automaton cannot change while it runs:
	///
	/// ```compile_fail
	/// # use mata::{Nfa, Symbol};
	/// let mut nfa = Nfa::with_states(1);
	/// for t in nfa.transitions() {
	///     nfa.add_transition(t.target, t.symbol, t.source); // error: `nfa` is borrowed
	/// }
	/// ```
	#[must_use]
	pub fn transitions(&self) -> Transitions<'_> {
		Transitions::new(self.automaton(), 0..raw_state_count(self.num_states()))
	}

	/// The transitions leaving `source`, ordered by symbol, then target. Empty for a state that does not
	/// exist.
	#[must_use]
	pub fn transitions_from(&self, source: State) -> Transitions<'_> {
		let num_states = raw_state_count(self.num_states());
		let start = source.raw().min(num_states);
		Transitions::new(
			self.automaton(),
			start..num_states.min(start.saturating_add(1)),
		)
	}

	/// The symbols that label some transition, ascending (with [`Symbol::EPSILON`] last, if present).
	#[must_use]
	pub fn used_symbols(&self) -> Vec<Symbol> {
		word_from_raw(check(ffi::used_symbols(self.automaton())))
	}

	/// Whether some transition is labelled with [`Symbol::EPSILON`].
	#[must_use]
	pub fn has_epsilon_transitions(&self) -> bool {
		ffi::has_epsilon_transitions(self.automaton())
	}

	// ── Properties ────────────────────────────────────────────────────────────────────────────────

	/// Whether the automaton accepts `word`, following epsilon transitions. [`Symbol::EPSILON`] in `word`
	/// reads nothing.
	#[must_use]
	#[doc(alias = "is_in_lang")]
	#[doc(alias = "contains")]
	pub fn accepts(&self, word: &[Symbol]) -> bool {
		check(ffi::accepts(self.raw(), symbols_to_raw(word)))
	}

	/// Whether the automaton is deterministic: no epsilon transitions, at most one initial state, and at
	/// most one target per state and symbol.
	#[must_use]
	pub fn is_deterministic(&self) -> bool {
		ffi::is_deterministic(self.raw())
	}

	/// Whether the automaton is the minimal DFA of its language as [`Nfa::minimize`] returns it, up to the
	/// numbering of states: deterministic (so without epsilon transitions), trimmed, and without two states
	/// with the same language. The empty language is minimal only as a single non-final initial state
	/// without transitions.
	///
	/// # Complexity
	///
	/// O(a·n·log n) for n states and a symbols, by Hopcroft's partition refinement.
	#[must_use]
	pub fn is_minimal(&self) -> bool {
		check(ffi::is_minimal(self.raw()))
	}

	/// Whether every reachable state has a transition under every symbol of `alphabet`. Transitions
	/// under symbols outside `alphabet` are an error, as in Mata's own check, except epsilon
	/// transitions, which are moves, not letters.
	///
	/// # Panics
	///
	/// If a reachable state has a transition under a symbol outside `alphabet`.
	#[must_use]
	pub fn is_complete(&self, alphabet: &Alphabet) -> bool {
		check(ffi::is_complete(self.raw(), &alphabet.raw_symbols()))
	}

	/// Whether no cycle (including a self-loop) runs through states reachable from an initial state.
	/// A trimmed acyclic automaton has a finite language.
	#[must_use]
	pub fn is_acyclic(&self) -> bool {
		check(ffi::is_acyclic(self.automaton()))
	}

	/// Structural equality: the same number of states, initial and final states, and transitions.
	///
	/// For equality of languages, use [`Nfa::is_equivalent_to`].
	#[must_use]
	pub fn is_identical(&self, other: &Nfa) -> bool {
		check(ffi::is_identical(self.raw(), other.raw()))
	}

	// ── In-place operations ───────────────────────────────────────────────────────────────────────

	/// Removes the states that are not useful (unreachable from an initial state, or reaching no final
	/// state) and renumbers the remaining ones. The language does not change.
	pub fn trim(&mut self) {
		check(ffi::trim(self.raw_mut()));
	}

	/// Replaces the epsilon transitions with the transitions they enable. The language does not change.
	pub fn remove_epsilon(&mut self) {
		check(ffi::remove_epsilon(self.raw_mut()));
	}

	/// Adds transitions to a sink state wherever a state lacks a transition under a symbol of `alphabet`.
	/// Returns the sink state (a new state, looping under every symbol) if anything was added.
	pub fn make_complete(&mut self, alphabet: &Alphabet) -> Option<State> {
		let sink = State::new(raw_state_count(self.num_states()));
		check(ffi::make_complete(self.raw_mut(), &alphabet.raw_symbols())).then_some(sink)
	}

	// ── Operations that build a new automaton ─────────────────────────────────────────────────────

	/// L(self) ∪ L(other), as the disjoint union of the two automata.
	#[must_use = "returns a new automaton and leaves `self` unchanged"]
	#[doc(alias = "union_nondet")]
	pub fn union(&self, other: &Nfa) -> Nfa {
		new_nfa(ffi::union_nondet(self.raw(), other.raw()))
	}

	/// L(self) ∩ L(other), by the product construction.
	#[must_use = "returns a new automaton and leaves `self` unchanged"]
	#[doc(alias = "product")]
	pub fn intersection(&self, other: &Nfa) -> Nfa {
		new_nfa(ffi::intersection(self.raw(), other.raw()))
	}

	/// L(self) · L(other).
	#[must_use = "returns a new automaton and leaves `self` unchanged"]
	#[doc(alias = "concatenate")]
	pub fn concat(&self, other: &Nfa) -> Nfa {
		new_nfa(ffi::concatenate(self.raw(), other.raw()))
	}

	/// L(self) \ L(other).
	///
	/// # Complexity
	///
	/// Determinizes both automata on the fly: exponential in the worst case. [`Nfa::difference_within`]
	/// stops at a limit.
	#[must_use = "returns a new automaton and leaves `self` unchanged"]
	#[doc(alias = "lang_difference")]
	pub fn difference(&self, other: &Nfa) -> Nfa {
		new_nfa(ffi::difference(self.raw(), other.raw()))
	}

	/// [`Nfa::difference`] within `limits`, which apply to the DFA that Mata builds for the difference.
	///
	/// # Errors
	///
	/// [`LimitExceeded::States`] if the DFA would have more than [`Limits::max_states`] states.
	pub fn difference_within(&self, other: &Nfa, limits: &Limits) -> Result<Nfa, LimitExceeded> {
		new_nfa_within(limits, |limits| {
			ffi::difference_within(self.raw(), other.raw(), limits)
		})
	}

	/// The complement of the language over `alphabet`: a complete DFA for Σ* \ L(self), where Σ is
	/// `alphabet`. Transitions under symbols outside Σ play no role.
	///
	/// # Complexity
	///
	/// Determinizes: exponential in the worst case. [`Nfa::complement_within`] stops at a limit.
	#[must_use = "returns a new automaton and leaves `self` unchanged"]
	pub fn complement(&self, alphabet: &Alphabet) -> Nfa {
		self.complement_with(alphabet, Complementation::default())
	}

	/// [`Nfa::complement`] with an explicit algorithm.
	#[must_use = "returns a new automaton and leaves `self` unchanged"]
	pub fn complement_with(&self, alphabet: &Alphabet, algorithm: Complementation) -> Nfa {
		new_nfa(ffi::complement(
			self.raw(),
			&alphabet.raw_symbols(),
			algorithm.into(),
		))
	}

	/// [`Nfa::complement_with`] within `limits`. The limit applies to each determinization (one for
	/// [`Complementation::Classical`] unless the automaton is deterministic, two for
	/// [`Complementation::Brzozowski`]) and to the result, its sink state included.
	///
	/// # Errors
	///
	/// [`LimitExceeded::States`] if one of these automata would have more than [`Limits::max_states`]
	/// states.
	pub fn complement_within(
		&self,
		alphabet: &Alphabet,
		algorithm: Complementation,
		limits: &Limits,
	) -> Result<Nfa, LimitExceeded> {
		new_nfa_within(limits, |limits| {
			ffi::complement_within(
				self.raw(),
				&alphabet.raw_symbols(),
				algorithm.into(),
				limits,
			)
		})
	}

	/// An equivalent DFA, by the subset construction, using [`Determinization::default`]. Its states are
	/// the reachable sets of states.
	///
	/// # Complexity
	///
	/// Exponential in the worst case. [`Nfa::determinize_within`] stops at a limit.
	#[must_use = "returns a new automaton and leaves `self` unchanged"]
	pub fn determinize(&self) -> Nfa {
		self.determinize_with(Determinization::default())
	}

	/// [`Nfa::determinize`] with an explicit algorithm.
	#[must_use = "returns a new automaton and leaves `self` unchanged"]
	pub fn determinize_with(&self, algorithm: Determinization) -> Nfa {
		new_nfa(ffi::determinize(self.raw(), algorithm.into()))
	}

	/// [`Nfa::determinize`] within `limits`.
	///
	/// # Errors
	///
	/// [`LimitExceeded::States`] if the DFA would have more than [`Limits::max_states`] states.
	pub fn determinize_within(&self, limits: &Limits) -> Result<Nfa, LimitExceeded> {
		new_nfa_within(limits, |limits| ffi::determinize_within(self.raw(), limits))
	}

	/// The minimal DFA of the language (trimmed, so possibly incomplete), using
	/// [`Minimization::default`]. The empty language gives a single non-final initial state.
	///
	/// # Complexity
	///
	/// Determinizes: exponential in the worst case. [`Nfa::minimize_within`] stops at a limit.
	#[must_use = "returns a new automaton and leaves `self` unchanged"]
	pub fn minimize(&self) -> Nfa {
		self.minimize_with(Minimization::default())
	}

	/// [`Nfa::minimize`] with an explicit algorithm.
	#[must_use = "returns a new automaton and leaves `self` unchanged"]
	pub fn minimize_with(&self, algorithm: Minimization) -> Nfa {
		new_nfa(ffi::minimize(self.raw(), algorithm.into()))
	}

	/// [`Nfa::minimize_with`] within `limits`. The limit applies to each determinization (one for
	/// [`Minimization::Hopcroft`] unless the automaton is deterministic, two for
	/// [`Minimization::Brzozowski`]) and to the result.
	///
	/// # Errors
	///
	/// [`LimitExceeded::States`] if one of these automata would have more than [`Limits::max_states`]
	/// states.
	pub fn minimize_within(
		&self,
		algorithm: Minimization,
		limits: &Limits,
	) -> Result<Nfa, LimitExceeded> {
		new_nfa_within(limits, |limits| {
			ffi::minimize_within(self.raw(), algorithm.into(), limits)
		})
	}

	/// A smaller automaton with the same language, using [`Reduction::default`] (simulation).
	#[must_use = "returns a new automaton and leaves `self` unchanged"]
	pub fn reduce(&self) -> Nfa {
		self.reduce_with(Reduction::default())
	}

	/// [`Nfa::reduce`] with an explicit reduction.
	#[must_use = "returns a new automaton and leaves `self` unchanged"]
	pub fn reduce_with(&self, reduction: Reduction) -> Nfa {
		let (algorithm, construction, direction) = match reduction {
			Reduction::Simulation => (
				ffi::Reduction::Simulation,
				ResidualConstruction::default(),
				Direction::default(),
			),
			Reduction::Residual {
				construction,
				direction,
			} => (ffi::Reduction::Residual, construction, direction),
		};
		new_nfa(ffi::reduce(
			self.raw(),
			algorithm,
			construction.into(),
			direction.into(),
		))
	}

	/// The automaton of the reversed language: every transition reversed, initial and final states
	/// swapped.
	#[must_use = "returns a new automaton and leaves `self` unchanged"]
	#[doc(alias = "revert")]
	pub fn reverse(&self) -> Nfa {
		new_nfa(ffi::revert(self.raw()))
	}

	// ── Decision procedures ───────────────────────────────────────────────────────────────────────

	/// Whether the language is empty.
	#[must_use]
	#[doc(alias = "is_empty")]
	pub fn is_lang_empty(&self) -> bool {
		check(ffi::is_lang_empty(self.automaton()))
	}

	/// Some accepted word, or `None` if the language is empty.
	#[must_use]
	#[doc(alias = "get_word")]
	pub fn find_accepted_word(&self) -> Option<Word> {
		let mut word = Vec::new();
		check(ffi::some_word(self.raw(), &mut word)).then(|| word_from_raw(word))
	}

	/// A shortest accepted word, or `None` if the language is empty.
	#[must_use]
	#[doc(alias = "get_shortest_word")]
	pub fn find_shortest_word(&self) -> Option<Word> {
		let mut word = Vec::new();
		check(ffi::shortest_word(self.raw(), &mut word)).then(|| word_from_raw(word))
	}

	/// An accepting run, or `None` if the language is empty.
	#[must_use]
	pub fn find_accepting_run(&self) -> Option<Run> {
		let mut word = Vec::new();
		let mut path = Vec::new();
		check(ffi::accepting_run(self.raw(), &mut word, &mut path)).then(|| Run {
			word: word_from_raw(word),
			path: states_from_vec(path),
		})
	}

	/// Whether every word over `alphabet` is accepted.
	///
	/// # Complexity
	///
	/// PSPACE-complete.
	#[must_use]
	pub fn is_universal(&self, alphabet: &Alphabet) -> bool {
		self.find_rejected_word(alphabet).is_none()
	}

	/// A word over `alphabet` that is not accepted, or `None` if the automaton is universal over it.
	#[must_use]
	pub fn find_rejected_word(&self, alphabet: &Alphabet) -> Option<Word> {
		self.find_rejected_word_with(alphabet, CheckAlgorithm::default())
	}

	/// [`Nfa::find_rejected_word`] with an explicit algorithm.
	#[must_use]
	pub fn find_rejected_word_with(
		&self,
		alphabet: &Alphabet,
		algorithm: CheckAlgorithm,
	) -> Option<Word> {
		let mut word = Vec::new();
		let universal = check(ffi::is_universal(
			self.raw(),
			&alphabet.raw_symbols(),
			algorithm.into(),
			&mut word,
		));
		(!universal).then(|| word_from_raw(word))
	}

	/// Whether L(self) ⊆ L(other).
	///
	/// # Complexity
	///
	/// PSPACE-complete.
	#[must_use]
	#[doc(alias = "is_included")]
	#[doc(alias = "is_subset")]
	pub fn is_included_in(&self, other: &Nfa) -> bool {
		self.find_word_not_in(other).is_none()
	}

	/// A word accepted by `self` but not by `other`, or `None` if L(self) ⊆ L(other).
	#[must_use]
	pub fn find_word_not_in(&self, other: &Nfa) -> Option<Word> {
		self.find_word_not_in_with(other, CheckAlgorithm::default())
	}

	/// [`Nfa::find_word_not_in`] with an explicit algorithm.
	#[must_use]
	pub fn find_word_not_in_with(&self, other: &Nfa, algorithm: CheckAlgorithm) -> Option<Word> {
		let mut word = Vec::new();
		let included = check(ffi::is_included(
			self.raw(),
			other.raw(),
			algorithm.into(),
			&mut word,
		));
		(!included).then(|| word_from_raw(word))
	}

	/// Whether L(self) ∩ L(other) = ∅, decided without building the product.
	///
	/// # Complexity
	///
	/// Polynomial: at most the |Q₁|·|Q₂| pairs of states of the product, stopping at the first common word.
	#[must_use]
	#[doc(alias = "is_intersection_empty")]
	#[doc(alias = "is_disjoint")]
	pub fn is_disjoint_from(&self, other: &Nfa) -> bool {
		self.find_common_word(other).is_none()
	}

	/// A word accepted by both `self` and `other` (not necessarily the shortest), or `None` if their
	/// languages are disjoint.
	#[must_use]
	pub fn find_common_word(&self, other: &Nfa) -> Option<Word> {
		let mut word = Vec::new();
		let disjoint = check(ffi::is_disjoint(self.raw(), other.raw(), &mut word));
		(!disjoint).then(|| word_from_raw(word))
	}

	/// Whether L(self) = L(other).
	///
	/// # Complexity
	///
	/// PSPACE-complete.
	#[must_use]
	#[doc(alias = "are_equivalent")]
	pub fn is_equivalent_to(&self, other: &Nfa) -> bool {
		self.find_distinguishing_word(other).is_none()
	}

	/// A word accepted by exactly one of the automata, or `None` if they are equivalent.
	///
	/// ```
	/// use mata::{Nfa, Side, Symbol};
	///
	/// let a_star = Nfa::from_regex("a*")?;
	/// let a_plus = Nfa::from_regex("a+")?;
	/// let distinction = a_star.find_distinguishing_word(&a_plus).unwrap();
	/// assert_eq!(distinction.word, []);
	/// assert_eq!(distinction.accepted_by, Side::Left);
	/// # Ok::<(), mata::RegexError>(())
	/// ```
	#[must_use]
	pub fn find_distinguishing_word(&self, other: &Nfa) -> Option<Distinction> {
		self.find_distinguishing_word_with(other, CheckAlgorithm::default())
	}

	/// [`Nfa::find_distinguishing_word`] with an explicit algorithm.
	#[must_use]
	pub fn find_distinguishing_word_with(
		&self,
		other: &Nfa,
		algorithm: CheckAlgorithm,
	) -> Option<Distinction> {
		let left_only = self.find_word_not_in_with(other, algorithm);
		left_only
			.map(|word| Distinction {
				word,
				accepted_by: Side::Left,
			})
			.or_else(|| {
				let right_only = other.find_word_not_in_with(self, algorithm);
				right_only.map(|word| Distinction {
					word,
					accepted_by: Side::Right,
				})
			})
	}

	/// The accepted words of length at most `max_length`, each once, in no particular order.
	///
	/// The words come from a search over sets of states, so a word with many runs costs no more than one.
	/// With GCC's libstdc++, the search runs lazily as the iterator advances. LLVM's libc++, the standard
	/// library on macOS, has no `std::generator`, so there this call runs the whole search first and stores
	/// every word: even `words_up_to(30).take(1)` can need memory for billions of words over two symbols.
	///
	/// ```
	/// use mata::{Nfa, Symbol};
	///
	/// let nfa = Nfa::from_regex("ab*")?;
	/// let mut words: Vec<_> = nfa.words_up_to(3).collect();
	/// words.sort();
	/// assert_eq!(words, [&b"a"[..], b"ab", b"abb"].map(|w| w.iter().copied().map(Symbol::from).collect::<Vec<_>>()));
	/// # Ok::<(), mata::RegexError>(())
	/// ```
	#[must_use]
	#[doc(alias = "get_words")]
	pub fn words_up_to(&self, max_length: usize) -> Words<'_> {
		Words {
			stream: check(ffi::words(self.raw(), max_length)),
			buffer: Vec::new(),
			nfa: PhantomData,
		}
	}

	// ── Printing ──────────────────────────────────────────────────────────────────────────────────

	/// Displays the automaton in the `.mata` format with symbol names from `alphabet` (numbers for unnamed
	/// symbols). [`Nfa::parse_mata`] with the same names reads it back, with the states renumbered, when
	/// every name survives the format (see [`Alphabet::insert_name`]).
	pub fn display_with<'a>(&'a self, alphabet: &'a Alphabet) -> DisplayMata<'a> {
		DisplayMata {
			nfa: self,
			alphabet,
		}
	}

	/// The automaton in the DOT format of Graphviz; set options on the adapter and print it with `{}`.
	///
	/// ```
	/// use mata::Nfa;
	///
	/// let nfa = Nfa::from_regex("ab")?;
	/// let dot = nfa.dot().ascii_labels(true).to_string();
	/// assert!(dot.starts_with("digraph"));
	/// # Ok::<(), mata::RegexError>(())
	/// ```
	pub fn dot(&self) -> Dot<'_> {
		Dot::new(self)
	}
}

fn take_parsed(mut parsed: UniquePtr<ffi::ParsedNfas>) -> Vec<Nfa> {
	(0..ffi::parsed_len(&parsed))
		.map(|index| new_nfa(ffi::parsed_take(parsed.pin_mut(), index)))
		.collect()
}

/// The only item of `items`, or a [`ParseError`] that counts the `what`s found.
pub(crate) fn exactly_one<T>(mut items: Vec<T>, what: &str) -> Result<T, ParseError> {
	match items.len() {
		1 => Ok(items.swap_remove(0)),
		n => Err(ParseError::new(format!("expected one {what}, found {n}"))),
	}
}

/// An automaton with no states.
impl Default for Nfa {
	fn default() -> Nfa {
		Nfa::new()
	}
}

/// A deep copy.
impl Clone for Nfa {
	fn clone(&self) -> Nfa {
		new_nfa(ffi::clone_nfa(self.raw()))
	}
}

/// Lists the states, the sorted initial and final states, and the transitions.
impl fmt::Debug for Nfa {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		f.debug_struct("Nfa")
			.field("num_states", &self.num_states())
			.field("initial", &Lines(sorted(self.initial_states()).into_iter()))
			.field("final", &Lines(sorted(self.final_states()).into_iter()))
			.field("transitions", &Lines(self.transitions()))
			.finish()
	}
}

/// The `.mata` format with numeric symbols, as Mata's `print_to_mata` writes it.
///
/// ```
/// use mata::{Nfa, Symbol};
///
/// let nfa = Nfa::from_word(&[Symbol::new(7)]);
/// let text = nfa.to_string();
/// assert_eq!(text, "@NFA-explicit\n%Alphabet-auto\n%Initial q0\n%Final q1\nq0 7 q1\n");
/// assert!(text.parse::<Nfa>()?.is_equivalent_to(&nfa));
/// # Ok::<(), mata::ParseError>(())
/// ```
impl fmt::Display for Nfa {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		f.write_str(&check(ffi::print_mata(self.raw(), &[], &[])))
	}
}

/// Parses one automaton in the `.mata` format whose symbols are numbers (Mata's `parse_from_mata`).
impl FromStr for Nfa {
	type Err = ParseError;

	fn from_str(text: &str) -> Result<Nfa, ParseError> {
		exactly_one(Nfa::parse_mata_numeric_all(text)?, "automaton")
	}
}

/// `&a & &b` is [`Nfa::intersection`].
impl BitAnd<&Nfa> for &Nfa {
	type Output = Nfa;

	fn bitand(self, other: &Nfa) -> Nfa {
		self.intersection(other)
	}
}

/// `&a | &b` is [`Nfa::union`].
impl BitOr<&Nfa> for &Nfa {
	type Output = Nfa;

	fn bitor(self, other: &Nfa) -> Nfa {
		self.union(other)
	}
}

/// `&a - &b` is [`Nfa::difference`].
impl Sub<&Nfa> for &Nfa {
	type Output = Nfa;

	fn sub(self, other: &Nfa) -> Nfa {
		self.difference(other)
	}
}

/// `a |= &b` adds `b` to `a` as a disjoint union, in place.
impl BitOrAssign<&Nfa> for Nfa {
	fn bitor_assign(&mut self, other: &Nfa) {
		check(ffi::unite_with(self.raw_mut(), other.raw()));
	}
}

/// Iterator returned by [`Nfa::states`].
#[derive(Clone, Debug)]
pub struct States {
	range: Range<u32>,
}

impl States {
	/// The states `0..num_states`.
	pub(crate) fn new(num_states: usize) -> States {
		States {
			range: 0..raw_state_count(num_states),
		}
	}
}

impl Iterator for States {
	type Item = State;

	fn next(&mut self) -> Option<State> {
		self.range.next().map(State::new)
	}

	fn size_hint(&self) -> (usize, Option<usize>) {
		self.range.size_hint()
	}
}

impl DoubleEndedIterator for States {
	fn next_back(&mut self) -> Option<State> {
		self.range.next_back().map(State::new)
	}
}

impl ExactSizeIterator for States {}

impl FusedIterator for States {}

/// Iterator returned by [`Nfa::words_up_to`].
pub struct Words<'a> {
	stream: UniquePtr<ffi::WordStream>,
	buffer: Vec<u32>,
	/// The stream reads a copy; the borrow keeps the option of reading the original.
	nfa: PhantomData<&'a Nfa>,
}

impl Iterator for Words<'_> {
	type Item = Word;

	fn next(&mut self) -> Option<Word> {
		check(ffi::next_word(self.stream.pin_mut(), &mut self.buffer))
			.then(|| self.buffer.iter().copied().map(Symbol::new).collect())
	}
}

impl FusedIterator for Words<'_> {}

// SAFETY: The stream owns its own copy of the automaton and a generator over it, with nothing tied to the
// thread that created it; as for `Nfa`, Mata shares no state between automata.
unsafe impl Send for Words<'_> {}

// SAFETY: `&Words` gives access to nothing (`next` takes `&mut self`).
unsafe impl Sync for Words<'_> {}

impl fmt::Debug for Words<'_> {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		f.debug_struct("Words").finish_non_exhaustive()
	}
}

/// Adapter returned by [`Nfa::display_with`]; prints the `.mata` format with `{}`.
#[derive(Clone, Copy, Debug)]
#[must_use = "an adapter does nothing until it is formatted"]
pub struct DisplayMata<'a> {
	nfa: &'a Nfa,
	alphabet: &'a Alphabet,
}

impl fmt::Display for DisplayMata<'_> {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		let (names, symbols) = self.alphabet.raw_names();
		f.write_str(&check(ffi::print_mata(self.nfa.raw(), &names, &symbols)))
	}
}

/// Adapter returned by [`Nfa::dot`] (and by `Nft::dot` for transducers); prints the DOT format of
/// Graphviz with `{}`.
#[must_use = "an adapter does nothing until it is formatted"]
pub struct Dot<'a, A = Nfa> {
	automaton: &'a A,
	alphabet: Option<&'a Alphabet>,
	ascii_labels: bool,
	intervals: bool,
	max_label_len: Option<usize>,
}

impl<'a, A> Dot<'a, A> {
	pub(crate) fn new(automaton: &'a A) -> Dot<'a, A> {
		Dot {
			automaton,
			alphabet: None,
			ascii_labels: false,
			intervals: false,
			max_label_len: None,
		}
	}

	/// Labels transitions with the symbol names of `alphabet`.
	pub fn alphabet(self, alphabet: &'a Alphabet) -> Dot<'a, A> {
		Dot {
			alphabet: Some(alphabet),
			..self
		}
	}

	/// Shows printable ASCII symbols as characters (for automata over bytes, such as regex automata).
	pub fn ascii_labels(self, yes: bool) -> Dot<'a, A> {
		Dot {
			ascii_labels: yes,
			..self
		}
	}

	/// Merges runs of consecutive symbols into ranges such as `[a-z]`.
	pub fn intervals(self, yes: bool) -> Dot<'a, A> {
		Dot {
			intervals: yes,
			..self
		}
	}

	/// Shortens labels longer than `len` characters.
	pub fn max_label_len(self, len: usize) -> Dot<'a, A> {
		Dot {
			max_label_len: Some(len),
			..self
		}
	}
}

impl<A> Clone for Dot<'_, A> {
	fn clone(&self) -> Self {
		*self
	}
}

impl<A> Copy for Dot<'_, A> {}

impl<A> fmt::Debug for Dot<'_, A> {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		f.debug_struct("Dot")
			.field("alphabet", &self.alphabet)
			.field("ascii_labels", &self.ascii_labels)
			.field("intervals", &self.intervals)
			.field("max_label_len", &self.max_label_len)
			.finish_non_exhaustive()
	}
}

impl<A: dot::Print> fmt::Display for Dot<'_, A> {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		let (names, symbols) = self.alphabet.map(Alphabet::raw_names).unwrap_or_default();
		let max_label_len = self
			.max_label_len
			.map_or(-1, |len| i32::try_from(len).unwrap_or(i32::MAX));
		let options = dot::Options {
			ascii_labels: self.ascii_labels,
			intervals: self.intervals,
			max_label_len,
			names: &names,
			symbols: &symbols,
		};
		f.write_str(&self.automaton.print_dot(&options))
	}
}

/// What [`Dot`] needs from an automaton (public in a private module, so other crates cannot use it).
pub(crate) mod dot {
	/// The options of a DOT printout, as the bridge takes them.
	#[derive(Debug)]
	pub struct Options<'a> {
		pub ascii_labels: bool,
		pub intervals: bool,
		/// -1 for no limit.
		pub max_label_len: i32,
		pub names: &'a [String],
		pub symbols: &'a [u32],
	}

	/// An automaton that [`Dot`](super::Dot) can print: `Nfa` and `Nft`.
	pub trait Print {
		fn print_dot(&self, options: &Options<'_>) -> String;
	}
}

impl dot::Print for Nfa {
	fn print_dot(&self, options: &dot::Options<'_>) -> String {
		check(ffi::print_dot(
			self.raw(),
			options.ascii_labels,
			options.intervals,
			options.max_label_len,
			options.names,
			options.symbols,
		))
	}
}
