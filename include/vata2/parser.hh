// TODO: some header

#ifndef _VATA2_PARSER_HH_
#define _VATA2_PARSER_HH_

#include <list>
#include <map>
#include <string>

namespace Vata2
{
namespace Parser
{

using KeyListStore = std::multimap<std::string, std::string>;
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
void parse_vtf(Parsed* parsed, const std::string* input);

// CLOSING NAMESPACES AND GUARDS
} /* Parser */
} /* Vata2 */

#endif /* _VATA2_PARSER_HH_ */


