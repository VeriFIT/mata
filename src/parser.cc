// TODO: some header

#include <vata2/parser.hh>
#include <vata2/util.hh>

#include <algorithm>
#include <cstring>
#include <sstream>

using Vata2::Parser::Parsed;
using Vata2::Parser::ParsedSection;

// macro for debug prints in the parser
// #define PARSER_DEBUG_PRINT_LN(x) { DEBUG_PRINT_LN(x) }
#define PARSER_DEBUG_PRINT_LN(x) {  }


namespace
{

/** Eats all whitespaces in the input stream */
void eat_whites(std::istream& input)
{
	while (input.good())
	{
		int ch = input.peek();
		if (!std::isblank(ch)) { return; }
		input.get();
	}
}


/** Determines whether the character is a token delimiter */
inline bool is_token_delim(char ch)
{
	return isspace(ch) || ('#' == ch) || ('\"' == ch);
}


/** Reads from a stream until the next token delimiter */
std::string read_until_delim(std::istream& input)
{ // {{{
	std::string result;
	while (input.good())
	{
		char ch = input.peek();
		if (!input.good() || is_token_delim(ch)) { break; }
		result.push_back(ch);
		input.get();           // eat the character
	}

	return result;
} // read_until_delim() }}}


/** Reads from a stream until the next token end of line */
std::string read_until_eol(std::istream& input)
{ // {{{
	std::string result;
	while (input.good())
	{
		char ch = input.peek();
		if (!input.good() || '\n' == ch) { break; }
		result.push_back(ch);
		input.get();           // eat the character
	}

	return result;
} // read_until_eol() }}}


/**
 * @brief  Gets a token from the input stream
 *
 * The function assumes that the stream does not span lines
 */
std::string get_token_new(std::istream& input, bool* quoted)
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
				if (std::isblank(ch))
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
				else
				{
					result += ch;
					state = TokenizerState::UNQUOTED;
				}
				break;
			}
			case TokenizerState::UNQUOTED:
			{
				if (std::isblank(ch)) return result;
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
					if (!std::isblank(ch) && std::char_traits<char>::eof() != ch)
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
{
	std::vector<std::pair<std::string, bool>> result;
	std::istringstream stream(line);
	bool first = true;
	while (stream.good())
	{
		bool quoted;
		std::string token = get_token_new(stream, &quoted);
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
} // tokenize_line
} // anonymous namespace


Parsed Vata2::Parser::parse_vtf(const std::string& input)
{ // {{{
	Parsed result;
	std::istringstream stream(input);

	while (stream)
	{
		try
		{
			ParsedSection parsec = parse_vtf_section(stream);
			result.push_back(parsec);
		}
		catch (const std::exception& ex)
		{
			assert(false);
		}
	}

	return result;
} // parse_vtf(std::string) }}}


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

		std::string line;
		getline(input, line);

		if (reading_type)
		{ // we're expecting a @TYPE declaration
			if ('#' == ch || '\n' == ch) { continue; /* skip the rest of the line */ }
			else if ('@' != ch)
			{
				throw std::runtime_error("expecting automaton type (@TYPE), got \"" +
					line + "\" instead");
			}

			std::vector<std::pair<std::string, bool>> token_line = tokenize_line(line);
			if (1 != token_line.size() ||
				token_line[0].second ||             /* quoted or not? */
				'@' != token_line[0].first[0] ||
				1 == token_line[0].first.size())
			{
				throw std::runtime_error("Invalid @TYPE declaration: " + line);
			}

			result.type = std::string(token_line[0].first.begin() + 1, token_line[0].first.end());
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
				std::tie(it, std::ignore) =
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
			std::vector<std::string> stripped_token_line;
			std::transform(token_line.begin(), token_line.end(),
				std::back_inserter(stripped_token_line),
				[](const std::pair<std::string, bool>& token) { return token.first; });
			result.body.push_back(stripped_token_line);
		}
	}

	if (reading_type)
	{
		throw std::runtime_error("Empty section");
	}

	return result;
} // parse_vtf_section(std::istream) }}}


ParsedSection Vata2::Parser::parse_vtf_section(const std::string& input)
{ // {{{
	std::istringstream stream(input);
	ParsedSection result = parse_vtf_section(stream);
	return result;
} // parse_vtf_section(std::string) }}}
