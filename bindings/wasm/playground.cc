/** @file
 * @brief The engine of the Mata playground on the website.
 */

#include "playground.hh"

#include "mata/alphabet.hh"
#include "mata/nfa/builder.hh"
#include "mata/nfa/nfa.hh"
#include "mata/parser/inter-aut.hh"
#include "mata/parser/parser.hh"
#include "mata/parser/re2parser.hh"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// Set by CMakeLists.txt.
#ifndef MATA_PLAYGROUND_REVISION
	#define MATA_PLAYGROUND_REVISION ""
#endif

namespace mata::playground {
namespace {

using nfa::EPSILON;
using nfa::Nfa;
using nfa::Run;
using nfa::State;
using nfa::Transition;

/// Larger automata are described without their transitions: the page could not draw them anyway.
constexpr size_t MAX_LISTED_TRANSITIONS{5'000};
/// The .mata text of larger automata is cut at the last line that fits.
constexpr size_t MAX_MATA_LENGTH{200'000};

/// An error in one of the inputs of a request, which the page shows next to that input.
class InputError : public std::runtime_error {
  public:
	InputError(std::string source, const std::string& message)
		: std::runtime_error{message},
		  source_{std::move(source)} {}

	const std::string& source() const { return source_; }

  private:
	std::string source_;
};

/// The name of byte @p byte: the character itself for a printable ASCII character, and \\xHH otherwise. The
///  library printer quotes a name when the reader would take it for something else. Backslash stays escaped: its
///  own name would be a trailing backslash, which the .mata reader cannot take back from a quoted token.
std::string byte_name(const unsigned byte) {
	if (byte > 0x20 && byte < 0x7f && byte != '\\') { return std::string(1, static_cast<char>(byte)); }
	constexpr std::string_view DIGITS{"0123456789abcdef"};
	return {'\\', 'x', DIGITS[byte >> 4], DIGITS[byte & 0xf]};
}

/// Appends @p text to @p out as a JSON string.
void append_json_string(std::string& out, const std::string_view text) {
	out += '"';
	for (const char ch : text) {
		switch (ch) {
			case '"':
				out += "\\\"";
				break;
			case '\\':
				out += "\\\\";
				break;
			case '\n':
				out += "\\n";
				break;
			case '\r':
				out += "\\r";
				break;
			case '\t':
				out += "\\t";
				break;
			default:
				if (static_cast<unsigned char>(ch) < 0x20) {
					char escape[8];
					std::snprintf(escape, sizeof(escape), "\\u%04x", static_cast<unsigned>(ch));
					out += escape;
				} else {
					out += ch;
				}
		}
	}
	out += '"';
}

std::string error_json(const std::string_view source, const std::string_view message) {
	std::string out{R"({"ok":false,"source":)"};
	append_json_string(out, source);
	out += R"(,"error":)";
	append_json_string(out, message);
	out += '}';
	return out;
}

bool has_epsilon(const Nfa& aut) {
	for (const Transition& transition : aut.delta.transitions()) {
		if (transition.symbol == EPSILON) { return true; }
	}
	return false;
}

/// An input automaton, with the names of its states when it is given in the .mata format.
struct Input {
	Nfa nfa;
	std::vector<std::string> state_names;
};

/// One evaluation of a request.
class Evaluation {
  public:
	explicit Evaluation(const Request& request) : request_{request} {
		for (unsigned byte{0}; byte < 256; ++byte) { alphabet_.add_new_symbol(byte_name(byte), byte); }
	}

	std::string run();

  private:
	Input parse(const std::string& source, const std::string& format, const std::string& text);
	Input parse_mata(const std::string& text);
	/// @p aut without its ε-transitions; records that they were removed from automaton @p source.
	Nfa without_epsilon(const Nfa& aut, const std::string& source);
	utils::OrdVector<Symbol> alphabet_symbols(const Nfa& a, const Nfa* b);
	void append_automaton(std::string& out, const Nfa& aut, const std::vector<std::string>& state_names);
	void append_word(std::string& out, const Word& word);
	/// @p aut in the .mata format. Unlike Nfa::print_to_mata(), it quotes the symbol names that need it, so that the
	///  text reads back as the same automaton.
	std::string to_mata(const Nfa& aut);
	int symbol_index(Symbol symbol);

	const Request& request_;
	OnTheFlyAlphabet alphabet_{};
	std::vector<Symbol> symbols_{}; ///< The symbols referred to in the response, in the order of their indices.
	std::unordered_map<Symbol, int> symbol_indices_{};
	std::vector<std::string> epsilon_removed_{};
};

Input Evaluation::parse(const std::string& source, const std::string& format, const std::string& text) {
	try {
		if (format == "regex") {
			parser::Re2Options options{};
			options.reduce = false;
			return {parser::create_nfa(text, options), {}};
		}
		if (format == "mata") { return parse_mata(text); }
	} catch (const std::bad_alloc&) { throw; } catch (const std::exception& error) {
		throw InputError{source, error.what()};
	}
	throw InputError{source, "Unknown format \"" + format + "\"."};
}

Input Evaluation::parse_mata(const std::string& text) {
	if (const size_t start{text.find_first_not_of(" \t\r\n")};
		start != std::string::npos && text[start] != '@' && text[start] != '#') {
		throw std::runtime_error(
			"An automaton in the .mata format starts with the line @NFA-explicit. For a regular expression, choose the "
			"format Regular expression."
		);
	}
	const parser::Parsed parsed{parser::parse_mf(text)};
	if (parsed.empty()) {
		throw std::runtime_error("The text contains no automaton. An automaton starts with the line @NFA-explicit.");
	}
	if (parsed.size() > 1) {
		throw std::runtime_error("The text contains " + std::to_string(parsed.size()) + " automata instead of one.");
	}
	if (parsed[0].type != "NFA-explicit") {
		throw std::runtime_error(
			"The playground reads automata of the type @NFA-explicit, not @" + parsed[0].type + "."
		);
	}
	const auto& dict{parsed[0].dict};
	if (std::ranges::none_of(dict, [](const auto& entry) { return entry.first.starts_with("Alphabet"); })) {
		throw std::runtime_error("Add the line %Alphabet-auto after @NFA-explicit, so that symbols are read by name.");
	}
	const std::vector<IntermediateAut> intermediate{IntermediateAut::parse_from_mf(parsed)};
	nfa::builder::NameStateMap state_map{};
	Input input{nfa::builder::construct(intermediate.at(0), &alphabet_, &state_map), {}};
	input.state_names.resize(input.nfa.num_of_states());
	for (const auto& [name, state] : state_map) { input.state_names[state] = name; }
	return input;
}

Nfa Evaluation::without_epsilon(const Nfa& aut, const std::string& source) {
	if (!has_epsilon(aut)) { return aut; }
	epsilon_removed_.push_back(source);
	return nfa::remove_epsilon(aut);
}

utils::OrdVector<Symbol> Evaluation::alphabet_symbols(const Nfa& a, const Nfa* b) {
	utils::OrdVector<Symbol> symbols{};
	if (request_.alphabet == "bytes") {
		for (Symbol byte{0}; byte < 256; ++byte) { symbols.push_back(byte); }
	}
	for (const Nfa* aut : {&a, b}) {
		if (aut == nullptr) { continue; }
		for (const Transition& transition : aut->delta.transitions()) {
			if (transition.symbol != EPSILON) { symbols.insert(transition.symbol); }
		}
	}
	return symbols;
}

int Evaluation::symbol_index(const Symbol symbol) {
	if (symbol == EPSILON) { return -1; }
	const auto [it, inserted] = symbol_indices_.try_emplace(symbol, static_cast<int>(symbols_.size()));
	if (inserted) { symbols_.push_back(symbol); }
	return it->second;
}

std::string Evaluation::to_mata(const Nfa& aut) {
	// Nfa::print_to_mata() quotes the symbol names that need it, so the text reads back as the same automaton.
	return aut.print_to_mata(&alphabet_);
}

void Evaluation::append_word(std::string& out, const Word& word) {
	out += '[';
	bool first{true};
	for (const Symbol symbol : word) {
		// A run through ε-transitions spells ε, which is not a letter of the word.
		if (symbol == EPSILON) { continue; }
		if (!first) { out += ','; }
		first = false;
		out += std::to_string(symbol_index(symbol));
	}
	out += ']';
}

void Evaluation::append_automaton(std::string& out, const Nfa& aut, const std::vector<std::string>& state_names) {
	const auto append_states = [&out](const auto& states) {
		out += '[';
		bool first{true};
		for (const State state : states) {
			if (!first) { out += ','; }
			first = false;
			out += std::to_string(state);
		}
		out += ']';
	};

	out += R"({"states":)";
	out += std::to_string(aut.num_of_states());
	out += R"(,"initial":)";
	append_states(aut.initial);
	out += R"(,"final":)";
	append_states(aut.final);
	const size_t num_of_transitions{aut.delta.num_of_transitions()};
	out += R"(,"transitionCount":)";
	out += std::to_string(num_of_transitions);
	if (num_of_transitions <= MAX_LISTED_TRANSITIONS) {
		out += R"(,"transitions":[)";
		bool first{true};
		for (const Transition& transition : aut.delta.transitions()) {
			if (!first) { out += ','; }
			first = false;
			out += '[';
			out += std::to_string(transition.source);
			out += ',';
			out += std::to_string(symbol_index(transition.symbol));
			out += ',';
			out += std::to_string(transition.target);
			out += ']';
		}
		out += ']';
	}
	// Nfa::is_deterministic() treats ε as a symbol; an automaton with ε-transitions is not deterministic here.
	out += R"(,"deterministic":)";
	out += !has_epsilon(aut) && aut.is_deterministic() ? "true" : "false";
	if (!state_names.empty()) {
		out += R"(,"stateNames":[)";
		bool first{true};
		for (const std::string& name : state_names) {
			if (!first) { out += ','; }
			first = false;
			append_json_string(out, name);
		}
		out += ']';
	}
	std::string mata{to_mata(aut)};
	const bool truncated{mata.size() > MAX_MATA_LENGTH};
	if (truncated) { mata.resize(mata.rfind('\n', MAX_MATA_LENGTH) + 1); }
	out += R"(,"mata":)";
	append_json_string(out, mata);
	out += R"(,"mataTruncated":)";
	out += truncated ? "true" : "false";
	out += '}';
}

std::string Evaluation::run() {
	const std::string& operation{request_.operation};
	const bool uses_b{
		operation == "intersection" || operation == "union" || operation == "concatenation" ||
		operation == "difference" || operation == "is_included" || operation == "are_equivalent"
	};

	const Input a{parse("a", request_.a_format, request_.a_text)};
	std::optional<Input> b{};
	if (uses_b) { b = parse("b", request_.b_format, request_.b_text); }

	std::optional<Nfa> result{};
	std::optional<bool> holds{};
	std::optional<Word> witness{};
	std::optional<bool> witness_in_a{};

	const auto start{std::chrono::steady_clock::now()};
	if (operation == "show") {
		// Only the inputs are described.
	} else if (operation == "determinize") {
		result = nfa::determinize(without_epsilon(a.nfa, "a"));
	} else if (operation == "minimize") {
		result = nfa::minimize(without_epsilon(a.nfa, "a"));
	} else if (operation == "reduce") {
		result = nfa::reduce(a.nfa);
	} else if (operation == "trim") {
		result = a.nfa;
		result->trim();
	} else if (operation == "complement") {
		const Nfa aut{without_epsilon(a.nfa, "a")};
		result = nfa::complement(aut, alphabet_symbols(aut, nullptr));
	} else if (operation == "reverse") {
		result = nfa::revert(a.nfa);
	} else if (operation == "remove_epsilon") {
		result = nfa::remove_epsilon(a.nfa);
	} else if (operation == "intersection") {
		result = nfa::intersection(a.nfa, b->nfa);
	} else if (operation == "union") {
		result = nfa::union_nondet(a.nfa, b->nfa);
	} else if (operation == "concatenation") {
		result = nfa::concatenate(a.nfa, b->nfa);
	} else if (operation == "difference") {
		result = nfa::lang_difference(without_epsilon(a.nfa, "a"), without_epsilon(b->nfa, "b"));
	} else if (operation == "is_empty") {
		Run run{};
		holds = a.nfa.is_lang_empty(&run);
		if (!*holds) { witness = run.word; }
	} else if (operation == "is_universal") {
		const Nfa aut{without_epsilon(a.nfa, "a")};
		const EnumAlphabet alphabet{alphabet_symbols(aut, nullptr)};
		Run counterexample{};
		holds = aut.is_universal(alphabet, &counterexample);
		if (!*holds) { witness = counterexample.word; }
	} else if (operation == "is_included" || operation == "are_equivalent") {
		const Nfa lhs{without_epsilon(a.nfa, "a")};
		const Nfa rhs{without_epsilon(b->nfa, "b")};
		const EnumAlphabet alphabet{alphabet_symbols(lhs, &rhs)};
		Run counterexample{};
		if (operation == "is_included") {
			holds = nfa::is_included(lhs, rhs, &counterexample, &alphabet);
		} else {
			holds = nfa::are_equivalent(lhs, rhs, &alphabet, {{"algorithm", "antichains"}}, &counterexample);
			if (!*holds) { witness_in_a = lhs.is_in_lang(counterexample.word); }
		}
		if (!*holds) { witness = counterexample.word; }
	} else if (operation == "is_in_lang") {
		Word word{};
		std::string_view names{request_.word};
		while (!names.empty()) {
			const size_t end{names.find('\n')};
			if (const std::string_view name{names.substr(0, end)}; !name.empty()) {
				word.push_back(alphabet_.translate_symb(std::string{name}));
			}
			names.remove_prefix(end == std::string_view::npos ? names.size() : end + 1);
		}
		holds = a.nfa.is_in_lang(word, has_epsilon(a.nfa));
	} else {
		throw InputError{"operation", "Unknown operation \"" + operation + "\"."};
	}
	const std::chrono::duration<double, std::milli> elapsed{std::chrono::steady_clock::now() - start};

	std::string out{R"({"ok":true,"automata":{"a":)"};
	append_automaton(out, a.nfa, a.state_names);
	if (b) {
		out += R"(,"b":)";
		append_automaton(out, b->nfa, b->state_names);
	}
	if (result) {
		out += R"(,"result":)";
		append_automaton(out, *result, {});
	}
	out += '}';
	if (holds) {
		out += R"(,"check":{"holds":)";
		out += *holds ? "true" : "false";
		out += R"(,"word":)";
		if (witness) {
			append_word(out, *witness);
		} else {
			out += "null";
		}
		out += '}';
	}
	if (witness_in_a) {
		out += R"(,"witnessInA":)";
		out += *witness_in_a ? "true" : "false";
	}
	out += R"(,"epsilonRemoved":[)";
	for (size_t i{0}; i < epsilon_removed_.size(); ++i) {
		if (i > 0) { out += ','; }
		append_json_string(out, epsilon_removed_[i]);
	}
	out += R"(],"symbols":[)";
	for (size_t i{0}; i < symbols_.size(); ++i) {
		if (i > 0) { out += ','; }
		append_json_string(out, alphabet_.reverse_translate_symbol(symbols_[i]));
	}
	out += R"(],"milliseconds":)";
	out += std::to_string(elapsed.count());
	out += '}';
	return out;
}

} // namespace

std::string evaluate(const Request& request) {
	try {
		return Evaluation{request}.run();
	} catch (const InputError& error) {
		return error_json(error.source(), error.what());
	} catch (const std::bad_alloc&) {
		return error_json("operation", "The computation ran out of memory.");
	} catch (const std::exception& error) { return error_json("operation", error.what()); }
}

std::string revision() { return MATA_PLAYGROUND_REVISION; }

} // namespace mata::playground
