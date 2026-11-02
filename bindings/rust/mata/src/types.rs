//! States, symbols, words and transitions.

use std::fmt;
use std::num::TryFromIntError;

/// A state of an automaton.
///
/// The states of an automaton with `n` states are `0..n`. States are 32-bit numbers, as in Mata, so an
/// automaton has fewer than `u32::MAX` states. Creating a `State` never fails; the methods of
/// [`Nfa`](crate::Nfa) check that the state exists.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(
    feature = "serde",
    derive(serde::Serialize, serde::Deserialize),
    serde(transparent)
)]
#[repr(transparent)]
pub struct State(u32);

impl State {
    /// The state with number `index`.
    #[must_use]
    pub const fn new(index: u32) -> State {
        State(index)
    }

    /// The number of this state, for indexing.
    #[must_use]
    pub const fn index(self) -> usize {
        self.0 as usize
    }

    /// The number of this state as the bridge passes it.
    pub(crate) const fn raw(self) -> u32 {
        self.0
    }
}

impl From<u32> for State {
    fn from(index: u32) -> State {
        State(index)
    }
}

/// Fails for an index that does not fit in 32 bits.
impl TryFrom<usize> for State {
    type Error = TryFromIntError;

    fn try_from(index: usize) -> Result<State, TryFromIntError> {
        u32::try_from(index).map(State)
    }
}

impl From<State> for u32 {
    fn from(state: State) -> u32 {
        state.0
    }
}

impl From<State> for usize {
    fn from(state: State) -> usize {
        state.index()
    }
}

/// Prints `q3`, as the `.mata` format does.
impl fmt::Display for State {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "q{}", self.0)
    }
}

/// A transition label.
///
/// Symbols are plain numbers; an [`Alphabet`](crate::Alphabet) can give them names. Automata built from
/// regular expressions read bytes, so `Symbol::from(b'a')` is the symbol of the letter `a`.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(
    feature = "serde",
    derive(serde::Serialize, serde::Deserialize),
    serde(transparent)
)]
#[repr(transparent)]
pub struct Symbol(u32);

impl Symbol {
    /// The epsilon symbol, `u32::MAX` (Mata's `EPSILON`). A transition under it reads no input.
    pub const EPSILON: Symbol = Symbol(mata_sys::EPSILON);

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

    /// Whether this is [`Symbol::EPSILON`].
    #[must_use]
    pub const fn is_epsilon(self) -> bool {
        self.0 == mata_sys::EPSILON
    }
}

impl From<u8> for Symbol {
    fn from(byte: u8) -> Symbol {
        Symbol(u32::from(byte))
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

/// Prints the number, or `<eps>` for [`Symbol::EPSILON`], as the `.mata` format does.
impl fmt::Display for Symbol {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.is_epsilon() {
            f.write_str("<eps>")
        } else {
            write!(f, "{}", self.0)
        }
    }
}

/// A finite word. Functions take words as `&[Symbol]`.
///
/// For a byte string, [`word_from_bytes`] gives the word over its bytes.
pub type Word = Vec<Symbol>;

/// The word of the bytes of `text`, one symbol per byte, as automata built by
/// [`Nfa::from_regex`](crate::Nfa::from_regex) read text.
///
/// ```
/// use mata::{Nfa, word_from_bytes};
///
/// let nfa = Nfa::from_regex("a(b|c)*")?;
/// assert!(nfa.accepts(&word_from_bytes("abcb")));
/// # Ok::<(), mata::RegexError>(())
/// ```
#[must_use]
pub fn word_from_bytes(text: impl AsRef<[u8]>) -> Word {
    text.as_ref().iter().copied().map(Symbol::from).collect()
}

/// The transition `source --symbol--> target`.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct Transition {
    /// The state the transition leaves.
    pub source: State,
    /// The symbol it reads.
    pub symbol: Symbol,
    /// The state it enters.
    pub target: State,
}

impl Transition {
    /// The transition `source --symbol--> target`.
    #[must_use]
    pub const fn new(source: State, symbol: Symbol, target: State) -> Transition {
        Transition {
            source,
            symbol,
            target,
        }
    }
}

impl From<(State, Symbol, State)> for Transition {
    fn from((source, symbol, target): (State, Symbol, State)) -> Transition {
        Transition {
            source,
            symbol,
            target,
        }
    }
}

/// Prints `q0 97 q1`, a transition line of the `.mata` format.
impl fmt::Display for Transition {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{} {} {}", self.source, self.symbol, self.target)
    }
}

/// An accepting run: a word together with the states it visits.
///
/// `path.len() == word.len() + 1`. The word lists every transition taken, so it contains
/// [`Symbol::EPSILON`] where the run follows an epsilon transition.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub struct Run {
    /// The symbols read.
    pub word: Word,
    /// The states visited, starting in an initial state and ending in a final one.
    pub path: Vec<State>,
}

/// A word accepted by exactly one of two automata, from
/// [`Nfa::find_distinguishing_word`](crate::Nfa::find_distinguishing_word).
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
#[non_exhaustive]
pub struct Distinction {
    /// The word.
    pub word: Word,
    /// The automaton that accepts it.
    pub accepted_by: Side,
}

/// One of the two automata of a binary check.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum Side {
    /// The receiver, `a` in `a.find_distinguishing_word(&b)`.
    Left,
    /// The argument, `b` in `a.find_distinguishing_word(&b)`.
    Right,
}

/// The states, sorted (the bridge gives initial and final states in no particular order).
pub(crate) fn sorted(states: &[State]) -> Vec<State> {
    let mut states = states.to_vec();
    states.sort_unstable();
    states
}

/// Debug-prints the items of an iterator as a list, each in its `Display` form (such as `q0 97 q1` for a
/// transition).
pub(crate) struct Lines<I>(pub(crate) I);

impl<I: Clone + Iterator<Item: fmt::Display>> fmt::Debug for Lines<I> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        /// Debug-prints an item with its `Display` form.
        struct Shown<T>(T);
        impl<T: fmt::Display> fmt::Debug for Shown<T> {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                fmt::Display::fmt(&self.0, f)
            }
        }
        f.debug_list().entries(self.0.clone().map(Shown)).finish()
    }
}

/// Views raw states from the bridge as [`State`]s.
pub(crate) fn states_from_raw(raw: &[u32]) -> &[State] {
    // SAFETY: `State` is `repr(transparent)` over `u32`, so both slices have the same layout.
    unsafe { std::slice::from_raw_parts(raw.as_ptr().cast::<State>(), raw.len()) }
}

/// Views [`Symbol`]s as the raw symbols of the bridge.
pub(crate) fn symbols_to_raw(symbols: &[Symbol]) -> &[u32] {
    // SAFETY: `Symbol` is `repr(transparent)` over `u32`, so both slices have the same layout.
    unsafe { std::slice::from_raw_parts(symbols.as_ptr().cast::<u32>(), symbols.len()) }
}

/// Converts raw symbols from the bridge into a [`Word`], reusing the allocation.
pub(crate) fn word_from_raw(raw: Vec<u32>) -> Word {
    raw.into_iter().map(Symbol).collect()
}

/// A number of states as a 32-bit state number (Mata keeps counts below `u32::MAX`).
pub(crate) fn raw_state_count(num_states: usize) -> u32 {
    u32::try_from(num_states).expect("Mata keeps the number of states below 2^32")
}

/// Converts raw states from the bridge into [`State`]s, reusing the allocation.
pub(crate) fn states_from_vec(raw: Vec<u32>) -> Vec<State> {
    raw.into_iter().map(State).collect()
}
