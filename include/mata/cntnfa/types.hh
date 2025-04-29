// TODO: Insert file header.

#ifndef MATA_TYPES_HH
#define MATA_TYPES_HH

#include "mata/cntnfa/counters.hh"
#include "mata/alphabet.hh"

#include <limits>

// Use this for undefined annotations
#define MAX_SIZE_T (std::numeric_limits<size_t>::max())
#define UNDEFINED_ANNOTATIONS MAX_SIZE_T
#define UNDEFINED_COUNTER MAX_SIZE_T

namespace mata::cntnfa {

extern const std::string TYPE_CNTNFA;

using State = unsigned long;
using StateSet = mata::utils::OrdVector<State>;

struct Run {
    Word word{}; ///< A finite-length word.
    std::vector<State> path{}; ///< A finite-length path through automaton.
};

// Configuration of the counter automaton.
struct Configuration {
    State state; ///< The current state of the automaton.
    CounterSet counters; ///< The current values of the counters.

    Configuration() : state(), counters() {}
    Configuration(State state, CounterSet counters)
        : state(state), counters(counters) {}

    bool operator==(const Configuration& other) const {
        return state == other.state && counters == other.counters;
    }
};

using StateRenaming = std::unordered_map<State, State>;

/**
 * @brief Map of additional parameter name and value pairs.
 *
 * Used by certain functions for specifying some additional parameters in the following format:
 * ```cpp
 * ParameterMap {
 *     { "algorithm", "classical" },
 *     { "minimize", "true" }
 * }
 * ```
 */
using ParameterMap = std::unordered_map<std::string, std::string>;

struct Limits {
public:
    static const State min_state = std::numeric_limits<State>::min();
    static const State max_state = std::numeric_limits<State>::max();
    static const Symbol min_symbol = std::numeric_limits<Symbol>::min();
    static const Symbol max_symbol = std::numeric_limits<Symbol>::max();
};

struct Cntnfa; ///< A non-deterministic finite automaton with counters.

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
constexpr Symbol EPSILON = Limits::max_symbol;

} // namespace mata::cntnfa.

#endif //MATA_TYPES_HH
