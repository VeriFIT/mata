/** @file
 * @brief Implementation of synchronized-symbol-post-iterator for the automaton.
 */

#include "mata/synchronized-symbol-post-iterator.hh"

#include <queue>
#include <utility>
#include <vector>

using namespace mata;

StateSet SynchronizedExistentialSymbolPostIterator::unify_targets() const {
	// TODO: decide which version performs the best.

	if (!is_synchronized()) { return {}; }

	StateSet unified_targets{};

	// Version with synchronized iterator.
	// static utils::SynchronizedExistentialIterator<StateSet::const_iterator> sync_iterator;
	// sync_iterator.reset();
	// size_t all_targets_size{ 0 };
	// const std::vector<StatePost::const_iterator>& current_symbol_post_its{ this->get_current() };
	// sync_iterator.reserve(current_symbol_post_its.size());
	// for (const auto symbol_post_it: current_symbol_post_its) {
	//     sync_iterator.push_back(symbol_post_it->cbegin(), symbol_post_it->cend());
	//     all_targets_size += symbol_post_it->num_of_targets();
	// }
	// unified_targets.reserve(all_targets_size);
	// while (sync_iterator.advance()) { unified_targets.push_back(*sync_iterator.get_current_minimum()); }

	// Version with set union.
	// for (const auto& symbol_post_it: get_current()) {
	//     unified_targets.insert(symbol_post_it->targets);
	// }

	// Version with priority queue.
	using TargetSetBeginEndPair = std::pair<StateSet::const_iterator, StateSet::const_iterator>;
	auto compare = [](const auto& a, const auto& b) { return *(a.first) > *(b.first); };
	std::priority_queue<TargetSetBeginEndPair, std::vector<TargetSetBeginEndPair>, decltype(compare)> queue(compare);
	for (const StatePost::const_iterator& symbol_post_it : get_current()) {
		queue.emplace(symbol_post_it->cbegin(), symbol_post_it->cend());
	}
	unified_targets.reserve(32);
	while (!queue.empty()) {
		auto item = queue.top();
		queue.pop();
		if (unified_targets.empty() || unified_targets.back() != *(item.first)) {
			unified_targets.push_back(*(item.first));
		}
		if (++item.first != item.second) { queue.emplace(item); }
	}

	return unified_targets;
}

bool SynchronizedExistentialSymbolPostIterator::synchronize_with(const Symbol sync_symbol) {
	do {
		if (is_synchronized()) {
			if (const auto current_min_symbol_post_it = get_current_minimum();
				current_min_symbol_post_it->symbol >= sync_symbol) {
				break;
			}
		}
	} while (advance());
	return is_synchronized() && get_current_minimum()->symbol == sync_symbol;
}

bool SynchronizedExistentialSymbolPostIterator::synchronize_with(const SymbolPost& sync) {
	return synchronize_with(sync.symbol);
}
