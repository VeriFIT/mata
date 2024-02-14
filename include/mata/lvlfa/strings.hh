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

} // Namespace mata::lvlfa.

#endif // MATA_LVLFA_STRING_SOLVING_HH_.
