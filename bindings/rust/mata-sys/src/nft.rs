//! The bridge to Mata's experimental transducers (`mata::nft`), over `cxx/nft.hh`. Every transducer it hands
//! out is well formed: levels within range, initial and final states on level 0.

/// The most levels a transducer may have (the shim unwinds jumps into one state per level).
pub const MAX_LEVELS: usize = 1024;

/// The NFT bridge.
#[cxx::bridge(namespace = "mata_sys")]
pub mod ffi {
	unsafe extern "C++" {
		include!("mata-sys/cxx/nft.hh");

		/// `mata::nfa::Nfa`, from the main bridge.
		#[namespace = "mata::nfa"]
		type Nfa = crate::ffi::Nfa;

		/// `mata::Automaton`, from the main bridge.
		#[namespace = "mata"]
		type Automaton = crate::ffi::Automaton;

		/// `mata::nft::Nft`.
		#[namespace = "mata::nft"]
		type Nft;

		/// The transducers of a parsed `.mata` text.
		type ParsedNfts;

		// Construction.

		/// The most levels a transducer may have, [`MAX_LEVELS`](super::MAX_LEVELS).
		fn nft_max_levels() -> usize;
		/// A transducer with `num_levels` levels (1 to `MAX_LEVELS`) and no states.
		fn nft_new(num_levels: usize) -> Result<UniquePtr<Nft>>;
		/// A deep copy.
		fn nft_clone(nft: &Nft) -> Result<UniquePtr<Nft>>;
		/// The identity relation on the language of `nfa`, with `num_levels` levels.
		fn nft_identity(nfa: &Nfa, num_levels: usize) -> Result<UniquePtr<Nft>>;
		/// The transducer accepting only the tuple of empty words.
		fn nft_empty_word(num_levels: usize) -> Result<UniquePtr<Nft>>;
		/// Prepares a transducer built by other C++ code for the bridge: drops its alphabets, throws unless
		/// it is well formed and has at most `MAX_LEVELS` levels, and unwinds its jumps.
		fn nft_adopt(nft: Pin<&mut Nft>) -> Result<()>;

		// Parsing.

		/// Parses every section of a `.mata` text, which must all be well-formed explicit NFTs
		/// (`@NFT-explicit`, with `%Levels` and `%LevelsNum`). Symbol names work as in
		/// `crate::ffi::parse_mata`, with one alphabet for all levels.
		fn nft_parse_mata(
			text: &str,
			numeric_symbols: bool,
			names: &[String],
			named_symbols: &[u32],
			next_symbol: u32,
			new_names: &mut Vec<String>,
			new_symbols: &mut Vec<u32>,
		) -> Result<UniquePtr<ParsedNfts>>;
		/// The number of parsed transducers.
		fn nft_parsed_len(parsed: &ParsedNfts) -> usize;
		/// Moves out the transducer at `index`.
		fn nft_parsed_take(parsed: Pin<&mut ParsedNfts>, index: usize) -> Result<UniquePtr<Nft>>;

		// Structure.

		/// The structural view of a transducer (states, initial and final states, level-by-level edges).
		fn nft_automaton(nft: &Nft) -> &Automaton;
		/// The number of levels (tapes).
		fn nft_num_levels(nft: &Nft) -> usize;
		/// The level of `state`. Throws if `state` does not exist.
		fn nft_level(nft: &Nft, state: u32) -> Result<usize>;
		/// Adds a state with `level` and returns it. Throws if `level` does not exist.
		fn nft_add_state(nft: Pin<&mut Nft>, level: usize) -> Result<u32>;
		/// Makes `state` initial. Throws unless `state` exists and has level 0.
		fn nft_add_initial(nft: Pin<&mut Nft>, state: u32) -> Result<()>;
		/// Makes `state` non-initial; false if it was not initial.
		fn nft_remove_initial(nft: Pin<&mut Nft>, state: u32) -> bool;
		/// Makes `state` final. Throws unless `state` exists and has level 0.
		fn nft_add_final(nft: Pin<&mut Nft>, state: u32) -> Result<()>;
		/// Makes `state` non-final; false if it was not final.
		fn nft_remove_final(nft: Pin<&mut Nft>, state: u32) -> bool;
		/// Adds the transition reading `symbols[i]` on level `i`, between two level-0 states, through new
		/// inner states. Throws unless there is one symbol per level and both states have level 0.
		fn nft_add_transition(
			nft: Pin<&mut Nft>,
			source: u32,
			symbols: &[u32],
			target: u32,
		) -> Result<()>;
		/// Adds a path from `source` to `target` (both level 0) that reads one word per level: the word
		/// for level `i` is the next `lengths[i]` symbols of `symbols`. Epsilon in the words is dropped.
		fn nft_insert_words(
			nft: Pin<&mut Nft>,
			source: u32,
			symbols: &[u32],
			lengths: &[usize],
			target: u32,
		) -> Result<()>;
		/// Whether the tuple of words (encoded as for `nft_insert_words`) is in the relation.
		fn nft_accepts(nft: &Nft, symbols: &[u32], lengths: &[usize]) -> Result<bool>;

		// Operations.

		/// Removes the states that are not both reachable and co-reachable, renumbering the rest.
		fn nft_trim(nft: Pin<&mut Nft>) -> Result<()>;
		/// The union of the relations. Throws if the numbers of levels differ.
		fn nft_union(lhs: &Nft, rhs: &Nft) -> Result<UniquePtr<Nft>>;
		/// The intersection of the relations. Throws if the numbers of levels differ.
		fn nft_intersection(lhs: &Nft, rhs: &Nft) -> Result<UniquePtr<Nft>>;
		/// The concatenation of the relations. Throws if the numbers of levels differ.
		fn nft_concatenate(lhs: &Nft, rhs: &Nft) -> Result<UniquePtr<Nft>>;
		/// Composes, synchronizing level `lhs_sync_level` of `lhs` with `rhs_sync_level` of `rhs`. Throws
		/// if a level does not exist or no level would be left.
		fn nft_compose(
			lhs: &Nft,
			rhs: &Nft,
			lhs_sync_level: usize,
			rhs_sync_level: usize,
			project_out_sync_level: bool,
		) -> Result<UniquePtr<Nft>>;
		/// Restricts level `level` to the language of `nfa`, optionally projecting that level out.
		fn nft_apply(
			nft: &Nft,
			nfa: &Nfa,
			level: usize,
			project_out: bool,
		) -> Result<UniquePtr<Nft>>;
		/// Removes the given levels (strictly increasing); at least one level must remain.
		fn nft_project_out(nft: &Nft, levels: &[usize]) -> Result<UniquePtr<Nft>>;
		/// Keeps only the given levels (strictly increasing, nonempty).
		fn nft_project_to(nft: &Nft, levels: &[usize]) -> Result<UniquePtr<Nft>>;
		/// Reverses the order of the levels (the inverse relation for two levels).
		fn nft_invert(nft: &Nft) -> Result<UniquePtr<Nft>>;
		/// Mata's `determinize`: the subset construction over the edges (epsilon counts as a symbol).
		fn nft_determinize(nft: &Nft) -> Result<UniquePtr<Nft>>;
		/// Mata's `remove_epsilon` (transitions reading epsilon on every level), then `trim`.
		fn nft_remove_epsilon(nft: &Nft) -> Result<UniquePtr<Nft>>;
		/// Every word reversed on its level (Mata's `revert`, then `invert_levels`).
		fn nft_reverse(nft: &Nft) -> Result<UniquePtr<Nft>>;
		/// The NFA over the interleaved encoding: jumps unwound, one symbol per level and step. With
		/// `expand_dont_care`, edges labelled `DONT_CARE` become one edge per symbol of
		/// `dont_care_replacements` (none if it is empty); otherwise they stay `DONT_CARE`.
		fn nft_to_nfa(
			nft: &Nft,
			expand_dont_care: bool,
			dont_care_replacements: &[u32],
		) -> Result<UniquePtr<Nfa>>;

		// Decision procedures.

		/// Whether the relation of `smaller` is included in that of `bigger`.
		fn nft_is_included(smaller: &Nft, bigger: &Nft) -> Result<bool>;
		/// Whether the relations are equal.
		fn nft_are_equivalent(lhs: &Nft, rhs: &Nft) -> Result<bool>;
		/// Mata's `Nft::is_deterministic`: one initial state and at most one edge per state and symbol,
		/// where `DONT_CARE` overlaps every symbol but epsilon unless both edges reach the same state.
		fn nft_is_deterministic(nft: &Nft) -> Result<bool>;

		// Printing.

		/// The `.mata` text (`@NFT-explicit`), with names from (`names`, `symbols`).
		fn nft_print_mata(nft: &Nft, names: &[String], symbols: &[u32]) -> Result<String>;
		/// DOT text. `max_label_length` -1 means no limit.
		fn nft_print_dot(
			nft: &Nft,
			ascii_labels: bool,
			intervals: bool,
			max_label_length: i32,
			names: &[String],
			symbols: &[u32],
		) -> Result<String>;
	}
}

impl std::fmt::Debug for ffi::Nft {
	fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
		f.debug_struct("Nft")
			.field("num_levels", &ffi::nft_num_levels(self))
			.field("automaton", ffi::nft_automaton(self))
			.finish()
	}
}

impl std::fmt::Debug for ffi::ParsedNfts {
	fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
		f.debug_struct("ParsedNfts")
			.field("len", &ffi::nft_parsed_len(self))
			.finish()
	}
}

#[cfg(test)]
mod tests {
	#[test]
	fn max_levels_agrees_with_the_shim() {
		assert_eq!(super::ffi::nft_max_levels(), super::MAX_LEVELS);
	}
}
