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

/** Parsed data */
struct Parsed
{
	std::string type;
	KeyListStore dict;
	std::list<ParsedTrans> trans_list;

	Parsed() : type(), dict(), trans_list() { }
};

/** Parses a string into an intermediary structure */
Parsed parse_vtf(const std::string& input);

// CLOSING NAMESPACES AND GUARDS
} /* Parser */
} /* Vata2 */

#endif /* _VATA2_PARSER_HH_ */


