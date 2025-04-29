/* cntnfa-complement.cc -- NFA complement
 */

// MATA headers
#include "mata/cntnfa/cntnfa.hh"
#include "mata/cntnfa/algorithms.hh"

using namespace mata::cntnfa;
using namespace mata::utils;

Cntnfa mata::cntnfa::algorithms::complement_classical(const Cntnfa& aut, const OrdVector<Symbol>& symbols) {
    return determinize(aut)
        .trim()
        .complement_deterministic(symbols);
}

Cntnfa algorithms::complement_brzozowski(const Cntnfa& aut, const OrdVector<Symbol>& symbols) {
    Cntnfa result{ minimize_brzozowski(aut) }; // Brzozowski minimization makes it deterministic.
    if (result.final.empty() && !result.initial.empty()) {
        assert(result.initial.size() == 1);
        // If the DFA does not accept anything, then there is only one (initial) state which can be the sink state (so
        //  we do not create an unnecessary new sink state).
        return result.complement_deterministic(symbols, *result.initial.begin());
    }
    return result.complement_deterministic(symbols);
}

Cntnfa mata::cntnfa::complement(const Cntnfa& aut, const Alphabet& alphabet, const ParameterMap& params) {
    return mata::cntnfa::complement(aut, alphabet.get_alphabet_symbols(), params);
}

Cntnfa mata::cntnfa::complement(const Cntnfa& aut, const mata::utils::OrdVector<mata::Symbol>& symbols, const ParameterMap& params) {
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
