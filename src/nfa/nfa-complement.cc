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
#include <mata/nfa.hh>
#include <mata/nfa-algorithms.hh>

using namespace Mata::Nfa;
using namespace Mata::util;

Nfa Mata::Nfa::Algorithms::complement_classical(
	const Nfa&         aut,
	const Alphabet&    alphabet,
	SubsetMap*         subset_map)
{ // {{{
 	Nfa result;

 	bool delete_subset_map = false;
	if  (nullptr == subset_map)
	{
		subset_map = new SubsetMap();
		delete_subset_map = true;
	}

	result = determinize(aut, subset_map);
	State sink_state = result.get_num_of_states() + 1;
	result.increase_size(sink_state+1);
	assert(sink_state < result.get_num_of_states());
	auto it_inserted_pair = subset_map->insert({{}, sink_state});
	if (!it_inserted_pair.second)
	{
		sink_state = it_inserted_pair.first->second;
	}

	make_complete(result, alphabet, sink_state);
	StateSet old_fs = std::move(result.finalstates);
	result.finalstates = { };
	assert(result.initialstates.size() == 1);

	auto make_final_if_not_in_old = [&](const State& state) {
		if (!haskey(old_fs, state))
		{
			result.finalstates.insert(state);
		}
	};

	make_final_if_not_in_old(*result.initialstates.begin());

	for (const auto& trs : result) {
                make_final_if_not_in_old(trs.tgt);
	}

	if (delete_subset_map)
	{
		delete subset_map;
	}

	return result;
} // complement_classical }}}


/// Complement
Nfa Mata::Nfa::Algorithms::complement_naive(
        const Nfa&         aut,
        const Alphabet&    alphabet,
        const StringDict&  params,
        SubsetMap*         subset_map)
{
    Nfa result = determinize(aut);
    complement_in_place(result);

    return result;
}

void Mata::Nfa::complement_in_place(Nfa& aut) {
    StateSet newFinalStates;

    const size_t size = aut.transitionrelation.size();
    for (State q = 0; q < size; ++q) {
        if (aut.finalstates.count(q) == 0) {
            newFinalStates.insert(q);
        }
    }

    aut.finalstates = newFinalStates;
}

Nfa Mata::Nfa::complement(
	const Nfa&         aut,
	const Alphabet&    alphabet,
	const StringDict&  params,
	SubsetMap*         subset_map)
{
	Nfa result;
	// setting the default algorithm
	decltype(Algorithms::complement_classical)* algo = Algorithms::complement_classical;
	if (!haskey(params, "algo")) {
		throw std::runtime_error(std::to_string(__func__) +
			" requires setting the \"algo\" key in the \"params\" argument; "
			"received: " + std::to_string(params));
	}

	const std::string& str_algo = params.at("algo");
	if ("classical" == str_algo) {  /* default */ }
	else {
		throw std::runtime_error(std::to_string(__func__) +
			" received an unknown value of the \"algo\" key: " + str_algo);
	}

	return algo(aut, alphabet, subset_map);
} // complement
