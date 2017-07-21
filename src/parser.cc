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
enum class TokenType
{ // {{{
	UNQUOTED_STRING,
	QUOTED_STRING,
	AUT_TYPE,
	KEY_STRING,
	COMMENT,
	END_OF_LINE,
	END_OF_FILE
};

/** TokenType to string */
std::string token_type_to_string(TokenType type)
{
	switch (type)
	{
		case TokenType::UNQUOTED_STRING: return "UNQUOTED_STRING";
		case TokenType::QUOTED_STRING: return "QUOTED_STRING";
		case TokenType::AUT_TYPE: return "AUT_TYPE";
		case TokenType::KEY_STRING: return "KEY_STRING";
		case TokenType::COMMENT: return "COMMENT";
		case TokenType::END_OF_LINE: return "END_OF_LINE";
		case TokenType::END_OF_FILE: return "END_OF_FILE";
		default:  throw std::runtime_error("Invalid value of TokenType");
	}
} // }}}

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


/** Gets one token from a stream
 *
 * @param[in,out]  input      Input stream (consumed partially in the function)
 * @param[out]     token      The read token
 *
 * @return  Type of the token
 */
TokenType get_token(
	std::istream&  input,
	std::string*   token)
{
	assert(nullptr != token);

	TokenType type;
	eat_whites(input);
	token->clear();

	if (!input.good())
	{
		PARSER_DEBUG_PRINT_LN(token_type_to_string(TokenType::END_OF_FILE));
		return TokenType::END_OF_FILE;
	}

	char ch = input.peek();
	switch (ch)
	{
		case '\n': // end of line
		{
			input.get();

			PARSER_DEBUG_PRINT_LN(token_type_to_string(TokenType::END_OF_LINE));
			return TokenType::END_OF_LINE;
		}
		case '\"': // quoted strings
		{
			input.get();
			bool is_escaped = false;
			while (input.good())
			{ // the state machine that reads quoted strings
				ch = input.peek();
				if (!input.good() || '\n' == ch)
				{ // end of file of line before end of quotes
					throw std::runtime_error("missing ending quotes: " + *token);
				}

				input.get();
				if ('\\' == ch)
				{
					if (is_escaped)
					{
						assert(false);
					}
					else
					{
						is_escaped = true;
					}
				}
				else if ('\"' == ch)
				{
					if (is_escaped)
					{
						token->push_back('\"');
						is_escaped = false;
					}
					else
					{
						ch = input.peek();
						if (input.good() && !isspace(ch))
						{ // for misplaced quotes
							throw std::runtime_error("misplaced quotes: \"" + *token + "\"");
						}

						break; // leave the loop
					}
				}
				else
				{
					if (is_escaped)
					{ // if the previous character was escape
						token->push_back('\\');
						is_escaped = false;
					}

					token->push_back(ch);
				}
			}

			PARSER_DEBUG_PRINT_LN(token_type_to_string(TokenType::QUOTED_STRING) +
				" - \"" + *token + "\"");
			return TokenType::QUOTED_STRING;
		}
		case '@': // automaton type
		{
			input.get();
			type = TokenType::AUT_TYPE;

			ch = input.peek();
			if (input.good() && is_token_delim(ch))
			{
				throw std::runtime_error("@TYPE name missing");
			}

			break;
		}
		case '%': // key
		{
			input.get();
			type = TokenType::KEY_STRING;

			ch = input.peek();
			if (input.good() && is_token_delim(ch))
			{
				throw std::runtime_error("%KEY name missing");
			}

			break;
		}
		case '#': // comment
		{
			*token = read_until_eol(input);
			PARSER_DEBUG_PRINT_LN(token_type_to_string(TokenType::COMMENT) +
				" - " + *token);
			return TokenType::COMMENT;
		}
		default: // sink hole for other strings
		{
			type = TokenType::UNQUOTED_STRING;
			break;
		}
	}

	*token = read_until_delim(input);
	if (token->empty())
	{
		assert(false);
		throw std::runtime_error("empty token found");
	}

	ch = input.peek();
	if (input.good() && '\"' == ch)
	{
		throw std::runtime_error("misplaced quotes: " + *token + "\"");
	}

	PARSER_DEBUG_PRINT_LN(token_type_to_string(type) + " - " + *token);
	return type;
}


/** Gets the next non-white token from a stream (not comment, not EOL)
 *
 * @param[in,out]  input      Input stream (consumed partially in the function)
 * @param[out]     token      The read token
 *
 * @return  Type of the token
 */
TokenType get_nonwhite_token(
	std::istream&  input,
	std::string*   token,
	bool           skip_eol)
{ // {{{
	assert(nullptr != token);

	TokenType token_type = get_token(input, token);
	while (
		(skip_eol && TokenType::END_OF_LINE == token_type) ||
		(TokenType::COMMENT == token_type))
	{
		token_type = get_token(input, token);
	}

	return token_type;
} // get_nonwhite_token }}}


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

	std::string token;
	TokenType token_type = get_nonwhite_token(input, &token, true);
	assert(TokenType::END_OF_LINE != token_type);
	assert(TokenType::COMMENT != token_type);

	if (TokenType::END_OF_FILE == token_type)
	{
		assert(false);
	}
	else if (TokenType::AUT_TYPE != token_type)
	{
		throw std::runtime_error("expecting automaton type (@TYPE), got \"" +
			token + "\" instead");
	}

	result.type = token;

	while (input.good())
	{
		std::string line;
		getline(input, line);

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

	return result;
} // parse_vtf_section(std::istream) }}}


ParsedSection Vata2::Parser::parse_vtf_section(const std::string& input)
{ // {{{
	std::istringstream stream(input);

	ParsedSection result = parse_vtf_section(stream);

	std::string token;
	TokenType token_type = get_token(stream, &token);
	if (TokenType::END_OF_FILE != token_type)
	{
		assert(false);
	}

	return result;
} // parse_vtf_section(std::string) }}}
