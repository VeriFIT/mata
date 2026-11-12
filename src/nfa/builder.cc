/** @file
 * @brief Functions to build predefined types of NFAs, to create them from regular expressions, and to load them from
 * files.
 */

#include "mata/nfa/builder.hh"
#include "mata/parser/mintermization.hh"
#include "mata/parser/re2parser.hh"
#include "mata/utils/assert.hh"

#include "../explicit-section-reader.hh"

#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <numeric>
#include <random>
#include <unordered_set>

using namespace mata::nfa;
using mata::Symbol;
using mata::nfa::Nfa;

namespace {
/// Maps the printed name of EPSILON back to the symbol; every other name goes to the alphabet, which never sees the
// special names.
Symbol translate_symbol_name(mata::Alphabet* alphabet, const std::string& name) {
	if (name == mata::EPSILON_NAME) { return EPSILON; }
	return alphabet->translate_symb(name);
}

/// The namings of states, symbols, and nodes the fast reader supports; see @c IntermediateAut::Naming.
enum class Naming : std::uint8_t { Marked, Auto };

/**
 * The namings of a section, read from its `%Alphabet`, `%States`, and `%Nodes` keys as
 *  @c IntermediateAut::parse_from_mf() reads them. They decide which tokens name states and what the names are.
 */
class Namings {
	enum Kind : std::uint8_t { STATES, SYMBOLS, NODES };
	std::array<Naming, 3> namings_{Naming::Marked, Naming::Marked, Naming::Marked};
	/// The key that set each naming; @c IntermediateAut::parse_from_mf() lets the last of two different keys of one
	///  kind win, in the key order rather than the file order.
	std::array<std::string, 3> keys_{};

  public:
	/// Whether @c IntermediateAut::parse_from_mf() reads @p key as a naming key.
	static bool is_naming_key(const std::string_view key) {
		return key.contains("Alphabet") || key.contains("States") || key.contains("Nodes");
	}

	/// Applies the naming key @p key. @return Whether the naming stays within the fast reader's subset.
	bool apply(const std::string_view key) {
		const Kind kind{key.contains("Alphabet") ? SYMBOLS : (key.contains("States") ? STATES : NODES)};
		if (!keys_[kind].empty() && keys_[kind] != key) { return false; }
		keys_[kind] = key;
		const size_t dash{key.find('-')};
		const std::string_view type{dash == std::string_view::npos ? "auto" : key.substr(dash + 1)};
		if (type == "auto") {
			namings_[kind] = Naming::Auto;
		} else if (type == "marked") {
			namings_[kind] = Naming::Marked;
		} else { // Enumerated, character, or unknown namings.
			return false;
		}
		return true;
	}

	/// The check of @c IntermediateAut::parse_from_mf(), named "has at most one automatic naming" there.
	bool valid() const {
		return (namings_[NODES] == Naming::Auto && namings_[SYMBOLS] == Naming::Auto) ||
			   namings_[STATES] != Naming::Auto;
	}

	/**
	 * The name of the state @p token stands for, following @c create_node() of @c IntermediateAut.
	 * @param quoted Whether @c create_node() sees the token as quoted, which keeps it from being read for an operator.
	 * @return The name, or @c std::nullopt when @p token is not a state.
	 */
	std::optional<std::string_view> state_name(const std::string_view token, const bool quoted) const {
		const char first{token.empty() ? '\0' : token.front()};
		if (!quoted && (first == '&' || first == '|' || first == '!' || token == "(" || token == ")")) {
			return std::nullopt;
		}
		if (token == "\\true" || token == "\\false") { return std::nullopt; }
		if (namings_[STATES] == Naming::Marked && first == 'q') { return token.substr(1); }
		if ((namings_[NODES] == Naming::Marked && first == 'n') ||
			(namings_[SYMBOLS] == Naming::Marked && first == 'a')) {
			return std::nullopt;
		}
		if (namings_[STATES] == Naming::Auto) { return token; }
		return std::nullopt;
	}
};

/// A body token as @c parse_mf() hands it over: with @p keep_quotes, a quoted token keeps its quotes.
std::string_view body_token(const mata::parser::ExplicitToken& token, const bool keep_quotes, std::string& buffer) {
	if (!keep_quotes || !token.quoted) { return token.text; }
	buffer.assign(1, '"');
	buffer += token.text;
	buffer += '"';
	return buffer;
}

/**
 * Reads an NFA from @p input with @c mata::parser::ExplicitSectionReader, producing the NFA that
 *  @c builder::construct() would produce from @c IntermediateAut::parse_from_mf() of @c parser::parse_mf() of @p input.
 * The initial states are numbered in the order of the file, not in the order of an unordered set as there.
 * @return The NFA, or @c std::nullopt when @p input leaves the subset of the format this reader covers.
 */
std::optional<Nfa> read_explicit_nfa(
	const std::string_view input,
	mata::Alphabet* alphabet,
	mata::parser::StateNameMap& state_names,
	const bool keep_quotes
) {
	using mata::parser::ExplicitSectionReader;
	using mata::parser::ExplicitToken;

	ExplicitSectionReader reader{input};
	if (const std::optional<std::string_view> type{reader.read_type()};
		!type.has_value() || (*type != TYPE_NFA && *type != TYPE_NFA + "-explicit")) {
		return std::nullopt;
	}

	Nfa aut;
	// See builder::construct(): a name mapped by the caller allocates its state.
	const auto get_state{[&](const std::string_view name) {
		if (const std::optional<State> state{state_names.find(name)}) { return aut.add_state(*state); }
		const State state{aut.add_state()};
		state_names.insert(name, state);
		return state;
	}};

	Namings namings;
	// The %Initial and %Final values. Their state names depend on the namings, which may follow them in the file.
	//  IntermediateAut::parse_from_mf() reads the values as unquoted tokens, so their quoting is dropped.
	std::vector<std::string> initial_names;
	std::vector<std::string> final_names;
	const auto add_names{[](const std::vector<ExplicitToken>& tokens, std::vector<std::string>& names) {
		for (const ExplicitToken& token : tokens) { names.emplace_back(token.text); }
	}};

	std::vector<Transition> transitions;
	bool in_body{false};
	const auto start_body{[&]() {
		if (!namings.valid()) { return false; }
		for (const std::string& name : initial_names) {
			const std::optional<std::string_view> state_name{namings.state_name(name, false)};
			if (!state_name.has_value()) { return false; }
			aut.initial.insert(get_state(*state_name));
		}
		in_body = true;
		return true;
	}};

	std::string source_buffer;
	std::string symbol_buffer;
	std::string target_buffer;
	while (true) {
		switch (reader.read_line()) {
			case ExplicitSectionReader::Line::Key: {
				const std::string_view key{reader.key()};
				if (Namings::is_naming_key(key)) {
					if (in_body || key.starts_with("Initial") || key.starts_with("Final") || !namings.apply(key)) {
						return std::nullopt;
					}
				} else if (key.starts_with("Initial")) {
					if (in_body || key != "Initial") { return std::nullopt; }
					add_names(reader.tokens(), initial_names);
				} else if (key.starts_with("Final")) {
					if (key != "Final") { return std::nullopt; }
					add_names(reader.tokens(), final_names);
				} // IntermediateAut::parse_from_mf() ignores any other key.
				break;
			}
			case ExplicitSectionReader::Line::Body: {
				if (!in_body && !start_body()) { return std::nullopt; }
				const std::vector<ExplicitToken>& tokens{reader.tokens()};
				// Other lines are formulas, such as `q1 a & q2`, or malformed.
				if (tokens.size() != 3) { return std::nullopt; }
				const std::optional<std::string_view> source_name{
					namings.state_name(body_token(tokens[0], keep_quotes, source_buffer), tokens[0].quoted)
				};
				const std::optional<std::string_view> target_name{
					namings.state_name(body_token(tokens[2], keep_quotes, target_buffer), tokens[2].quoted)
				};
				if (!source_name.has_value() || !target_name.has_value()) { return std::nullopt; }
				// Sequenced as in builder::construct().
				const State source{get_state(*source_name)};
				if (keep_quotes && tokens[1].quoted) {
					body_token(tokens[1], keep_quotes, symbol_buffer);
				} else {
					symbol_buffer.assign(tokens[1].text);
				}
				const Symbol symbol{translate_symbol_name(alphabet, symbol_buffer)};
				const State target{get_state(*target_name)};
				transitions.emplace_back(source, symbol, target);
				break;
			}
			case ExplicitSectionReader::Line::End: {
				if (!in_body && !start_body()) { return std::nullopt; }
				for (const std::string& name : final_names) {
					const std::optional<std::string_view> state_name{namings.state_name(name, false)};
					if (!state_name.has_value()) { return std::nullopt; }
					aut.final.insert(get_state(*state_name));
				}
				aut.delta.add(std::move(transitions));
				return aut;
			}
			case ExplicitSectionReader::Line::Unsupported:
				return std::nullopt;
		}
	}
}
} // namespace

Nfa builder::construct(const parser::ParsedSection& parsec, Alphabet* alphabet, NameStateMap* state_map) {
	Nfa aut;
	MATA_ASSERT(nullptr != alphabet);

	if (parsec.type != TYPE_NFA) {
		throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" + TYPE_NFA + "\"");
	}

	NameStateMap tmp_state_map;
	if (nullptr == state_map) { state_map = &tmp_state_map; }

	// a lambda for translating state names to identifiers; a name mapped by the caller allocates its state, so that
	//  a new name never gets its identifier while the transitions are only gathered
	auto get_state_name = [&state_map, &aut](const std::string& str) {
		if (!state_map->contains(str)) {
			const State state = aut.add_state();
			state_map->insert({str, state});
			return state;
		}
		return aut.add_state((*state_map)[str]);
	};

	auto it = parsec.dict.find("Initial");
	if (parsec.dict.end() != it) {
		for (const auto& str : it->second) { aut.initial.insert(get_state_name(str)); }
	}

	it = parsec.dict.find("Final");
	if (parsec.dict.end() != it) {
		for (const auto& str : it->second) { aut.final.insert(get_state_name(str)); }
	}

	// The transitions arrive in file order, so they are gathered and imported in one batch instead of being
	//  inserted one by one, each with its own search and vector shift.
	std::vector<Transition> transitions;
	transitions.reserve(parsec.body.size());
	for (const auto& body_line : parsec.body) {
		if (body_line.size() != 3) {
			if (body_line.size() == 2) {
				throw std::runtime_error("Epsilon transitions not supported: " + std::to_string(body_line));
			}
			throw std::runtime_error("Invalid transition: " + std::to_string(body_line));
		}

		// Sequenced, so that a mapped source is allocated before a new target gets an identifier.
		const State src_state{get_state_name(body_line[0].token)};
		const Symbol symbol{translate_symbol_name(alphabet, body_line[1].token)};
		const State tgt_state{get_state_name(body_line[2].token)};
		transitions.emplace_back(src_state, symbol, tgt_state);
	}
	aut.delta.add(std::move(transitions));

	return aut;
}

Nfa builder::construct(const IntermediateAut& inter_aut, Alphabet* alphabet, NameStateMap* state_map) {
	Nfa aut;
	if (nullptr == alphabet) { throw std::runtime_error("construct(): alphabet must not be null"); }

	if (!inter_aut.is_nfa()) {
		throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" + TYPE_NFA + "\"");
	}

	NameStateMap tmp_state_map;
	if (nullptr == state_map) { state_map = &tmp_state_map; }

	// a lambda for translating state names to identifiers, see the ParsedSection overload above
	auto get_state_name = [&state_map, &aut](const std::string& str) {
		if (!state_map->contains(str)) {
			const State state = aut.add_state();
			state_map->insert({str, state});
			return state;
		}
		return aut.add_state((*state_map)[str]);
	};

	for (const auto& str : inter_aut.initial_formula.collect_node_names()) {
		const State state = get_state_name(str);
		aut.initial.insert(state);
	}

	// Gathered and imported in one batch, see the ParsedSection overload above.
	std::vector<Transition> transitions;
	transitions.reserve(inter_aut.transitions.size());
	for (const auto& [formula_node, formula_graph] : inter_aut.transitions) {
		if (formula_graph.children.size() != 2) {
			if (formula_graph.children.size() == 1) { throw std::runtime_error("Epsilon transitions not supported"); }
			throw std::runtime_error("Invalid transition");
		}

		const State src_state{get_state_name(formula_node.name)};
		const Symbol symbol{translate_symbol_name(alphabet, formula_graph.children[0].node.name)};
		const State tgt_state{get_state_name(formula_graph.children[1].node.name)};

		transitions.emplace_back(src_state, symbol, tgt_state);
	}
	aut.delta.add(std::move(transitions));

	std::unordered_set<std::string> final_formula_nodes;
	if (!(inter_aut.final_formula.node.is_constant())) {
		// we do not want to parse true/false (constant) as a state so we do not collect it
		final_formula_nodes = inter_aut.final_formula.collect_node_names();
	}
	// for constant true, we will pretend that final nodes are negated with empty final_formula_nodes

	if (inter_aut.final_formula.node.is_true() || inter_aut.are_final_states_conjunction_of_negation()) {
		// we add all states NOT in final_formula_nodes to final states
		for (const auto& [state_name, state] : *state_map) {
			if (!final_formula_nodes.contains(state_name)) { aut.final.insert(state); }
		}
	} else {
		// we add all states in final_formula_nodes to final states
		for (const auto& str : final_formula_nodes) {
			const State state = get_state_name(str);
			aut.final.insert(state);
		}
	}

	return aut;
} // construct().

void builder::construct(Nfa* result, const IntermediateAut& inter_aut, Alphabet* alphabet, NameStateMap* state_map) {
	*result = construct(inter_aut, alphabet, state_map);
}

Nfa builder::create_single_word_nfa(const std::vector<Symbol>& word) {
	const size_t word_size{word.size()};
	Nfa nfa{word_size + 1, {0}, {to_state(word_size)}};

	for (State state{0}; state < word_size; ++state) { nfa.delta.add(state, word[state], state + 1); }
	return nfa;
}

Nfa builder::create_single_word_nfa(const std::vector<std::string>& word, std::shared_ptr<Alphabet> alphabet) {
	if (!alphabet) { alphabet = std::make_shared<OnTheFlyAlphabet>(word); }
	const size_t word_size{word.size()};
	Nfa nfa{word_size + 1, {0}, {to_state(word_size)}, alphabet};

	for (State state{0}; state < word_size; ++state) {
		nfa.delta.add(state, alphabet->translate_symb(word[state]), state + 1);
	}
	return nfa;
}

Nfa builder::create_empty_string_nfa() { return Nfa{1, {0}, {0}}; }

Nfa builder::create_sigma_star_nfa(const std::shared_ptr<Alphabet>& alphabet) {
	Nfa nfa{1, {0}, {0}, alphabet};
	for (const Symbol& symbol : alphabet->get_alphabet_symbols()) { nfa.delta.add(0, symbol, 0); }
	return nfa;
}

Nfa builder::create_random_nfa_tabakov_vardi(
	const size_t num_of_states,
	const size_t alphabet_size,
	const double states_transitions_ratio_per_symbol,
	const double final_state_density,
	const std::optional<unsigned int>& seed
) {
	// Attach an alphabet holding exactly the symbols this generator uses. An empty alphabet would hide them from
	//  `get_symbols_to_work_with()`, which prefers the attached alphabet over the symbols actually used, and would
	//  thus break printing, completeness checks and completion.
	std::vector<Symbol> alphabet_symbols(alphabet_size);
	// 'std::ranges::iota' would read better, but it is unavailable in older libc++ releases, while CMake still
	// supports Clang 18.
	// NOLINTNEXTLINE(modernize-use-ranges)
	std::iota(alphabet_symbols.begin(), alphabet_symbols.end(), 0);
	auto alphabet{std::make_shared<EnumAlphabet>(alphabet_symbols.begin(), alphabet_symbols.end())};

	if (num_of_states == 0) { return Nfa{0, {}, {}, alphabet}; }
	if (states_transitions_ratio_per_symbol < 0 ||
		static_cast<size_t>(states_transitions_ratio_per_symbol) > num_of_states) {
		// Maximum of num_of_states^2 unique transitions for one symbol can be created.
		throw std::runtime_error("Transition density must be in range [0, num_of_states]");
	}
	if (final_state_density < 0 || final_state_density > 1) {
		// Maximum of num_of_states final states can be created.
		throw std::runtime_error("Final state density must be in range (0, 1]");
	}

	Nfa nfa{num_of_states, {0}, {0}, alphabet};

	// Initialize the random number generator
	const unsigned int seed_value{seed.value_or(std::random_device{}())}; // Seed for the random number engine
	std::mt19937 gen(seed_value); // Mersenne Twister engine

	// Unique final state generator
	std::vector<State> states(num_of_states);
	// 'std::ranges::iota' would read better, but it is unavailable in older libc++ releases, while CMake still
	// supports Clang 18.
	// NOLINTNEXTLINE(modernize-use-ranges)
	std::iota(states.begin(), states.end(), 0);
	std::shuffle(states.begin() + 1, states.end(), gen); // Starting from 1, because 0 is allways final state.

	// Create final states
	const size_t num_of_final_states{
		static_cast<size_t>(std::round(static_cast<double>(num_of_states) * final_state_density))
	};
	for (size_t i = 0; i < num_of_final_states; ++i) { nfa.final.insert(states[i]); }

	// The transitions of one symbol are distinct cells of the num_of_states x num_of_states matrix. Floyd's algorithm
	//  samples them uniformly in time and memory proportional to their number instead of shuffling all the cells.
	//  Cells already taken are kept in a bitmap over all cells when it is not much larger than the sample, otherwise
	//  in a hash set.
	const size_t num_of_cells{num_of_states * num_of_states};
	// Using std::min because, in some universe, casting and rounding might cause the number of transitions to exceed
	// the number of possible transitions by 1.
	const size_t num_of_transitions_per_symbol{std::min(
		static_cast<size_t>(std::round(static_cast<double>(num_of_states) * states_transitions_ratio_per_symbol)),
		num_of_cells
	)};
	const bool use_bitmap{num_of_cells / 64 <= num_of_transitions_per_symbol};
	std::vector<bool> taken_bitmap(use_bitmap ? num_of_cells : 0);
	std::unordered_set<size_t> taken_set{};
	if (!use_bitmap) { taken_set.reserve(num_of_transitions_per_symbol); }
	std::vector<size_t> cells{};
	cells.reserve(num_of_transitions_per_symbol);
	for (Symbol symbol{0}; symbol < alphabet_size; ++symbol) {
		cells.clear();
		taken_set.clear();
		for (size_t upper{num_of_cells - num_of_transitions_per_symbol}; upper < num_of_cells; ++upper) {
			const size_t candidate{std::uniform_int_distribution<size_t>{0, upper}(gen)};
			const bool candidate_taken{use_bitmap ? taken_bitmap[candidate] : taken_set.contains(candidate)};
			const size_t cell{candidate_taken ? upper : candidate};
			if (use_bitmap) {
				taken_bitmap[cell] = true;
			} else {
				taken_set.insert(cell);
			}
			cells.push_back(cell);
		}
		// Cells in increasing order append to the delta: sources, the symbol and targets all ascend.
		std::ranges::sort(cells);
		for (const size_t cell : cells) {
			if (use_bitmap) { taken_bitmap[cell] = false; }
			nfa.delta.add(static_cast<State>(cell / num_of_states), symbol, static_cast<State>(cell % num_of_states));
		}
	}
	return nfa;
}

std::optional<Nfa> builder::parse_explicit_from_mata(
	const std::string_view nfa_in_mata, Alphabet* alphabet, NameStateMap* state_map, const bool keep_quotes
) {
	IntAlphabet int_alphabet;
	if (alphabet == nullptr) { alphabet = &int_alphabet; }
	// The caller's map stays untouched when the reader gives up halfway. A state name takes a few bytes of the input,
	//  which bounds the numbers worth indexing.
	mata::parser::StateNameMap state_names{state_map != nullptr ? *state_map : NameStateMap{}, nfa_in_mata.size() / 4};
	std::optional<Nfa> nfa{read_explicit_nfa(nfa_in_mata, alphabet, state_names, keep_quotes)};
	if (nfa.has_value() && state_map != nullptr) { *state_map = state_names.to_map(); }
	return nfa;
}

Nfa builder::parse_from_mata(
	const std::string& nfa_in_mata, Alphabet* alphabet, NameStateMap* state_map, const bool keep_quotes
) {
	IntAlphabet int_alphabet;
	if (alphabet == nullptr) { alphabet = &int_alphabet; }
	if (std::optional<Nfa> nfa{parse_explicit_from_mata(nfa_in_mata, alphabet, state_map, keep_quotes)}) {
		return std::move(*nfa);
	}

	const parser::Parsed parsed{parser::parse_mf(nfa_in_mata, keep_quotes)};
	if (parsed.size() != 1) {
		throw std::runtime_error(
			"The number of sections in the input file is '" + std::to_string(parsed.size()) + "'. Required is '1'.\n"
		);
	}
	if (const std::string& automaton_type{parsed[0].type}; !automaton_type.starts_with(TYPE_NFA)) {
		throw std::runtime_error("The type of input automaton is '" + automaton_type + "'. Required is 'NFA'\n");
	}
	IntermediateAut inter_aut{std::move(IntermediateAut::parse_from_mf(parsed)[0])};
	if (inter_aut.is_bitvector()) { inter_aut = Mintermization{}.mintermize(inter_aut); }
	return construct(inter_aut, alphabet, state_map);
}

Nfa builder::parse_from_mata(
	std::istream& nfa_stream, Alphabet* alphabet, NameStateMap* state_map, const bool keep_quotes
) {
	return parse_from_mata(parser::read_whole_stream(nfa_stream), alphabet, state_map, keep_quotes);
}

Nfa builder::parse_from_mata(
	const std::filesystem::path& nfa_file, Alphabet* alphabet, NameStateMap* state_map, const bool keep_quotes
) {
	std::ifstream file_stream{nfa_file};
	if (!file_stream) { throw std::runtime_error("Could not open file \'" + nfa_file.string() + "'\n"); }
	return parse_from_mata(file_stream, alphabet, state_map, keep_quotes);
}

Nfa builder::create_from_regex(const std::string& regex, const bool reduce) {
	return parser::create_nfa(regex, false, EPSILON, reduce);
}

Nfa builder::create_from_regex(const std::string& regex, const parser::Re2Options& options) {
	return parser::create_nfa(regex, options);
}
