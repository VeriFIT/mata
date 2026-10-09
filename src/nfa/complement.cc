/* nfa-complement.cc -- NFA complement
 */

// MATA headers
#include "mata/nfa/algorithms.hh"
#include "mata/nfa/nfa.hh"
#include "mata/utils/assert.hh"

using namespace mata::nfa;
using namespace mata::utils;

namespace {

/**
 * Drop transitions over symbols not in the provided alphabet.
 * Required because is_complete() expects transitions only over the declared symbols.
 * This ensures the result is a language over the desired alphabet only.
 *
 * @param[in,out] aut Automaton to filter
 * @param[in] symbols Target alphabet; transitions over symbols not in this set are dropped
 */
void drop_foreign_symbols(Nfa& aut, const OrdVector<mata::Symbol>& symbols) {
	if (symbols.empty()) { return; }

	for (State state{0}; state < aut.num_of_states(); ++state) {
		StatePost& state_post{aut.delta.mutable_state_post(state)};
		// Use erase-remove idiom to drop SymbolPosts over foreign symbols.
		//  std::remove_if moves elements to drop to the end and returns the new end iterator;
		//  erase removes them.
		state_post.erase(
			std::remove_if(state_post.begin(), state_post.end(),
				[&symbols](const SymbolPost& sp) { return !haskey(symbols, sp.symbol); }),
			state_post.end()
		);
	}
}

} // namespace

Nfa algorithms::complement_classical(const Nfa& aut, const OrdVector<Symbol>& symbols) {
	if (aut.is_deterministic()) {
		// DFA fast path: copy once, trim, and call complement_deterministic.
		Nfa result{aut};
		result = result.trim();
		drop_foreign_symbols(result, symbols);
		result.make_complete(symbols);
		result.complement_deterministic(symbols);
		return result;
	}
	// NFA: determinize, trim, and complement.
	Nfa result{determinize(aut).trim()};
	drop_foreign_symbols(result, symbols);
	result.make_complete(symbols);
	result.complement_deterministic(symbols);
	return result;
}

Nfa algorithms::complement_brzozowski(const Nfa& aut, const OrdVector<Symbol>& symbols) {
	Nfa result{minimize_brzozowski(aut)}; // Brzozowski minimization makes it deterministic.
	drop_foreign_symbols(result, symbols);
	result.make_complete(symbols);
	if (result.final.empty() && !result.initial.empty()) {
		MATA_ASSERT(result.initial.size() == 1);
		// If the DFA does not accept anything, then there is only one (initial) state which can be the sink state (so
		//  we do not create an unnecessary new sink state).
		return result.complement_deterministic(symbols, *result.initial.begin());
	}
	return result.complement_deterministic(symbols);
}

Nfa mata::nfa::complement(const Nfa& aut, const Alphabet& alphabet, const ParameterMap& params) {
	return complement(aut, alphabet.get_alphabet_symbols(), params);
}

Nfa mata::nfa::complement(const Nfa& aut, const OrdVector<mata::Symbol>& symbols, const ParameterMap& params) {
	if (aut.is_deterministic()) {
		// DFA fast path: copy, trim, and complement directly.
		Nfa result{aut};
		result = result.trim();
		drop_foreign_symbols(result, symbols);
		result.make_complete(symbols);
		result.complement_deterministic(symbols);
		return result;
	}

	// Setting the requested algorithm for NFA.
	decltype(algorithms::complement_classical)* algo = algorithms::complement_classical;
	if (!haskey(params, "algorithm")) {
		throw std::runtime_error(
			std::to_string(__func__) +
			" requires setting the \"algorithm\" key in the \"params\" argument; "
			"received: " +
			std::to_string(params)
		);
	}

	if (const std::string& str_algo = params.at("algorithm"); "classical" == str_algo) { /* default */
	} else if ("brzozowski" == str_algo) {
		algo = algorithms::complement_brzozowski;
	} else {
		throw std::runtime_error(
			std::to_string(__func__) + " received an unknown value of the \"algorithm\" key: " + str_algo
		);
	}

	return algo(aut, symbols);
}

Nfa mata::nfa::complement(Nfa&& aut, const OrdVector<mata::Symbol>& symbols, const ParameterMap& params) {
	if (aut.is_deterministic()) {
		// DFA fast path: trim and complement in place.
		aut = aut.trim();
		drop_foreign_symbols(aut, symbols);
		aut.make_complete(symbols);
		aut.complement_deterministic(symbols);
		return std::move(aut);
	}

	// NFA: delegate to const overload since algorithms make their own copy anyway.
	return complement(static_cast<const Nfa&>(aut), symbols, params);
}
