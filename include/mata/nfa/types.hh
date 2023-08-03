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

/// A single transition in Delta.
struct Trans {
    State src;
    Symbol symb;
    State tgt;

    Trans() : src(), symb(), tgt() { }
    Trans(const Trans &) = default;
    Trans(Trans &&) = default;
    Trans &operator=(const Trans &) = default;
    Trans &operator=(Trans &&) = default;
    Trans(State src, Symbol symb, State tgt) : src(src), symb(symb), tgt(tgt) {}

    bool operator==(const Trans& rhs) const { return src == rhs.src && symb == rhs.symb && tgt == rhs.tgt; }
    bool operator!=(const Trans& rhs) const { return !this->operator==(rhs); }
};

struct Nfa; ///< A non-deterministic finite automaton.

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
constexpr Symbol EPSILON = Limits::max_symbol;

} // namespace Mata::Nfa.

#endif //MATA_TYPES_HH
