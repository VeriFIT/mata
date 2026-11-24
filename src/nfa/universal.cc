/* nfa-universal.cc -- NFA universality
 */

// MATA headers
#include "mata/nfa/algorithms.hh"
#include "mata/nfa/nfa.hh"
#include "mata/utils/sparse-set.hh"

#include "antichain-inclusion.hh"

using namespace mata::nfa;
using namespace mata::utils;

// TODO: this could be merged with inclusion, or even removed, universality could be implemented using inclusion,
//  it is not something needed in practice, so some little overhead is ok

bool mata::nfa::algorithms::is_universal_naive(const Nfa& aut, const Alphabet& alphabet, Run* cex) {
	if (cex != nullptr) {
		cex->word.clear();
		cex->path.clear();
	}
	// The fused subset search stops at the first rejected word; it reads EPSILON as a letter, as complement() does.
	const std::optional<Word> rejected_word{aut.get_word_from_complement(&alphabet)};
	if (!rejected_word.has_value()) { return true; }
	if (cex != nullptr) { cex->word = *rejected_word; }
	return false;
}

bool mata::nfa::algorithms::is_universal_antichains(const Nfa& aut, const Alphabet& alphabet,
													Run* cex) { // {{{
	if (cex != nullptr) {
		cex->word.clear();
		cex->path.clear();
	}

	/// Stored once so worklist and dominance buckets can share stable IDs.
	struct Macrostate {
		StateSet bigger_set{};
		uint64_t signature{0};
	};

	// process parameters
	// TODO: set correctly!!!!
	constexpr bool is_dfs = true;

	// check the initial state
	if (are_disjoint(aut.initial, aut.final)) {
		if (nullptr != cex) { cex->word.clear(); }
		return false;
	}

	// initialize
	StateSet initial_set{aut.initial};
	const uint64_t initial_signature{macrostate_signature(initial_set)};
	std::vector<Macrostate> nodes{{std::move(initial_set), initial_signature}};
	std::vector<size_t> worklist{0};
	internal::ProcessedAntichain processed{};
	processed.insert(nodes, 0);
	const mata::utils::OrdVector<Symbol> alph_symbols = alphabet.get_alphabet_symbols();

	// Record parent IDs only when a counterexample is requested. They remain valid when a node is pruned.
	std::vector<internal::Step> steps{};
	if (nullptr != cex) { steps.push_back(internal::Step{.parent = 0, .symbol = 0}); }
	auto set_rejected_word = [&](const size_t node_idx, const Symbol symbol) {
		cex->word.push_back(symbol);
		for (size_t current{node_idx}; steps[current].parent != current; current = steps[current].parent) {
			cex->word.push_back(steps[current].symbol);
		}
		std::ranges::reverse(cex->word);
	};

	using SyncIterator = mata::nfa::SynchronizedExistentialSymbolPostIterator;

	while (!worklist.empty()) {
		size_t node_idx{0};
		if (is_dfs) {
			node_idx = worklist.back();
			worklist.pop_back();
		} else { // BFS
			node_idx = worklist.front();
			worklist.erase(worklist.begin());
		}
		// Every live macrostate contains a final state and is nonempty; pruning empties the set to mark dominance.
		if (nodes[node_idx].bigger_set.empty()) { continue; }

		// The synchronized iterator owns its state-post cursors even if a later successor prunes this node.
		SyncIterator sync_it{};
		for (const State orig_state : nodes[node_idx].bigger_set) {
			mata::utils::push_back(sync_it, aut.delta[orig_state]);
		}

		auto alph_it = alph_symbols.begin();
		const auto alph_end = alph_symbols.end();
		bool sync_it_advanced = sync_it.advance();

		while (sync_it_advanced || alph_it != alph_end) {
			if (!sync_it_advanced) {
				// The iterator is exhausted but there are remaining alphabet symbols.
				// This means a symbol is missing: immediate counterexample.
				MATA_ASSERT(alph_it != alph_end);
				if (nullptr != cex) { set_rejected_word(node_idx, *alph_it); }
				return false;
			}

			MATA_ASSERT(sync_it_advanced);
			const Symbol sync_symbol = sync_it.get_current_minimum()->symbol;

			// Skip foreign symbols (those not in the alphabet), or check for gaps
			if (alph_it == alph_end || sync_symbol < *alph_it) {
				// Foreign symbol: skip the iterator, stay at current alphabet position.
				sync_it_advanced = sync_it.advance();
				continue;
			}

			if (sync_symbol > *alph_it) {
				// Missing alphabet symbol: immediate counterexample.
				if (nullptr != cex) { set_rejected_word(node_idx, *alph_it); }
				return false;
			}

			// sync_symbol == *alph_it: compute the successor macrostate
			StateSet succ_set = sync_it.unify_targets();
			if (!aut.final.intersects_with(succ_set)) {
				// The successor macrostate does not contain any final state: counterexample found.
				if (nullptr != cex) { set_rejected_word(node_idx, sync_symbol); }
				return false;
			}

			const uint64_t succ_signature{macrostate_signature(succ_set)};
			if (processed.subsumes(nodes, succ_set, succ_signature)) {
				++alph_it;
				sync_it_advanced = sync_it.advance();
				continue;
			}

			// Mark strict supersets dominated rather than rescanning the worklist; IDs and witness steps stay valid.
			processed.prune(nodes, succ_set, succ_signature, [](Macrostate& node) { node.bigger_set = StateSet{}; });
			const size_t succ_idx{nodes.size()};
			nodes.push_back(Macrostate{std::move(succ_set), succ_signature});
			if (nullptr != cex) { steps.push_back(internal::Step{.parent = node_idx, .symbol = sync_symbol}); }
			processed.insert(nodes, succ_idx);
			worklist.push_back(succ_idx);

			// Advance both iterators
			++alph_it;
			sync_it_advanced = sync_it.advance();
		}
	}

	return true;
} // }}}

namespace {
/// Does @p nfa have a transition on @c EPSILON? EPSILON is the largest symbol, so one symbol post per state is read.
bool contains_epsilon(const Nfa& nfa) {
	const size_t num_of_states{nfa.num_of_states()};
	for (State state{0}; state < num_of_states; ++state) {
		const StatePost& state_post{nfa.delta[state]};
		if (Delta::epsilon_symbol_posts(state_post) != state_post.end()) { return true; }
	}
	return false;
}
} // namespace

// The dispatching method that calls the correct one based on parameters.
bool mata::nfa::Nfa::is_universal(const Alphabet& alphabet, Run* cex, const ParameterMap& params) const {
	// setting the default algorithm
	// 'algo' is reassigned in the 'else if' below, which 'misc-const-correctness' does not see.
	// NOLINTNEXTLINE(misc-const-correctness)
	decltype(algorithms::is_universal_naive)* algo = algorithms::is_universal_naive;
	if (!haskey(params, "algorithm")) {
		throw std::runtime_error(
			std::string{__func__} +
			" requires setting the \"algorithm\" key in the \"params\" argument; "
			"received: " +
			mata::utils::to_string(params)
		);
	}

	if (const std::string& str_algo = params.at("algorithm"); "naive" == str_algo) { /* default */
	} else if ("antichains" == str_algo) {
		algo = algorithms::is_universal_antichains;
	} else {
		throw std::runtime_error(
			std::string{__func__} + " received an unknown value of the \"algorithm\" key: " + str_algo
		);
	}
	// Both algorithms read EPSILON as an ordinary letter, so epsilon has to go before they see the automaton, and it
	//  must not stay in the alphabet either (e.g. one built by `create_alphabet()` from an epsilon-NFA).
	const auto run{[&](const Alphabet& letters) {
		if (contains_epsilon(*this)) { return algo(mata::nfa::remove_epsilon(*this), letters, cex); }
		return algo(*this, letters, cex);
	}};
	if (OrdVector<Symbol> symbols{alphabet.get_alphabet_symbols()}; !symbols.empty() && symbols.back() == EPSILON) {
		symbols.pop_back();
		return run(EnumAlphabet{std::move(symbols)});
	}
	return run(alphabet);
} // is_universal()

bool mata::nfa::Nfa::is_universal(const Alphabet& alphabet, const ParameterMap& params) const {
	return this->is_universal(alphabet, nullptr, params);
}
