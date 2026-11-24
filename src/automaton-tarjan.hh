/* Internal iterative Tarjan SCC traversal; the generic one lives in mata/core/tarjan.hh. */
#ifndef MATA_AUTOMATON_TARJAN_HH
#define MATA_AUTOMATON_TARJAN_HH

#include "mata/core/tarjan.hh"
#include "mata/nfa/delta.hh"

namespace mata::nfa::internal {
using mata::internal::call_callback;
using mata::internal::TarjanCallbacks;
using mata::internal::tarjan_scc_discover;
} // namespace mata::nfa::internal

#endif // MATA_AUTOMATON_TARJAN_HH
