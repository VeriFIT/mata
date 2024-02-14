/* nfa-strings.hh -- Operations on NFAs for string solving.
 */

#ifndef MATA_LVLFA_STRING_SOLVING_HH_
#define MATA_LVLFA_STRING_SOLVING_HH_

#include "mata/nfa/strings.hh"
#include "lvlfa.hh"

namespace mata::lvlfa {
/**
 * Create identity transducer over the @p alphabet with @p level_cnt levels.
 */
Lvlfa create_identity(mata::Alphabet* alphabet, Level level_cnt = 2);

/**
 * Create identity input/output transducer with 2 levels over the @p alphabet with @p level_cnt levels with single
 *  symbol @p from_symbol replaced with @to_symbol.
 */
Lvlfa create_identity_with_single_replace(mata::Alphabet* alphabet, Symbol from_symbol, Symbol to_symbol);

} // Namespace mata::lvlfa.

#endif // MATA_LVLFA_STRING_SOLVING_HH_.
