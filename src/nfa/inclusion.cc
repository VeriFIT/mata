/* nfa-incl.cc -- NFA language inclusion
 */

// MATA headers
#include "mata/nfa/algorithms.hh"
#include "mata/nfa/nfa.hh"
#include "mata/utils/sparse-set.hh"

using namespace mata::nfa;
using namespace mata::utils;

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
	Nfa nfa_isect = intersection(smaller, bigger_cmpl, Limits::max_symbol, cex != nullptr ? &prod_map : nullptr);

	bool result = nfa_isect.is_lang_empty(cex);
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

	using ProdStateType = std::tuple<State, StateSet, size_t>;
	using ProdStatesType = std::vector<ProdStateType>;

	/**
	 * @brief The macrostates already processed for one state of the smaller automaton, bucketed by cardinality.
	 *
	 * All entries stored here share the same state of the smaller automaton, so the dominance relation reduces to
	 *  set inclusion between the macrostates of the bigger automaton. Cardinality alone then decides which entries
	 *  can possibly be related to a candidate @c succ:
	 *  - @c anti ⊆ @c succ requires @c |anti| ≤ @c |succ|, so the subsumption test reads nothing above @c |succ|;
	 *  - @c succ ⊆ @c d requires @c |d| ≥ @c |succ|, so pruning reads nothing below @c |succ|;
	 *  - at equal cardinality both reduce to equality, which is one comparison of two sorted vectors, and an entry
	 *    equal to @c succ subsumes it, so pruning never has to consider that bucket.
	 *
	 * Only cardinalities that actually hold an entry get a bucket, kept in increasing order: the macrostates of one
	 *  run occupy few distinct cardinalities out of the @c |bigger| possible ones, so indexing by cardinality would
	 *  pay for (and scan) a bucket header per unused size.
	 */
	class ProcessedAntichain {
	  public:
		/// Is some stored macrostate a subset of @p succ, and therefore dominates it?
		bool subsumes(const StateSet& succ) const {
			const size_t cardinality{succ.size()};
			for (const Bucket& bucket : buckets_) {
				if (bucket.cardinality > cardinality) { break; }
				if (bucket.cardinality == cardinality) {
					// A subset of the same cardinality is the set itself.
					return std::ranges::any_of(bucket.entries, [&](const ProdStateType& anti) {
						return std::get<1>(anti) == succ;
					});
				}
				for (const ProdStateType& anti : bucket.entries) {
					if (std::get<1>(anti).is_subset_of(succ)) { return true; }
				}
			}
			return false;
		}

		/// Removes every stored macrostate that @p succ dominates, that is, every strict superset of @p succ.
		void prune(const StateSet& succ) {
			auto bucket{std::ranges::upper_bound(buckets_, succ.size(), {}, &Bucket::cardinality)};
			while (bucket != buckets_.end()) {
				std::erase_if(bucket->entries, [&](const ProdStateType& stored) {
					return succ.is_subset_of(std::get<1>(stored));
				});
				bucket = bucket->entries.empty() ? buckets_.erase(bucket) : bucket + 1;
			}
		}

		void insert(const ProdStateType& pair) {
			const size_t cardinality{std::get<1>(pair).size()};
			auto bucket{std::ranges::lower_bound(buckets_, cardinality, {}, &Bucket::cardinality)};
			if (bucket == buckets_.end() || bucket->cardinality != cardinality) {
				bucket = buckets_.insert(bucket, Bucket{.cardinality = cardinality, .entries = {}});
			}
			bucket->entries.push_back(pair);
		}

	  private:
		struct Bucket {
			size_t cardinality;
			ProdStatesType entries;
		};

		/// The occupied cardinalities only, ordered by increasing cardinality, never holding an empty bucket.
		std::vector<Bucket> buckets_{};
	};

	// Indexed by the states of the smaller nfa; tailored for the pure antichain approach (the simulation-based
	//  antichain would not work without changes).
	using ProcessedType = std::vector<ProcessedAntichain>;

	auto subsumes = [](const ProdStateType& lhs, const ProdStateType& rhs) {
		if (std::get<0>(lhs) != std::get<0>(rhs)) { return false; }
		return std::get<1>(lhs).is_subset_of(std::get<1>(rhs));
	};

	// initialize
	ProdStatesType worklist{}; // Pairs (q,S) to be processed.
	ProcessedType processed(smaller.num_of_states()); // Allocate to the number of states of the smaller nfa.

	// Is |S| < |S'| for the inut pairs (q,S) and (q',S')?
	//  auto smaller_set = [](const ProdStateType & a, const ProdStateType & b) { return std::get<1>(a).size() <
	//  std::get<1>(b).size(); };

	std::vector<State> distances_smaller = smaller.distances_to_final();
	std::vector<State> distances_bigger = bigger.distances_to_final();

	// auto closer_dist = [&](const ProdStateType & a, const ProdStateType & b) {
	//     return distances_smaller[a.first] < distances_smaller[b.first];
	// };

	// auto closer_smaller = [&](const ProdStateType & a, const ProdStateType & b) {
	//     if (distances_smaller[a.first] != distances_smaller[b.first])
	//         return distances_smaller[a.first] < distances_smaller[b.first];
	//     else
	//         return a.second.size() < b.second.size();
	// };

	// auto smaller_closer = [&](const ProdStateType & a, const ProdStateType & b) {
	//     if (a.second.size() != b.second.size())
	//         return a.second.size() < b.second.size();
	//     else
	//         return distances_smaller[a.first] < distances_smaller[b.first];
	// };

	auto min_dst = [&](const StateSet& set) {
		if (set.empty()) { return Limits::max_state; }
		return distances_bigger[*std::ranges::min_element(set, [&](const State a, const State b) {
			return distances_bigger[a] < distances_bigger[b];
		})];
	};

	auto lengths_incompatible = [&](const ProdStateType& pair) {
		return distances_smaller[std::get<0>(pair)] < std::get<2>(pair);
	};

	auto insert_to_pairs = [](ProdStatesType& pairs, const ProdStateType& pair) { pairs.push_back(pair); };

	// 'paths[s] == t' denotes that state 's' was accessed from state 't',
	// 'paths[s] == s' means that 's' is an initial state
	std::map<ProdStateType, std::pair<ProdStateType, Symbol>> paths;

	// check initial states first // TODO: this would be done in the main loop as the first thing anyway?
	for (const auto& state : smaller.initial) {
		if (smaller.final[state] && are_disjoint(bigger.initial, bigger.final)) {
			if (cex != nullptr) {
				cex->word.clear();
				cex->path = {state};
			}
			return false;
		}

		StateSet bigger_state_set{bigger.initial};
		const ProdStateType st = std::tuple(state, bigger_state_set, min_dst(bigger_state_set));
		insert_to_pairs(worklist, st);
		processed[state].insert(st);

		if (cex != nullptr) { paths.insert({st, {st, 0}}); }
	}

	// For synchronised iteration over the set of states
	SynchronizedExistentialSymbolPostIterator sync_iterator;

	// We use DFS strategy for the worklist processing
	while (!worklist.empty()) {
		// get a next product state
		ProdStateType prod_state = *worklist.rbegin();
		worklist.pop_back();

		const State& smaller_state = std::get<0>(prod_state);
		const StateSet& bigger_set = std::get<1>(prod_state);

		sync_iterator.reset();
		for (State q : bigger_set) { mata::utils::push_back(sync_iterator, bigger.delta[q]); }

		// process transitions leaving smaller_state
		for (const auto& smaller_move : smaller.delta[smaller_state]) {
			const Symbol& smaller_symbol = smaller_move.symbol;

			StateSet bigger_succ = {};
			if (sync_iterator.synchronize_with(smaller_move)) { bigger_succ = sync_iterator.unify_targets(); }

			for (const State& smaller_succ : smaller_move.targets) {
				const ProdStateType succ = {smaller_succ, bigger_succ, min_dst(bigger_succ)};

				if (lengths_incompatible(succ) ||
					(smaller.final[smaller_succ] && !bigger.final.intersects_with(bigger_succ))) {
					if (cex != nullptr) {
						cex->word.push_back(smaller_symbol);
						cex->path.push_back(smaller_state);
						auto next_on_path = paths.find(prod_state);
						while (next_on_path->second.first != next_on_path->first) { // go back until initial state
							cex->word.push_back(next_on_path->second.second);
							cex->path.push_back(std::get<0>(next_on_path->second.first));
							next_on_path = paths.find(next_on_path->second.first);
						}

						std::ranges::reverse(cex->word);
						std::ranges::reverse(cex->path);

						// it is possible that lengths_incompatible(succ) was true, which means that cex is not
						// finished, we need to add some shortest accepting run from smaller_suc
						auto [word, path] =
							smaller.get_shortest_accepting_run_from_state(smaller_succ, distances_smaller);
						cex->word.insert(cex->word.end(), word.begin(), word.end());
						cex->path.insert(cex->path.end(), path.begin(), path.end());
					}

					return false;
				}

				// Is some already processed macrostate of smaller_succ a subset of bigger_succ, and therefore
				//  dominates it?
				if (processed[smaller_succ].subsumes(bigger_succ)) { continue; }

				// Pruning: drop every stored pair that succ dominates, in the processed antichain and in the
				//  worklist, and store succ in both.
				processed[smaller_succ].prune(bigger_succ);
				processed[smaller_succ].insert(succ);
				std::erase_if(worklist, [&](const ProdStateType& d) { return subsumes(succ, d); });
				insert_to_pairs(worklist, succ);

				if (cex != nullptr) {
					// also set that succ was accessed from state
					paths[succ] = {prod_state, smaller_symbol};
				}
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
