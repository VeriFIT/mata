/** @file
 * @brief Complement of NFTs.
 */

#include "mata/nft/algorithms.hh"
#include "mata/nft/nft.hh"

using namespace mata::nft;
using namespace mata::utils;

#ifdef MATA_NFT_NOT_IMPLEMENTED
Nft mata::nft::algorithms::complement_classical(const Nft& aut, const OrdVector<Symbol>& symbols,
                                                const bool minimize_during_determinization) {
    Nft result;
    State sink_state;
    if (minimize_during_determinization) {
        result = minimize_brzozowski(aut); // brzozowski minimization makes it deterministic
        if (result.final.empty() && !result.initial.empty()) {
            assert(result.initial.size() == 1);
            // if automaton does not accept anything, then there is only one (initial) state
            // which can be the sink state (so we do not create unnecessary one)
            sink_state = *result.initial.begin();
        } else {
            sink_state = result.num_of_states();
        }
    } else {
        std::unordered_map<StateSet, State> subset_map;
        result = determinize(aut, &subset_map);
        // check if a sink state was not created during determinization
        if (const auto sink_state_iter = subset_map.find({}); sink_state_iter != subset_map.end()) {
            sink_state = sink_state_iter->second;
        } else {
            sink_state = result.num_of_states();
        }
    }

    result.make_complete(symbols, sink_state);
    result.final.complement(result.num_of_states());

    return result;
}

Nft mata::nft::complement(const Nft& aut, const Alphabet& alphabet, const ParameterMap& params) {
    return mata::nft::complement(aut, alphabet.get_alphabet_symbols(), params);
}

Nft mata::nft::complement(const Nft& aut, const mata::utils::OrdVector<mata::Symbol>& symbols, const ParameterMap& params) {
    Nft result;
    // Setting the requested algorithm.
    decltype(algorithms::complement_classical)* algo = algorithms::complement_classical;
    if (!haskey(params, "algorithm")) {
        throw std::runtime_error(std::to_string(__func__) +
                                 " requires setting the \"algorithm\" key in the \"params\" argument; "
                                 "received: " + std::to_string(params));
    }

    const std::string& str_algo = params.at("algorithm");
    if ("classical" == str_algo) {  /* default */ }
    else {
        throw std::runtime_error(std::to_string(__func__) +
                                 " received an unknown value of the \"algorithm\" key: " + str_algo);
    }

    bool minimize_during_determinization = false;
    if (params.contains("minimize")) {
        const std::string& minimize_arg = params.at("minimize");
        if ("true" == minimize_arg) { minimize_during_determinization = true; }
        else if ("false" == minimize_arg) { minimize_during_determinization = false; }
        else {
            throw std::runtime_error(std::to_string(__func__) +
                                     " received an unknown value of the \"minimize\" key: " + str_algo);
        }
    }

    return algo(aut, symbols, minimize_during_determinization);
}
#endif
