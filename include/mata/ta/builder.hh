/** @file
 * @brief Builders for explicit ranked bottom-up tree automata.
 */

#ifndef MATA_TA_BUILDER_HH
#define MATA_TA_BUILDER_HH

#include "mata/parser/parser.hh"
#include "mata/ta/ta.hh"

#include <filesystem>
#include <istream>
#include <string>

namespace mata::ta::builder {

using NameStateMap = utils::StringMap<State>;

/**
 * Builds an automaton from one @c \@TA-explicit section.
 *
 * The accepted syntax is deliberately explicit and ranked. @c %Final declares final root states, and an empty
 * @c %Alphabet-auto directive is accepted as an explicit naming declaration:
 * @code
 * %Alphabet-auto
 * %Final q_accept q_other_accept
 * @endcode
 * Body lines are either nullary rules @c a->q or ordered hyperedges
 * @c f(q1,q2)->q. White space and commas around names are flexible, but parentheses are required for non-nullary
 * rules. Names are strings; equal state names and equal symbol names always denote equal objects. There are no
 * initial declarations, formulas, symbolic alphabets, or additional directives.
 *
 * @param alphabet Translates symbol names. If null, an on-the-fly alphabet is used; pass a persistent alphabet when
 * the numeric symbols are needed afterwards for constructing trees.
 * @param state_map Optional input/output state-name map. Pre-mapped names retain their states; all parsed names are
 * added to it.
 * @throws std::runtime_error On malformed input.
 * @throws std::invalid_argument On inconsistent ranks of one symbol.
 */
Ta construct(const parser::ParsedSection& section, Alphabet* alphabet = nullptr, NameStateMap* state_map = nullptr);

Ta parse_from_mata(const std::string& input, Alphabet* alphabet = nullptr, NameStateMap* state_map = nullptr);
inline Ta parse_from_mata(const char* input, Alphabet* alphabet = nullptr, NameStateMap* state_map = nullptr) {
	return parse_from_mata(std::string{input}, alphabet, state_map);
}
Ta parse_from_mata(std::istream& input, Alphabet* alphabet = nullptr, NameStateMap* state_map = nullptr);
Ta parse_from_mata(const std::filesystem::path& input, Alphabet* alphabet = nullptr, NameStateMap* state_map = nullptr);

} // namespace mata::ta::builder

#endif // MATA_TA_BUILDER_HH
