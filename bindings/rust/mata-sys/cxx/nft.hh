// The C++ side of the NFT bridge (src/nft.rs): flat functions over Mata's experimental transducers.
// Transducers handed to Rust are well formed, and arguments that Mata would trust are checked.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "mata/nft/nft.hh"
#include "rust/cxx.h"

namespace mata_sys {

using mata::nft::Nft;

/// The most levels a transducer may have (the shim unwinds jumps into one state per level).
inline constexpr std::size_t MAX_LEVELS = 1'024;

/// The transducers of one parsed `.mata` text, taken out one by one with `nft_parsed_take()`.
class ParsedNfts {
  public:
	explicit ParsedNfts(std::vector<Nft> transducers) : transducers_(std::move(transducers)) {}

	std::size_t size() const { return transducers_.size(); }
	std::unique_ptr<Nft> take(std::size_t index);

  private:
	std::vector<Nft> transducers_;
};

// Construction.
std::size_t nft_max_levels();
std::unique_ptr<Nft> nft_new(std::size_t num_levels);
std::unique_ptr<Nft> nft_clone(const Nft& nft);
std::unique_ptr<Nft> nft_identity(const mata::nfa::Nfa& nfa, std::size_t num_levels);
std::unique_ptr<Nft> nft_empty_word(std::size_t num_levels);
void nft_check_well_formed(const Nft& nft);
void nft_adopt(Nft& nft);

// Parsing.
std::unique_ptr<ParsedNfts> nft_parse_mata(
	rust::Str text,
	bool numeric_symbols,
	rust::Slice<const rust::String> names,
	rust::Slice<const mata::Symbol> named_symbols,
	mata::Symbol next_symbol,
	rust::Vec<rust::String>& new_names,
	rust::Vec<mata::Symbol>& new_symbols
);
std::size_t nft_parsed_len(const ParsedNfts& parsed);
std::unique_ptr<Nft> nft_parsed_take(ParsedNfts& parsed, std::size_t index);

// Structure.
const mata::Automaton& nft_automaton(const Nft& nft);
std::size_t nft_num_levels(const Nft& nft);
std::size_t nft_level(const Nft& nft, mata::nfa::State state);
mata::nfa::State nft_add_state(Nft& nft, std::size_t level);
void nft_add_initial(Nft& nft, mata::nfa::State state);
bool nft_remove_initial(Nft& nft, mata::nfa::State state);
void nft_add_final(Nft& nft, mata::nfa::State state);
bool nft_remove_final(Nft& nft, mata::nfa::State state);
void nft_add_transition(
	Nft& nft, mata::nfa::State source, rust::Slice<const mata::Symbol> symbols, mata::nfa::State target
);
void nft_insert_words(
	Nft& nft,
	mata::nfa::State source,
	rust::Slice<const mata::Symbol> symbols,
	rust::Slice<const std::size_t> lengths,
	mata::nfa::State target
);
bool nft_accepts(const Nft& nft, rust::Slice<const mata::Symbol> symbols, rust::Slice<const std::size_t> lengths);

// Operations.
void nft_trim(Nft& nft);
std::unique_ptr<Nft> nft_union(const Nft& lhs, const Nft& rhs);
std::unique_ptr<Nft> nft_intersection(const Nft& lhs, const Nft& rhs);
std::unique_ptr<Nft> nft_concatenate(const Nft& lhs, const Nft& rhs);
std::unique_ptr<Nft> nft_compose(
	const Nft& lhs, const Nft& rhs, std::size_t lhs_sync_level, std::size_t rhs_sync_level, bool project_out_sync_level
);
std::unique_ptr<Nft> nft_apply(const Nft& nft, const mata::nfa::Nfa& nfa, std::size_t level, bool project_out);
std::unique_ptr<Nft> nft_project_out(const Nft& nft, rust::Slice<const std::size_t> levels);
std::unique_ptr<Nft> nft_project_to(const Nft& nft, rust::Slice<const std::size_t> levels);
std::unique_ptr<Nft> nft_invert(const Nft& nft);
std::unique_ptr<Nft> nft_determinize(const Nft& nft);
std::unique_ptr<Nft> nft_remove_epsilon(const Nft& nft);
std::unique_ptr<Nft> nft_reverse(const Nft& nft);
std::unique_ptr<mata::nfa::Nfa>
	nft_to_nfa(const Nft& nft, bool expand_dont_care, rust::Slice<const mata::Symbol> dont_care_replacements);

// Decision procedures.
bool nft_is_included(const Nft& smaller, const Nft& bigger);
bool nft_are_equivalent(const Nft& lhs, const Nft& rhs);
bool nft_is_deterministic(const Nft& nft);

// Printing.
rust::String
	nft_print_mata(const Nft& nft, rust::Slice<const rust::String> names, rust::Slice<const mata::Symbol> symbols);
rust::String nft_print_dot(
	const Nft& nft,
	bool ascii_labels,
	bool intervals,
	std::int32_t max_label_length,
	rust::Slice<const rust::String> names,
	rust::Slice<const mata::Symbol> symbols
);

// The other operations of the script language's `nft:` namespace.
std::unique_ptr<Nft> nft_complement(const Nft& nft, rust::Slice<const mata::Symbol> symbols);
bool nft_is_complete(const Nft& nft, rust::Slice<const mata::Symbol> symbols);
std::unique_ptr<Nft> nft_concatenate_nth_power(const Nft& nft, std::uint32_t power);
bool nft_contains_jump_transitions(const Nft& nft);
std::unique_ptr<Nft> nft_sigma_star(std::size_t num_levels);
std::unique_ptr<Nft> nft_from_nfa_with_levels_advancing(const mata::nfa::Nfa& nfa, std::size_t num_levels);
bool nft_has_epsilon_cycle(const Nft& nft);
bool nft_is_identical(const Nft& lhs, const Nft& rhs);
bool nft_is_in_lang(const Nft& nft, rust::Slice<const mata::Symbol> word, bool match_prefix, bool has_epsilon_cycles);
std::unique_ptr<Nft> nft_insert_level(const Nft& nft, std::size_t level);
std::unique_ptr<Nft> nft_insert_levels(const Nft& nft, rust::Slice<const bool> mask);
std::unique_ptr<Nft> nft_reduce(const Nft& nft);
std::unique_ptr<Nft> nft_revert(const Nft& nft);
bool nft_is_universal(const Nft& nft, rust::Slice<const mata::Symbol> symbols);
std::unique_ptr<Nft> nft_unwind_jumps(const Nft& nft);

} // namespace mata_sys
