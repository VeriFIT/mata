//! Native build of [Mata](https://github.com/VeriFIT/mata), the automata library of the VeriFIT group,
//! and a [cxx] bridge to its NFA API ([`ffi`]) and its experimental transducer API ([`nft::ffi`]).
//!
//! The `mata` crate is the safe API on top. The bridge is public so that other crates can add their own C++
//! code on the same `libmata`: build scripts get the headers as `DEP_MATA_INCLUDE`, and a bridge names the
//! types as `type Nfa = mata_sys::ffi::Nfa;`.
//!
//! # Conventions of the bridge
//!
//! - States and symbols are `u32`; counts and indices are `usize`.
//! - Functions that can throw return `Result<_, cxx::Exception>`. The `_within` functions return null when a
//!   limit stopped them.
//! - Every function is safe to call: the shim checks the preconditions that Mata checks only in debug builds.
//! - Automata never carry an alphabet; alphabets are passed per call.
//! - Language operations remove epsilon transitions first (on a copy), as Mata reads epsilon as a letter there.
//!
//! # Build
//!
//! The build script builds the Mata sources packaged with the crate (or, in the Mata repository, those of the
//! repository), or uses an installed Mata.
//! The crate README lists the requirements and options.

pub use cxx;

pub mod nft;

/// The bridge: shared enums, the opaque C++ types and the shim functions (`cxx/shim.hh`).
#[cxx::bridge(namespace = "mata_sys")]
pub mod ffi {
	/// Limits of the `_within` operations.
	#[derive(Clone, Copy, Debug)]
	struct Limits {
		/// The most states of each deterministic automaton built, the result included; `usize::MAX` for none.
		max_states: usize,
	}

	/// Complementation algorithm.
	#[repr(u8)]
	#[derive(Debug)]
	enum Complementation {
		/// Determinize, complete, swap final and non-final states.
		Classical,
		/// Minimize with Brzozowski's algorithm, complete, swap final and non-final states.
		Brzozowski,
	}

	/// Determinization algorithm.
	#[repr(u8)]
	#[derive(Debug)]
	enum Determinization {
		/// The "Four Russians" subset construction.
		FourRussians,
		/// The classical subset construction.
		Classical,
	}

	/// Minimization algorithm.
	#[repr(u8)]
	#[derive(Debug)]
	enum Minimization {
		/// Determinize the reversal twice.
		Brzozowski,
		/// Partition refinement on a trimmed DFA.
		Hopcroft,
	}

	/// Language-preserving reduction.
	#[repr(u8)]
	#[derive(Debug)]
	enum Reduction {
		/// Merge states by forward simulation.
		Simulation,
		/// Build the residual automaton.
		Residual,
	}

	/// When a residual reduction computes the residual states (Mata's `"type"` parameter).
	#[repr(u8)]
	#[derive(Debug)]
	enum ResidualConstruction {
		/// After determinization (`"after"`).
		After,
		/// During determinization (`"with"`).
		With,
	}

	/// Direction of a residual reduction.
	#[repr(u8)]
	#[derive(Debug)]
	enum Direction {
		/// Forward residuals.
		Forward,
		/// Backward residuals (of the reversed automaton).
		Backward,
	}

	/// Algorithm of the inclusion and universality checks.
	#[repr(u8)]
	#[derive(Debug)]
	enum CheckAlgorithm {
		/// Complement and intersect.
		Naive,
		/// Antichain-based search.
		Antichains,
	}

	unsafe extern "C++" {
		include!("mata-sys/cxx/shim.hh");

		/// `mata::nfa::Nfa`.
		#[namespace = "mata::nfa"]
		type Nfa;

		/// `mata::Automaton`, the structural base class of NFAs and NFTs.
		#[namespace = "mata"]
		type Automaton;

		/// The automata of a parsed `.mata` text.
		type ParsedNfas;

		/// A stream of the words of an automaton up to a length.
		type WordStream;

		// Construction.

		/// An automaton with states `0..num_states` and nothing else.
		fn new_nfa(num_states: usize) -> Result<UniquePtr<Nfa>>;
		/// A deep copy.
		fn clone_nfa(nfa: &Nfa) -> Result<UniquePtr<Nfa>>;
		/// The automaton accepting only the empty word.
		fn empty_word_nfa() -> Result<UniquePtr<Nfa>>;
		/// The automaton accepting only `word`.
		fn word_nfa(word: &[u32]) -> Result<UniquePtr<Nfa>>;
		/// The one-state automaton accepting every word over `symbols`.
		fn sigma_star_nfa(symbols: &[u32]) -> Result<UniquePtr<Nfa>>;
		/// Mata's `create_from_regex`: an RE2 pattern over bytes (Latin-1). Throws on invalid patterns.
		fn regex_nfa(pattern: &str) -> Result<UniquePtr<Nfa>>;

		// Parsing.

		/// Parses every automaton section of a `.mata` text. With `numeric_symbols`, symbols must be
		/// numbers; otherwise symbol names are looked up in the pairs (`names`, `named_symbols`), and new
		/// names get fresh symbols from `next_symbol` on, reported in (`new_names`, `new_symbols`).
		/// Bit-vector sections are mintermized together.
		fn parse_mata(
			text: &str,
			numeric_symbols: bool,
			names: &[String],
			named_symbols: &[u32],
			next_symbol: u32,
			new_names: &mut Vec<String>,
			new_symbols: &mut Vec<u32>,
		) -> Result<UniquePtr<ParsedNfas>>;
		/// The number of parsed automata.
		fn parsed_len(parsed: &ParsedNfas) -> usize;
		/// Moves out the automaton at `index`; taking one twice is an error (the length does not change).
		fn parsed_take(parsed: Pin<&mut ParsedNfas>, index: usize) -> Result<UniquePtr<Nfa>>;

		// Read-only views, shared by NFAs and NFTs through their base class `mata::Automaton`.

		/// The structural view of an NFA.
		fn nfa_automaton(nfa: &Nfa) -> &Automaton;
		/// The number of states (one more than the largest state in use).
		fn num_states(automaton: &Automaton) -> usize;
		/// Whether `state` is initial.
		fn is_initial(automaton: &Automaton, state: u32) -> bool;
		/// The initial states, in no particular order.
		fn initial_states(automaton: &Automaton) -> &[u32];
		/// Whether `state` is final.
		fn is_final(automaton: &Automaton, state: u32) -> bool;
		/// The final states, in no particular order.
		fn final_states(automaton: &Automaton) -> &[u32];
		/// Whether the transition is present.
		fn contains_transition(
			automaton: &Automaton,
			source: u32,
			symbol: u32,
			target: u32,
		) -> bool;
		/// The number of transitions (linear time).
		fn num_transitions(automaton: &Automaton) -> usize;
		/// The number of distinct symbols on the transitions leaving `source`.
		fn num_symbol_posts(automaton: &Automaton, source: u32) -> usize;
		/// The `index`-th symbol leaving `source`, in ascending order.
		fn symbol_post_symbol(automaton: &Automaton, source: u32, index: usize) -> Result<u32>;
		/// The targets of the `index`-th symbol leaving `source`, sorted.
		fn symbol_post_targets(automaton: &Automaton, source: u32, index: usize) -> Result<&[u32]>;
		/// The targets of `source` under `symbol`, sorted.
		fn successors(automaton: &Automaton, source: u32, symbol: u32) -> &[u32];
		/// The symbols on transitions, sorted.
		fn used_symbols(automaton: &Automaton) -> Result<Vec<u32>>;
		/// Whether some transition reads epsilon.
		fn has_epsilon_transitions(automaton: &Automaton) -> bool;
		/// The states reachable from an initial state, sorted.
		fn reachable_states(automaton: &Automaton) -> Result<Vec<u32>>;
		/// The states that are reachable and can reach a final state, sorted.
		fn useful_states(automaton: &Automaton) -> Result<Vec<u32>>;
		/// No cycle among the states reachable from an initial state.
		fn is_acyclic(automaton: &Automaton) -> Result<bool>;
		/// Whether no path leads from an initial to a final state.
		fn is_lang_empty(automaton: &Automaton) -> Result<bool>;

		// States.

		/// Adds a state and returns it.
		fn add_state(nfa: Pin<&mut Nfa>) -> Result<u32>;
		/// Makes `state` initial. Throws if `state` is not below `num_states`.
		fn add_initial(nfa: Pin<&mut Nfa>, state: u32) -> Result<()>;
		/// Makes `state` non-initial; false if it was not initial.
		fn remove_initial(nfa: Pin<&mut Nfa>, state: u32) -> bool;
		/// Makes `state` final. Throws if `state` is not below `num_states`.
		fn add_final(nfa: Pin<&mut Nfa>, state: u32) -> Result<()>;
		/// Makes `state` non-final; false if it was not final.
		fn remove_final(nfa: Pin<&mut Nfa>, state: u32) -> bool;

		// Transitions.

		/// Adds `source --symbol--> target`. Throws if a state is not below `num_states`.
		fn add_transition(nfa: Pin<&mut Nfa>, source: u32, symbol: u32, target: u32) -> Result<()>;
		/// Removes a transition; false if it was not there.
		fn remove_transition(
			nfa: Pin<&mut Nfa>,
			source: u32,
			symbol: u32,
			target: u32,
		) -> Result<bool>;

		// Structural queries.

		/// No epsilon transitions, at most one initial state, at most one target per state and symbol.
		fn is_deterministic(nfa: &Nfa) -> bool;
		/// Deterministic, trimmed and without equivalent states; for the empty language, a single
		/// non-final initial state without transitions.
		fn is_minimal(nfa: &Nfa) -> Result<bool>;
		/// Every reachable state has a successor under every symbol of `symbols`; a transition over
		/// a symbol outside `symbols` is an error (epsilon is a move, never outside).
		fn is_complete(nfa: &Nfa, symbols: &[u32]) -> Result<bool>;
		/// The same number of states, initial and final states and transitions.
		fn is_identical(lhs: &Nfa, rhs: &Nfa) -> Result<bool>;
		/// Prepares an automaton built by other C++ code: drops its alphabet, and throws unless its
		/// transitions are sorted, unique and between existing states.
		fn adopt(nfa: Pin<&mut Nfa>) -> Result<()>;
		/// Whether `word` is accepted, following epsilon transitions.
		fn accepts(nfa: &Nfa, word: &[u32]) -> Result<bool>;

		// In-place operations.

		/// Removes the states that are not both reachable and co-reachable, renumbering the rest.
		fn trim(nfa: Pin<&mut Nfa>) -> Result<()>;
		/// Replaces epsilon transitions by the transitions they enable; the language is unchanged.
		fn remove_epsilon(nfa: Pin<&mut Nfa>) -> Result<()>;
		/// Adds transitions to a sink state (a new state, `num_states` before the call) where a symbol of
		/// `symbols` is missing. Returns whether anything was added.
		fn make_complete(nfa: Pin<&mut Nfa>, symbols: &[u32]) -> Result<bool>;
		/// Adds `other` to `nfa` as a disjoint union.
		fn unite_with(nfa: Pin<&mut Nfa>, other: &Nfa) -> Result<()>;

		// Operations that build a new automaton.

		/// L(lhs) ∪ L(rhs), as the disjoint union of the two automata.
		fn union_nondet(lhs: &Nfa, rhs: &Nfa) -> Result<UniquePtr<Nfa>>;
		/// L(lhs) ∩ L(rhs), by the product construction.
		fn intersection(lhs: &Nfa, rhs: &Nfa) -> Result<UniquePtr<Nfa>>;
		/// L(lhs) · L(rhs).
		fn concatenate(lhs: &Nfa, rhs: &Nfa) -> Result<UniquePtr<Nfa>>;
		/// L(lhs) \ L(rhs).
		fn difference(lhs: &Nfa, rhs: &Nfa) -> Result<UniquePtr<Nfa>>;
		/// The complement over `symbols`: a complete DFA.
		fn complement(
			nfa: &Nfa,
			symbols: &[u32],
			algorithm: Complementation,
		) -> Result<UniquePtr<Nfa>>;
		/// An equivalent DFA, by the subset construction.
		fn determinize(nfa: &Nfa, algorithm: Determinization) -> Result<UniquePtr<Nfa>>;
		/// The trimmed minimal DFA (determinizes and trims first for Hopcroft); for the empty language, a
		/// single non-final initial state.
		fn minimize(nfa: &Nfa, algorithm: Minimization) -> Result<UniquePtr<Nfa>>;
		/// `construction` and `direction` matter only for [`Reduction::Residual`].
		fn reduce(
			nfa: &Nfa,
			algorithm: Reduction,
			construction: ResidualConstruction,
			direction: Direction,
		) -> Result<UniquePtr<Nfa>>;
		/// The automaton of the reversed language.
		fn revert(nfa: &Nfa) -> Result<UniquePtr<Nfa>>;

		// The same operations within limits. Null means that a limit stopped the construction.

		/// `difference` within `limits`.
		fn difference_within(lhs: &Nfa, rhs: &Nfa, limits: &Limits) -> Result<UniquePtr<Nfa>>;
		/// `complement` within `limits`; the result counts with its sink state.
		fn complement_within(
			nfa: &Nfa,
			symbols: &[u32],
			algorithm: Complementation,
			limits: &Limits,
		) -> Result<UniquePtr<Nfa>>;
		/// `determinize` within `limits`.
		fn determinize_within(nfa: &Nfa, limits: &Limits) -> Result<UniquePtr<Nfa>>;
		/// `minimize` within `limits`, which apply to each determinization and to the result.
		fn minimize_within(
			nfa: &Nfa,
			algorithm: Minimization,
			limits: &Limits,
		) -> Result<UniquePtr<Nfa>>;

		// Decision procedures.

		/// An accepting run (its word may contain epsilon); false if the language is empty.
		fn accepting_run(nfa: &Nfa, word: &mut Vec<u32>, path: &mut Vec<u32>) -> Result<bool>;
		/// Some accepted word; false if the language is empty.
		fn some_word(nfa: &Nfa, word: &mut Vec<u32>) -> Result<bool>;
		/// A shortest accepted word; false if the language is empty.
		fn shortest_word(nfa: &Nfa, word: &mut Vec<u32>) -> Result<bool>;
		/// Whether every word over `symbols` is accepted; otherwise writes a rejected word.
		fn is_universal(
			nfa: &Nfa,
			symbols: &[u32],
			algorithm: CheckAlgorithm,
			counterexample: &mut Vec<u32>,
		) -> Result<bool>;
		/// [`is_universal`] without a counterexample, which Mata decides with less bookkeeping.
		fn decide_universal(nfa: &Nfa, symbols: &[u32], algorithm: CheckAlgorithm) -> Result<bool>;
		/// Whether L(smaller) ⊆ L(bigger); otherwise writes a word of L(smaller) \ L(bigger).
		fn is_included(
			smaller: &Nfa,
			bigger: &Nfa,
			algorithm: CheckAlgorithm,
			counterexample: &mut Vec<u32>,
		) -> Result<bool>;
		/// [`is_included`] without a counterexample, which Mata decides with less bookkeeping.
		fn decide_included(smaller: &Nfa, bigger: &Nfa, algorithm: CheckAlgorithm) -> Result<bool>;
		/// Whether L(lhs) ∩ L(rhs) = ∅, without building the product; otherwise writes a common word.
		fn is_disjoint(lhs: &Nfa, rhs: &Nfa, common_word: &mut Vec<u32>) -> Result<bool>;
		/// [`is_disjoint`] without a common word, which Mata decides with less bookkeeping.
		fn decide_disjoint(lhs: &Nfa, rhs: &Nfa) -> Result<bool>;

		// Words.

		/// The accepted words of length at most `max_length`, without repetition, in no particular
		/// order. The stream owns an epsilon-free copy of `nfa`.
		fn words(nfa: &Nfa, max_length: usize) -> Result<UniquePtr<WordStream>>;
		/// Writes the next word; false at the end.
		fn next_word(stream: Pin<&mut WordStream>, word: &mut Vec<u32>) -> Result<bool>;

		// Printing.

		/// The `.mata` text, with names from (`names`, `symbols`) and numbers for other symbols.
		fn print_mata(nfa: &Nfa, names: &[String], symbols: &[u32]) -> Result<String>;
		/// DOT text. `max_label_length` -1 means no limit.
		fn print_dot(
			nfa: &Nfa,
			ascii_labels: bool,
			intervals: bool,
			max_label_length: i32,
			names: &[String],
			symbols: &[u32],
		) -> Result<String>;

		// Further operations of the library.

		/// L(lhs) · L(rhs), joined by epsilon transitions from the final states of `lhs` to the initial
		/// states of `rhs`.
		fn concatenate_over_epsilon(lhs: &Nfa, rhs: &Nfa) -> Result<UniquePtr<Nfa>>;
	}
}

/// Mata's epsilon symbol, `u32::MAX`.
pub const EPSILON: u32 = u32::MAX;

/// The version of the `libmata` this crate links, as its CMake package declares it.
pub const LIBRARY_VERSION: &str = env!("MATA_LIBRARY_VERSION");

impl std::fmt::Debug for ffi::Automaton {
	fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
		f.debug_struct("Automaton")
			.field("num_states", &ffi::num_states(self))
			.field("initial", &ffi::initial_states(self))
			.field("final", &ffi::final_states(self))
			.field("num_transitions", &ffi::num_transitions(self))
			.finish()
	}
}

impl std::fmt::Debug for ffi::Nfa {
	fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
		f.debug_tuple("Nfa")
			.field(ffi::nfa_automaton(self))
			.finish()
	}
}

impl std::fmt::Debug for ffi::ParsedNfas {
	fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
		f.debug_struct("ParsedNfas")
			.field("len", &ffi::parsed_len(self))
			.finish()
	}
}

impl std::fmt::Debug for ffi::WordStream {
	fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
		f.debug_struct("WordStream").finish_non_exhaustive()
	}
}
