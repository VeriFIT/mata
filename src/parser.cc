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


enum class ParserState
{ // {{{
	AWAITING_NEWLINE,
	KEY_AT_NEWLINE,
	KEY_OR_VALUE,
	KEY_OR_VALUE_AT_NEWLINE,
	TRANSITIONS,
	TRANSITIONS_AT_NEWLINE
};

std::string parser_state_to_string(ParserState state)
{
	switch (state)
	{
		case ParserState::AWAITING_NEWLINE: return "AWAITING_NEWLINE";
		case ParserState::KEY_OR_VALUE_AT_NEWLINE: return "KEY_OR_VALUE_AT_NEWLINE";
		case ParserState::KEY_OR_VALUE: return "KEY_OR_VALUE";
		case ParserState::KEY_AT_NEWLINE: return "KEY_AT_NEWLINE";
		case ParserState::TRANSITIONS: return "TRANSITIONS";
		case ParserState::TRANSITIONS_AT_NEWLINE: return "TRANSITIONS_AT_NEWLINE";
		default:  throw std::runtime_error("Invalid value of ParserState");
	}
} // }}}


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

	ParserState state = ParserState::AWAITING_NEWLINE;
	std::string key;
	std::vector<std::string> values;
	while (true)
	{
		PARSER_DEBUG_PRINT_LN("state: " + parser_state_to_string(state));
		switch (state)
		{ // the parser's state machine
			case ParserState::AWAITING_NEWLINE:
			{
				token_type = get_nonwhite_token(input, &token, false);
				if (TokenType::END_OF_FILE == token_type)
				{
					return result;
				}
				else if (TokenType::END_OF_LINE != token_type)
				{
					PARSER_DEBUG_PRINT_LN("TokenType = " + token_type_to_string(token_type));
					assert(false);
				}

				state = ParserState::KEY_AT_NEWLINE;
				break;
			}

			case ParserState::KEY_AT_NEWLINE:
			{
				token_type = get_nonwhite_token(input, &token, true);
				if (TokenType::END_OF_FILE == token_type)
				{
					return result;
				}
				else if (TokenType::KEY_STRING != token_type)
				{
					throw std::runtime_error("expecting key (%KEY), got " +
						token_type_to_string(token_type) + ": " + token);
				}

				key = token;
				if ("Transitions" == key)
				{
					state = ParserState::TRANSITIONS_AT_NEWLINE;
				}
				else
				{
					state = ParserState::KEY_OR_VALUE;
				}
				break;
			}
			case ParserState::KEY_OR_VALUE: // falling through to the next case!
			case ParserState::KEY_OR_VALUE_AT_NEWLINE:
			{
				token_type = get_nonwhite_token(input, &token, false);
				if (TokenType::END_OF_FILE == token_type)
				{
					// process value for the previous key first
					result.dict.insert({key, values});

					return result;
				}
				else if (TokenType::END_OF_LINE == token_type)
				{
					state = ParserState::KEY_OR_VALUE_AT_NEWLINE;
				}
				else if (TokenType::KEY_STRING == token_type)
				{
					// process value for the previous key first
					result.dict.insert({key, values});

					// start processing the new key
					key = token;
					values = {};

					if (result.dict.find(key) != result.dict.end())
					{
						assert(false);
					}

					if ("Transitions" == key)
					{
						if (ParserState::KEY_OR_VALUE_AT_NEWLINE == state)
						{
							state = ParserState::TRANSITIONS_AT_NEWLINE;
						}
						else
						{
							assert(false);
						}
					}
					else { state = ParserState::KEY_OR_VALUE; }
				}
				else if ((TokenType::UNQUOTED_STRING == token_type) ||
					(TokenType::QUOTED_STRING == token_type))
				{
					values.push_back(token);
				}
				else if (TokenType::AUT_TYPE == token_type)
				{
					if (ParserState::KEY_OR_VALUE_AT_NEWLINE == state)
					{
						assert(false);

					}
					else
					{
						throw std::runtime_error("invalid position of @TYPE: @" + token);
					}

					assert(false);
				}

				break;
			}

			case ParserState::TRANSITIONS:
			case ParserState::TRANSITIONS_AT_NEWLINE:
			{
				bool skip_eols = (ParserState::TRANSITIONS_AT_NEWLINE == state);
				token_type = get_nonwhite_token(input, &token, skip_eols);
				if (TokenType::END_OF_FILE == token_type)
				{
					if (ParserState::TRANSITIONS_AT_NEWLINE != state)
					{
						result.body.push_back(values);
					}

					return result;
				}
				else if (TokenType::END_OF_LINE == token_type)
				{
					assert(ParserState::TRANSITIONS_AT_NEWLINE != state);
					assert(!values.empty());

					result.body.push_back(values);
					values.clear();

					state = ParserState::TRANSITIONS_AT_NEWLINE;
				}
				else if ((TokenType::UNQUOTED_STRING == token_type) ||
					(TokenType::QUOTED_STRING == token_type))
				{
					values.push_back(token);

					state = ParserState::TRANSITIONS;
				}
				else if (TokenType::AUT_TYPE == token_type)
				{
					if (ParserState::TRANSITIONS_AT_NEWLINE == state)
					{
						assert(false);
					}
					else
					{
						assert(ParserState::TRANSITIONS == state);
						throw std::runtime_error("invalid position of @TYPE: @" + token);
					}
				}
				else if (TokenType::KEY_STRING == token_type)
				{
					throw std::runtime_error("invalid position of %KEY: %" + token);
				}
				else
				{
					assert(false);
				}

				break;
			}
		}
	}

	assert(false);

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
