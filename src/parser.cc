/** @file
 * @brief MATA format parser implementation.
 */

#include "mata/parser/parser.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/utils.hh"

#include "explicit-section-reader.hh"

#include <algorithm>
#include <cstdint>
#include <sstream>

using std::tie;

using mata::parser::Parsed;
using mata::parser::ParsedSection;

namespace {
/// @c std::isspace() in the "C" locale.
bool is_space(const char ch) { return ch == ' ' || (ch >= '\t' && ch <= '\r'); }

/// One token of a line, as @c get_token_from_line() produced it.
using Token = std::pair<std::string, bool>;

/**
 * @brief Gets a token from the line, mirroring @c get_token_from_line() of the stream tokenizer.
 *
 * @param pos The position to read from, advanced past the token and the white space that follows it.
 * @param quoted Receives whether the token was read in quotes.
 * @return The token, empty when the line ends. A comment ends the line: everything from @c # on is dropped.
 */
Token get_token_from_line(const std::string_view line, size_t& pos, bool* quoted) {
	MATA_ASSERT(nullptr != quoted);
	*quoted = false;
	std::string result;
	while (true) {
		while (pos < line.size() && is_space(line[pos])) { ++pos; }
		if (pos == line.size()) { return {}; }

		const char ch{line[pos]};
		if (ch == '#') { // clear the rest of the line
			pos = line.size();
			return {};
		}
		if (ch == '(' || ch == ')') {
			++pos;
			while (pos < line.size() && is_space(line[pos])) { ++pos; }
			return {std::string(1, ch), false};
		}
		if (ch == '"') {
			*quoted = true;
			++pos;
			while (pos < line.size() && line[pos] != '"') {
				if (line[pos] == '\\' && pos + 1 < line.size()) {
					// Only a quote is escaped; any other character keeps its backslash.
					if (line[++pos] != '"') { result += '\\'; }
				}
				result += line[pos++];
			}
			if (pos == line.size()) { throw std::runtime_error("missing ending quotes: " + result); }
			++pos;
			if (pos < line.size() && !is_space(line[pos]) && line[pos] != '#' && line[pos] != ')') {
				std::string msg = "misplaced quotes: \"";
				msg += result;
				msg += "_\"_";
				msg += line.substr(pos);
				throw std::runtime_error(msg);
			}
			while (pos < line.size() && is_space(line[pos])) { ++pos; }
			return {std::move(result), true};
		}

		// An unquoted token, which may not contain quotes, @, or % inside; the first character may be @ or %.
		const size_t start{pos};
		do {
			const char c{line[pos]};
			if (is_space(c) || c == '#' || c == '(' || c == ')') { break; }
			if (c == '"' || c == '@' || c == '%') {
				if (pos == start) {
					result += c;
					++pos;
					continue;
				}
				if (c == '"') {
					std::string msg = "misplaced quotes: ";
					msg += line.substr(start, pos - start);
					msg += "_\"_";
					msg += line.substr(pos);
					throw std::runtime_error(msg);
				}
				std::string msg = "misplaced character '";
				msg += c;
				msg += "' in string \"";
				msg += line.substr(start, pos - start);
				msg += c;
				msg += line.substr(pos + 1);
				msg += "\"";
				throw std::runtime_error(msg);
			}
			result += c;
			++pos;
		} while (pos < line.size());
		while (pos < line.size() && is_space(line[pos])) { ++pos; }
		return {std::move(result), false};
	}
}

/**
 * @brief Transforms a line into a vector of tokens
 */
std::vector<Token> tokenize_line(const std::string_view line) {
	std::vector<Token> token_line;
	bool first = true;
	size_t pos = 0;
	while (true) {
		Token token{get_token_from_line(line, pos, &token_line.emplace_back().second)};
		if (!token.second && token.first.empty()) {
			token_line.pop_back();
			break;
		}
		token_line.back().first = std::move(token.first);

		if (!first && !token_line.back().second) {
			MATA_ASSERT(!token_line.back().first.empty());
			if ('@' == token_line.back().first[0]) {
				throw std::runtime_error("invalid position of @TYPE: " + std::string(line));
			}
			if ('%' == token_line.back().first[0]) {
				throw std::runtime_error("invalid position of %KEY: " + std::string(line));
			}
		}
		first = false;
		if (pos >= line.size()) { break; }
	}

	return token_line;
}

std::vector<std::pair<std::string, bool>> split_tokens(std::vector<std::pair<std::string, bool>> tokens) {
	std::vector<std::pair<std::string, bool>> result;
	result.reserve(tokens.size());
	for (auto& token : tokens) {
		if (token.second) { // is quoted?
			result.push_back(std::move(token));
			continue;
		}

		const std::string_view token_string{token.first};
		size_t last_operator{0};
		for (size_t i = 0, token_string_size{token_string.size()}; i < token_string_size; ++i) {
			// One of the logical operators of a transition formula.
			if (token_string[i] == '&' || token_string[i] == '|' || token_string[i] == '!') {
				// there is token before logical operator (this is case of binary operators, e.g., a&b)
				if (const std::string_view token_candidate = token_string.substr(last_operator, i - last_operator);
					!token_candidate.empty()) {
					result.emplace_back(token_candidate, false);
				}
				result.emplace_back(std::string(1, token_string[i]), false);
				last_operator = i + 1;
			}
		}

		const size_t length{token_string.length()};
		if (last_operator == 0) {
			result.emplace_back(std::move(token));
		} else if (last_operator != length) {
			// operator was not last, we need parse rest of token
			result.emplace_back(token_string.substr(last_operator, length - last_operator), false);
		}
	}

	return result;
}
} // namespace

Parsed mata::parser::parse_mf(const std::string& input, const bool keep_quotes) {
	Parsed result;
	std::string_view view{input};
	while (!view.empty()) {
		if (ParsedSection parsec{parse_mf_section_view(view, keep_quotes)}; !parsec.empty()) {
			result.push_back(std::move(parsec));
		}
	}
	return result;
}

Parsed mata::parser::parse_mf(std::istream& input, const bool keep_quotes) {
	return parse_mf(read_whole_stream(input), keep_quotes);
}

ParsedSection mata::parser::parse_mf_section(std::istream& input, const bool keep_quotes) {
	// Gather the lines of this section only, so that the stream stays at the next @TYPE line, as before.
	std::string text;
	bool type_seen = false;
	std::string line;
	while (input.good()) {
		while (input.good() && std::isspace(input.peek())) { input.get(); }
		if (!input.good() || std::char_traits<char>::eof() == input.peek()) { break; }
		if (type_seen && '@' == input.peek()) { break; }
		std::getline(input, line);
		if (!type_seen && !line.empty() && '#' != line[0]) { type_seen = true; }
		text += line;
		text += '\n';
	}
	std::string_view view{text};
	return parse_mf_section_view(view, keep_quotes);
}

ParsedSection mata::parser::parse_mf_section(const std::string& input, const bool keep_quotes) {
	std::string_view view{input};
	return parse_mf_section_view(view, keep_quotes);
}

ParsedSection mata::parser::parse_mf_section_view(std::string_view& input, const bool keep_quotes) {
	ParsedSection result;

	bool reading_type = true;
	std::vector<std::pair<std::string, bool>> token_line{};
	bool append_line = false;
	size_t pos = 0;

	while (pos < input.size()) {
		while (pos < input.size() && is_space(input[pos])) { ++pos; }
		if (pos == input.size()) { break; }
		if (!reading_type && '@' == input[pos]) {
			// another @TYPE declaration
			break;
		}

		const size_t line_end{std::min(input.find('\n', pos), input.size())};
		std::string_view line{input.substr(pos, line_end - pos)};
		pos = line_end + (line_end < input.size() ? 1 : 0);

		// getline drops the delimiter, so an empty line stays empty: line.back() would read out of bounds.
		// Type lines are never continuation lines, so they keep the backslash and fail on the trailing characters.
		const bool backslash_ending = !line.empty() && line.back() == '\\' && !reading_type;
		if (backslash_ending) { line.remove_suffix(1); }

		if (reading_type) { // we're expecting a @TYPE declaration
			if ('#' == line[0] || '\n' == line[0]) { continue; /* skip the rest of the line */ }
			if ('@' != line[0]) {
				throw std::runtime_error("expecting automaton type (@TYPE), got \"" + std::string(line) + "\" instead");
			}

			std::string type;
			size_t i;
			for (i = 1; i < line.size(); ++i) {
				// A character of the type name, as is_string_char() of the stream tokenizer read it.
				if (line[i] == '"' || line[i] == '(' || line[i] == ')' || line[i] == '#' || line[i] == '%' ||
					line[i] == '@' || line[i] == '\\' || line[i] == ' ' || line[i] == '\t') {
					break;
				}
				type += line[i];
			}

			if (type.empty()) {
				throw std::runtime_error("expecting automaton type (@TYPE), got \"" + std::string(line) + "\" instead");
			}

			while (i < line.size()) {
				if (std::isspace(line[i])) {
					++i;
				} else if ('#' == line[i]) {
					break;
				} else {
					std::string msg = "invalid trailing characters \"";
					msg += line.substr(i);
					msg += "\" on the line \"";
					msg += line;
					msg += "\"";
					throw std::runtime_error(msg);
				}
			}

			result.type = type;
			reading_type = false;
			continue;
		}

		if (result.type == "Regex") { // so far we do not support regexs
			continue;
		}

		std::vector<std::pair<std::string, bool>> temp_token_line = tokenize_line(line);
		if (temp_token_line.empty()) { continue; }

		if (append_line) {
			token_line.insert(
				token_line.end(), std::make_move_iterator(temp_token_line.begin()),
				std::make_move_iterator(temp_token_line.end())
			);
		} else {
			token_line = std::move(temp_token_line);
		}

		append_line = backslash_ending;
		if (append_line) { continue; }

		token_line = split_tokens(std::move(token_line));

		const std::string& maybe_key = token_line[0].first;
		const bool& quoted = token_line[0].second;
		MATA_ASSERT(quoted || '@' != maybe_key[0]);

		if (!quoted && '%' == maybe_key[0]) {
			const std::string key = maybe_key.substr(1);
			if (key.empty()) { throw std::runtime_error("%KEY name missing: " + std::string(line)); }

			auto it = result.dict.find(key);
			if (result.dict.end() == it) { // insert an empty list
				tie(it, std::ignore) = result.dict.insert({key, std::vector<std::string>()});
			}

			std::vector<std::string>& val_list = it->second;
			std::transform(
				std::make_move_iterator(token_line.begin() + 1), std::make_move_iterator(token_line.end()),
				std::back_inserter(val_list),
				[](std::pair<std::string, bool>&& token) { return std::move(token.first); }
			);
		} else {
			BodyLine stripped_token_line;
			stripped_token_line.reserve(token_line.size());
			for (auto& [token, token_quoted] : token_line) {
				if (keep_quotes && token_quoted) { token = "\"" + token + "\""; }
				stripped_token_line.emplace_back(std::move(token), token_quoted);
			}
			result.body.push_back(std::move(stripped_token_line));
		}
	}

	input.remove_prefix(pos);
	return result;
} // parse_mf_section_view }}}

const std::vector<std::string>& mata::parser::ParsedSection::operator[](const std::string& key) const {
	const auto it = this->dict.find(key);
	if (this->dict.end() == it) { throw std::runtime_error("Key '" + key + "' not found in parsed section"); }
	return it->second;
}

bool mata::parser::ParsedSection::operator==(const ParsedSection& rhs) const {
	return this->type == rhs.type && this->dict == rhs.dict && this->body == rhs.body;
}

std::ostream& std::operator<<(std::ostream& os, const ParsedSection& parsec) {
	os << "@" << parsec.type << "\n";
	for (const auto& [string, strings] : parsec.dict) {
		os << "%" << string;
		for (const std::string& str : strings) { os << " " << str; }
		os << "\n";
	}

	os << "# Body:\n";
	for (const auto& body_line : parsec.body) {
		bool first = true;
		for (const auto& token : body_line) {
			if (!first) { os << " "; }
			first = false;
			os << token.token;
		}
		os << "\n";
	}
	return os;
}
