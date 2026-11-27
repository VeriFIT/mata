/** @file
 * @brief Explicit ranked tree transducer reader.
 */

#include "mata/ta/transducer-builder.hh"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <unordered_set>

namespace mata::ta::transducer_builder {
namespace {

[[noreturn]] void invalid_transition(const size_t line, const std::string& reason) {
	throw std::runtime_error("TT transition " + std::to_string(line) + ": " + reason);
}

bool is_reserved_name(const std::string_view name) {
	return name.empty() || name == "(" || name == ")" || name == "&" || name == "|" || name == "!" || name == "->" ||
		   name == ":";
}

parser::BodyLine split_syntax(const parser::BodyLine& input) {
	parser::BodyLine result{};
	for (const parser::BodyToken& token : input) {
		if (token.quoted) {
			result.push_back(token);
			continue;
		}
		size_t start{0};
		size_t position{0};
		while (position < token.token.size()) {
			const bool arrow{token.token.substr(position, 2) == "->"};
			if (!arrow && token.token[position] != ':') {
				++position;
				continue;
			}
			if (position != start) { result.emplace_back(token.token.substr(start, position - start), false); }
			const size_t syntax_size{arrow ? 2U : 1U};
			result.emplace_back(token.token.substr(position, syntax_size), false);
			position += syntax_size;
			start = position;
		}
		if (start < token.token.size()) { result.emplace_back(token.token.substr(start), false); }
	}
	return result;
}

std::vector<std::string_view>
	parse_children(const parser::BodyLine& line, const size_t arrow_index, const size_t line_index) {
	if (arrow_index < 4 || line[1].quoted || line[1].token != "(" || line[arrow_index - 1].quoted ||
		line[arrow_index - 1].token != ")") {
		invalid_transition(line_index, "expected input(child,...) -> output : target");
	}

	std::vector<std::string_view> children{};
	bool expecting_child{true};
	for (size_t token_index{2}; token_index + 1 < arrow_index; ++token_index) {
		const parser::BodyToken& token{line[token_index]};
		if (token.quoted) {
			if (!expecting_child) { invalid_transition(line_index, "missing comma between child states"); }
			if (is_reserved_name(token.token)) { invalid_transition(line_index, "invalid child state name"); }
			children.push_back(token.token);
			expecting_child = false;
			continue;
		}
		size_t start{0};
		while (start <= token.token.size()) {
			const size_t comma{token.token.find(',', start)};
			const std::string_view part{
				token.token.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start)
			};
			if (!part.empty()) {
				if (!expecting_child) { invalid_transition(line_index, "missing comma between child states"); }
				if (part == "->" || part == ":" || part == "(" || part == ")") {
					invalid_transition(line_index, "invalid child state name");
				}
				children.push_back(part);
				expecting_child = false;
			}
			if (comma == std::string_view::npos) { break; }
			if (expecting_child) { invalid_transition(line_index, "empty child state before comma"); }
			expecting_child = true;
			start = comma + 1;
		}
	}
	if (children.empty() || expecting_child) { invalid_transition(line_index, "empty or trailing child state"); }
	return children;
}

TreeTransducer parse_single(const parser::Parsed& parsed, Alphabet* alphabet, NameStateMap* state_map) {
	if (parsed.size() != 1) { throw std::runtime_error("TT parser expects exactly one @TT-explicit section"); }
	return construct(parsed.front(), alphabet, state_map);
}

} // namespace

TreeTransducer construct(const parser::ParsedSection& section, Alphabet* alphabet, NameStateMap* state_map) {
	if (section.type != TYPE_TT) { throw std::runtime_error("TT parser expects @TT-explicit, got @" + section.type); }
	OnTheFlyAlphabet local_alphabet{};
	if (alphabet == nullptr) { alphabet = &local_alphabet; }
	NameStateMap local_state_map{};
	if (state_map == nullptr) { state_map = &local_state_map; }

	for (const auto& [key, values] : section.dict) {
		if (key != "Initial" && key != "Final" && key != "Alphabet-auto") {
			throw std::runtime_error("unsupported TT directive %" + key);
		}
		if (key == "Alphabet-auto" && !values.empty()) {
			throw std::runtime_error("%Alphabet-auto does not take values in @TT-explicit");
		}
	}

	TreeTransducer aut{};
	std::unordered_set<State> used_states{};
	for (const auto& [name, state] : *state_map) { used_states.insert(state); }
	State next_new_state{0};
	const auto get_state{[&](const std::string_view name) {
		if (const auto it{state_map->find(name)}; it != state_map->end()) { return aut.add_state(it->second); }
		while (used_states.contains(next_new_state)) {
			if (next_new_state == Limits::max_state) { nfa::throw_state_overflow(); }
			++next_new_state;
		}
		const State state{aut.add_state(next_new_state)};
		used_states.insert(state);
		state_map->emplace(name, state);
		return state;
	}};

	if (const auto initial_it{section.dict.find("Initial")}; initial_it != section.dict.end()) {
		for (const std::string& name : initial_it->second) { get_state(name); }
	}
	if (const auto final_it{section.dict.find("Final")}; final_it != section.dict.end()) {
		for (const std::string& name : final_it->second) { aut.final.insert(get_state(name)); }
	}

	for (size_t line_index{0}; line_index < section.body.size(); ++line_index) {
		const parser::BodyLine line{split_syntax(section.body[line_index])};
		if (line.size() < 4) { invalid_transition(line_index, "too few tokens"); }
		const auto arrow{std::find_if(line.begin(), line.end(), [](const parser::BodyToken& token) {
			return !token.quoted && token.token == "->";
		})};
		if (arrow == line.end()) { invalid_transition(line_index, "missing ->"); }
		if (std::find_if(arrow + 1, line.end(), [](const parser::BodyToken& token) {
				return !token.quoted && token.token == "->";
			}) != line.end()) {
			invalid_transition(line_index, "multiple -> tokens");
		}
		const size_t arrow_index{static_cast<size_t>(arrow - line.begin())};
		if (arrow_index == 0) { invalid_transition(line_index, "missing input symbol"); }
		if (arrow_index != 1 && arrow_index < 4) { invalid_transition(line_index, "invalid left-hand side"); }
		if (line.size() - arrow_index != 4 || (arrow + 2)->quoted || (arrow + 2)->token != ":") {
			invalid_transition(line_index, "expected output -> symbol : target");
		}
		if (is_reserved_name(line.front().token)) { invalid_transition(line_index, "invalid input symbol"); }
		if (is_reserved_name((arrow + 1)->token)) { invalid_transition(line_index, "invalid output symbol"); }
		if (is_reserved_name((arrow + 3)->token)) { invalid_transition(line_index, "invalid target state"); }

		std::vector<State> children{};
		if (arrow_index != 1) {
			const std::vector<std::string_view> child_names{parse_children(line, arrow_index, line_index)};
			children.reserve(child_names.size());
			for (const std::string_view name : child_names) { children.push_back(get_state(name)); }
		}
		const Symbol input_symbol{alphabet->translate_symb(line.front().token)};
		const Symbol output_symbol{alphabet->translate_symb((arrow + 1)->token)};
		const State target{get_state((arrow + 3)->token)};
		aut.add_transition(input_symbol, std::move(children), output_symbol, target);
	}
	return aut;
}

TreeTransducer parse_from_mata(const std::string& input, Alphabet* alphabet, NameStateMap* state_map) {
	return parse_single(parser::parse_mf(input), alphabet, state_map);
}

TreeTransducer parse_from_mata(std::istream& input, Alphabet* alphabet, NameStateMap* state_map) {
	return parse_single(parser::parse_mf(input), alphabet, state_map);
}

TreeTransducer parse_from_mata(const std::filesystem::path& input, Alphabet* alphabet, NameStateMap* state_map) {
	std::ifstream stream{input};
	if (!stream) { throw std::runtime_error("Cannot open TT file: " + input.string()); }
	return parse_from_mata(stream, alphabet, state_map);
}

} // namespace mata::ta::transducer_builder
