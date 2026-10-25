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

	/// A pair (q, S) of a state of the smaller automaton and a macrostate of the bigger one, as a node of the search.
	///  Nodes are never removed from @c nodes, so their indices stay valid as the parents a counterexample is read
	///  back through.
	struct Node {
		State smaller_state;
		StateSet bigger_set;
		uint64_t signature;
		/// A later node dominates this one: the worklist skips it and its macrostate is released.
		bool dominated;
	};
	/// How a node was reached, recorded only when a counterexample is requested.
	struct Step {
		/// The node this one was reached from, or the node itself for an initial one.
		size_t parent;
		/// The symbol read on the transition from @c parent.
		Symbol symbol;
	};

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
	 * Inside a bucket the stored signature rejects most of the remaining pairs in O(1), before the node is read and
	 *  the two sorted vectors are merged; see @c algorithms::macrostate_signature().
	 *
	 * Only cardinalities that actually hold an entry get a bucket, kept in increasing order: the macrostates of one
	 *  run occupy few distinct cardinalities out of the @c |bigger| possible ones, so indexing by cardinality would
	 *  pay for (and scan) a bucket header per unused size.
	 *
	 * Every node on the worklist is stored here as well, so marking the pruned nodes as dominated also removes them
	 *  from the worklist, without scanning it.
	 */
	class ProcessedAntichain {
	  public:
		/// Is some stored macrostate a subset of @p succ_set, and therefore dominates it?
		bool subsumes(const std::vector<Node>& nodes, const StateSet& succ_set, const uint64_t succ_signature) const {
			const size_t cardinality{succ_set.size()};
			for (const Bucket& bucket : buckets_) {
				if (bucket.cardinality > cardinality) { break; }
				if (bucket.cardinality == cardinality) {
					// A subset of the same cardinality is the set itself.
					return std::ranges::any_of(bucket.entries, [&](const Entry& anti) {
						return anti.signature == succ_signature && nodes[anti.node].bigger_set == succ_set;
					});
				}
				for (const Entry& anti : bucket.entries) {
					if ((anti.signature & ~succ_signature) != 0) { continue; }
					if (nodes[anti.node].bigger_set.is_subset_of(succ_set)) { return true; }
				}
			}
			return false;
		}

		/// Removes every stored macrostate that @p succ_set dominates, that is, every strict superset of it, and
		///  marks its node as dominated.
		void prune(std::vector<Node>& nodes, const StateSet& succ_set, const uint64_t succ_signature) {
			auto bucket{std::ranges::upper_bound(buckets_, succ_set.size(), {}, &Bucket::cardinality)};
			while (bucket != buckets_.end()) {
				std::erase_if(bucket->entries, [&](const Entry& stored) {
					if ((succ_signature & ~stored.signature) != 0) { return false; }
					Node& node{nodes[stored.node]};
					if (!succ_set.is_subset_of(node.bigger_set)) { return false; }
					node.dominated = true;
					node.bigger_set = StateSet{};
					return true;
				});
				bucket = bucket->entries.empty() ? buckets_.erase(bucket) : bucket + 1;
			}
		}

		void insert(const std::vector<Node>& nodes, const size_t node_idx) {
			const Node& node{nodes[node_idx]};
			const size_t cardinality{node.bigger_set.size()};
			auto bucket{std::ranges::lower_bound(buckets_, cardinality, {}, &Bucket::cardinality)};
			if (bucket == buckets_.end() || bucket->cardinality != cardinality) {
				bucket = buckets_.insert(bucket, Bucket{.cardinality = cardinality, .entries = {}});
			}
			bucket->entries.push_back(Entry{.signature = node.signature, .node = node_idx});
		}

	  private:
		struct Entry {
			uint64_t signature;
			size_t node;
		};
		struct Bucket {
			size_t cardinality;
			std::vector<Entry> entries;
		};

		/// The occupied cardinalities only, ordered by increasing cardinality, never holding an empty bucket.
		std::vector<Bucket> buckets_{};
	};

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
		for (State q : nodes[node_idx].bigger_set) { mata::utils::push_back(sync_iterator, bigger.delta[q]); }

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
						for (size_t on_path{node_idx}; steps[on_path].parent != on_path;
							 on_path = steps[on_path].parent) { // go back until initial state
							cex->word.push_back(steps[on_path].symbol);
							cex->path.push_back(nodes[steps[on_path].parent].smaller_state);
						}

						std::ranges::reverse(cex->word);
						std::ranges::reverse(cex->path);

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
