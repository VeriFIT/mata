/* lvlfa.hh -- Nondeterministic finite automaton (over finite words).
 */

#ifndef MATA_LVLFA_HH_
#define MATA_LVLFA_HH_

// Static data structures, such as search stack, in algorithms. Might have some effect on some algorithms (like
//  fragile_revert).
//#define _STATIC_STRUCTURES_

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <limits>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "mata/alphabet.hh"
#include "mata/parser/parser.hh"
#include "mata/utils/utils.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/parser/inter-aut.hh"
#include "mata/utils/synchronized-iterator.hh"
#include "mata/utils/sparse-set.hh"
#include "types.hh"
#include "delta.hh"

#include "mata/nfa/nfa.hh"

/**
 * Nondeterministic Finite Automata including structures, transitions and algorithms.
 *
 * In particular, this includes:
 *   1. Structures (Automaton, Transitions, Results, Delta),
 *   2. Algorithms (operations, checks, tests),
 *   3. Constructions.
 *
 * Other algorithms are included in mata::lvlfa::Plumbing (simplified API for, e.g., binding)
 * and mata::lvlfa::algorithms (concrete implementations of algorithms, such as for complement).
 */
namespace mata::lvlfa {

/**
 * A struct representing an LVLFA.
 */
struct Lvlfa : public mata::nfa::Nfa {
public:
    /**
     * @brief For state q, delta[q] keeps the list of transitions ordered by symbols.
     *
     * The set of states of this automaton are the numbers from 0 to the number of states minus one.
     */
    std::vector<Level> levels{};
    Level levels_cnt = 0;
    /// Key value store for additional attributes for the LVLFA. Keys are attribute names as strings and the value types
    ///  are up to the user.
    /// For example, we can set up attributes such as "state_dict" for state dictionary attribute mapping states to their
    ///  respective names, or "transition_dict" for transition dictionary adding a human-readable meaning to each
    ///  transition.
    // TODO: When there is a need for state dictionary, consider creating default library implementation of state
    //  dictionary in the attributes.

public:
    explicit Lvlfa(Delta delta = {}, utils::SparseSet<State> initial_states = {},
                 utils::SparseSet<State> final_states = {}, std::vector<Level> levels = {}, Level levels_cnt = 1, Alphabet* alphabet = nullptr)
        : mata::nfa::Nfa(delta, initial_states, final_states, alphabet), levels(std::move(levels)), levels_cnt(levels_cnt) {}

    /**
     * @brief Construct a new explicit LVLFA with num_of_states states and optionally set initial and final states.
     *
     * @param[in] num_of_states Number of states for which to preallocate Delta.
     */
    explicit Lvlfa(const unsigned long num_of_states, StateSet initial_states = {},
                 StateSet final_states = {}, std::vector<Level> levels = {}, Level levels_cnt = 1, Alphabet* alphabet = nullptr)
        : mata::nfa::Nfa(num_of_states, initial_states, final_states, alphabet), levels(levels), levels_cnt(levels_cnt) {}

    explicit Lvlfa(const mata::nfa::Nfa& other)
        : mata::nfa::Nfa(other.delta, other.initial, other.final, other.alphabet),
          levels(std::move(std::vector<Level>(other.num_of_states(), 0))), levels_cnt(1) {}

    /**
     * @brief Construct a new explicit LVLFA from other LVLFA.
     */
    Lvlfa(const Lvlfa& other) = default;

    Lvlfa(Lvlfa&& other) noexcept
        : levels { std::move(other.levels) }, levels_cnt{ other.levels_cnt } {
            delta = std::move(other.delta);
            initial = std::move(other.initial);
            final = std::move(other.final);
            attributes = std::move(other.attributes);
            alphabet = other.alphabet;
            other.alphabet = nullptr;
    }

    Lvlfa& operator=(const Lvlfa& other) = default;
    Lvlfa& operator=(Lvlfa&& other) noexcept;

    /**
     * Add a new (fresh) state to the automaton.
     * @return The newly created state.
     */
    State add_state();

    /**
     * Add state @p state to @c delta if @p state is not in @c delta yet.
     * @return The requested @p state.
     */
    State add_state(State state);

    /**
     * @brief Clear the underlying LVLFA to a blank LVLFA.
     *
     * The whole LVLFA is cleared, each member is set to its zero value.
     */
    void clear();

    /**
     * @brief Check if @c this is exactly identical to @p aut.
     *
     * This is exact equality of automata, including state numbering (so even stronger than isomorphism),
     *  essentially only useful for testing purposes.
     * @return True if automata are exactly identical, false otherwise.
     */
    bool is_identical(const Lvlfa& aut) const;

    /**
     * @brief Remove inaccessible (unreachable) and not co-accessible (non-terminating) states in-place.
     *
     * Remove states which are not accessible (unreachable; state is accessible when the state is the endpoint of a path
     * starting from an initial state) or not co-accessible (non-terminating; state is co-accessible when the state is
     * the starting point of a path ending in a final state).
     *
     * @param[out] state_renaming Mapping of trimmed states to new states.
     * @return @c this after trimming.
     */
    Lvlfa& trim(StateRenaming* state_renaming = nullptr);

    /**
     * @brief In-place concatenation.
     */
    Lvlfa& concatenate(const Lvlfa& aut);

    /**
     * @brief In-place union
     */
    Lvlfa& uni(const Lvlfa &aut);

    /**
     * Unify transitions to create a directed graph with at most a single transition between two states.
     * @param[in] abstract_symbol Abstract symbol to use for transitions in digraph.
     * @return An automaton representing a directed graph.
     */
    Lvlfa get_one_letter_aut(Symbol abstract_symbol = 'x') const;

    /**
     * Unify transitions to create a directed graph with at most a single transition between two states.
     *
     * @param[out] result An automaton representing a directed graph.
     */
    void get_one_letter_aut(Lvlfa& result) const;

    void make_one_level_aut(const utils::OrdVector<Symbol> &dcare_replacements = { DONT_CARE });

    Lvlfa get_one_level_aut(const utils::OrdVector<Symbol> &dcare_replacements = { DONT_CARE }) const;

    void get_one_level_aut(Lvlfa& result, const utils::OrdVector<Symbol> &dcare_replacements = { DONT_CARE }) const;

    /**
     * @brief Prints the automaton in DOT format
     *
     * @return automaton in DOT format
     */
    std::string print_to_DOT() const;
    /**
     * @brief Prints the automaton to the output stream in DOT format
     */
    void print_to_DOT(std::ostream &output) const;
    /**
     * @brief Prints the automaton in mata format
     *
     * If you need to parse the automaton again, use IntAlphabet in construct()
     *
     * @return automaton in mata format
     * TODO handle alphabet of the automaton, currently we print the exact value of the symbols
     */
    std::string print_to_mata() const;
    /**
     * @brief Prints the automaton to the output stream in mata format
     *
     * If you need to parse the automaton again, use IntAlphabet in construct()
     *
     * TODO handle alphabet of the automaton, currently we print the exact value of the symbols
     */
    void print_to_mata(std::ostream &output) const;

    /// Is the language of the automaton universal?
    bool is_universal(const Alphabet& alphabet, Run* cex = nullptr,
                      const ParameterMap& params = {{ "algorithm", "antichains" }}) const;
    /// Is the language of the automaton universal?
    bool is_universal(const Alphabet& alphabet, const ParameterMap& params) const;

    /// Checks whether a word is in the language of an automaton.
    bool is_in_lang(const Run& word) const;
    /// Checks whether a word is in the language of an automaton.
    bool is_in_lang(const Word& word) { return is_in_lang(Run{ word, {} }); }

    /// Checks whether the prefix of a string is in the language of an automaton
    bool is_prfx_in_lang(const Run& word) const;

    std::pair<Run, bool> get_word_for_path(const Run& run) const;

    /**
     * @brief Get the set of all words in the language of the automaton whose length is <= @p max_length
     *
     * If you have an automaton with finite language (can be checked using @ref is_acyclic),
     * you can get all words by calling
     *      get_words(aut.num_of_states())
     */
    std::set<Word> get_words(unsigned max_length);

}; // struct Lvlfa.

// Allow variadic number of arguments of the same type.
//
// Using parameter pack and variadic arguments.
// Adapted from: https://www.fluentcpp.com/2019/01/25/variadic-number-function-parameters-type/.
/// Pack of bools for reasoning about a sequence of parameters.
template<bool...> struct bool_pack{};
/// Check that for all values in a pack @p Ts are 'true'.
template<typename... Ts> using conjunction = std::is_same<bool_pack<true,Ts::value...>, bool_pack<Ts::value..., true>>;
/// Check that all types in a sequence of parameters @p Ts are of type @p T.
template<typename T, typename... Ts> using AreAllOfType = typename conjunction<std::is_same<Ts, T>...>::type;

Lvlfa uni(const Lvlfa &lhs, const Lvlfa &rhs);

/**
 * @brief Compute intersection of two LVLFAs.
 *
 * Both automata can contain ε-transitions. The product preserves the ε-transitions, i.e.,
 * for each each product state `(s, t)` with`s -ε-> p`, `(s, t) -ε-> (p, t)` is created, and vice versa.
 *
 * Automata must share alphabets. //TODO: this is not implemented yet.
 *
 * @param[in] lhs First LVLFA to compute intersection for.
 * @param[in] rhs Second LVLFA to compute intersection for.
 * @param[in] first_epsilon smallest epsilon. //TODO: this should eventually be taken from the alphabet as anything larger than the largest symbol?
 * @param[out] prod_map Mapping of pairs of the original states (lhs_state, rhs_state) to new product states (not used internally, allocated only when !=nullptr, expensive).
 * @return LVLFA as a product of LVLFAs @p lhs and @p rhs with ε-transitions preserved.
 */
Lvlfa intersection(const Lvlfa& lhs, const Lvlfa& rhs,
                 const Symbol first_epsilon = EPSILON, std::unordered_map<std::pair<State, State>, State> *prod_map = nullptr);

/**
 * @brief Concatenate two LVLFAs.
 *
 * Supports epsilon symbols when @p use_epsilon is set to true.
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] use_epsilon Whether to concatenate over epsilon symbol.
 * @param[out] lhs_state_renaming Map mapping lhs states to result states.
 * @param[out] rhs_state_renaming Map mapping rhs states to result states.
 * @return Concatenated automaton.
 */
// TODO: check how fast is using just concatenate over epsilon and then call remove_epsilon().
Lvlfa concatenate(const Lvlfa& lhs, const Lvlfa& rhs, bool use_epsilon = false,
                StateRenaming* lhs_state_renaming = nullptr, StateRenaming* rhs_state_renaming = nullptr);

/**
 * @brief Compute automaton accepting complement of @p aut.
 *
 * @param[in] aut Automaton whose complement to compute.
 * @param[in] alphabet Alphabet used for complementation.
 * @param[in] params Optional parameters to control the complementation algorithm:
 * - "algorithm": "classical" (classical algorithm determinizes the automaton, makes it complete and swaps final and non-final states);
 * - "minimize": "true"/"false" (whether to compute minimal deterministic automaton for classical algorithm);
 * @return Complemented automaton.
 */
Lvlfa complement(const Lvlfa& aut, const Alphabet& alphabet,
               const ParameterMap& params = {{ "algorithm", "classical" }, { "minimize", "false" }});

/**
 * @brief Compute automaton accepting complement of @p aut.
 *
 * This overloaded version complements over an already created ordered set of @p symbols instead of an alphabet.
 * This is a more efficient solution in case you already have @p symbols precomputed or want to complement multiple
 *  automata over the same set of @c symbols: the function does not need to compute the ordered set of symbols from
 *  the alphabet again (and for each automaton).
 *
 * @param[in] aut Automaton whose complement to compute.
 * @param[in] symbols Symbols to complement over.
 * @param[in] params Optional parameters to control the complementation algorithm:
 * - "algorithm": "classical" (classical algorithm determinizes the automaton, makes it complete and swaps final and non-final states);
 * - "minimize": "true"/"false" (whether to compute minimal deterministic automaton for classical algorithm);
 * @return Complemented automaton.
 */
Lvlfa complement(const Lvlfa& aut, const utils::OrdVector<Symbol>& symbols,
               const ParameterMap& params = {{ "algorithm", "classical" }, { "minimize", "false" }});

/**
 * @brief Compute minimal deterministic automaton.
 *
 * @param[in] aut Automaton whose minimal version to compute.
 * @param[in] params Optional parameters to control the minimization algorithm:
 * - "algorithm": "brzozowski"
 * @return Minimal deterministic automaton.
 */
Lvlfa minimize(const Lvlfa &aut, const ParameterMap& params = {{ "algorithm", "brzozowski" }});

/**
 * @brief Determinize automaton.
 *
 * @param[in] aut Automaton to determinize.
 * @param[out] subset_map Map that maps sets of states of input automaton to states of determinized automaton.
 * @return Determinized automaton.
 */
Lvlfa determinize(const Lvlfa& aut, std::unordered_map<StateSet, State> *subset_map = nullptr);

/**
 * @brief Reduce the size of the automaton.
 *
 * @param[in] aut Automaton to reduce.
 * @param[out] state_renaming Mapping of original states to reduced states.
 * @param[in] params Optional parameters to control the reduction algorithm:
 * - "algorithm": "simulation".
 * @return Reduced automaton.
 */
Lvlfa reduce(const Lvlfa &aut, StateRenaming *state_renaming = nullptr,
           const ParameterMap& params = {{ "algorithm", "simulation" } });

/**
 * @brief Checks inclusion of languages of two LVLFAs: @p smaller and @p bigger (smaller <= bigger).
 *
 * @param[in] smaller First automaton to concatenate.
 * @param[in] bigger Second automaton to concatenate.
 * @param[out] cex Counterexample for the inclusion.
 * @param[in] alphabet Alphabet of both LVLFAs to compute with.
 * @param[in] params Optional parameters to control the equivalence check algorithm:
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p smaller is included in @p bigger, false otherwise.
 */
bool is_included(const Lvlfa& smaller, const Lvlfa& bigger, Run* cex, const Alphabet* alphabet = nullptr,
                 const ParameterMap& params = {{ "algorithm", "antichains" }});

/**
 * @brief Checks inclusion of languages of two LVLFAs: @p smaller and @p bigger (smaller <= bigger).
 *
 * @param[in] smaller First automaton to concatenate.
 * @param[in] bigger Second automaton to concatenate.
 * @param[in] alphabet Alphabet of both LVLFAs to compute with.
 * @param[in] params Optional parameters to control the equivalence check algorithm:
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p smaller is included in @p bigger, false otherwise.
 */
inline bool is_included(const Lvlfa& smaller, const Lvlfa& bigger, const Alphabet* const alphabet = nullptr,
                        const ParameterMap& params = {{ "algorithm", "antichains" }}) {
    return is_included(smaller, bigger, nullptr, alphabet, params);
}

/**
 * @brief Perform equivalence check of two LVLFAs: @p lhs and @p rhs.
 *
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] alphabet Alphabet of both LVLFAs to compute with.
 * @param[in] params[ Optional parameters to control the equivalence check algorithm:
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p lhs and @p rhs are equivalent, false otherwise.
 */
bool are_equivalent(const Lvlfa& lhs, const Lvlfa& rhs, const Alphabet* alphabet,
                    const ParameterMap& params = {{ "algorithm", "antichains"}});

/**
 * @brief Perform equivalence check of two LVLFAs: @p lhs and @p rhs.
 *
 * The current implementation of @c Lvlfa does not accept input alphabet. For this reason, an alphabet
 * has to be created from all transitions each time an operation on alphabet is called. When calling this function,
 * the alphabet has to be computed first.
 *
 * Hence, this function is less efficient than its alternative taking already defined alphabet as its parameter.
 * That way, alphabet has to be computed only once, as opposed to the current ad-hoc construction of the alphabet.
 * The use of the alternative with defined alphabet should be preferred.
 *
 * @param[in] lhs First automaton to concatenate.
 * @param[in] rhs Second automaton to concatenate.
 * @param[in] params Optional parameters to control the equivalence check algorithm:
 * - "algorithm": "naive", "antichains" (Default: "antichains")
 * @return True if @p lhs and @p rhs are equivalent, false otherwise.
 */
bool are_equivalent(const Lvlfa& lhs, const Lvlfa& rhs, const ParameterMap& params = {{ "algorithm", "antichains"}});

// Reverting the automaton by one of the three functions below,
// currently simple_revert seems best (however, not tested enough).
Lvlfa revert(const Lvlfa& aut);

// This revert algorithm is fragile, uses low level accesses to Lvlfa and static data structures,
// and it is potentially dangerous when there are used symbols with large numbers (allocates an array indexed by symbols)
// It is faster asymptotically and for somewhat dense automata,
// the same or a little bit slower than simple_revert otherwise.
// Not affected by pre-reserving vectors.
Lvlfa fragile_revert(const Lvlfa& aut);

// Reverting the automaton by a simple algorithm, which does a lot of random access addition to Post and Move.
//  Much affected by pre-reserving vectors.
Lvlfa simple_revert(const Lvlfa& aut);

// Reverting the automaton by a modification of the simple algorithm.
// It replaces random access addition to SymbolPost by push_back and sorting later, so far seems the slowest of all, except on
//  dense automata, where it is almost as slow as simple_revert. Candidate for removal.
Lvlfa somewhat_simple_revert(const Lvlfa& aut);

// Removing epsilon transitions
Lvlfa remove_epsilon(const Lvlfa& aut, Symbol epsilon = EPSILON);

/** Encodes a vector of strings (each corresponding to one symbol) into a
 *  @c Word instance
 */
 // TODO: rename to something, but no idea to what.
 // Maybe we need some terminology - Symbols and Words are made of numbers.
 // What are the symbol names and their sequences?
Run encode_word(const Alphabet* alphabet, const std::vector<std::string>& input);

} // namespace mata::lvlfa.

namespace std {
std::ostream& operator<<(std::ostream& os, const mata::lvlfa::Lvlfa& lvlfa);
} // namespace std.

#endif /* MATA_LVLFA_HH_ */
