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

template<typename T> using Set = Mata::Util::OrdVector<T>;

using WordSet = std::set<std::vector<Symbol>>;
struct Run {
    std::vector<Symbol> word{}; ///< A finite-length word.
    std::vector<State> path{}; ///< A finite-length path through automaton.
};

using StringToStateMap = std::unordered_map<std::string, State>;

using StateToStringMap = std::unordered_map<State, std::string>;
// using StateToPostMap = StateMap<PostSymb>; ///< Transitions.
/// Mapping of states to states, used, for example, to map original states to reindexed states of new automaton, etc.
using StateToStateMap = std::unordered_map<State, State>;

using SymbolToStringMap = std::unordered_map<Symbol, std::string>;
/*TODO: this should become a part of the automaton somehow.
 * It should be a vector indexed by states.
 * */

using StringMap = std::unordered_map<std::string, std::string>;

/*TODO: What about to
 * have names Set, UMap/OMap, State, Symbol, Sequence... and name by Set<State>, UMap<State>, ...
 * maybe something else is needed for the more complex maps*/

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

using TransSequence = std::vector<Trans>; ///< Set of transitions.

struct Nfa; ///< A non-deterministic finite automaton.

//TODO: Kill these types names? Some of them?
template<typename T> using Sequence = std::vector<T>; ///< A sequence of elements.
using AutSequence = Sequence<Nfa>; ///< A sequence of non-deterministic finite automata.

template<typename T> using RefSequence = Sequence<std::reference_wrapper<T>>; ///< A sequence of references to elements.
using AutRefSequence = RefSequence<Nfa>; ///< A sequence of references to non-deterministic finite automata.
using ConstAutRefSequence = RefSequence<const Nfa>; ///< A sequence of const references to non-deterministic finite automata.

template<typename T> using PtrSequence = Sequence<T*>; ///< A sequence of pointers to elements.
using AutPtrSequence = PtrSequence<Nfa>; ///< A sequence of pointers to non-deterministic finite automata.
using ConstAutPtrSequence = PtrSequence<const Nfa>; ///< A sequence of pointers to const non-deterministic finite automata.

template<typename T> using ConstPtrSequence = Sequence<T* const>; ///< A sequence of const pointers to elements.
using AutConstPtrSequence = ConstPtrSequence<Nfa>; ///< A sequence of const pointers to non-deterministic finite automata.
using ConstAutConstPtrSequence = ConstPtrSequence<const Nfa>; ///< A sequence of const pointers to const non-deterministic finite automata.

// TODO: why introduce this type name?
using SharedPtrAut = std::shared_ptr<Nfa>; ///< A shared pointer to NFA.

/// serializes Nfa into a ParsedSection
Mata::Parser::ParsedSection serialize(
        const Nfa&                aut,
        const SymbolToStringMap*  symbol_map = nullptr,
        const StateToStringMap*   state_map = nullptr);

/// An epsilon symbol which is now defined as the maximal value of data type used for symbols.
constexpr Symbol EPSILON = Limits::max_symbol;

} // namespace Mata::Nfa.

#endif //MATA_TYPES_HH
