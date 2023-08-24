/* nfa-complement.cc -- NFA complement
 *
 * Copyright (c) 2020 Ondrej Lengal <ondra.lengal@gmail.com>
 *
 * This file is a part of libmata.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

// MATA headers
#include "mata/nfa/nfa.hh"
#include "mata/nfa/algorithms.hh"

using namespace mata::nfa;
using namespace mata::utils;

Nfa mata::nfa::algorithms::complement_classical(const Nfa& aut, const OrdVector<Symbol>& symbols,
                                                bool minimize_during_determinization) {
    Nfa result;
    State sink_state;
    if (minimize_during_determinization) {
        result = minimize_brzozowski(aut); // brzozowski minimization makes it deterministic
        if (result.final.empty() && !result.initial.empty()) {
            assert(result.initial.size() == 1);
            // if automaton does not accept anything, then there is only one (initial) state
            // which can be the sink state (so we do not create unnecessary one)
            sink_state = *result.initial.begin();
        } else {
            sink_state = result.size();
        }
    } else {
        std::unordered_map<StateSet, State> subset_map;
        result = determinize(aut, &subset_map);
        // check if a sink state was not created during determinization
        auto sink_state_iter = subset_map.find({});
        if (sink_state_iter != subset_map.end()) {
            sink_state = sink_state_iter->second;
        } else {
            sink_state = result.size();
        }
    }

    make_complete(result, symbols, sink_state);
    result.final.complement(result.size());
    return result;
}

Nfa mata::nfa::complement(const Nfa& aut, const Alphabet& alphabet, const ParameterMap& params) {
    return mata::nfa::complement(aut, alphabet.get_alphabet_symbols(), params);
}

Nfa mata::nfa::complement(const Nfa& aut, const mata::utils::OrdVector<mata::Symbol>& symbols, const ParameterMap& params) {
    Nfa result;
    // Setting the requested algorithm.
    decltype(algorithms::complement_classical)* algo = algorithms::complement_classical;
    if (!haskey(params, "algorithm")) {
        throw std::runtime_error(std::to_string(__func__) +
                                 " requires setting the \"algo\" key in the \"params\" argument; "
                                 "received: " + std::to_string(params));
    }

    const std::string& str_algo = params.at("algorithm");
    if ("classical" == str_algo) {  /* default */ }
    else {
        throw std::runtime_error(std::to_string(__func__) +
                                 " received an unknown value of the \"algo\" key: " + str_algo);
    }

    bool minimize_during_determinization = false;
    if (params.find("minimize") != params.end()) {
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
