/* nfa-incl.cc -- NFA language inclusion
 */

// MATA headers
#include "mata/nfa/algorithms.hh"
#include "mata/nfa/nfa.hh"
#include "mata/utils/sparse-set.hh"

#include "antichain-inclusion.hh"

using namespace mata::nfa;
using namespace mata::utils;
using mata::nfa::internal::Node;
using mata::nfa::internal::ProcessedAntichain;
using mata::nfa::internal::Step;

/// naive language inclusion check (complementation + intersection + emptiness)
bool mata::nfa::algorithms::is_included_naive(
	const Nfa& smaller,
	const Nfa& bigger,
	const Alphabet* const alphabet, // TODO: this should not be needed, likewise for equivalence
	Run* cex
) { // {{{
	if (cex != nullptr) {
		// A reused Run is left holding its previous counterexample when inclusion does hold.
		cex->word.clear();
		cex->path.clear();
	}
	Nfa bigger_cmpl;
	if (alphabet == nullptr) {
		bigger_cmpl = complement(bigger, create_alphabet(smaller, bigger));
	} else {
		bigger_cmpl = complement(bigger, *alphabet);
	}

	// The map is only read to translate the counterexample path back to `smaller`'s states, but `intersection()`
	//  inserts every product state into it.
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>> prod_map;
	const Nfa nfa_isect = intersection(smaller, bigger_cmpl, Limits::max_symbol, cex != nullptr ? &prod_map : nullptr);

	const bool result = nfa_isect.is_lang_empty(cex);
	if (cex != nullptr && !result) {
		std::unordered_map<State, State> nfa_isect_state_to_smaller_state;
		for (const auto& [states_orig, state_res] : prod_map) {
			nfa_isect_state_to_smaller_state[state_res] = states_orig.first;
		}
		for (State& path_state : cex->path) { path_state = nfa_isect_state_to_smaller_state[path_state]; }
	}
	return result;
} // is_included_naive }}}

/// language inclusion check using Antichains
// TODO, what about to construct the separator from this?
bool mata::nfa::algorithms::is_included_antichains(
	const Nfa& smaller,
	const Nfa& bigger,
	[[maybe_unused]] const Alphabet* const
		alphabet, // Parameter exists so signature matches AlgoType used by set_algorithm
	Run* cex
) { // {{{
	(void) alphabet;
	if (cex != nullptr) {
		// The counterexample is built by appending; a reused Run would otherwise grow a word its automaton rejects.
		cex->word.clear();
		cex->path.clear();
	}

	// Indexed by the states of the smaller nfa; tailored for the pure antichain approach (the simulation-based
	//  antichain would not work without changes).
	using ProcessedType = std::vector<ProcessedAntichain>;

	// initialize
	std::vector<Node> nodes{};
	std::vector<Step> steps{}; // Indexed like @c nodes; empty unless @p cex is requested.
	std::vector<size_t> worklist{}; // Indices of the nodes to be processed.
	ProcessedType processed(smaller.num_of_states()); // Allocate to the number of states of the smaller nfa.

	const std::vector<State> distances_smaller = smaller.distances_to_final();
	const std::vector<State> distances_bigger = bigger.distances_to_final();

	// A state of either automaton that cannot reach a final state contributes no word. Such states of the smaller
	//  automaton are not explored, and such states of the bigger one are dropped from the macrostates, which only
	//  makes the macrostates smaller and the subsumption more frequent. Final states have distance 0, so neither
	//  hides a counterexample.
	auto drop_useless_bigger_states = [&](StateSet& set) {
		set.erase_if([&](const State state) { return distances_bigger[state] == Limits::max_state; });
	};

	auto min_dst = [&](const StateSet& set) {
		if (set.empty()) { return Limits::max_state; }
		return distances_bigger[*std::ranges::min_element(set, [&](const State a, const State b) {
			return distances_bigger[a] < distances_bigger[b];
		})];
	};

	// check initial states first // TODO: this would be done in the main loop as the first thing anyway?
	for (const auto& state : smaller.initial) {
		if (smaller.final[state] && are_disjoint(bigger.initial, bigger.final)) {
			if (cex != nullptr) {
				cex->word.clear();
				cex->path = {state};
			}
			return false;
		}
		if (distances_smaller[state] == Limits::max_state) { continue; }

		StateSet bigger_state_set{bigger.initial};
		drop_useless_bigger_states(bigger_state_set);
		const uint64_t signature{algorithms::macrostate_signature(bigger_state_set)};
		const size_t node_idx{nodes.size()};
		nodes.push_back(
			Node{
				.smaller_state = state,
				.bigger_set = std::move(bigger_state_set),
				.signature = signature,
				.dominated = false,
			}
		);
		if (cex != nullptr) { steps.push_back(Step{.parent = node_idx, .symbol = 0}); }
		worklist.push_back(node_idx);
		processed[state].insert(nodes, node_idx);
	}

	// For synchronised iteration over the set of states
	SynchronizedExistentialSymbolPostIterator sync_iterator;

	// We use DFS strategy for the worklist processing
	while (!worklist.empty()) {
		// get a next product state
		const size_t node_idx{worklist.back()};
		worklist.pop_back();
		if (nodes[node_idx].dominated) { continue; }

		// Read by value: new nodes are appended below and may move the node.
		const State smaller_state{nodes[node_idx].smaller_state};

		sync_iterator.reset();
		for (const State q : nodes[node_idx].bigger_set) { mata::utils::push_back(sync_iterator, bigger.delta[q]); }

		// process transitions leaving smaller_state
		for (const auto& smaller_move : smaller.delta[smaller_state]) {
			const Symbol& smaller_symbol = smaller_move.symbol;

			StateSet bigger_succ = {};
			if (sync_iterator.synchronize_with(smaller_move)) { bigger_succ = sync_iterator.unify_targets(); }
			drop_useless_bigger_states(bigger_succ);
			// Functions of `bigger_succ`, which all the targets of this move share.
			const size_t bigger_succ_min_dst{min_dst(bigger_succ)};
			const uint64_t bigger_succ_signature{algorithms::macrostate_signature(bigger_succ)};

			const auto& smaller_targets{smaller_move.targets};
			for (size_t target_idx{0}; target_idx < smaller_targets.size(); ++target_idx) {
				const State smaller_succ{smaller_targets[target_idx]};
				const State smaller_succ_dst{distances_smaller[smaller_succ]};
				if (smaller_succ_dst == Limits::max_state) { continue; }

				// Exactly the final states have distance 0, so this also catches a final smaller_succ whose
				//  bigger_succ holds no final state.
				if (smaller_succ_dst < bigger_succ_min_dst) {
					if (cex != nullptr) {
						cex->word.push_back(smaller_symbol);
						cex->path.push_back(smaller_state);
						internal::trace_back(nodes, steps, node_idx, *cex);

						// The successor may only be too far from a final state, so the counterexample is completed
						//  by a shortest accepting run from smaller_succ.
						auto [word, path] =
							smaller.get_shortest_accepting_run_from_state(smaller_succ, distances_smaller);
						cex->word.insert(cex->word.end(), word.begin(), word.end());
						cex->path.insert(cex->path.end(), path.begin(), path.end());
					}

					return false;
				}

				// Is some already processed macrostate of smaller_succ a subset of bigger_succ, and therefore
				//  dominates it?
				// 'bigger_succ' is moved from only for the last target, after which the loop ends.
				// NOLINTNEXTLINE(bugprone-use-after-move)
				if (processed[smaller_succ].subsumes(nodes, bigger_succ, bigger_succ_signature)) { continue; }

				// Pruning: drop every stored node that succ dominates, in the processed antichain and thereby in the
				//  worklist, and store succ in both.
				processed[smaller_succ].prune(nodes, bigger_succ, bigger_succ_signature);
				const size_t succ_idx{nodes.size()};
				// The last target of the move is the last reader of bigger_succ.
				const bool last_target{target_idx + 1 == smaller_targets.size()};
				nodes.push_back(
					Node{
						.smaller_state = smaller_succ,
						.bigger_set = last_target ? std::move(bigger_succ) : bigger_succ,
						.signature = bigger_succ_signature,
						.dominated = false,
					}
				);
				if (cex != nullptr) { steps.push_back(Step{.parent = node_idx, .symbol = smaller_symbol}); }
				processed[smaller_succ].insert(nodes, succ_idx);
				worklist.push_back(succ_idx);
			}
		}
	}
	return true;
} // }}}

namespace {
using AlgoType = decltype(algorithms::is_included_naive)*;

bool compute_equivalence(
	const Nfa& lhs, const Nfa& rhs, const mata::Alphabet* const alphabet, const AlgoType& algo, Run* const cex
) {
	// alphabet should not be needed as input parameter
	if (algo(lhs, rhs, alphabet, cex)) {
		if (algo(rhs, lhs, alphabet, cex)) { return true; }
	}

	return false;
}

AlgoType set_algorithm(const std::string& function_name, const ParameterMap& params) {
	if (!haskey(params, "algorithm")) {
		throw std::runtime_error(
			function_name +
			" requires setting the \"algorithm\" key in the \"params\" argument; "
			"received: " +
			std::to_string(params)
		);
	}

	decltype(algorithms::is_included_naive)* algo;
	if (const std::string& str_algo = params.at("algorithm"); "naive" == str_algo) {
		algo = algorithms::is_included_naive;
	} else if ("antichains" == str_algo) {
		algo = algorithms::is_included_antichains;
	} else {
		throw std::runtime_error(
			std::to_string(__func__) + " received an unknown value of the \"algorithm\" key: " + str_algo
		);
	}

	return algo;
}

/// Does @p nfa have a transition on @c EPSILON? EPSILON is the largest symbol, so one symbol post per state is read.
bool contains_epsilon(const Nfa& nfa) {
	const size_t num_of_states{nfa.num_of_states()};
	for (State state{0}; state < num_of_states; ++state) {
		const StatePost& state_post{nfa.delta[state]};
		if (Delta::epsilon_symbol_posts(state_post) != state_post.end()) { return true; }
	}
	return false;
}

/// An epsilon-free view of @p nfa, stored in @p storage only when @p nfa actually has epsilon transitions.
const Nfa& epsilon_free(const Nfa& nfa, Nfa& storage) {
	if (!contains_epsilon(nfa)) { return nfa; }
	storage = remove_epsilon(nfa);
	return storage;
}

} // namespace

bool mata::nfa::algorithms::are_equivalent_epsilon_as_symbol(
	const Nfa& lhs, const Nfa& rhs, const Alphabet* const alphabet, const ParameterMap& params, Run* const cex
) {
	// __func__ names the caller in set_algorithm\'s error message
	AlgoType algo{set_algorithm(std::to_string(__func__), params)};

	if (params.at("algorithm") == "naive") {
		if (alphabet == nullptr) {
			const auto computed_alphabet{create_alphabet(lhs, rhs)};
			return compute_equivalence(lhs, rhs, &computed_alphabet, algo, cex);
		}
	}

	return compute_equivalence(lhs, rhs, alphabet, algo, cex);
}

// The dispatching method that calls the correct one based on parameters
bool mata::nfa::is_included(
	const Nfa& smaller,
	const Nfa& bigger,
	Run* cex,
	const Alphabet* const alphabet,
	const ParameterMap& params
) { // {{{
	AlgoType algo{set_algorithm(std::to_string(__func__), params)};
	// Both algorithms read EPSILON as an ordinary letter, so epsilon has to go before they see the automata.
	Nfa smaller_storage{};
	Nfa bigger_storage{};
	return algo(epsilon_free(smaller, smaller_storage), epsilon_free(bigger, bigger_storage), alphabet, cex);
} // is_included }}}

bool mata::nfa::are_equivalent(
	const Nfa& lhs, const Nfa& rhs, const Alphabet* alphabet, const ParameterMap& params, Run* const cex
) {
	Nfa lhs_storage{};
	Nfa rhs_storage{};
	return algorithms::are_equivalent_epsilon_as_symbol(
		epsilon_free(lhs, lhs_storage), epsilon_free(rhs, rhs_storage), alphabet, params, cex
	);
}

bool mata::nfa::are_equivalent(const Nfa& lhs, const Nfa& rhs, const ParameterMap& params, Run* const cex) {
	return are_equivalent(lhs, rhs, nullptr, params, cex);
}
