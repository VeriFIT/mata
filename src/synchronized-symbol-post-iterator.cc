/** @file
 * @brief Implementation of synchronized-symbol-post-iterator for the automaton.
 */

#include "mata/synchronized-symbol-post-iterator.hh"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

using namespace mata;

StateSet SynchronizedExistentialSymbolPostIterator::unify_targets() const {
	if (!is_synchronized()) { return {}; }

	const std::vector<StatePost::const_iterator>& current{get_current()};

	// One synchronized post contributes all its targets, already sorted and deduplicated.
	if (current.size() == 1) {
		const SmallStateSet& targets{current[0]->targets};
		StateSet result{StateSet::with_reserved(targets.size())};
		std::ranges::copy(targets, std::back_inserter(result));
		return result;
	}

	// Two posts merge with one linear pass over both sorted target sets.
	if (current.size() == 2) {
		const SmallStateSet& lhs{current[0]->targets};
		const SmallStateSet& rhs{current[1]->targets};
		StateSet result{StateSet::with_reserved(lhs.size() + rhs.size())};
		std::ranges::set_union(lhs, rhs, std::back_inserter(result));
		return result;
	}

	// Three or more posts. Targets of one post are sorted, so the range of all targets is known up front.
	size_t num_of_targets{0};
	State min_target{Limits::max_state};
	State max_target{0};
	for (const StatePost::const_iterator& symbol_post_it : current) {
		const SmallStateSet& targets{symbol_post_it->targets};
		if (targets.empty()) { continue; }
		num_of_targets += targets.size();
		min_target = std::min(min_target, targets.front());
		max_target = std::max(max_target, targets.back());
	}
	if (num_of_targets == 0) { return {}; }

	// Dense targets: mark them in a bitset over the words spanning [min_target, max_target] and read them back in
	//  order, which costs O(targets + words) instead of a sort.
	const size_t first_word{min_target / 64};
	const size_t num_of_words{(max_target / 64) - first_word + 1};
	if (num_of_words <= num_of_targets) {
		unify_words_.assign(num_of_words, 0);
		for (const StatePost::const_iterator& symbol_post_it : current) {
			for (const State target : symbol_post_it->targets) {
				unify_words_[(target / 64) - first_word] |= uint64_t{1} << (target % 64);
			}
		}
		size_t num_of_unique_targets{0};
		for (const uint64_t word : unify_words_) { num_of_unique_targets += static_cast<size_t>(std::popcount(word)); }
		StateSet result{StateSet::with_reserved(num_of_unique_targets)};
		for (size_t word_idx{0}; word_idx < num_of_words; ++word_idx) {
			const State word_base{static_cast<State>((first_word + word_idx) * 64)};
			for (uint64_t word{unify_words_[word_idx]}; word != 0; word &= word - 1) {
				result.push_back(word_base + static_cast<State>(std::countr_zero(word)));
			}
		}
		return result;
	}

	// Sparse targets: gather into the reused buffer, then sort and deduplicate once.
	unify_buffer_.clear();
	unify_buffer_.reserve(num_of_targets);
	for (const StatePost::const_iterator& symbol_post_it : current) {
		for (const State target : symbol_post_it->targets) { unify_buffer_.push_back(target); }
	}
	std::ranges::sort(unify_buffer_);
	const auto duplicates{std::ranges::unique(unify_buffer_)};
	unify_buffer_.erase(duplicates.begin(), duplicates.end());
	// The buffer is reused by the next call, so its content is copied out rather than moved.
	StateSet result{StateSet::with_reserved(unify_buffer_.size())};
	for (const State target : unify_buffer_) { result.push_back(target); }
	return result;
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
