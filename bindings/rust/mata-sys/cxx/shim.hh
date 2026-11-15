// The C++ side of the mata-sys bridge: flat functions over Mata's NFA API. Automata never carry an alphabet.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

#include "mata/automaton.hh"
#include "mata/nfa/nfa.hh"
#include "mata/parser/inter-aut.hh"
#include "mata/utils/generator-support.hh"
#include "rust/cxx.h"

#if MATA_HAS_GENERATOR_SUPPORT
	#include <generator>
	#include <ranges>
#endif

namespace mata_sys {

using mata::Symbol;
using mata::nfa::Nfa;
using mata::nfa::State;

// The bridge passes states as `u32`.
static_assert(
	std::is_same_v<State, std::uint32_t>,
	"mata-sys requires Mata built with 32-bit states (MATA_STATE_BITS=32, the default)"
);

// Shared types, defined by the bridge in src/lib.rs.
struct Limits;
enum class Complementation : std::uint8_t;
enum class Determinization : std::uint8_t;
enum class Minimization : std::uint8_t;
enum class Reduction : std::uint8_t;
enum class ResidualConstruction : std::uint8_t;
enum class Direction : std::uint8_t;
enum class CheckAlgorithm : std::uint8_t;

/// The automata of one parsed `.mata` text, taken out one by one with `parsed_take()`.
class ParsedNfas {
  public:
	explicit ParsedNfas(std::vector<Nfa> automata) : automata_(std::move(automata)), taken_(automata_.size(), false) {}

	std::size_t size() const { return automata_.size(); }
	std::unique_ptr<Nfa> take(std::size_t index);

  private:
	std::vector<Nfa> automata_;
	std::vector<bool> taken_;
};

/// The words of an automaton up to some length (produced lazily where `std::generator` exists).
class WordStream {
  public:
	WordStream(const Nfa& nfa, std::size_t max_length);
	// Not movable: the generator refers to `source_`.
	WordStream(const WordStream&) = delete;
	WordStream& operator=(const WordStream&) = delete;

	/// Writes the next word into `word`; false when there are no more words.
	bool next(rust::Vec<Symbol>& word);

  private:
	// The epsilon-free copy that the words come from.
	Nfa source_;
#if MATA_HAS_GENERATOR_SUPPORT
	std::generator<mata::Word> generator_;
	std::optional<std::ranges::iterator_t<std::generator<mata::Word>>> position_ = std::nullopt;
#else
	std::vector<mata::Word> words_ = {};
	std::size_t position_ = 0;
#endif
};

// Construction.
std::unique_ptr<Nfa> new_nfa(std::size_t num_states);
std::unique_ptr<Nfa> clone_nfa(const Nfa& nfa);
std::unique_ptr<Nfa> empty_word_nfa();
std::unique_ptr<Nfa> word_nfa(rust::Slice<const Symbol> word);
std::unique_ptr<Nfa> sigma_star_nfa(rust::Slice<const Symbol> symbols);
std::unique_ptr<Nfa> regex_nfa(rust::Str pattern);

// Parsing.
std::unique_ptr<ParsedNfas> parse_mata(
	rust::Str text,
	bool numeric_symbols,
	rust::Slice<const rust::String> names,
	rust::Slice<const Symbol> named_symbols,
	Symbol next_symbol,
	rust::Vec<rust::String>& new_names,
	rust::Vec<Symbol>& new_symbols
);
std::size_t parsed_len(const ParsedNfas& parsed);
std::unique_ptr<Nfa> parsed_take(ParsedNfas& parsed, std::size_t index);

// Read-only views shared by NFAs and NFTs, through their common base class.
const mata::Automaton& nfa_automaton(const Nfa& nfa);
std::size_t num_states(const mata::Automaton& automaton);
bool is_initial(const mata::Automaton& automaton, State state);
rust::Slice<const State> initial_states(const mata::Automaton& automaton);
bool is_final(const mata::Automaton& automaton, State state);
rust::Slice<const State> final_states(const mata::Automaton& automaton);
bool contains_transition(const mata::Automaton& automaton, State source, Symbol symbol, State target);
std::size_t num_transitions(const mata::Automaton& automaton);
std::size_t num_symbol_posts(const mata::Automaton& automaton, State source);
Symbol symbol_post_symbol(const mata::Automaton& automaton, State source, std::size_t index);
rust::Slice<const State> symbol_post_targets(const mata::Automaton& automaton, State source, std::size_t index);
rust::Slice<const State> successors(const mata::Automaton& automaton, State source, Symbol symbol);
rust::Vec<Symbol> used_symbols(const mata::Automaton& automaton);
bool has_epsilon_transitions(const mata::Automaton& automaton);
rust::Vec<State> reachable_states(const mata::Automaton& automaton);
rust::Vec<State> useful_states(const mata::Automaton& automaton);
bool is_acyclic(const mata::Automaton& automaton);
bool is_lang_empty(const mata::Automaton& automaton);

// States.
State add_state(Nfa& nfa);
void add_initial(Nfa& nfa, State state);
bool remove_initial(Nfa& nfa, State state);
void add_final(Nfa& nfa, State state);
bool remove_final(Nfa& nfa, State state);

// Transitions.
void add_transition(Nfa& nfa, State source, Symbol symbol, State target);
bool remove_transition(Nfa& nfa, State source, Symbol symbol, State target);

// Structural queries.
bool is_deterministic(const Nfa& nfa);
bool is_minimal(const Nfa& nfa);
bool is_complete(const Nfa& nfa, rust::Slice<const Symbol> symbols);
bool is_identical(const Nfa& lhs, const Nfa& rhs);
void adopt(Nfa& nfa);
bool accepts(const Nfa& nfa, rust::Slice<const Symbol> word);

// In-place operations.
void trim(Nfa& nfa);
void remove_epsilon(Nfa& nfa);
bool make_complete(Nfa& nfa, rust::Slice<const Symbol> symbols);
void unite_with(Nfa& nfa, const Nfa& other);

// Operations that build a new automaton.
std::unique_ptr<Nfa> union_nondet(const Nfa& lhs, const Nfa& rhs);
std::unique_ptr<Nfa> intersection(const Nfa& lhs, const Nfa& rhs);
std::unique_ptr<Nfa> concatenate(const Nfa& lhs, const Nfa& rhs);
std::unique_ptr<Nfa> difference(const Nfa& lhs, const Nfa& rhs);
std::unique_ptr<Nfa> complement(const Nfa& nfa, rust::Slice<const Symbol> symbols, Complementation algorithm);
std::unique_ptr<Nfa> determinize(const Nfa& nfa, Determinization algorithm);
std::unique_ptr<Nfa> minimize(const Nfa& nfa, Minimization algorithm);
std::unique_ptr<Nfa>
	reduce(const Nfa& nfa, Reduction algorithm, ResidualConstruction construction, Direction direction);
std::unique_ptr<Nfa> revert(const Nfa& nfa);

// The same operations within limits. Null means that a limit stopped the construction.
std::unique_ptr<Nfa> difference_within(const Nfa& lhs, const Nfa& rhs, const Limits& limits);
std::unique_ptr<Nfa> complement_within(
	const Nfa& nfa, rust::Slice<const Symbol> symbols, Complementation algorithm, const Limits& limits
);
std::unique_ptr<Nfa> determinize_within(const Nfa& nfa, const Limits& limits);
std::unique_ptr<Nfa> minimize_within(const Nfa& nfa, Minimization algorithm, const Limits& limits);

// Decision procedures. A `false` from the checks fills `counterexample`; the `decide_` variants compute none.
bool accepting_run(const Nfa& nfa, rust::Vec<Symbol>& word, rust::Vec<State>& path);
bool some_word(const Nfa& nfa, rust::Vec<Symbol>& word);
bool shortest_word(const Nfa& nfa, rust::Vec<Symbol>& word);
bool is_universal(
	const Nfa& nfa, rust::Slice<const Symbol> symbols, CheckAlgorithm algorithm, rust::Vec<Symbol>& counterexample
);
bool decide_universal(const Nfa& nfa, rust::Slice<const Symbol> symbols, CheckAlgorithm algorithm);
bool is_included(const Nfa& smaller, const Nfa& bigger, CheckAlgorithm algorithm, rust::Vec<Symbol>& counterexample);
bool decide_included(const Nfa& smaller, const Nfa& bigger, CheckAlgorithm algorithm);
bool is_disjoint(const Nfa& lhs, const Nfa& rhs, rust::Vec<Symbol>& common_word);
bool decide_disjoint(const Nfa& lhs, const Nfa& rhs);

// Words.
std::unique_ptr<WordStream> words(const Nfa& nfa, std::size_t max_length);
bool next_word(WordStream& stream, rust::Vec<Symbol>& word);

// Strings (`mata::applications::strings`). Words are returned concatenated, with their lengths.
void strings_shortest_words(const Nfa& nfa, rust::Vec<Symbol>& symbols, rust::Vec<std::size_t>& lengths);
bool strings_longest_words(const Nfa& nfa, rust::Vec<Symbol>& symbols, rust::Vec<std::size_t>& lengths);
rust::Vec<Symbol> strings_accepted_symbols(const Nfa& nfa);
rust::Vec<std::size_t> strings_word_lengths(const Nfa& nfa);
bool strings_is_lang_eps(const Nfa& nfa);
std::unique_ptr<ParsedNfas> strings_segments(const Nfa& nfa, rust::Slice<const Symbol> epsilons);
std::unique_ptr<Nfa> strings_reluctant_nfa(const Nfa& nfa);

// Printing.
rust::String print_mata(const Nfa& nfa, rust::Slice<const rust::String> names, rust::Slice<const Symbol> symbols);
rust::String print_dot(
	const Nfa& nfa,
	bool ascii_labels,
	bool intervals,
	std::int32_t max_label_length,
	rust::Slice<const rust::String> names,
	rust::Slice<const Symbol> symbols
);

// Further operations of the library.
std::unique_ptr<Nfa> concatenate_over_epsilon(const Nfa& lhs, const Nfa& rhs);
std::unique_ptr<Nfa> as_dfa(const Nfa& nfa);
std::unique_ptr<Nfa> concatenate_nth_power(const Nfa& nfa, std::uint32_t power);
std::unique_ptr<Nfa> union_det_complete(const Nfa& lhs, const Nfa& rhs, rust::Slice<const Symbol> symbols);
std::unique_ptr<Nfa> product(const Nfa& lhs, const Nfa& rhs, bool either_final, Symbol first_epsilon);
std::unique_ptr<Nfa> left_quotient(const Nfa& nfa, const Nfa& prefixes, Symbol first_epsilon);
std::unique_ptr<Nfa> right_quotient(const Nfa& nfa, const Nfa& suffixes, Symbol first_epsilon);
std::unique_ptr<Nfa> decode_utf8(const Nfa& nfa);
std::unique_ptr<Nfa> one_letter_automaton(const Nfa& nfa, Symbol abstract_symbol);
bool is_flat(const Nfa& nfa);
bool accepts_prefix(const Nfa& nfa, rust::Slice<const Symbol> word, bool use_epsilon);
bool word_from_complement(const Nfa& nfa, rust::Slice<const Symbol> symbols, rust::Vec<Symbol>& word);
bool word_from_difference(const Nfa& included, const Nfa& excluded, rust::Vec<Symbol>& word);
std::unique_ptr<Nfa> random_tabakov_vardi(
	std::size_t num_states,
	std::size_t alphabet_size,
	double transition_ratio,
	double final_density,
	bool has_seed,
	std::uint32_t seed
);

// Intermediate automata: parsed `.mata` sections that are not constructed yet.

/// Intermediate automata of one parse or one mintermization, taken out one by one with `intermediate_auts_take()`.
class IntermediateAuts {
  public:
	explicit IntermediateAuts(std::vector<mata::IntermediateAut> automata)
		: automata_(std::move(automata)),
		  taken_(automata_.size(), false) {}

	std::size_t size() const { return automata_.size(); }
	std::unique_ptr<mata::IntermediateAut> take(std::size_t index);

  private:
	std::vector<mata::IntermediateAut> automata_;
	std::vector<bool> taken_;
};

/// Borrowed intermediate automata to mintermize together; the caller keeps them alive.
class IntermediateRefs {
  public:
	std::vector<const mata::IntermediateAut*> automata = {};
};

std::unique_ptr<IntermediateAuts> intermediate_parse_mata(rust::Str text);
std::size_t intermediate_auts_len(const IntermediateAuts& automata);
std::unique_ptr<mata::IntermediateAut> intermediate_auts_take(IntermediateAuts& automata, std::size_t index);
std::unique_ptr<IntermediateRefs> intermediate_refs_new();
void intermediate_refs_push(IntermediateRefs& refs, const mata::IntermediateAut& automaton);
std::unique_ptr<IntermediateAuts> intermediate_mintermize(const IntermediateRefs& refs);
std::unique_ptr<Nfa> intermediate_construct_nfa(
	const mata::IntermediateAut& automaton,
	rust::Slice<const rust::String> names,
	rust::Slice<const Symbol> named_symbols,
	Symbol next_symbol,
	rust::Vec<rust::String>& new_names,
	rust::Vec<Symbol>& new_symbols
);
rust::String intermediate_print(const mata::IntermediateAut& automaton);

} // namespace mata_sys
