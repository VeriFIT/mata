/* parser.cc -- implementation of VTF format parser
 */

#include "mata/parser/parser.hh"
#include "mata/utils/utils.hh"

#include <algorithm>
#include <cstring>
#include <sstream>

using std::tie;

using mata::parser::Parsed;
using mata::parser::ParsedSection;
using mata::utils::haskey;

// macro for debug prints in the parser
// #define PARSER_DEBUG_PRINT_LN(x) { DEBUG_PRINT_LN(x) }
#define PARSER_DEBUG_PRINT_LN(x) {  }


namespace
{
/** Eats all whitespaces in the input stream */
void eat_whites(std::istream& input)
{ // {{{
	while (input.good())
	{
		int ch = input.peek();
		if (!std::isspace(ch)) { return; }
		input.get();
	}
} // eat_whites(istream) }}}

/// Determines whether the character is a character of a string
bool is_string_char(char ch)
{ // {{{
	return !std::isblank(ch) &&
		!(haskey(std::set<char>{'"', '(', ')', '#', '%', '@', '\\'}, ch));
} // is_string_char }}}

bool is_logical_operator(char ch)
{
    return (haskey(std::set<char>{'&', '|', '!'}, ch));
}

/**
 * @brief  Gets a token from the input stream
 *
 * The function assumes that the stream does not span lines
 */
std::string get_token_from_line(std::istream& input, bool* quoted) {
	assert(nullptr != quoted);

	enum class TokenizerState {
		INIT,
		UNQUOTED,
		QUOTED,
		QUOTED_ESCAPE
	};

	std::string result;
	*quoted = false;
	TokenizerState state = TokenizerState::INIT;
	int ch = input.get();
	while (input.good()) {
		assert(std::char_traits<char>::eof() != ch);

		switch (state) {
			case TokenizerState::INIT: {
				if (std::isspace(ch)) { /* do nothing */ }
				else if ('"' == ch) {
					state = TokenizerState::QUOTED;
					*quoted = true;
				} else if ('#' == ch) { // clear the rest of the line
					std::string aux;
					std::getline(input, aux);
					return {};
				} else if ('(' == ch || ')' == ch) {
					return std::to_string(static_cast<char>(ch));
				} else {
					result += static_cast<char>(ch);
					state = TokenizerState::UNQUOTED;
				}
				break;
			}
			case TokenizerState::UNQUOTED: {
				if (std::isspace(ch)) { return result; }
				else if ('#' == ch) { // clear the rest of the line
					std::string aux;
					std::getline(input, aux);
					return result;
				} else if ('"' == ch) {
					std::string context;
					std::getline(input, context);
					throw std::runtime_error("misplaced quotes: " + result + "_\"_" + context);
				} else if ('(' == ch || ')' == ch) {
					input.unget();
					return result;
				} else if ('@' == ch || '%' == ch) {
					std::string context;
					std::getline(input, context);
					throw std::runtime_error(std::to_string("misplaced character \'") +
						static_cast<char>(ch) + "\' in string \"" + result +
						static_cast<char>(ch) + context + "\"");
				} else {
					result += static_cast<char>(ch);
				}
				break;
			}
			case TokenizerState::QUOTED: {
				if ('"' == ch) {
					ch = input.peek();
					if (!std::isspace(ch) && ('#' != ch) && (')' != ch) &&
						std::char_traits<char>::eof() != ch)
					{
						std::string context;
						std::getline(input, context);
						throw std::runtime_error("misplaced quotes: \"" + result + "_\"_" +
							context);
					}

					return result;
				} else if ('\\' == ch) {
					state = TokenizerState::QUOTED_ESCAPE;
				} else { result += static_cast<char>(ch); }
				break;
			}
			case TokenizerState::QUOTED_ESCAPE: {
				if ('"' != ch) { result += '\\'; }
				result += static_cast<char>(ch);
				state = TokenizerState::QUOTED;
				break;
			}
			default: { // LCOV_EXCL_START
				throw std::runtime_error("Invalid tokenizer state");
			} // LCOV_EXCL_STOP
		}

		ch = input.get();
	}

	if (TokenizerState::QUOTED == state || TokenizerState::QUOTED_ESCAPE == state) {
		throw std::runtime_error("missing ending quotes: " + result);
	}

	return result;
} // get_token_new(istream) }}}


/**
 * @brief  Transforms a line into a vector of tokens
 */
std::vector<std::pair<std::string, bool>> tokenize_line(const std::string& line)
{ // {{{
	std::vector<std::pair<std::string, bool>> result;
	std::istringstream stream(line);
	bool first = true;
	while (stream.good())
	{
		bool quoted;
		std::string token = get_token_from_line(stream, &quoted);
		if (!quoted && token.empty()) { break; }

		result.emplace_back(token, quoted);

		if (!first && !quoted)
		{
			assert(!token.empty());
			if ('@' == token[0])
			{
				throw std::runtime_error("invalid position of @TYPE: " + line);
			}
			else if ('%' == token[0])
			{
				throw std::runtime_error("invalid position of %KEY: " + line);
			}
		}

		first = false;
	}

	return result;
} // tokenize_line(string) }}}

std::vector<std::pair<std::string, bool>> split_tokens(std::vector<std::pair<std::string, bool>> tokens) {
    std::vector<std::pair<std::string, bool>> result;
    for (auto& token: tokens) {
        if (token.second) { // is quoted?
            result.push_back(std::move(token));
            continue;
        }

        const std::string_view token_string{ token.first };
        size_t last_operator{ 0 };
        for (size_t i = 0, token_string_size{ token_string.size() }; i < token_string_size; ++i) {
            if (is_logical_operator(token_string[i])) {
                const std::string_view token_candidate = token_string.substr(last_operator, i - last_operator);

                // there is token before logical operator (this is case of binary operators, e.g., a&b)
                if (!token_candidate.empty()) {
                    result.emplace_back(token_candidate, false);
								}
                result.emplace_back(std::string(1,token_string[i]), false);
                last_operator = i + 1;
            }
        }

				const size_t length{ token_string.length() };
        if (last_operator == 0) {
            result.emplace_back(std::move(token));
        } else if (last_operator != length) { // operator was not last, we need parse rest of token
            result.emplace_back(token_string.substr(last_operator, length-last_operator), false);
        }
    }

    return result;
} // split_tokens(std::vector) }}}
} // anonymous namespace


Parsed mata::parser::parse_mf(
	const std::string&  input,
	bool                keepQuotes)
{ // {{{
	std::istringstream stream(input);
	return parse_mf(stream, keepQuotes);
} // parse_mf(std::string) }}}


Parsed mata::parser::parse_mf(
	std::istream&  input,
	bool           keepQuotes)
{ // {{{
	Parsed result;

	while (input.good()) {
		ParsedSection parsec = parse_mf_section(input, keepQuotes);
		if (!parsec.empty())
		{
			result.push_back(parsec);
		}
	}

	return result;
} // parse_mf(std::istream) }}}


ParsedSection mata::parser::parse_mf_section(
	std::istream&  input,
	bool           keepQuotes)
{ // {{{
	ParsedSection result;

	bool reading_type = true;
	std::vector<std::pair<std::string, bool>> token_line{};
	bool append_line = false;

	while (input.good()) {
		eat_whites(input);
		int ch = input.peek();
		if (std::char_traits<char>::eof() == ch) { break; }
		else if (!reading_type && '@' == ch)
		{ // another @TYPE declaration
			break;
		}

		std::string line;
		getline(input, line);

		bool backslash_ending = (line.back() == '\\');
		if (backslash_ending) {
		    line.pop_back();
		}

		PARSER_DEBUG_PRINT_LN(line);

		if (reading_type) { // we're expecting a @TYPE declaration
			assert(ch == line[0]);
			if ('#' == line[0] || '\n' == line[0]) {
				continue; /* skip the rest of the line */
			} else if ('@' != line[0]) {
				throw std::runtime_error("expecting automaton type (@TYPE), got \"" +
					line + "\" instead");
			}

			std::string type;
			size_t i;
			for (i = 1; i < line.size(); ++i) {
				if (!is_string_char(line[i])) {
					break;
				}

				type += line[i];
			}

			if (type.empty()) {
				throw std::runtime_error("expecting automaton type (@TYPE), got \"" +
					line + "\" instead");
			}

			while (i < line.size()) {
				if (std::isspace(line[i])) {
					++i;
				} else if ('#' == line[i]) {
					break;
				} else {
					std::string trailing(line, i);
					throw std::runtime_error("invalid trailing characters \"" +
						trailing + "\" on the line \"" + line + "\"");
				}
			}

			result.type = type;
			reading_type = false;
			continue;
		}

		if (result.type == "Regex") // so far we do not support regexs
			continue;

		std::vector<std::pair<std::string, bool>> temp_token_line = tokenize_line(line);
		if (temp_token_line.empty()) {
			continue;
		}

		if (append_line) {
		    token_line.insert(token_line.end(), temp_token_line.begin(), temp_token_line.end());
		} else {
		    token_line = temp_token_line;
		}

		append_line = backslash_ending;
		if (append_line) {
		    continue;
		}

		token_line = split_tokens(std::move(token_line));

		const std::string& maybe_key = token_line[0].first;
		const bool& quoted = token_line[0].second;
		assert(quoted || '@' != maybe_key[0]);

		if (!quoted && '%' == maybe_key[0]) {
			std::string key = maybe_key.substr(1);
			if (key.empty()) {
				throw std::runtime_error("%KEY name missing: " + line);
			}

			auto it = result.dict.find(key);
			if (result.dict.end() == it) { // insert an empty list
				tie(it, std::ignore) =
					result.dict.insert({ key, std::vector<std::string>() });
			}

			std::vector<std::string>& val_list = it->second;
			std::transform(token_line.begin() + 1, token_line.end(),
				std::back_inserter(val_list),
				[](const std::pair<std::string, bool>& token) { return token.first; });
		} else {
			BodyLine stripped_token_line;
			std::transform(token_line.begin(), token_line.end(),
				std::back_inserter(stripped_token_line),
				[&](const std::pair<std::string, bool>& token) {
					if (keepQuotes && token.second) return "\"" + token.first + "\"";
					else return token.first;
				});
			result.body.push_back(stripped_token_line);
		}
	}

	return result;
} // parse_mf_section(std::istream) }}}


ParsedSection mata::parser::parse_mf_section(
	const std::string&  input,
	bool                keepQuotes)
{ // {{{
	std::istringstream stream(input);
	ParsedSection result = parse_mf_section(stream, keepQuotes);
	return result;
} // parse_mf_section(std::string) }}}

const std::vector<std::string>& mata::parser::ParsedSection::operator[](const std::string& key) const {
    auto it = this->dict.find(key);
    if (this->dict.end() == it) {
        assert(false);
        // return this->dict.at("");
    }

    return it->second;
}

bool mata::parser::ParsedSection::operator==(const ParsedSection& rhs) const {
    return
        this->type == rhs.type &&
        this->dict == rhs.dict &&
        this->body == rhs.body;
}

std::ostream& std::operator<<(std::ostream& os, const ParsedSection& parsec) {
    os << "@" << parsec.type << "\n";
    for (const auto& string_list_pair : parsec.dict) {
        os << "%" << string_list_pair.first;
        for (const std::string& str : string_list_pair.second) {
            os << " " << str;
        }
        os << "\n";
    }

    os << "# Body:\n";
    for (const auto& body_line : parsec.body) {
        bool first = true;
        for (const std::string& str : body_line) {
            if (!first) { os << " ";}
            first = false;
            os << str;
        }
        os << "\n";
    }
    return os;
}
