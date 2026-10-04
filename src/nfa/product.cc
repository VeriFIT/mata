/** @file
 * @brief NFA intersection (product) construction.
 */

// MATA headers
#include "mata/nfa/algorithms.hh"
#include "mata/nfa/nfa.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/two-dimensional-map.hh"
#include <algorithm>
#include <functional>

using namespace mata::nfa;

namespace mata::nfa {

// TODO: move this method to nfa.hh? It is something one might want to use (e.g. for union, inclusion, equivalence of
// DFAs).
Nfa mata::nfa::algorithms::product(
	const Nfa& lhs,
	const Nfa& rhs,
	const std::function<bool(State, State)>& final_condition,
	const Symbol first_epsilon,
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>>* product_map
) {
	Nfa product{}; // The product automaton.
	utils::TwoDimensionalMap<State> product_storage{lhs.num_of_states(), rhs.num_of_states()};
	std::deque<State> worklist{}; // Set of product states to process.

	/**
	 * Add symbol_post for the product state (lhs,rhs) to the product, used for epsilons only (it is simpler for normal
	 * symbols).
	 * @param[in] pair_to_process Currently processed pair of original states.
	 * @param[in] new_product_symbol_post State transitions to add to the product.
	 */
	auto add_product_e_post = [&](const State lhs_source, const State rhs_source, SymbolPost& new_product_symbol_post) {
		if (new_product_symbol_post.empty()) { return; }

		const State product_source = product_storage.get(lhs_source, rhs_source);

		if (StatePost& product_state_post{product.delta.mutable_state_post(product_source)};
			product_state_post.empty() || new_product_symbol_post.symbol > product_state_post.back().symbol) {
			product_state_post.push_back(std::move(new_product_symbol_post));
		} else {
			if (const auto symbol_post_it = product_state_post.find(new_product_symbol_post.symbol);
				symbol_post_it == product_state_post.end()) {
				product_state_post.insert(std::move(new_product_symbol_post));
			}
			// Epsilons are not inserted in order, we insert all lhs epsilons and then all rhs epsilons.
			//  It can happen that we insert an e-transition from lhs and then another with the same e from rhs. Both
			//  sides can reach the same product state (e.g. with an e-self-loop on both sides), so this merge
			//  deduplicates.
			else {
				symbol_post_it->insert(new_product_symbol_post.targets);
			}
		}
	};

	/**
	 * Create product state if it does not exist in storage yet and fill in its symbol_post from lhs and rhs targets.
	 * @param[in] lhs_target Target state in NFA @c lhs.
	 * @param[in] rhs_target Target state in NFA @c rhs.
	 * @param[out] product_symbol_post New SymbolPost of the product state.
	 */
	auto create_product_state_and_symbol_post = [&](const State lhs_target, const State rhs_target,
													SymbolPost& product_symbol_post) {
		State product_target = product_storage.get(lhs_target, rhs_target);

		if (product_target == Limits::max_state) {
			product_target = product.add_state();
			MATA_ASSERT(product_target < Limits::max_state);

			product_storage.insert(lhs_target, rhs_target, product_target);
			if (product_map != nullptr) { (*product_map)[{lhs_target, rhs_target}] = product_target; }

			worklist.push_back(product_target);

			if (final_condition(lhs_target, rhs_target)) { product.final.insert(product_target); }
		}
		// The caller sorts @p product_symbol_post once it is complete: inserting the targets in a sorted order here
		//  would shift O(|targets|) elements per target, since the ids do not arrive in an increasing order.
		product_symbol_post.push_back(product_target);
	};

	// Initialize pairs to process with initial state pairs.
	for (const State lhs_initial_state : lhs.initial) {
		for (const State rhs_initial_state : rhs.initial) {
			// Update product with initial state pairs.
			const State product_initial_state = product.add_state();
			product_storage.insert(lhs_initial_state, rhs_initial_state, product_initial_state);
			if (product_map != nullptr) {
				(*product_map)[{lhs_initial_state, rhs_initial_state}] = product_initial_state;
			}
			worklist.push_back(product_initial_state);
			product.initial.insert(product_initial_state);
			if (final_condition(lhs_initial_state, rhs_initial_state)) { product.final.insert(product_initial_state); }
		}
	}

	while (!worklist.empty()) {
		const State product_source = worklist.back();
		worklist.pop_back();
		const State lhs_source = product_storage.get_first_inverted(product_source);
		const State rhs_source = product_storage.get_second_inverted(product_source);

		// Compute classic product for current state pair.
		const StatePost& lhs_state_post{lhs.delta[lhs_source]};
		const StatePost& rhs_state_post{rhs.delta[rhs_source]};
		// Epsilons sort after all normal symbols and are handled separately below.
		const StatePost::const_iterator lhs_symbols_end{lhs_state_post.first_epsilon_it(first_epsilon)};
		const StatePost::const_iterator rhs_symbols_end{rhs_state_post.first_epsilon_it(first_epsilon)};

		// Find all transitions that have the same symbol for the first and the second state of the processed pair, and
		//  create a transition to every pair of a target of the first one and a target of the second one. Both state
		//  posts are sorted by symbol, so a single two-pointer merge finds the shared symbols.
		auto lhs_symbol_post{lhs_state_post.begin()};
		auto rhs_symbol_post{rhs_state_post.begin()};
		while (lhs_symbol_post != lhs_symbols_end && rhs_symbol_post != rhs_symbols_end) {
			if (lhs_symbol_post->symbol < rhs_symbol_post->symbol) {
				++lhs_symbol_post;
				continue;
			}
			if (rhs_symbol_post->symbol < lhs_symbol_post->symbol) {
				++rhs_symbol_post;
				continue;
			}

			SymbolPost product_symbol_post{lhs_symbol_post->symbol};
			product_symbol_post.targets.reserve(lhs_symbol_post->targets.size() * rhs_symbol_post->targets.size());
			for (const State lhs_target : lhs_symbol_post->targets) {
				for (const State rhs_target : rhs_symbol_post->targets) {
					create_product_state_and_symbol_post(lhs_target, rhs_target, product_symbol_post);
				}
			}
			// The pairs of this symbol post are pairwise distinct, hence so are their product states. Sorting them
			//  therefore restores the invariant of SymbolPost::targets, no deduplication is needed.
			std::ranges::sort(product_symbol_post.targets);
			// Here we are sure that we are working with the largest symbol so far, since we iterate through
			// the symbol posts of the lhs and rhs in order. So we can just push_back (not insert).
			product.delta.mutable_state_post(product_source).push_back(std::move(product_symbol_post));
			++lhs_symbol_post;
			++rhs_symbol_post;
		}

		// Add epsilon transitions, from lhs e-transitions.
		// TODO: handling of epsilons might not be ideal, don't know, it would need some brain cycles to improve.
		//  (handling of normal symbols is ok though)
		for (auto lhs_epsilon_post{lhs_symbols_end}; lhs_epsilon_post != lhs_state_post.end(); ++lhs_epsilon_post) {
			SymbolPost prod_symbol_post{lhs_epsilon_post->symbol};
			prod_symbol_post.targets.reserve(lhs_epsilon_post->targets.size());
			for (const State lhs_target : lhs_epsilon_post->targets) {
				create_product_state_and_symbol_post(lhs_target, rhs_source, prod_symbol_post);
			}
			// The pairs (lhs_target, rhs_source) are pairwise distinct, so sorting is enough here, too.
			std::ranges::sort(prod_symbol_post.targets);
			add_product_e_post(lhs_source, rhs_source, prod_symbol_post);
		}

		// Add epsilon transitions, from rhs e-transitions.
		for (auto rhs_epsilon_post{rhs_symbols_end}; rhs_epsilon_post != rhs_state_post.end(); ++rhs_epsilon_post) {
			SymbolPost prod_symbol_post{rhs_epsilon_post->symbol};
			prod_symbol_post.targets.reserve(rhs_epsilon_post->targets.size());
			for (const State rhs_target : rhs_epsilon_post->targets) {
				create_product_state_and_symbol_post(lhs_source, rhs_target, prod_symbol_post);
			}
			std::ranges::sort(prod_symbol_post.targets);
			add_product_e_post(lhs_source, rhs_source, prod_symbol_post);
		}
	}
	return product;
} // intersection().

} // namespace mata::nfa.
