// TODO: Insert file header.

#ifndef MATA_TYPES_HH
#define MATA_TYPES_HH

#include "mata/alphabet.hh"
#include "mata/parser/parser.hh"

#include <memory>

namespace Mata::Nfa {

extern const std::string TYPE_NFA;

using State = unsigned long;
using StateSet = Mata::Util::OrdVector<State>;

using WordSet = std::set<std::vector<Symbol>>;
struct Run {
    std::vector<Symbol> word{}; ///< A finite-length word.
    std::vector<State> path{}; ///< A finite-length path through automaton.
};

/// Mapping of states to states, used, for example, to map original states to reindexed states of new automaton, etc.
using StateToStateMap = std::unordered_map<State, State>;
using SymbolToStringMap = std::unordered_map<Symbol, std::string>;

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

/*TODO: Ideally remove functions using this struct as a parameter.
 * unpack the trans. relation to transitions is inefficient, goes against the hairs of the library.
 * Do we want to support it?
 */
/// A single transition.
struct Trans {
    State src;
    Symbol symb;
    State tgt;

    Trans() : src(), symb(), tgt() { }
    Trans(State src, Symbol symb, State tgt) : src(src), symb(symb), tgt(tgt) { }

    bool operator==(const Trans& rhs) const
    { // {{{
        return src == rhs.src && symb == rhs.symb && tgt == rhs.tgt;
    } // operator== }}}
    bool operator!=(const Trans& rhs) const { return !this->operator==(rhs); }
};

struct Nfa; ///< A non-deterministic finite automaton.

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
constexpr Symbol EPSILON = Limits::max_symbol;

} // namespace Mata::Nfa.

#endif //MATA_TYPES_HH
