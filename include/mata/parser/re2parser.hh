/* re2parser.hh -- Parser transforming re2 regular expressions to their corresponding automata representations.
 */

#ifndef MATA_RE2PARSER_HH
#define MATA_RE2PARSER_HH

#include <string>

#include "mata/nfa/nfa.hh"


// Encoding for the regular expression
// FIXME: Use enum class re2::Regexp::ParseFlags from re2/regexp.h instead. It is not possible to include it here. Need to fix cmake.
enum class Encoding {
    UTF8 = 0,
    Latin1 = 1 << 5
};


namespace mata::parser {
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
    nfa::Nfa create_nfa(const std::string &pattern, bool use_epsilon = false, mata::Symbol epsilon_value = 306,
                    bool use_reduce = true, const Encoding encoding = Encoding::Latin1);

    // version for python binding
    void create_nfa(nfa::Nfa* nfa, const std::string &pattern, bool use_epsilon = false, mata::Symbol epsilon_value = 306,
                    bool use_reduce = true, const Encoding encoding = Encoding::Latin1);
}

#endif // MATA_RE2PARSER_HH
