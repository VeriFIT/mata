// TODO: some header

#include <vata2/parser.hh>
#include <vata2/util.hh>

#include <algorithm>
#include <cstring>
#include <sstream>

using std::tie;

using Vata2::Parser::Parsed;
using Vata2::Parser::ParsedSection;
using Vata2::util::haskey;

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


/**
 * @brief  Gets a token from the input stream
 *
 * The function assumes that the stream does not span lines
 */
std::string get_token_from_line(std::istream& input, bool* quoted)
{ // {{{
	assert(nullptr != quoted);

	enum class TokenizerState
	{
		INIT,
		UNQUOTED,
		QUOTED,
		QUOTED_ESCAPE
	};

	std::string result;
	*quoted = false;
	TokenizerState state = TokenizerState::INIT;
	int ch = input.get();
	while (input.good())
	{
		assert(std::char_traits<char>::eof() != ch);

		switch (state)
		{
			case TokenizerState::INIT:
			{
				if (std::isspace(ch))
				{ /* do nothing */ }
				else if ('"' == ch)
				{
					state = TokenizerState::QUOTED;
					*quoted = true;
				}
				else if ('#' == ch)
				{ // clear the rest of the line
					std::string aux;
					std::getline(input, aux);
					return std::string();
				}
				else if ('(' == ch || ')' == ch)
				{
					return std::to_string(static_cast<char>(ch));
				}
				else
				{
					result += ch;
					state = TokenizerState::UNQUOTED;
				}
				break;
			}
			case TokenizerState::UNQUOTED:
			{
				if (std::isspace(ch)) { return result; }
				else if ('#' == ch)
				{ // clear the rest of the line
					std::string aux;
					std::getline(input, aux);
					return result;
				}
				else if ('"' == ch)
				{
					std::string context;
					std::getline(input, context);
					throw std::runtime_error("misplaced quotes: " + result + "_\"_" +
						context);
				}
				else if ('(' == ch || ')' == ch)
				{
					input.unget();
					return result;
				}
				else if ('@' == ch || '%' == ch)
				{
					std::string context;
					std::getline(input, context);
					throw std::runtime_error(std::to_string("misplaced character \'") +
						static_cast<char>(ch) + "\' in string \"" + result +
						static_cast<char>(ch) + context + "\"");
				}
				else
				{
					result += ch;
				}
				break;
			}
			case TokenizerState::QUOTED:
			{
				if ('"' == ch)
				{
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
				}
				else if ('\\' == ch)
				{
					state = TokenizerState::QUOTED_ESCAPE;
				}
				else
				{
					result += ch;
				}
				break;
			}
			case TokenizerState::QUOTED_ESCAPE:
			{
				if ('"' != ch)
				{
					result += '\\';
				}
				result += ch;
				state = TokenizerState::QUOTED;
				break;
			}
			default:
			{
				throw std::runtime_error("Invalid tokenizer state");
			}
		}

		ch = input.get();
	}

	if (TokenizerState::QUOTED == state || TokenizerState::QUOTED_ESCAPE == state)
	{
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

		result.push_back({ token, quoted });

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
} // tokenize_lin(string) }}}
} // anonymous namespace


Parsed Vata2::Parser::parse_vtf(const std::string& input)
{ // {{{
	std::istringstream stream(input);
	return parse_vtf(stream);
} // parse_vtf(std::string) }}}


Parsed Vata2::Parser::parse_vtf(std::istream& input)
{ // {{{
	Parsed result;

	while (input)
	{
		ParsedSection parsec = parse_vtf_section(input);
		if (!parsec.empty())
		{
			result.push_back(parsec);
		}
	}

	return result;
} // parse_vtf(std::istream) }}}


ParsedSection Vata2::Parser::parse_vtf_section(std::istream& input)
{ // {{{
	ParsedSection result;

	bool reading_type = true;

	while (input.good())
	{
		eat_whites(input);
		int ch = input.peek();
		if (std::char_traits<char>::eof() == ch)
		{
			break;
		}
		else if (!reading_type && '@' == ch)
		{ // another @TYPE declaration
			break;
		}

		std::string line;
		getline(input, line);

		PARSER_DEBUG_PRINT_LN(line);

		if (reading_type)
		{ // we're expecting a @TYPE declaration
			assert(ch == line[0]);
			if ('#' == line[0] || '\n' == line[0])
			{ continue; /* skip the rest of the line */ }
			else if ('@' != line[0])
			{
				throw std::runtime_error("expecting automaton type (@TYPE), got \"" +
					line + "\" instead");
			}

			std::string type;
			size_t i;
			for (i = 1; i < line.size(); ++i)
			{
				if (!is_string_char(line[i])) { break; }

				type += line[i];
			}

			if (type.empty())
			{
				throw std::runtime_error("expecting automaton type (@TYPE), got \"" +
					line + "\" instead");
			}

			while (i < line.size())
			{
				if (std::isspace(line[i])) { ++i; }
				else if ('#' == line[i]) { break; }
				else
				{
					std::string trailing(line, i);
					throw std::runtime_error("invalid trailing characters \"" +
						trailing + "\" on the line \"" + line + "\"");
				}
			}

			result.type = type;
			reading_type = false;
			continue;
		}
		else if (!reading_type && '@' == ch)
		{ // next type declaration
			return result;
		}

		std::vector<std::pair<std::string, bool>> token_line = tokenize_line(line);
		if (token_line.empty()) continue;

		const std::string& maybe_key = token_line[0].first;
		const bool& quoted = token_line[0].second;
		if (!quoted && '%' == maybe_key[0])
		{
			std::string key = maybe_key.substr(1);
			if (key.empty())
			{
				throw std::runtime_error("%KEY name missing: " + line);
			}

			auto it = result.dict.find(key);
			if (result.dict.end() == it)
			{ // insert an empty list
				tie(it, std::ignore) =
					result.dict.insert({ key, std::vector<std::string>() });
			}

			std::vector<std::string>& val_list = it->second;
			std::transform(token_line.begin() + 1, token_line.end(),
				std::back_inserter(val_list),
				[](const std::pair<std::string, bool> token) { return token.first; });
		}
		else if (!quoted && '@' == maybe_key[0])
		{
			assert(false);
		}
		else
		{
			BodyLine stripped_token_line;
			std::transform(token_line.begin(), token_line.end(),
				std::back_inserter(stripped_token_line),
				[](const std::pair<std::string, bool>& token) { return token.first; });
			result.body.push_back(stripped_token_line);
		}
	}

	return result;
} // parse_vtf_section(std::istream) }}}


ParsedSection Vata2::Parser::parse_vtf_section(const std::string& input)
{ // {{{
	std::istringstream stream(input);
	ParsedSection result = parse_vtf_section(stream);
	return result;
} // parse_vtf_section(std::string) }}}
