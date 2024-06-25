/* nfa-complement.cc -- NFA complement
 */

// MATA headers
#include "mata/nfa/nfa.hh"
#include "mata/nfa/algorithms.hh"

using namespace mata::nfa;
using namespace mata::utils;

Nfa mata::nfa::algorithms::complement_classical(const Nfa& aut, const OrdVector<Symbol>& symbols) {
    return determinize(aut)
        .trim()
        .complement_deterministic(symbols);
}

Nfa algorithms::complement_brzozowski(const Nfa& aut, const OrdVector<Symbol>& symbols) {
    Nfa result{ minimize_brzozowski(aut) }; // Brzozowski minimization makes it deterministic.
    if (result.final.empty() && !result.initial.empty()) {
        assert(result.initial.size() == 1);
        // If the DFA does not accept anything, then there is only one (initial) state which can be the sink state (so
        //  we do not create an unnecessary new sink state).
        return result.complement_deterministic(symbols, *result.initial.begin());
    }
    return result.complement_deterministic(symbols);
}

Nfa mata::nfa::complement(const Nfa& aut, const Alphabet& alphabet, const ParameterMap& params) {
    return mata::nfa::complement(aut, alphabet.get_alphabet_symbols(), params);
}

Nfa mata::nfa::complement(const Nfa& aut, const mata::utils::OrdVector<mata::Symbol>& symbols, const ParameterMap& params) {
    // Setting the requested algorithm.
    decltype(algorithms::complement_classical)* algo = algorithms::complement_classical;
    if (!haskey(params, "algorithm")) {
        throw std::runtime_error(std::to_string(__func__) +
                                 " requires setting the \"algo\" key in the \"params\" argument; "
                                 "received: " + std::to_string(params));
    }

    const std::string& str_algo = params.at("algorithm");
    if ("classical" == str_algo) {  /* default */ }
    else if ("brzozowski" == str_algo) {  algo = algorithms::complement_brzozowski; }
    else {
        throw std::runtime_error(std::to_string(__func__) +
                                 " received an unknown value of the \"algo\" key: " + str_algo);
    }

    return algo(aut, symbols);
}
