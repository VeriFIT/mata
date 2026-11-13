/** @file
 * @brief Functions to build predefined types of NFTs, to create them from regular expressions, and to load them from
 *  files.
 */

#include "mata/nft/builder.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/sparse-set.hh"
#include "mata/utils/utils.hh"

#include "../explicit-section-reader.hh"

#include <charconv>
#include <fstream>
#include <limits>

using namespace mata::nft;
using mata::Symbol;
using mata::nft::Nft;

namespace {
/// Maps the printed names of EPSILON and DONT_CARE back to their symbols; every other name goes to the alphabet, which
/// never sees the special names.
Symbol translate_symbol_name(mata::Alphabet* alphabet, const std::string_view name) {
	if (name == mata::EPSILON_NAME) { return EPSILON; }
	if (name == mata::DONT_CARE_NAME) { return DONT_CARE; }
	return alphabet->translate_symb(name);
}

/// The per-level variant of @c translate_symbol_name.
Symbol translate_symbol_name(
	const std::shared_ptr<mata::AlphabetLevels>& alphabets, const std::string_view name, const Level level
) {
	if (name == mata::EPSILON_NAME) { return EPSILON; }
	if (name == mata::DONT_CARE_NAME) { return DONT_CARE; }
	return alphabets->translate_symb(name, level);
}

/// Parses a level as @c std::stol() would, but only a plain decimal number that fits @c Level, without leading zeros:
///  a caller can bound the digits of the levels the general reader reads by bounding the values read here.
/// @return The level, or @c std::nullopt.
std::optional<Level> parse_plain_level(const std::string_view level) {
	Level value{};
	const char* const end{level.data() + level.size()};
	if (level.empty() || level.front() < '0' || level.front() > '9' || (level.front() == '0' && level.size() > 1)) {
		return std::nullopt;
	}
	if (const auto [ptr, error]{std::from_chars(level.data(), end, value)}; error != std::errc{} || ptr != end) {
		return std::nullopt;
	}
	return value;
}

/**
 * Reads an NFT from @p input with @c mata::parser::ExplicitSectionReader, producing the NFT that
 *  @c builder::construct() produces from @c parser::parse_mf() of @p input, with the states numbered the same.
 * @return The NFT, or @c std::nullopt when @p input leaves the subset of the format this reader covers.
 */
std::optional<Nft>
	read_explicit_nft(const std::string_view input, mata::Alphabet* alphabet, mata::parser::StateNameMap& state_names) {
	using mata::parser::ExplicitSectionReader;
	using mata::parser::ExplicitToken;

	ExplicitSectionReader reader{input};
	if (const std::optional<std::string_view> type{reader.read_type()};
		!type.has_value() || (*type != TYPE_NFT && *type != TYPE_NFT + "-explicit")) {
		return std::nullopt;
	}

	Nft aut;
	const auto get_state{[&](const std::string_view name) {
		if (const std::optional<State> state{state_names.find(name)}) { return *state; }
		const State state{aut.add_state()};
		state_names.insert(name, state);
		return state;
	}};

	// builder::construct() reads the keys before the body, in this order, whatever their order in the file.
	std::vector<std::string> initial_names;
	std::vector<std::string> final_names;
	std::vector<std::string> levels;
	// Present even without values; the key alone is malformed.
	std::optional<std::vector<std::string>> num_of_levels;
	const auto start_body{[&]() {
		for (const std::string& name : initial_names) { aut.initial.insert(get_state(name)); }
		for (const std::string& name : final_names) { aut.final.insert(get_state(name)); }
		// A state listed twice keeps its last level. Left to the general reader, whose entries callers can check one
		//  by one, as the Rust binding does.
		std::vector<bool> has_level;
		for (const std::string_view state_level : levels) {
			const size_t colon{state_level.find(':')};
			if (colon == std::string_view::npos) { return false; }
			const State state{get_state(state_level.substr(0, colon))};
			const std::optional<Level> level{parse_plain_level(state_level.substr(colon + 1))};
			if (!level.has_value()) { return false; }
			if (state >= has_level.size()) { has_level.resize(state + 1); }
			if (has_level[state]) { return false; }
			has_level[state] = true;
			aut.levels[state] = *level;
		}
		if (num_of_levels.has_value()) {
			if (num_of_levels->size() != 1) { return false; }
			const std::optional<Level> level{parse_plain_level(num_of_levels->front())};
			if (!level.has_value()) { return false; }
			aut.levels.num_of_levels = *level;
		}
		return true;
	}};

	std::vector<Transition> transitions;
	bool in_body{false};
	while (true) {
		switch (reader.read_line()) {
			case ExplicitSectionReader::Line::Key: {
				std::vector<std::string>* values{nullptr};
				if (const std::string_view key{reader.key()}; key == "Initial") {
					values = &initial_names;
				} else if (key == "Final") {
					values = &final_names;
				} else if (key == "Levels") {
					values = &levels;
				} else if (key == "LevelsNum") {
					if (!num_of_levels.has_value()) { num_of_levels.emplace(); }
					values = &*num_of_levels;
				} else { // builder::construct() ignores any other key.
					break;
				}
				// Would be numbered before the body.
				if (in_body) { return std::nullopt; }
				for (const ExplicitToken& token : reader.tokens()) { values->emplace_back(token.text); }
				break;
			}
			case ExplicitSectionReader::Line::Body: {
				if (!in_body && !start_body()) { return std::nullopt; }
				in_body = true;
				const std::vector<ExplicitToken>& tokens{reader.tokens()};
				if (tokens.size() != 3) { return std::nullopt; }
				const State source{get_state(tokens[0].text)};
				const Symbol symbol{translate_symbol_name(alphabet, tokens[1].text)};
				const State target{get_state(tokens[2].text)};
				transitions.emplace_back(source, symbol, target);
				break;
			}
			case ExplicitSectionReader::Line::End: {
				if (!in_body && !start_body()) { return std::nullopt; }
				aut.delta.add(std::move(transitions));
				return aut;
			}
			case ExplicitSectionReader::Line::Unsupported:
				return std::nullopt;
		}
	}
}
} // namespace

Nft builder::construct(const mata::parser::ParsedSection& parsec, mata::Alphabet* alphabet, NameStateMap* state_map) {
	Nft aut;
	MATA_ASSERT(nullptr != alphabet);

	// HACK - it should be only "parsec.type != TYPE_NFA" without the conjunction
	if (parsec.type != TYPE_NFT && parsec.type != TYPE_NFT + "-explicit") {
		throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" + TYPE_NFT + "\"");
	}

	NameStateMap tmp_state_map;
	if (nullptr == state_map) { state_map = &tmp_state_map; }

	// a lambda for translating state names to identifiers
	auto get_state_name = [&](const std::string_view name) {
		if (const auto it{state_map->find(name)}; it != state_map->end()) { return it->second; }
		const State state = aut.add_state();
		state_map->emplace(name, state);
		return state;
	};

	auto it = parsec.dict.find("Initial");
	if (parsec.dict.end() != it) {
		for (const auto& str : it->second) {
			const State state = get_state_name(str);
			aut.initial.insert(state);
		}
	}

	it = parsec.dict.find("Final");
	if (parsec.dict.end() != it) {
		for (const auto& str : it->second) {
			const State state = get_state_name(str);
			aut.final.insert(state);
		}
	}

	it = parsec.dict.find("Levels");
	if (parsec.dict.end() != it) {
		for (const auto& str : it->second) {
			std::stringstream ss(str);
			std::string level_str;
			try {
				std::string state_name;
				std::getline(ss, state_name, ':');
				std::getline(ss, level_str, ':');
				if (!ss.eof()) { throw std::runtime_error("Bad format of levels: too many colons in " + str); }

				const State state = get_state_name(state_name);
				const long level = std::stol(level_str);
				if (level < 0 || static_cast<unsigned long>(level) > std::numeric_limits<Level>::max()) {
					throw std::runtime_error("Bad format of levels: level " + level_str + " is out of range.");
				}
				aut.levels[state] = static_cast<Level>(level);

			} catch (const std::invalid_argument&) {
				throw std::runtime_error("Bad format of levels: unsupported level " + level_str);
			} catch (const std::out_of_range&) {
				throw std::runtime_error("Bad format of levels: level " + level_str + " is out of range.");
			} catch (...) { throw std::runtime_error("Bad format of levels."); }
		}
	}

	it = parsec.dict.find("LevelsNum");
	if (parsec.dict.end() != it) {
		if (it->second.empty()) { throw std::runtime_error("LevelsNum has to be specified."); }
		if (it->second.size() > 1) { throw std::runtime_error("Only one LevelsNum can be specified."); }
		try {
			const long level = std::stol(it->second[0]);
			if (level < 0 || static_cast<unsigned long>(level) > std::numeric_limits<Level>::max()) {
				throw std::runtime_error("Bad format of levels: level " + it->second[0] + " is out of range.");
			}
			aut.levels.num_of_levels = static_cast<Level>(level);
		} catch (const std::invalid_argument&) {
			throw std::runtime_error("Bad format of levels: unsupported level " + it->second[0]);
		} catch (const std::out_of_range&) {
			throw std::runtime_error("Bad format of levels: level " + it->second[0] + " is out of range.");
		}
	}

	for (const auto& body_line : parsec.body) {
		if (body_line.size() != 3) {
			if (body_line.size() == 2) {
				throw std::runtime_error("Epsilon transitions not supported: " + std::to_string(body_line));
			}
			throw std::runtime_error("Invalid transition: " + std::to_string(body_line));
		}

		const State source = get_state_name(body_line[0].token);
		const Symbol symbol = translate_symbol_name(alphabet, body_line[1].token);
		const State target = get_state_name(body_line[2].token);
		aut.delta.add(source, symbol, target);
	}

	return aut;
} // construct().

Nft builder::construct(const mata::IntermediateAut& inter_aut, mata::Alphabet* alphabet, NameStateMap* state_map) {
	Nft aut;
	MATA_ASSERT(nullptr != alphabet);

	if (!inter_aut.is_nft()) {
		throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" + TYPE_NFT + "\"");
	}

	NameStateMap tmp_state_map;
	if (nullptr == state_map) { state_map = &tmp_state_map; }

	// a lambda for translating state names to identifiers
	auto get_state_name = [&state_map, &aut](const std::string_view name) {
		if (const auto it{state_map->find(name)}; it != state_map->end()) { return it->second; }
		const State state = aut.add_state();
		state_map->emplace(name, state);
		return state;
	};

	for (const auto& str : inter_aut.initial_formula.collect_node_names()) {
		const State state = get_state_name(str);
		aut.initial.insert(state);
	}

	for (const auto& [formula_node, formula_graph] : inter_aut.transitions) {
		const FormulaGraphChildren children{formula_graph.children()};
		if (children.size() != 2) {
			if (children.size() == 1) { throw std::runtime_error("Epsilon transitions not supported"); }
			throw std::runtime_error("Invalid transition");
		}

		const State source = get_state_name(formula_node.name());
		const Symbol symbol = translate_symbol_name(alphabet, children[0].node().name());
		const State target = get_state_name(children[1].node().name());

		aut.delta.add(source, symbol, target);
	}

	std::unordered_set<std::string> final_formula_nodes;
	if (!(inter_aut.final_formula.node().is_constant())) {
		// we do not want to parse true/false (constant) as a state so we do not collect it
		final_formula_nodes = inter_aut.final_formula.collect_node_names();
	}

	// for constant true, we will pretend that final nodes are negated with empty final_formula_nodes
	if (inter_aut.final_formula.node().is_true() || inter_aut.are_final_states_conjunction_of_negation()) {
		// Final nodes are negated.
		// We add all states NOT in final_formula_nodes to final states.
		for (const auto& [state_name, state] : *state_map) {
			if (!final_formula_nodes.contains(state_name)) { aut.final.insert(state); }
		}
	} else {
		// we add all states in final_formula_nodes to final states
		for (const auto& str : final_formula_nodes) { aut.final.insert(get_state_name(str)); }
	}

	return aut;
} // construct().

void builder::construct(
	mata::nft::Nft* result,
	const mata::IntermediateAut& inter_aut,
	mata::Alphabet* alphabet,
	mata::nft::builder::NameStateMap* state_map
) {
	*result = construct(inter_aut, alphabet, state_map);
}

Nft builder::construct(
	const mata::IntermediateAut& inter_aut,
	const std::shared_ptr<mata::AlphabetLevels>& alphabets,
	NameStateMap* state_map
) {
	MATA_ASSERT(nullptr != alphabets);

	if (!inter_aut.is_nft()) {
		throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" + TYPE_NFT + "\"");
	}

	Nft aut;
	aut.alphabets = alphabets;

	NameStateMap tmp_state_map;
	if (nullptr == state_map) { state_map = &tmp_state_map; }

	auto get_state_name = [&state_map, &aut](const std::string_view name) {
		if (const auto it{state_map->find(name)}; it != state_map->end()) { return it->second; }
		const State state = aut.add_state();
		state_map->emplace(name, state);
		return state;
	};

	for (const auto& str : inter_aut.initial_formula.collect_node_names()) {
		const State state = get_state_name(str);
		aut.initial.insert(state);
	}

	for (const auto& [formula_node, formula_graph] : inter_aut.transitions) {
		const FormulaGraphChildren children{formula_graph.children()};
		if (children.size() != 2) {
			if (children.size() == 1) { throw std::runtime_error("Epsilon transitions not supported"); }
			throw std::runtime_error("Invalid transition");
		}

		const State source = get_state_name(formula_node.name());
		const Symbol symbol = translate_symbol_name(alphabets, children[0].node().name(), aut.levels[source]);
		const State target = get_state_name(children[1].node().name());
		aut.delta.add(source, symbol, target);
	}

	std::unordered_set<std::string> final_formula_nodes;
	if (!(inter_aut.final_formula.node().is_constant())) {
		final_formula_nodes = inter_aut.final_formula.collect_node_names();
	}

	if (inter_aut.final_formula.node().is_true() || inter_aut.are_final_states_conjunction_of_negation()) {
		for (const auto& [state_name, state] : *state_map) {
			if (!final_formula_nodes.contains(state_name)) { aut.final.insert(state); }
		}
	} else {
		for (const auto& str : final_formula_nodes) { aut.final.insert(get_state_name(str)); }
	}

	return aut;
}

void builder::construct(
	mata::nft::Nft* result,
	const mata::IntermediateAut& inter_aut,
	const std::shared_ptr<mata::AlphabetLevels>& alphabets,
	mata::nft::builder::NameStateMap* state_map
) {
	*result = construct(inter_aut, alphabets, state_map);
}

Nft builder::create_single_word_nft(const std::vector<Symbol>& word) {
	return Nft(mata::nfa::builder::create_single_word_nfa(word));
}

Nft builder::create_single_word_nft(const std::vector<std::string>& word, std::shared_ptr<mata::Alphabet> alphabet) {
	return Nft(mata::nfa::builder::create_single_word_nfa(word, std::move(alphabet)));
}

Nft builder::create_empty_string_nft(const size_t num_of_levels) {
	Nft nft{nfa::builder::create_empty_string_nfa()};
	nft.levels.num_of_levels = num_of_levels;
	return nft;
}

Nft builder::create_sigma_star_nft(const size_t num_of_levels) {
	Nft nft{Nft::with_levels({num_of_levels, {0}}, 1, {0}, {0})};
	nft.delta.add(0, DONT_CARE, 0);
	return nft;
}

Nft builder::create_sigma_star_nft(const std::shared_ptr<mata::Alphabet>& alphabet, const size_t num_of_levels) {
	Nft nft{Nft::with_levels({num_of_levels, {0}}, 1, {0}, {0})};
	// A null alphabet means no symbols, like an empty one: the result then accepts only the empty string.
	if (alphabet) { nft.insert_identity(0, alphabet->get_alphabet_symbols().to_vector(), JumpMode::NoJump); }
	return nft;
}

std::optional<Nft>
	builder::parse_explicit_from_mata(const std::string_view nft_in_mata, Alphabet* alphabet, NameStateMap* state_map) {
	IntAlphabet int_alphabet;
	if (alphabet == nullptr) { alphabet = &int_alphabet; }
	// The caller's map stays untouched when the reader gives up halfway. A state name takes a few bytes of the input,
	//  which bounds the numbers worth indexing.
	mata::parser::StateNameMap state_names{state_map != nullptr ? *state_map : NameStateMap{}, nft_in_mata.size() / 4};
	std::optional<Nft> nft{read_explicit_nft(nft_in_mata, alphabet, state_names)};
	if (nft.has_value() && state_map != nullptr) { *state_map = state_names.to_map(); }
	return nft;
}

Nft builder::parse_from_mata(const std::string& nft_in_mata) {
	IntAlphabet alphabet;
	if (std::optional<Nft> nft{parse_explicit_from_mata(nft_in_mata, &alphabet)}) { return std::move(*nft); }

	const parser::Parsed parsed{parser::parse_mf(nft_in_mata)};
	if (parsed.size() != 1) {
		throw std::runtime_error(
			"The number of sections in the input file is '" + std::to_string(parsed.size()) + "'. Required is '1'.\n"
		);
	}
	if (const std::string& automaton_type{parsed[0].type}; !automaton_type.starts_with(TYPE_NFT)) {
		throw std::runtime_error("The type of input automaton is '" + automaton_type + "'. Required is 'NFT'\n");
	}
	return construct(parsed[0], &alphabet);
}

Nft builder::parse_from_mata(std::istream& nft_stream) {
	return parse_from_mata(parser::read_whole_stream(nft_stream));
}

Nft builder::parse_from_mata(const std::filesystem::path& nft_file) {
	std::ifstream file_stream{nft_file};
	if (!file_stream) { throw std::runtime_error("Could not open file \'" + nft_file.string() + "'\n"); }
	return parse_from_mata(file_stream);
}

Nft builder::from_nfa_with_levels_zero(
	const nfa::Nfa& nfa,
	const size_t num_of_levels,
	const bool explicit_transitions,
	const std::optional<Symbol> next_levels_symbol
) {
	if (num_of_levels == 1 || (!explicit_transitions && !next_levels_symbol.has_value())) {
		return Nft(nfa, num_of_levels, 0);
	}

	const size_t nfa_num_of_states{nfa.num_of_states()};
	Nft nft{Nft::with_levels(num_of_levels, nfa_num_of_states, nfa.initial, nfa.final)};

	for (State src{0}; src < nfa_num_of_states; ++src) {
		for (const SymbolPost& symbol_post : nfa.delta[src]) {
			if (symbol_post.symbol == EPSILON &&
				(!next_levels_symbol.has_value() || next_levels_symbol.value() == EPSILON)) {
				nft.delta.add(src, symbol_post.symbol, symbol_post.targets);
				continue;
			}

			const State first_level_state = nft.add_state_with_level(1);
			const Symbol inner_symbol =
				next_levels_symbol.has_value() ? next_levels_symbol.value() : symbol_post.symbol;
			for (const State tgt : symbol_post.targets) {
				// Transition from level 0 to level 1
				nft.delta.add(src, symbol_post.symbol, first_level_state);
				State inner_src = first_level_state;

				// Transition from level 1 to level num_of_levels-1
				if (explicit_transitions) {
					for (Level tgt_level{2}; tgt_level < num_of_levels; ++tgt_level) {
						const State inner_tgt = nft.add_state_with_level(tgt_level);
						nft.delta.add(inner_src, inner_symbol, inner_tgt);
						inner_src = inner_tgt;
					}
				}

				// Transition from level num_of_levels-1 to level num_of_levels (which is the target state of level 0 =
				// num_of_levels % num_of_levels)
				nft.delta.add(inner_src, inner_symbol, tgt);
			}
		}
	}

	return nft;
}

Nft builder::from_nfa_with_levels_advancing(mata::nfa::Nfa nfa, const size_t num_of_levels) {
	Nft result{std::move(nfa)};
	// num_of_levels represents that the state does not have level assigned yet
	std::vector<Level> levels(result.num_of_states(), static_cast<Level>(num_of_levels));

	// We apply simple DFS
	std::vector<State> worklist; // worklist for DFS
	worklist.reserve(result.initial.size());
	for (const State initial_state : result.initial) {
		// start with initial states, which should be level 0
		levels[initial_state] = 0;
		worklist.push_back(initial_state);
	}

	while (!worklist.empty()) {
		const State state_to_process = worklist.back();
		worklist.pop_back();
		for (const State tgt : result.delta[state_to_process].get_successors()) {
			Level next_level = levels[state_to_process] + 1;
			if (next_level == num_of_levels) { next_level = 0; }
			if (levels[tgt] == num_of_levels) { // tgt does not have a level yet
				if (next_level != 0 && result.final.contains(tgt)) {
					throw std::runtime_error(
						"Creating Nft from Nfa that does not represent a valid Nft (final state is not at level 0) in "
						"mata::nft::Nft::from_nfa_with_levels_advancing()"
					);
				}
				levels[tgt] = next_level;
				worklist.push_back(tgt);
			} else if (levels[tgt] != next_level) {
				throw std::runtime_error(
					"Creating Nft from Nfa that does not represent a valid Nft (a state has more possible levels) in "
					"mata::nft::Nft::from_nfa_with_levels_advancing()"
				);
			}
		}
	}
	// States unreachable from the initial states are assigned the default level: any level is vacuously
	//  correct for them, and an in-range level prevents out-of-range indexing into per-level structures.
	std::ranges::replace(levels, static_cast<Level>(num_of_levels), DEFAULT_LEVEL);

	result.levels = Levels{num_of_levels, std::move(levels)};

	return result;
}
