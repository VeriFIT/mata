/** @file
 * @brief Parser transforming re2 regular expressions to their corresponding automata representations.
 */

#ifndef MATA_RE2_PARSER_HH
#define MATA_RE2_PARSER_HH

#include <string>

#include "mata/nfa/nfa.hh"

// Encoding for the regular expression
// Encoding mirrors re2::Regexp::ParseFlags; enum class cannot be included in public header.
enum class Encoding { Utf8 = 0, Latin1 = 1 << 5 };

namespace mata::parser {
/**
 * @brief Post-processing options for RE2-to-NFA conversion.
 *
 * Parsing a regex must not silently trim and reduce: @ref create_nfa(const std::string&, const Re2Options&) runs
 *  only the steps the options ask for. The compatibility overloads below forward with defaults that reproduce
 *  the historical behaviour (epsilon removed, trimmed, simulation-reduced).
 */
struct Re2Options {
	bool use_epsilon{false}; ///< Whether to keep epsilon transitions in the created NFA.
	Symbol epsilon_value{306}; ///< Symbol representing epsilon.
	bool trim{true}; ///< Trim the created NFA.
	bool reduce{true}; ///< Reduce the (trimmed) NFA with simulation reduction.
	Encoding encoding{Encoding::Latin1}; ///< Encoding of the regex.
};

/**
 * @brief Creates NFA from regular expression using RE2 parser
 *
 * @sa mata::parser::create_nfa() with the compatibility overloads for the individual parameters.
 * @param pattern regex as a string
 * @param options conversion and post-processing options
 * @return Nfa corresponding to pattern
 * @throw std::runtime_error on a regex program containing an instruction the converter does not support.
 */
nfa::Nfa create_nfa(const std::string& pattern, const Re2Options& options);

/**
 * @brief Creates NFA from regular expression using RE2 parser
 *
 * At https://github.com/google/re2/wiki/Syntax, you can find the syntax
 * of regular expressions with following futher limitations:
 *  1) If you use UTF8 encoding, the created NFA will have the values of
 *     bytes instead of full symbols. For example, the character Ā whose
 *     Unicode code point is U+0100 and is represented in UTF8 as two
 *     bytes c4 80 will have two transitions, one with c4 followed with
 *     by 80, to encode it.
 *  2) The created automaton represents the language of the regex and
 *     is not expected to be used in regex matching. Therefore, stuff
 *     like ^, $, \b, etc. are ignored in the regex.
 *
 * @sa mata::nfa::builder::create_from_regex()
 *
 * @param pattern regex as a string
 * @param use_epsilon whether to keep epsilon transitions in created NFA
 * @param epsilon_value symbol representing epsilon
 * @param use_reduce if set to true the result is trimmed and reduced using simulation reduction
 * @param encoding encoding of the regex, default is Latin1
 * @return Nfa corresponding to pattern
 */
nfa::Nfa create_nfa(
	const std::string& pattern,
	bool use_epsilon = false,
	Symbol epsilon_value = 306,
	bool use_reduce = true,
	Encoding encoding = Encoding::Latin1
);

// version for python binding
void create_nfa(
	nfa::Nfa* nfa,
	const std::string& pattern,
	bool use_epsilon = false,
	Symbol epsilon_value = 306,
	bool use_reduce = true,
	Encoding encoding = Encoding::Latin1
);
} // namespace mata::parser

#endif // MATA_RE2PARSER_HH
