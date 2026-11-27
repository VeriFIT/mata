/** @file
 * @brief Language inclusion for ranked bottom-up tree automata.
 */

#ifndef MATA_TA_INCLUSION_HH
#define MATA_TA_INCLUSION_HH

#include "mata/ta/ta.hh"

namespace mata::ta {

/**
 * Checks language inclusion by a bottom-up antichain search over pairs of a smaller state and a bigger macrostate.
 *
 * @param counterexample If non-null and inclusion does not hold, receives a tree accepted by @p smaller and rejected
 * by @p bigger. It is left unchanged when inclusion holds.
 */
bool is_included(const Ta& smaller, const Ta& bigger, Tree* counterexample = nullptr);

} // namespace mata::ta

#endif // MATA_TA_INCLUSION_HH
