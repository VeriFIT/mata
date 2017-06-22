// TODO: some header

#include <vata2/parser.hh>

#include <algorithm>
#include <sstream>

using Vata2::Parser::Parsed;
using Vata2::Parser::ParsedSection;

namespace
{

/**
 * @brief  Trim whitespaces from a string (both left and right)
 */
std::string trim(const std::string& str)
{ // std::string
	std::string result = str;

	// trim from start
	result.erase(result.begin(), std::find_if(result.begin(), result.end(),
		std::not1(std::ptr_fun<int, int>(std::isspace))));

	// trim from end
	result.erase(std::find_if(result.rbegin(), result.rend(),
		std::not1(std::ptr_fun<int, int>(std::isspace))).base(), result.end());

	return result;
} // trim(std::string) }}}

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

	assert(false);

	return result;
} // parse_vtf_section(std::istream) }}}


ParsedSection Vata2::Parser::parse_vtf_section(const std::string& input)
{ // {{{
	std::istringstream stream(input);

	ParsedSection result = parse_vtf_section(stream);

	// TODO: CHECK that nothing follows?
	assert(false);

	return result;
} // parse_vtf_section(std::string) }}}
