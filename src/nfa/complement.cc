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
 * Drop the transitions over symbols outside of @p symbols.
 *
 * Complementing with respect to @p symbols yields a language over @p symbols only, so a transition over any other
 *  symbol must not survive the complementation. Restricting before the trim lets the trim remove the states that
 *  only the dropped transitions reached.
 *
 * Only an already deterministic input and Brzozowski minimization need this pass. The classical algorithm restricts
 *  the symbols inside determinize(), which never creates a foreign symbol post in the first place.
 *
 * @param[in,out] aut Automaton to restrict.
 * @param[in] symbols Symbols to keep; transitions over any other symbol are dropped.
 */
void restrict_to_symbols(Nfa& aut, const OrdVector<mata::Symbol>& symbols) {
	for (State state{0}; state < aut.num_of_states(); ++state) {
		StatePost& state_post{aut.delta.mutable_state_post(state)};
		// Use erase-remove idiom to drop SymbolPosts over foreign symbols.
		//  std::remove_if moves elements to drop to the end and returns the new end iterator;
		//  erase removes them.
		state_post.erase(
			std::remove_if(
				state_post.begin(), state_post.end(),
				[&symbols](const SymbolPost& symbol_post) { return !haskey(symbols, symbol_post.symbol); }
			),
			state_post.end()
		);
	}
}

/// Get the complementation algorithm requested by the "algorithm" key of @p params.
decltype(algorithms::complement_classical)* select_algorithm(const ParameterMap& params) {
	if (!haskey(params, "algorithm")) {
		throw std::runtime_error(
			"complement requires setting the \"algorithm\" key in the \"params\" argument; "
			"received: " +
			std::to_string(params)
		);
	}
	const std::string& str_algo{params.at("algorithm")};
	if ("classical" == str_algo) { return algorithms::complement_classical; }
	if ("brzozowski" == str_algo) { return algorithms::complement_brzozowski; }
	throw std::runtime_error("complement received an unknown value of the \"algorithm\" key: " + str_algo);
}

} // namespace

Nfa algorithms::complement_classical(const Nfa& aut, const OrdVector<Symbol>& symbols) {
	if (aut.is_deterministic()) {
		// DFA fast path: determinization would only copy the automaton, so complement the copy directly.
		Nfa result{aut};
		restrict_to_symbols(result, symbols);
		result.trim();
		result.complement_deterministic(symbols); // Completes the automaton over 'symbols' and swaps the finals.
		return result;
	}
	// Determinization keeps only 'symbols', so no separate pass over the delta is needed to drop the other ones.
	Nfa result{determinize(aut, nullptr, std::nullopt, &symbols)};
	result.trim();
	result.complement_deterministic(symbols);
	return result;
}

Nfa algorithms::complement_brzozowski(const Nfa& aut, const OrdVector<Symbol>& symbols) {
	Nfa result{minimize_brzozowski(aut)}; // Brzozowski minimization makes it deterministic.
	restrict_to_symbols(result, symbols);
	if (result.final.empty() && !result.initial.empty()) {
		MATA_ASSERT(result.initial.size() == 1);
		// If the DFA does not accept anything, then there is only one (initial) state which can be the sink state (so
		//  we do not create an unnecessary new sink state). The sink has to be chosen before the completion done by
		//  complement_deterministic(), which would otherwise add a second one.
		result.complement_deterministic(symbols, *result.initial.begin());
		return result;
	}
	result.complement_deterministic(symbols);
	return result;
}

Nfa mata::nfa::complement(const Nfa& aut, const Alphabet& alphabet, const ParameterMap& params) {
	return complement(aut, alphabet.get_alphabet_symbols(), params);
}

Nfa mata::nfa::complement(const Nfa& aut, const OrdVector<mata::Symbol>& symbols, const ParameterMap& params) {
	return select_algorithm(params)(aut, symbols);
}

Nfa mata::nfa::complement(Nfa&& aut, const OrdVector<mata::Symbol>& symbols, const ParameterMap& params) {
	const auto algorithm{select_algorithm(params)};
	// Only the classical algorithm can reuse the input. Brzozowski minimization builds a new automaton anyway and a
	//  nondeterministic input is replaced by its determinization.
	if (algorithm != algorithms::complement_classical || !aut.is_deterministic()) { return algorithm(aut, symbols); }
	// DFA fast path: complement in place, without copying the input.
	restrict_to_symbols(aut, symbols);
	aut.trim();
	aut.complement_deterministic(symbols);
	return std::move(aut);
}
