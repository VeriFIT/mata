/** @file
 * @brief Complement of NFTs.
 */

#include "mata/nft/algorithms.hh"
#include "mata/nft/nft.hh"

using namespace mata::nft;
using namespace mata::utils;

Nft algorithms::complement_classical(
        const Nft& aut, const OrdVector<Symbol>& symbols, const bool minimize_during_determinization) {
    Nft result;
    if (minimize_during_determinization) {
        throw std::runtime_error(
                std::string(__func__) +
                ": unimplemented option 'minimize' = 'true' for the complementation algorithm on NFTs."
                );
        // TODO(nft): implement minimization for NFTs.
        // result = determinize_and_minimize(aut);
        // result = minimize_brzozowski(aut); // brzozowski minimization makes it deterministic
        // if (result.final.empty() && !result.initial.empty()) {
        //     assert(result.initial.size() == 1);
        //     // if automaton does not accept anything, then there is only one (initial) state
        //     // which can be the sink state (so we do not create unnecessary one)
        //     sink_state = *result.initial.begin();
        // } else {
        //     sink_state = result.num_of_states();
        // }
    } else { result = determinize(aut); }

    result.make_complete(symbols);
    // TODO(nft): Should empty nft produce a complement with one initial and final state accepting { {epsilon}, {epsilon}, {epsilon} }?
    if (result.initial.empty()) {
        const State initial{ result.add_state() };
        result.initial.insert(initial);
    }
    SparseSet<State> new_final_states{};
    const size_t num_of_states{ result.num_of_states() };
    for (State state{ 0 }; state < num_of_states; ++state) {
        if (result.levels[state] == 0 && !result.final.contains(state)) { new_final_states.insert(state); }
    }
    result.final = new_final_states;
    return result;
}

Nft mata::nft::complement(const Nft& nft, const Alphabet& alphabet, const ParameterMap& params) {
    return complement(nft, alphabet.get_alphabet_symbols(), params);
}

Nft mata::nft::complement(const Nft& nft, const OrdVector<Symbol>& symbols, const ParameterMap& params) {
    // Setting the requested algorithm.
    decltype(algorithms::complement_classical)* algo = algorithms::complement_classical;
    if (!haskey(params, "algorithm")) {
        throw std::runtime_error(
                std::to_string(__func__) +
                " requires setting the 'algorithm' key in the 'params' argument; "
                "received: " + std::to_string(params)
                );
    }

    if (const std::string& str_algo = params.at("algorithm"); "classical" == str_algo) { /* default */ } else {
        throw std::runtime_error(
            std::to_string(__func__) +
            " received an unknown value of the 'algorithm' key: " + str_algo
        );
    }

    bool minimize_during_determinization{ false };
    if (params.contains("minimize")) {
        if (const std::string& minimize_arg = params.at("minimize");
            "true" == minimize_arg) {
            throw std::runtime_error(
                    std::to_string(__func__) +
                    " received unimplemented option 'minimize' = 'true' for the complementation algorithm on NFTs."
                    );
            // TODO(nft): implement minimization for NFTs.
            // minimize_during_determinization = true;
        } else if ("false" == minimize_arg) { minimize_during_determinization = false; } else {
            throw std::runtime_error(
                    std::to_string(__func__) + " received an unknown value of the 'minimize' key: " + minimize_arg
                    );
        }
    }
    return algo(nft, symbols, minimize_during_determinization);
}
