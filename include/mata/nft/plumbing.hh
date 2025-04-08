/* nfa-plumbings.hh -- Wrapping up different supporting functions.
 */

#ifndef MATA_NFT_PLUMBING_HH_
#define MATA_NFT_PLUMBING_HH_


#include "nft.hh"
#include "builder.hh"


using namespace mata::nft::builder;

/**
 * Simplified NFA API, used in binding to call NFA algorithms.
 *
 * In particular, this mostly includes operations and checks, that do not return Automaton,
 * but instead take resulting automaton as pointer (e.g. `void f(Nft* result, const Nft& lhs, const Nft& rhs)`).
 */
namespace mata::nft::plumbing {


inline void get_elements(StateSet* element_set, const BoolVector& bool_vec) {
    element_set->clear();
    element_set->reserve(bool_vec.count());
    for (size_t i{ 0 }; i < bool_vec.size(); ++i) {
        if (bool_vec[i] == 1) {
            element_set->push_back(i);
        }
    }
}

inline void complement(
        Nft*               result,
        const Nft&         aut,
        const Alphabet&    alphabet,
        const ParameterMap&  params = {{ "algorithm", "classical"},
                                       { "minimize",  "false"}}) { *result = complement(aut, alphabet, params);
}

inline void minimize(Nft* res, const Nft &aut) { *res = minimize(aut); }

inline void determinize(Nft* result, const Nft& aut, std::unordered_map<StateSet, State> *subset_map = nullptr) {
    *result = determinize(aut, subset_map);
}

inline void reduce(Nft* result, const Nft &aut, StateRenaming *state_renaming = nullptr,
                   const ParameterMap& params = {{ "algorithm", "simulation"}}) {
    *result = reduce(aut, state_renaming, params);
}

inline void revert(Nft* result, const Nft& aut) { *result = revert(aut); }

inline void remove_epsilon(Nft* result, const Nft& aut, Symbol epsilon = EPSILON) { *result = remove_epsilon(aut, epsilon); }

/** Loads an automaton from Parsed object */
template <class ParsedObject>
void construct(Nft* result, const ParsedObject& parsed, Alphabet* alphabet = nullptr,
               NameStateMap* state_map = nullptr) {
    OnTheFlyAlphabet tmp_alphabet{};
    if (!alphabet) { alphabet = &tmp_alphabet; }
    *result = builder::construct(parsed, alphabet, state_map);
}

inline void union_nondet(Nft *unionAutomaton, const Nft &lhs, const Nft &rhs) { *unionAutomaton = union_nondet(lhs, rhs); }

/**
 * @brief Compute intersection of two NFTs.
 *
 * Both automata can contain ε-transitions. Epsilons will be handled as alphabet symbols.
 *
 * Transducers must share alphabets. //TODO: this is not implemented yet.
 * Transducers must have equal values of @c num_of_levels.
 *
 * @param[in] lhs First NFT to compute intersection for.
 * @param[in] rhs Second NFT to compute intersection for.
 * @param[out] prod_map Mapping of pairs of the original states (lhs_state, rhs_state) to new product states (not used internally, allocated only when !=nullptr, expensive).
 * @param[in] jump_mode Specifies if the symbol on a jump transition (a transition with a length greater than 1)
 *  is interpreted as a sequence repeating the same symbol or as a single instance of the symbol followed by a sequence of @c DONT_CARE
 * @param[in] lhs_first_aux_state The first auxiliary state in @p lhs. Two auxiliary states does not form a product state.
 * @param[in] rhs_first_aux_state The first auxiliary state in @p rhs. Two auxiliary states does not form a product state.
 * @return NFT as a product of NFTs @p lhs and @p rhs.
 */
inline void intersection(Nft* res, const Nft& lhs, const Nft& rhs,
                  std::unordered_map<std::pair<State, State>, State> *prod_map = nullptr, const JumpMode jump_mode = JumpMode::RepeatSymbol,
                  const State lhs_first_aux_state = Limits::max_state, const State rhs_first_aux_state = Limits::max_state) {
    *res = intersection(lhs, rhs, prod_map, jump_mode, lhs_first_aux_state, rhs_first_aux_state);
}

/**
 * @brief Concatenate two NFAs.
 * @param[out] lhs_result_state_renaming Map mapping lhs states to result states.
 * @param[out] rhs_result_state_renaming Map mapping rhs states to result states.
 */
inline void concatenate(Nft* res, const Nft& lhs, const Nft& rhs, bool use_epsilon = false,
                 StateRenaming* lhs_result_state_renaming = nullptr, StateRenaming* rhs_result_state_renaming = nullptr) {
    *res = concatenate(lhs, rhs, use_epsilon, lhs_result_state_renaming, rhs_result_state_renaming);
}

} // namespace mata::nfa::Plumbing.

#endif // MATA_NFA_PLUMBING_HH_
