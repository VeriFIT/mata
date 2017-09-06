// TODO: some header

#ifndef _VATA2_PARSER_HH_
#define _VATA2_PARSER_HH_

#include <cassert>
#include <list>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include <vata2/util.hh>

namespace Vata2
{
namespace Parser
{

using KeyListStore = std::map<std::string, std::vector<std::string>>;
using BodyLine = std::vector<std::string>;

/** Parsed data (single section) */
struct ParsedSection
{
	std::string type;
	KeyListStore dict;
	std::list<BodyLine> body;

	ParsedSection() : type(), dict(), body() { }

	/// Is the section empty?
	bool empty() const { return type.empty() && dict.empty() && body.empty(); }

	/** Output stream operator */
	friend std::ostream& operator<<(std::ostream& os, const ParsedSection& parsec)
	{ // {{{
		os << "Type: " << parsec.type << "\n";
		os << "Keys:\n";
		for (auto string_list_pair : parsec.dict)
		{
			os << "  " << string_list_pair.first << ": " <<
				std::to_string(string_list_pair.second) << "\n";
		}

		os << "Body:\n";
		for (auto body_line : parsec.body)
		{
			os << "  " << std::to_string(body_line) << "\n";
		}

		return os;
	} // operator<< }}}
};



/** Parsed data */
using Parsed = std::vector<ParsedSection>;

/** Parses a string into an intermediary structure */
Parsed parse_vtf(const std::string& input);

/** Parses a stream into an intermediary structure */
Parsed parse_vtf(std::istream& input);

/** Parses one section from a stream into an intermediary structure */
ParsedSection parse_vtf_section(std::istream& input);

/** Parses one section from a string into an intermediary structure */
ParsedSection parse_vtf_section(const std::string& input);

// CLOSING NAMESPACES AND GUARDS
} /* Parser */
} /* Vata2 */

#endif /* _VATA2_PARSER_HH_ */


