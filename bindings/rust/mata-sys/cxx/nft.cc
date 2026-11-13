#include "mata-sys/cxx/nft.hh"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "mata-sys/cxx/common.hh"
#include "mata-sys/src/nft.rs.h"
#include "mata/applications/strings.hh"
#include "mata/nft/builder.hh"
#include "mata/parser/parser.hh"

namespace mata_sys {

namespace {

using mata::Symbol;
using mata::Word;
using mata::nfa::State;
using mata::nft::Level;
using LevelSet = mata::utils::OrdVector<Level>;
using detail::check_state;
using detail::to_word;

/// Unwinds every jump of `nft` into one transition per level; the relation does not change.
void unwind_jumps(Nft& nft) {
	if (!nft.contains_jump_transitions()) { return; }
	// unwind_jumps_inplace() needs exactly one level per state.
	nft.levels.resize(nft.num_of_states());
	nft.unwind_jumps_inplace({mata::nft::DONT_CARE}, mata::nft::JumpMode::RepeatSymbol);
}

/// Wraps a transducer for Rust, without alphabets and without jumps (Mata's product mishandles jumps).
std::unique_ptr<Nft> boxed(Nft&& nft) {
	nft.alphabets = nullptr;
	unwind_jumps(nft);
	return std::make_unique<Nft>(std::move(nft));
}

std::string describe_state(const State state) { return "state " + std::to_string(state); }

/// Throws unless a transducer may have `num_levels` levels.
void check_num_levels(const std::size_t num_levels) {
	if (num_levels == 0) { throw std::invalid_argument("a transducer needs at least one level"); }
	if (num_levels > MAX_LEVELS) {
		throw std::invalid_argument(
			std::to_string(num_levels) + " levels are more than the " + std::to_string(MAX_LEVELS) + " supported"
		);
	}
}

/// Throws unless `level` is a level of `nft`.
Level check_level(const Nft& nft, const std::size_t level) {
	if (level >= nft.levels.num_of_levels) {
		throw std::out_of_range(
			"level " + std::to_string(level) + " does not exist (the transducer has " +
			std::to_string(nft.levels.num_of_levels) + " levels)"
		);
	}
	return static_cast<Level>(level);
}

/// Throws unless `state` is a state of `nft` with level 0, where transducer transitions start and end.
void check_level_zero_state(const Nft& nft, const State state) {
	check_state(nft, state);
	if (nft.levels[state] != 0) {
		throw std::invalid_argument(
			describe_state(state) + " has level " + std::to_string(nft.levels[state]) + ", not 0"
		);
	}
}

void check_same_levels(const Nft& lhs, const Nft& rhs) {
	if (lhs.levels.num_of_levels != rhs.levels.num_of_levels) {
		throw std::invalid_argument(
			"the transducers have " + std::to_string(lhs.levels.num_of_levels) + " and " +
			std::to_string(rhs.levels.num_of_levels) + " levels"
		);
	}
}

/// Checks that `levels` are levels of `nft` in strictly increasing order and returns them as a set.
LevelSet to_level_set(const Nft& nft, const rust::Slice<const std::size_t> levels) {
	auto result = std::vector<Level>();
	result.reserve(levels.size());
	for (const std::size_t level : levels) {
		if (!result.empty() && level <= result.back()) {
			throw std::invalid_argument("levels must be given in strictly increasing order");
		}
		result.push_back(check_level(nft, level));
	}
	return LevelSet(std::move(result));
}

/// Splits `symbols` into one word per level, `lengths[i]` symbols for level `i`, dropping epsilon.
std::vector<Word>
	split_words(const Nft& nft, const rust::Slice<const Symbol> symbols, const rust::Slice<const std::size_t> lengths) {
	if (lengths.size() != nft.levels.num_of_levels) {
		throw std::invalid_argument(
			"expected one word per level (" + std::to_string(nft.levels.num_of_levels) + "), got " +
			std::to_string(lengths.size())
		);
	}
	auto words = std::vector<Word>(lengths.size());
	std::size_t position = 0;
	for (std::size_t level = 0; level < lengths.size(); ++level) {
		if (lengths[level] > symbols.size() - position) {
			throw std::invalid_argument("word lengths exceed the symbols");
		}
		for (std::size_t i = 0; i < lengths[level]; ++i) {
			const Symbol symbol = symbols[position + i];
			if (symbol != mata::nft::EPSILON) { words[level].push_back(symbol); }
		}
		position += lengths[level];
	}
	if (position != symbols.size()) { throw std::invalid_argument("word lengths do not cover the symbols"); }
	return words;
}

/// `nft`, or a jump-free copy in `storage` (for transducers built by other C++ code).
const Nft& without_jumps(const Nft& nft, std::optional<Nft>& storage) {
	if (!nft.contains_jump_transitions()) { return nft; }
	storage.emplace(nft);
	unwind_jumps(*storage);
	return *storage;
}

/// A trimmed, jump-free copy of `nft`.
Nft trimmed_without_jumps(const Nft& nft) {
	auto copy = Nft(nft);
	unwind_jumps(copy);
	copy.trim();
	return copy;
}

/// The composition of `lhs_level` of `lhs` with `rhs_level` of `rhs`, on trimmed operands.
Nft compose_one_level(
	const Nft& lhs, const Nft& rhs, const Level lhs_level, const Level rhs_level, const bool project_out
) {
	auto result = mata::nft::compose(
		trimmed_without_jumps(lhs), trimmed_without_jumps(rhs), lhs_level, rhs_level, project_out,
		mata::nft::JumpMode::NoJump, mata::nft::CompositionMode::FastNoJump
	);
	result.trim();
	return result;
}

/// The sorted symbols of `lhs` and `rhs` other than EPSILON and DONT_CARE, and the smallest symbol neither uses.
std::pair<std::vector<Symbol>, Symbol> used_and_unused_symbols(const Nft& lhs, const Nft& rhs) {
	auto used = lhs.delta.get_used_symbols();
	used.insert(rhs.delta.get_used_symbols());
	auto symbols = std::vector<Symbol>();
	for (const Symbol symbol : used) {
		if (symbol != mata::nft::EPSILON && symbol != mata::nft::DONT_CARE) { symbols.push_back(symbol); }
	}
	Symbol unused = 0;
	while (std::ranges::binary_search(symbols, unused)) { ++unused; }
	return {symbols, unused};
}

/// Copies of `lhs` and `rhs` with `DONT_CARE` spelled out as their symbols plus one unused symbol, which
/// stands for all the others (Mata uses only the used symbols, so "any symbol" equals "a").
std::pair<Nft, Nft> with_dont_care_spelled_out(const Nft& lhs, const Nft& rhs) {
	auto copies = std::pair(Nft(lhs), Nft(rhs));
	auto [replacements, unused] = used_and_unused_symbols(lhs, rhs);
	replacements.insert(std::ranges::lower_bound(replacements, unused), unused);
	const auto spelled = mata::utils::OrdVector<Symbol>(replacements);
	for (Nft* copy : {&copies.first, &copies.second}) {
		copy->levels.resize(copy->num_of_states());
		copy->unwind_jumps_inplace(spelled, mata::nft::JumpMode::RepeatSymbol);
	}
	return copies;
}

bool uses_dont_care(const Nft& nft) {
	return std::ranges::any_of(nft.delta, [](const mata::nfa::StatePost& state_post) {
		return state_post.find(mata::nft::DONT_CARE) != state_post.end();
	});
}

/// Throws unless the raw `%LevelsNum` and `%Levels` entries are numbers within `MAX_LEVELS` (Mata narrows them).
void check_level_entries(const mata::parser::ParsedSection& section) {
	const auto check_number = [](const std::string& text, const std::size_t bound, const std::string& what) {
		if (text.empty() || !std::ranges::all_of(text, [](const char c) { return c >= '0' && c <= '9'; })) {
			throw std::runtime_error(what + " '" + text + "' is not a number");
		}
		// More than 10 digits is more than any bound here, and too long for stoull.
		if (text.size() > 10 || std::stoull(text) > bound) {
			throw std::runtime_error(
				what + " " + text + " is more than the " + std::to_string(MAX_LEVELS) + " supported levels allow"
			);
		}
	};
	if (const auto it = section.dict.find("LevelsNum"); it != section.dict.end()) {
		for (const auto& entry : it->second) { check_number(entry, MAX_LEVELS, "%LevelsNum"); }
	}
	if (const auto it = section.dict.find("Levels"); it != section.dict.end()) {
		for (const auto& entry : it->second) {
			const auto colon = entry.rfind(':');
			if (colon == std::string::npos) { continue; } // Mata reports the format error.
			check_number(entry.substr(colon + 1), MAX_LEVELS - 1, "%Levels " + entry + ": level");
		}
	}
}

/// Reads every section of `text` with Mata's fast reader of explicit transducers. @return The transducers, or
/// `std::nullopt` when a section needs the general reader or has levels beyond `MAX_LEVELS`; `alphabet` may then
/// have learned names.
std::optional<std::vector<Nft>> read_explicit_nfts(const std::string_view text, mata::Alphabet& alphabet) {
	const auto sections = detail::split_sections(text);
	auto transducers = std::vector<Nft>();
	transducers.reserve(sections.size());
	for (const auto& section : sections) {
		auto nft = std::optional<Nft>();
		try {
			nft = mata::nft::builder::parse_explicit_from_mata(section.text, &alphabet);
		} catch (const std::exception&) {
			return std::nullopt; // A name the alphabet rejects; the general reader reports it.
		}
		if (!nft.has_value() || nft->levels.num_of_levels > MAX_LEVELS ||
			std::ranges::any_of(nft->levels, [](const Level level) { return level >= MAX_LEVELS; })) {
			return std::nullopt;
		}
		transducers.push_back(std::move(*nft));
	}
	return transducers;
}

/// Calls `parse`, prefixing its errors with `section N: ` for the section at `index`.
template <class Parse> auto in_section(const std::size_t index, Parse&& parse) {
	try {
		return parse();
	} catch (const std::runtime_error& error) {
		throw std::runtime_error("section " + std::to_string(index + 1) + ": " + error.what());
	}
}

void adopt_parsed(Nft& nft) {
	nft.alphabets = nullptr;
	nft_check_well_formed(nft);
}

/// Reads the sections of `text` with Mata's general reader, from the raw sections, as Mata's parse_from_mata does:
/// IntermediateAut ignores the levels.
std::vector<Nft> construct_nfts(const std::string_view text, mata::Alphabet& alphabet) {
	const auto sections = mata::parser::parse_mf(std::string(text));
	auto transducers = std::vector<Nft>();
	transducers.reserve(sections.size());
	for (std::size_t i = 0; i < sections.size(); ++i) {
		const auto& type = sections[i].type;
		if (type != mata::nft::TYPE_NFT && type != mata::nft::TYPE_NFT + "-explicit") {
			throw std::runtime_error(
				"section " + std::to_string(i + 1) + " is '@" + type + "', not an explicit NFT ('@NFT-explicit')"
			);
		}
		transducers.push_back(in_section(i, [&]() {
			check_level_entries(sections[i]);
			auto nft = mata::nft::builder::construct(sections[i], &alphabet);
			adopt_parsed(nft);
			return nft;
		}));
	}
	return transducers;
}

} // namespace

std::unique_ptr<Nft> ParsedNfts::take(const std::size_t index) {
	if (index >= transducers_.size()) { throw std::out_of_range("parsed transducer index out of range"); }
	return boxed(std::move(transducers_[index]));
}

// Construction.

std::size_t nft_max_levels() { return MAX_LEVELS; }

std::unique_ptr<Nft> nft_new(const std::size_t num_levels) {
	check_num_levels(num_levels);
	return boxed(Nft::with_levels(num_levels));
}

std::unique_ptr<Nft> nft_clone(const Nft& nft) { return boxed(Nft(nft)); }

std::unique_ptr<Nft> nft_identity(const mata::nfa::Nfa& nfa, const std::size_t num_levels) {
	check_num_levels(num_levels);
	return boxed(mata::nft::builder::from_nfa_with_levels_zero(nfa, num_levels, /*explicit_transitions=*/true));
}

std::unique_ptr<Nft> nft_empty_word(const std::size_t num_levels) {
	check_num_levels(num_levels);
	return boxed(mata::nft::builder::create_empty_string_nft(num_levels));
}

void nft_check_well_formed(const Nft& nft) {
	detail::check_delta(nft);
	const auto num_levels = nft.levels.num_of_levels;
	try {
		check_num_levels(num_levels);
	} catch (const std::invalid_argument& error) { throw std::runtime_error(error.what()); }
	if (nft.levels.size() < nft.num_of_states()) {
		throw std::runtime_error(
			"the transducer has " + std::to_string(nft.num_of_states()) + " states but levels for " +
			std::to_string(nft.levels.size())
		);
	}
	for (State state = 0; state < nft.num_of_states(); ++state) {
		if (nft.levels[state] >= num_levels) {
			throw std::runtime_error(
				describe_state(state) + " has level " + std::to_string(nft.levels[state]) + ", but there are only " +
				std::to_string(num_levels) + " levels"
			);
		}
	}
	for (const auto* states : {&nft.initial, &nft.final}) {
		for (const State state : *states) {
			// `levels` has room for `num_of_states()` states, but the sparse sets can hold any state of
			// their domain, which other C++ code may have grown past it.
			if (state >= nft.levels.size()) {
				throw std::runtime_error(
					"the transducer has levels for " + std::to_string(nft.levels.size()) + " states, but " +
					describe_state(state) + " is initial or final"
				);
			}
			if (nft.levels[state] != 0) {
				throw std::runtime_error(
					"initial and final states must have level 0, but " + describe_state(state) + " has level " +
					std::to_string(nft.levels[state])
				);
			}
		}
	}
	for (const auto& transition : nft.delta.transitions()) {
		const Level source_level = nft.levels[transition.source];
		const Level target_level = nft.levels[transition.target];
		if (target_level != 0 && target_level <= source_level) {
			throw std::runtime_error(
				"the transition from " + describe_state(transition.source) + " (level " + std::to_string(source_level) +
				") to " + describe_state(transition.target) + " (level " + std::to_string(target_level) +
				") does not move to a later level or back to level 0"
			);
		}
	}
}

void nft_adopt(Nft& nft) {
	nft.alphabets = nullptr;
	nft_check_well_formed(nft);
	unwind_jumps(nft);
}

// Parsing.

std::unique_ptr<ParsedNfts> nft_parse_mata(
	const rust::Str text,
	const bool numeric_symbols,
	const rust::Slice<const rust::String> names,
	const rust::Slice<const Symbol> named_symbols,
	const Symbol next_symbol,
	rust::Vec<rust::String>& new_names,
	rust::Vec<Symbol>& new_symbols
) {
	auto alphabet =
		std::optional<detail::ParseAlphabet>(std::in_place, numeric_symbols, names, named_symbols, next_symbol);
	const auto input = std::string_view(text.data(), text.size());
	auto transducers = read_explicit_nfts(input, *alphabet->get());
	if (transducers.has_value()) {
		// Accepted sections pass the checks of the general reader, except these.
		for (std::size_t i = 0; i < transducers->size(); ++i) {
			in_section(i, [&]() { adopt_parsed((*transducers)[i]); });
		}
	} else {
		// From a fresh alphabet, so that the general reader numbers the names and reports the errors as always.
		alphabet.emplace(numeric_symbols, names, named_symbols, next_symbol);
		transducers = construct_nfts(input, *alphabet->get());
	}
	alphabet->report_new_names(new_names, new_symbols);
	return std::make_unique<ParsedNfts>(std::move(*transducers));
}

std::size_t nft_parsed_len(const ParsedNfts& parsed) { return parsed.size(); }

std::unique_ptr<Nft> nft_parsed_take(ParsedNfts& parsed, const std::size_t index) { return parsed.take(index); }

// Structure.

const mata::Automaton& nft_automaton(const Nft& nft) { return nft; }

std::size_t nft_num_levels(const Nft& nft) { return nft.levels.num_of_levels; }

std::size_t nft_level(const Nft& nft, const State state) {
	check_state(nft, state);
	return nft.levels[state];
}

State nft_add_state(Nft& nft, const std::size_t level) { return nft.add_state_with_level(check_level(nft, level)); }

void nft_add_initial(Nft& nft, const State state) {
	check_level_zero_state(nft, state);
	nft.initial.insert(state);
}

bool nft_remove_initial(Nft& nft, const State state) {
	if (!nft.initial.contains(state)) { return false; }
	nft.initial.erase(state);
	return true;
}

void nft_add_final(Nft& nft, const State state) {
	check_level_zero_state(nft, state);
	nft.final.insert(state);
}

bool nft_remove_final(Nft& nft, const State state) {
	if (!nft.final.contains(state)) { return false; }
	nft.final.erase(state);
	return true;
}

void nft_add_transition(Nft& nft, const State source, const rust::Slice<const Symbol> symbols, const State target) {
	check_level_zero_state(nft, source);
	check_level_zero_state(nft, target);
	if (symbols.size() != nft.levels.num_of_levels) {
		throw std::invalid_argument(
			"expected one symbol per level (" + std::to_string(nft.levels.num_of_levels) + "), got " +
			std::to_string(symbols.size())
		);
	}
	nft.add_transition_by_levels(source, to_word(symbols), target);
}

void nft_insert_words(
	Nft& nft,
	const State source,
	const rust::Slice<const Symbol> symbols,
	const rust::Slice<const std::size_t> lengths,
	const State target
) {
	check_level_zero_state(nft, source);
	check_level_zero_state(nft, target);
	const auto words = split_words(nft, symbols, lengths);
	if (std::ranges::all_of(words, [](const Word& word) { return word.empty(); })) {
		throw std::invalid_argument("at least one word must be nonempty");
	}
	nft.insert_word_by_levels(source, words, target);
}

bool nft_accepts(
	const Nft& nft, const rust::Slice<const Symbol> symbols, const rust::Slice<const std::size_t> lengths
) {
	return nft.is_in_lang_by_levels(split_words(nft, symbols, lengths));
}

// Operations.

void nft_trim(Nft& nft) { nft.trim(); }

std::unique_ptr<Nft> nft_union(const Nft& lhs, const Nft& rhs) {
	check_same_levels(lhs, rhs);
	return boxed(mata::nft::union_nondet(lhs, rhs));
}

std::unique_ptr<Nft> nft_intersection(const Nft& lhs, const Nft& rhs) {
	check_same_levels(lhs, rhs);
	if (!uses_dont_care(lhs) && !uses_dont_care(rhs)) {
		auto lhs_storage = std::optional<Nft>();
		auto rhs_storage = std::optional<Nft>();
		return boxed(mata::nft::intersection(without_jumps(lhs, lhs_storage), without_jumps(rhs, rhs_storage)));
	}
	// The product pairs DONT_CARE with EPSILON, but DONT_CARE reads one symbol: spell it out, then turn the
	//  symbol that stands for all the others back into DONT_CARE.
	const Symbol other = used_and_unused_symbols(lhs, rhs).second;
	const auto [spelled_lhs, spelled_rhs] = with_dont_care_spelled_out(lhs, rhs);
	auto result = mata::nft::intersection(spelled_lhs, spelled_rhs);
	auto other_edges = std::vector<mata::nfa::Transition>();
	for (const auto& transition : result.delta.transitions()) {
		if (transition.symbol == other) { other_edges.push_back(transition); }
	}
	for (const auto& transition : other_edges) {
		result.delta.remove(transition);
		result.delta.add(transition.source, mata::nft::DONT_CARE, transition.target);
	}
	return boxed(std::move(result));
}

std::unique_ptr<Nft> nft_concatenate(const Nft& lhs, const Nft& rhs) {
	check_same_levels(lhs, rhs);
	return boxed(mata::nft::concatenate(lhs, rhs));
}

std::unique_ptr<Nft> nft_compose(
	const Nft& lhs,
	const Nft& rhs,
	const std::size_t lhs_sync_level,
	const std::size_t rhs_sync_level,
	const bool project_out_sync_level
) {
	const Level lhs_level = check_level(lhs, lhs_sync_level);
	const Level rhs_level = check_level(rhs, rhs_sync_level);
	const std::size_t removed = project_out_sync_level ? 2 : 1;
	if (lhs.levels.num_of_levels + rhs.levels.num_of_levels <= removed) {
		throw std::invalid_argument("the composition would have no levels left");
	}
	check_num_levels(lhs.levels.num_of_levels + rhs.levels.num_of_levels - removed);
	return boxed(compose_one_level(lhs, rhs, lhs_level, rhs_level, project_out_sync_level));
}

std::unique_ptr<Nft>
	nft_apply(const Nft& nft, const mata::nfa::Nfa& nfa, const std::size_t level, const bool project_out) {
	const Level applied_level = check_level(nft, level);
	if (project_out && nft.levels.num_of_levels < 2) {
		throw std::invalid_argument("projecting out the only level would leave no levels");
	}
	// Compose the language, as a one-level transducer, with `level` of `nft`.
	const auto language = Nft(mata::nfa::remove_epsilon(nfa), 1);
	return boxed(compose_one_level(language, nft, 0, applied_level, project_out));
}

std::unique_ptr<Nft> nft_project_out(const Nft& nft, const rust::Slice<const std::size_t> levels) {
	const auto removed = to_level_set(nft, levels);
	if (removed.size() >= nft.levels.num_of_levels) { throw std::invalid_argument("at least one level must remain"); }
	if (removed.empty()) { return boxed(Nft(nft)); }
	return boxed(mata::nft::project_out(nft, removed));
}

std::unique_ptr<Nft> nft_project_to(const Nft& nft, const rust::Slice<const std::size_t> levels) {
	const auto kept = to_level_set(nft, levels);
	if (kept.empty()) { throw std::invalid_argument("at least one level must remain"); }
	return boxed(mata::nft::project_to(nft, kept));
}

std::unique_ptr<Nft> nft_invert(const Nft& nft) { return boxed(mata::nft::invert_levels(nft)); }

std::unique_ptr<Nft> nft_determinize(const Nft& nft) {
	auto storage = std::optional<Nft>();
	auto result = mata::nft::determinize(without_jumps(nft, storage));
	// Keep the initial state of the empty set, which Mata drops.
	if (result.initial.empty()) { result.initial.insert(result.add_state_with_level(0)); }
	return boxed(std::move(result));
}

std::unique_ptr<Nft> nft_remove_epsilon(const Nft& nft) {
	auto storage = std::optional<Nft>();
	auto result = mata::nft::remove_epsilon(without_jumps(nft, storage));
	result.trim();
	return boxed(std::move(result));
}

std::unique_ptr<Nft> nft_reverse(const Nft& nft) {
	// revert() also reverses the order of the levels; invert_levels() restores it.
	auto storage = std::optional<Nft>();
	return boxed(mata::nft::invert_levels(mata::nft::revert(without_jumps(nft, storage))));
}

std::unique_ptr<mata::nfa::Nfa>
	nft_to_nfa(const Nft& nft, const bool expand_dont_care, const rust::Slice<const Symbol> dont_care_replacements) {
	auto replacements = detail::to_symbol_set(dont_care_replacements);
	const bool drop_dont_care = expand_dont_care && replacements.empty();
	if (!expand_dont_care || drop_dont_care) { replacements = {mata::nft::DONT_CARE}; }
	auto nfa = nft.to_nfa_update_copy(replacements);
	nfa.alphabet = nullptr;
	if (drop_dont_care) {
		// No symbol can stand for DONT_CARE, so its edges read nothing at all.
		auto dont_care_edges = std::vector<mata::nfa::Transition>();
		for (const auto& transition : nfa.delta.transitions()) {
			if (transition.symbol == mata::nft::DONT_CARE) { dont_care_edges.push_back(transition); }
		}
		for (const auto& transition : dont_care_edges) { nfa.delta.remove(transition); }
	}
	return std::make_unique<mata::nfa::Nfa>(std::move(nfa));
}

// Decision procedures.

bool nft_is_included(const Nft& smaller, const Nft& bigger) {
	check_same_levels(smaller, bigger);
	mata::nfa::Run* const no_counterexample = nullptr;
	const mata::Alphabet* const no_alphabet = nullptr;
	if (uses_dont_care(smaller) || uses_dont_care(bigger)) {
		const auto [spelled_smaller, spelled_bigger] = with_dont_care_spelled_out(smaller, bigger);
		return mata::nft::is_included(spelled_smaller, spelled_bigger, no_counterexample, no_alphabet);
	}
	auto smaller_storage = std::optional<Nft>();
	auto bigger_storage = std::optional<Nft>();
	return mata::nft::is_included(
		without_jumps(smaller, smaller_storage), without_jumps(bigger, bigger_storage), no_counterexample, no_alphabet
	);
}

bool nft_are_equivalent(const Nft& lhs, const Nft& rhs) {
	check_same_levels(lhs, rhs);
	if (uses_dont_care(lhs) || uses_dont_care(rhs)) {
		const auto [spelled_lhs, spelled_rhs] = with_dont_care_spelled_out(lhs, rhs);
		return mata::nft::are_equivalent(spelled_lhs, spelled_rhs);
	}
	auto lhs_storage = std::optional<Nft>();
	auto rhs_storage = std::optional<Nft>();
	return mata::nft::are_equivalent(without_jumps(lhs, lhs_storage), without_jumps(rhs, rhs_storage));
}

bool nft_is_deterministic(const Nft& nft) {
	auto storage = std::optional<Nft>();
	return without_jumps(nft, storage).is_deterministic();
}

// Strings.

namespace {

using mata::applications::strings::replace::ReplaceMode;

ReplaceMode replace_mode(const bool single) { return single ? ReplaceMode::Single : ReplaceMode::All; }

/// The alphabet of a replace transducer. Throws on epsilon, `DONT_CARE` and `marker`, which the construction
///  gives another meaning, and on a `marker` that is epsilon or `DONT_CARE`, which cannot mark anything.
mata::EnumAlphabet
	replace_alphabet(const rust::Slice<const Symbol> symbols, const std::optional<Symbol> marker = std::nullopt) {
	if (marker.has_value() && (*marker == mata::nft::EPSILON || *marker == mata::nft::DONT_CARE)) {
		throw std::invalid_argument(
			"the marker of the replace cannot be epsilon or DONT_CARE, found " + std::to_string(*marker)
		);
	}
	if (marker.has_value() && std::ranges::find(symbols, *marker) != symbols.end()) {
		throw std::invalid_argument(
			"the alphabet cannot contain the marker symbol " + std::to_string(*marker) + " of the replace"
		);
	}
	return mata::EnumAlphabet(detail::to_symbol_set(symbols));
}

/// Wraps a transducer that a library construction built, checked as one from other C++ code.
std::unique_ptr<Nft> built(Nft&& nft) {
	nft_check_well_formed(nft);
	return boxed(std::move(nft));
}

} // namespace

Symbol strings_begin_marker() { return mata::applications::strings::replace::BEGIN_MARKER; }

Symbol strings_end_marker() { return mata::applications::strings::replace::END_MARKER; }

bool strings_words_of_lengths(
	const Nft& nft,
	const rust::Slice<const std::uint32_t> lengths,
	rust::Vec<Symbol>& symbols,
	rust::Vec<std::size_t>& word_lengths
) {
	auto storage = std::optional<Nft>();
	const auto words = mata::applications::strings::get_words_of_lengths(
		without_jumps(nft, storage), std::vector<unsigned>(lengths.begin(), lengths.end())
	);
	if (!words.has_value()) { return false; }
	detail::assign_words(symbols, word_lengths, *words);
	return true;
}

std::unique_ptr<Nft> strings_create_identity(const rust::Slice<const Symbol> alphabet, const std::size_t num_levels) {
	check_num_levels(num_levels);
	const auto symbols = replace_alphabet(alphabet);
	return built(mata::applications::strings::replace::create_identity(&symbols, num_levels));
}

// The overloads taking a `Symbol` replacement build `Word{replacement}`, so the bridge has only the `Word` ones.
std::unique_ptr<Nft> strings_identity_with_replace(
	const rust::Slice<const Symbol> alphabet,
	const Symbol from_symbol,
	const rust::Slice<const Symbol> replacement,
	const bool single
) {
	const auto symbols = replace_alphabet(alphabet);
	return built(
		mata::applications::strings::replace::create_identity_with_single_symbol_replace(
			&symbols, from_symbol, to_word(replacement), replace_mode(single)
		)
	);
}

std::unique_ptr<Nft> strings_replace_regex(
	const rust::Str regex,
	const rust::Slice<const Symbol> replacement,
	const rust::Slice<const Symbol> alphabet,
	const bool single,
	const Symbol begin_marker
) {
	const auto symbols = replace_alphabet(alphabet, begin_marker);
	return built(
		mata::applications::strings::replace::replace_reluctant_regex(
			std::string(regex), to_word(replacement), &symbols, replace_mode(single), begin_marker
		)
	);
}

std::unique_ptr<Nft> strings_replace_regex_nfa(
	const mata::nfa::Nfa& regex,
	const rust::Slice<const Symbol> replacement,
	const rust::Slice<const Symbol> alphabet,
	const bool single,
	const Symbol begin_marker
) {
	const auto symbols = replace_alphabet(alphabet, begin_marker);
	// The library needs a deterministic automaton, as the String form builds it.
	auto dfa = mata::nfa::remove_epsilon(regex);
	if (!dfa.is_deterministic()) { dfa = mata::nfa::determinize(dfa); }
	return built(
		mata::applications::strings::replace::replace_reluctant_regex(
			std::move(dfa), to_word(replacement), &symbols, replace_mode(single), begin_marker
		)
	);
}

std::unique_ptr<Nft> strings_replace_literal(
	const rust::Slice<const Symbol> literal,
	const rust::Slice<const Symbol> replacement,
	const rust::Slice<const Symbol> alphabet,
	const bool single,
	const Symbol end_marker
) {
	const auto symbols = replace_alphabet(alphabet, end_marker);
	return built(
		mata::applications::strings::replace::replace_reluctant_literal(
			to_word(literal), to_word(replacement), &symbols, replace_mode(single), end_marker
		)
	);
}

std::unique_ptr<Nft> strings_replace_symbol(
	const Symbol from_symbol,
	const rust::Slice<const Symbol> replacement,
	const rust::Slice<const Symbol> alphabet,
	const bool single
) {
	const auto symbols = replace_alphabet(alphabet);
	return built(
		mata::applications::strings::replace::replace_reluctant_single_symbol(
			from_symbol, to_word(replacement), &symbols, replace_mode(single)
		)
	);
}

// Printing.

rust::String nft_print_mata(
	const Nft& nft, const rust::Slice<const rust::String> names, const rust::Slice<const Symbol> symbols
) {
	if (names.empty()) { return rust::String(nft.print_to_mata()); }
	// The printer reads symbol names from the transducer's own alphabets, so print a copy that has them.
	auto named = Nft(nft);
	named.alphabets = std::make_shared<mata::AlphabetLevels>(
		std::shared_ptr<mata::Alphabet>(std::make_shared<detail::NameTable>(names, symbols))
	);
	return rust::String(named.print_to_mata());
}

rust::String nft_print_dot(
	const Nft& nft,
	const bool ascii_labels,
	const bool intervals,
	const std::int32_t max_label_length,
	const rust::Slice<const rust::String> names,
	const rust::Slice<const Symbol> symbols
) {
	const auto table = detail::NameTable(names, symbols);
	const mata::Alphabet* const alphabet = table.empty() ? nullptr : &table;
	return rust::String(nft.print_to_dot(ascii_labels, intervals, max_label_length, alphabet));
}

// The other operations of the script language's `nft:` namespace.

namespace {

bool has_epsilon_edge(const Nft& nft) {
	return std::ranges::any_of(nft.delta, [](const mata::nfa::StatePost& state_post) {
		return state_post.find(mata::nft::EPSILON) != state_post.end();
	});
}

} // namespace

std::unique_ptr<Nft> nft_complement(const Nft& nft, const rust::Slice<const Symbol> symbols) {
	const auto alphabet = detail::to_symbol_set(symbols);
	// Mata documents the precondition but does not check it.
	if (has_epsilon_edge(nft)) {
		throw std::invalid_argument("the complement needs a transducer without epsilon transitions");
	}
	// Mata completes only from the initial states, so without one the complement would be the empty
	// word alone: seed a non-final initial state.
	const bool seed = nft.initial.empty();
	if (!seed && !uses_dont_care(nft)) { return boxed(mata::nft::complement(nft, alphabet)); }
	auto prepared = Nft(nft);
	prepared.levels.resize(prepared.num_of_states());
	if (seed) { prepared.initial.insert(prepared.add_state_with_level(0)); }
	// Mata's determinization reads DONT_CARE as one more symbol: spell it out over the alphabet first.
	if (uses_dont_care(prepared)) { prepared.unwind_jumps_inplace(alphabet, mata::nft::JumpMode::RepeatSymbol); }
	return boxed(mata::nft::complement(prepared, alphabet));
}

bool nft_is_complete(const Nft& nft, const rust::Slice<const Symbol> symbols) {
	return nft.is_complete(detail::to_symbol_set(symbols));
}

std::unique_ptr<Nft> nft_concatenate_nth_power(const Nft& nft, const std::uint32_t power) {
	return boxed(mata::nft::concatenate_nth_power(Nft(nft), power));
}

bool nft_contains_jump_transitions(const Nft& nft) { return nft.contains_jump_transitions(); }

std::unique_ptr<Nft> nft_sigma_star(const std::size_t num_levels) {
	check_num_levels(num_levels);
	return boxed(mata::nft::builder::create_sigma_star_nft(num_levels));
}

std::unique_ptr<Nft> nft_from_nfa_with_levels_advancing(const mata::nfa::Nfa& nfa, const std::size_t num_levels) {
	check_num_levels(num_levels);
	auto nft = mata::nft::builder::from_nfa_with_levels_advancing(nfa, num_levels);
	nft_adopt(nft);
	return std::make_unique<Nft>(std::move(nft));
}

bool nft_has_epsilon_cycle(const Nft& nft) { return mata::nft::has_epsilon_cycle(nft); }

// Mata's is_identical() ignores the number of states.
bool nft_is_identical(const Nft& lhs, const Nft& rhs) {
	return lhs.num_of_states() == rhs.num_of_states() && lhs.is_identical(rhs);
}

bool nft_is_in_lang(
	const Nft& nft, const rust::Slice<const Symbol> word, const bool match_prefix, const bool has_epsilon_cycles
) {
	// Mata only asserts this: told there is no epsilon cycle, its search would follow one forever.
	if (!has_epsilon_cycles && mata::nft::has_epsilon_cycle(nft)) {
		throw std::invalid_argument("the transducer has a cycle of epsilon transitions");
	}
	return nft.is_in_lang(to_word(word), match_prefix, mata::nft::JumpMode::RepeatSymbol, has_epsilon_cycles);
}

std::unique_ptr<Nft> nft_insert_level(const Nft& nft, const std::size_t level) {
	// A level past the last one appends every level up to it.
	check_num_levels(std::max(nft.levels.num_of_levels, level) + 1);
	return boxed(mata::nft::insert_level(nft, static_cast<Level>(level)));
}

std::unique_ptr<Nft> nft_insert_levels(const Nft& nft, const rust::Slice<const bool> mask) {
	const auto kept = static_cast<std::size_t>(std::count(mask.begin(), mask.end(), false));
	// Mata asserts these.
	if (kept != nft.levels.num_of_levels) {
		throw std::invalid_argument(
			"the mask keeps " + std::to_string(kept) + " levels, but the transducer has " +
			std::to_string(nft.levels.num_of_levels)
		);
	}
	check_num_levels(mask.size());
	auto new_levels_mask = mata::BoolVector(mask.size(), false);
	std::copy(mask.begin(), mask.end(), new_levels_mask.begin());
	return boxed(mata::nft::insert_levels(nft, new_levels_mask));
}

std::unique_ptr<Nft> nft_reduce(const Nft& nft) { return boxed(mata::nft::reduce(nft)); }

std::unique_ptr<Nft> nft_revert(const Nft& nft) { return boxed(mata::nft::revert(nft)); }

bool nft_is_universal(const Nft& nft, const rust::Slice<const Symbol> symbols) {
	const auto alphabet = mata::EnumAlphabet(detail::to_symbol_set(symbols));
	// Mata's check takes no epsilon closure, so remove the epsilon transitions first, as for NFAs.
	if (!has_epsilon_edge(nft)) { return nft.is_universal(alphabet); }
	return mata::nft::remove_epsilon(nft).is_universal(alphabet);
}

std::unique_ptr<Nft> nft_unwind_jumps(const Nft& nft) { return boxed(nft.unwind_jumps()); }

// Intermediate automata.

std::unique_ptr<Nft> intermediate_construct_nft(
	const mata::IntermediateAut& automaton,
	const rust::Slice<const rust::String> names,
	const rust::Slice<const Symbol> named_symbols,
	const Symbol next_symbol,
	rust::Vec<rust::String>& new_names,
	rust::Vec<Symbol>& new_symbols
) {
	if (!automaton.is_nft()) { throw std::runtime_error("the intermediate automaton is not an NFT"); }
	if (automaton.is_bitvector()) {
		throw std::runtime_error("the intermediate automaton has a bit-vector alphabet, which transducers cannot have");
	}
	auto alphabet = detail::ParseAlphabet(false, names, named_symbols, next_symbol);
	auto nft = mata::nft::builder::construct(automaton, alphabet.get());
	nft.alphabets = nullptr;
	nft_check_well_formed(nft);
	alphabet.report_new_names(new_names, new_symbols);
	return boxed(std::move(nft));
}

} // namespace mata_sys
