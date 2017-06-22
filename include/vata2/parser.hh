// TODO: some header

#ifndef _VATA2_PARSER_HH_
#define _VATA2_PARSER_HH_

#include <cassert>
#include <list>
#include <map>
#include <string>
#include <vector>

namespace Vata2
{
namespace Parser
{

using KeyListStore = std::map<std::string, std::vector<std::string>>;
using ParsedTrans = std::vector<std::string>;

/** Parsed data (single section) */
struct ParsedSection
{
	std::string type;
	KeyListStore dict;
	std::list<ParsedTrans> trans_list;

	ParsedSection() : type(), dict(), trans_list() { }
};

/** Parsed data */
using Parsed = std::vector<ParsedSection>;

/** Parses a string into an intermediary structure */
Parsed parse_vtf(const std::string& input);

/** Parses one section from a stream into an intermediary structure */
ParsedSection parse_vtf_section(std::istream& input);

/** Parses one section from a string into an intermediary structure */
ParsedSection parse_vtf_section(const std::string& input);

// CLOSING NAMESPACES AND GUARDS
} /* Parser */
} /* Vata2 */

#endif /* _VATA2_PARSER_HH_ */


