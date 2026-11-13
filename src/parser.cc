/** @file
 * @brief MATA format parser implementation.
 */

#include "mata/parser/parser.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/utils.hh"

#include "explicit-section-reader.hh"

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <span>
#include <sstream>

using mata::parser::Parsed;
using mata::parser::ParsedSection;

namespace {
/// @c std::isspace() in the "C" locale.
bool is_space(const char ch) { return ch == ' ' || (ch >= '\t' && ch <= '\r'); }

/// One token of a line: a view into the input, or into @c LineTokens::unescaped for a quoted token with escapes.
struct Token {
	std::string_view text;
	bool quoted;
};

/**
 * The tokens of a line and of the lines a backslash continues it with. Unquoted tokens are split at the logical
 *  operators of a transition formula. The buffers are reused from line to line, so reading a line usually allocates
 *  nothing.
 */
class LineTokens {
  public:
	const std::vector<Token>& tokens() const { return tokens_; }

	void clear() {
		tokens_.clear();
		unescaped_.clear();
	}

	/**
	 * Appends the tokens of @p line, which has to outlive them. A comment ends the line: everything from @c # on is
	 *  dropped.
	 */
	void tokenize(const std::string_view line) {
		size_t pos{0};
		bool first{true};
		while (true) {
			while (pos < line.size() && is_space(line[pos])) { ++pos; }
			if (pos == line.size() || line[pos] == '#') { return; }
			if (const char ch{line[pos]}; ch == '(' || ch == ')') {
				tokens_.push_back({line.substr(pos++, 1), false});
			} else if (ch == '"') {
				read_quoted(line, pos);
			} else {
				read_unquoted(line, pos, first);
			}
			first = false;
		}
	}

  private:
	/// Reads the quoted token at @p pos. Only a quote is escaped; any other character keeps its backslash.
	void read_quoted(const std::string_view line, size_t& pos) {
		const size_t start{++pos};
		while (pos < line.size() && line[pos] != '"' && line[pos] != '\\') { ++pos; }
		std::string_view token{line.substr(start, pos - start)};
		if (pos < line.size() && line[pos] == '\\') {
			std::string result{token};
			while (pos < line.size() && line[pos] != '"') {
				if (line[pos] == '\\' && pos + 1 < line.size()) {
					if (line[++pos] != '"') { result += '\\'; }
				}
				result += line[pos++];
			}
			token = unescaped_.emplace_back(std::move(result));
		}
		if (pos == line.size()) { throw std::runtime_error("missing ending quotes: " + std::string{token}); }
		++pos;
		if (pos < line.size() && !is_space(line[pos]) && line[pos] != '#' && line[pos] != ')') {
			std::string msg = "misplaced quotes: \"";
			msg += token;
			msg += "_\"_";
			msg += line.substr(pos);
			throw std::runtime_error(msg);
		}
		tokens_.push_back({token, true});
	}

	/// Reads the unquoted token at @p pos, which may not contain quotes, @, or % inside; the first character may be @
	///  or %, unless the token is not the @p first one of the line.
	void read_unquoted(const std::string_view line, size_t& pos, const bool first) {
		const size_t start{pos};
		// The start of the part of the token after the last operator.
		size_t piece{pos};
		if (line[pos] == '@' || line[pos] == '%') { ++pos; }
		for (; pos < line.size(); ++pos) {
			const CharKind kind{CHAR_KINDS[static_cast<unsigned char>(line[pos])]};
			if (kind == CharKind::Plain) { continue; }
			if (kind == CharKind::End) { break; }
			if (kind == CharKind::Misplaced) { throw_misplaced(line, start, pos); }
			// One of the logical operators of a transition formula, a token of its own.
			if (pos > piece) { tokens_.push_back({line.substr(piece, pos - piece), false}); }
			tokens_.push_back({line.substr(pos, 1), false});
			piece = pos + 1;
		}
		if (!first) {
			if ('@' == line[start]) { throw std::runtime_error("invalid position of @TYPE: " + std::string(line)); }
			if ('%' == line[start]) { throw std::runtime_error("invalid position of %KEY: " + std::string(line)); }
		}
		if (pos > piece) { tokens_.push_back({line.substr(piece, pos - piece), false}); }
	}

	/// Throws the error of the misplaced character at @p pos of the unquoted token at @p start.
	[[noreturn]] static void throw_misplaced(const std::string_view line, const size_t start, const size_t pos) {
		const char c{line[pos]};
		std::string msg;
		if (c == '"') {
			msg = "misplaced quotes: ";
			msg += line.substr(start, pos - start);
			msg += "_\"_";
			msg += line.substr(pos);
		} else {
			msg = "misplaced character '";
			msg += c;
			msg += "' in string \"";
			msg += line.substr(start, pos - start);
			msg += c;
			msg += line.substr(pos + 1);
			msg += "\"";
		}
		throw std::runtime_error(msg);
	}

	/// How an unquoted token reads a character.
	enum class CharKind : uint8_t { Plain, End, Misplaced, Operator };
	static constexpr std::array<CharKind, 256> CHAR_KINDS{[] {
		std::array<CharKind, 256> kinds{};
		for (const char c : {' ', '\t', '\n', '\v', '\f', '\r', '#', '(', ')'}) {
			kinds[static_cast<unsigned char>(c)] = CharKind::End;
		}
		for (const char c : {'"', '@', '%'}) { kinds[static_cast<unsigned char>(c)] = CharKind::Misplaced; }
		for (const char c : {'&', '|', '!'}) { kinds[static_cast<unsigned char>(c)] = CharKind::Operator; }
		return kinds;
	}()};

	std::vector<Token> tokens_{};
	/// The quoted tokens with their escapes resolved; a deque keeps the strings, and so the views of the tokens, in
	///  place.
	std::deque<std::string> unescaped_{};
};
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
	LineTokens line_tokens{};
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

		if (!append_line) { line_tokens.clear(); }
		const size_t num_of_tokens_before{line_tokens.tokens().size()};
		line_tokens.tokenize(line);
		if (line_tokens.tokens().size() == num_of_tokens_before) { continue; }

		append_line = backslash_ending;
		if (append_line) { continue; }

		const std::vector<Token>& tokens{line_tokens.tokens()};
		const bool quoted{tokens[0].quoted};
		// An unquoted token is never empty.
		MATA_ASSERT(quoted || '@' != tokens[0].text[0]);

		if (!quoted && '%' == tokens[0].text[0]) {
			const std::string key{tokens[0].text.substr(1)};
			if (key.empty()) { throw std::runtime_error("%KEY name missing: " + std::string(line)); }

			std::vector<std::string>& val_list = result.dict[key];
			for (const Token& token : std::span{tokens}.subspan(1)) { val_list.emplace_back(token.text); }
		} else {
			BodyLine& body_line{result.body.emplace_back()};
			body_line.reserve(tokens.size());
			for (const auto& [token, token_quoted] : tokens) {
				if (keep_quotes && token_quoted) {
					std::string with_quotes;
					with_quotes.reserve(token.size() + 2);
					with_quotes += '"';
					with_quotes += token;
					with_quotes += '"';
					body_line.emplace_back(std::move(with_quotes), true);
				} else {
					body_line.emplace_back(std::string{token}, token_quoted);
				}
			}
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
