/** @file
 * @brief Member definitions of @c mata::posts::DeltaBase.
 */

#ifndef MATA_CORE_DELTA_TPP
#define MATA_CORE_DELTA_TPP

#include <algorithm>
#include <utility>

namespace mata::posts {

template <typename P, typename TT>
typename DeltaBase<P, TT>::PostType::const_iterator DeltaBase<P, TT>::epsilon_symbol_posts(const State state, const Key<0> epsilon) const {
	return epsilon_symbol_posts(state_post(state), epsilon);
}

template <typename P, typename TT>
typename DeltaBase<P, TT>::PostType::const_iterator DeltaBase<P, TT>::epsilon_symbol_posts(const PostType& state_post, const Key<0> epsilon) {
	if (state_post.empty()) { return state_post.end(); }
	// Fast path: when nothing can sort above the relation's own epsilon, an entry carrying it can
	//  only be the last one, and it is also the *smallest* epsilon because it is the only possible one. 
	if constexpr (Reserved<0>::epsilon_is_greatest) {
		if (epsilon == Reserved<0>::min_epsilon) {
			if (const auto& back = state_post.back(); back.key() == epsilon) {
				return std::prev(state_post.end());
			}
			return state_post.end();
		}
	}
	return state_post.find(epsilon);
}

template <typename P, typename TT>
typename DeltaBase<P, TT>::TargetSet DeltaBase<P, TT>::get_successors(const State state) const {
	return state_post(state).get_successors();
}

template <typename P, typename TT>
typename DeltaBase<P, TT>::KeyedSuccessors DeltaBase<P, TT>::get_successors(const State state, const Key<0> symbol) const {
	return state_post(state).get_successors(symbol);
}

template <typename P, typename TT>
std::vector<typename DeltaBase<P, TT>::TransitionType> DeltaBase<P, TT>::get_transitions_to(const State state_to) const {
	std::vector<TransitionType> transitions_to_state{};
	for (State state_from{0}, num_of_states{this->num_of_states()}; state_from < num_of_states; ++state_from) {
		collect_transitions_to_(state_from, state_to, transitions_to_state);
	}
	return transitions_to_state;
}

template <typename P, typename TT>
std::vector<typename DeltaBase<P, TT>::TransitionType>
DeltaBase<P, TT>::get_transitions_between(const State state_from, const State state_to) const {
	std::vector<TransitionType> transitions_between{};
	collect_transitions_to_(state_from, state_to, transitions_between);
	return transitions_between;
}

template <typename D, typename P>
void KeyedAccess<D, P, std::index_sequence<0>>::add(const State source, Key symbol, TargetArg target) {
	self().resize_for_states(source, D::state_of(target));

	if (P& state_transitions{self().state_posts_[source]}; state_transitions.empty()) {
		state_transitions.insert({symbol, target});
	} else if (state_transitions.back().symbol < symbol) {
		state_transitions.insert({symbol, target});
	} else {
		// We need to insert into the middle of the vector.
		if (const auto symbol_transitions{state_transitions.find(typename P::Entry{symbol})};
			symbol_transitions != state_transitions.end()) {
			symbol_transitions->insert(target);
		} else {
			// Add transition to a new Move struct with symbol yet unused on transitions from state_from.
			const typename P::Entry new_symbol_transitions{symbol, target};
			state_transitions.insert(new_symbol_transitions);
		}
	}
}

template <typename D, typename P>
void KeyedAccess<D, P, std::index_sequence<0>>::add(const State source, const Key symbol, const Nested& targets) {
	if (targets.empty()) { return; }
	self().resize_for_states(source, targets.back());

	if (P& state_transitions{self().state_posts_[source]}; state_transitions.empty()) {
		state_transitions.insert({symbol, targets});
	} else if (state_transitions.back().symbol < symbol) {
		state_transitions.insert({symbol, targets});
	} else {
		// We need to insert into the middle of the vector.
		if (const auto symbol_transitions{state_transitions.find(symbol)};
			symbol_transitions != state_transitions.end()) {
			symbol_transitions->insert(targets);

		} else {
			// Add transition to a new Move struct with symbol yet unused on transitions from state_from.
			state_transitions.insert(typename P::Entry{symbol, targets});
		}
	}
}

template <typename D, typename P>
void KeyedAccess<D, P, std::index_sequence<0>>::remove(const State source, const Key symbol, TargetArg target) {
	if (source >= self().state_posts_.size()) { return; }

	if (P& state_transitions{self().state_posts_[source]}; state_transitions.empty()) {
		throw std::invalid_argument(
			"TransitionType [" + 
			utils::format_or_unprintable(source) + ", " + 
			utils::format_or_unprintable(symbol) + ", " + 
			utils::format_or_unprintable(D::state_of(target)) +
			"] does not exist."
		);
	} else if (state_transitions.back().symbol < symbol) {
		throw std::invalid_argument(
			"TransitionType [" + 
			utils::format_or_unprintable(source) + ", " + 
			utils::format_or_unprintable(symbol) + ", " + 
			utils::format_or_unprintable(D::state_of(target)) +
			"] does not exist."
		);
	} else {
		if (const auto symbol_transitions{state_transitions.find(symbol)};
			symbol_transitions == state_transitions.end()) {
			throw std::invalid_argument(
				"TransitionType [" + 
				utils::format_or_unprintable(source) + ", " + 
				utils::format_or_unprintable(symbol) + ", " +
				utils::format_or_unprintable(D::state_of(target)) + 
				"] does not exist."
			);
		} else {
			symbol_transitions->erase(target);
			if (symbol_transitions->empty()) { 
				self().state_posts_[source].erase(*symbol_transitions); 
			}
		}
	}
}

template <typename D, typename P>
bool KeyedAccess<D, P, std::index_sequence<0>>::contains(const State source, const Key symbol, TargetArg target) const {
	if (self().state_posts_.size() <= source) { 
		// Such source does not exist in the relation.
		return false; 
	}

	const P& tl = self().state_posts_[source];
	if (tl.empty()) { 
		// The source state has no transitions.
		return false; 
	}
	const auto symbol_transitions{tl.find(typename P::Entry{symbol})};
	if (symbol_transitions == tl.cend()) { 
		// The source state has no transitions under the given symbol.
		return false; 
	}

	return symbol_transitions->targets.find(target) != symbol_transitions->targets.end();
}

template <typename P, typename TT>
size_t DeltaBase<P, TT>::num_of_transitions() const {
	size_t number_of_transitions{0};
	for (const PostType& state_post : state_posts_) { 
		number_of_transitions += count_targets(state_post); 
	}
	return number_of_transitions;
}

template <typename P, typename TT>
bool DeltaBase<P, TT>::empty() const {
	return std::ranges::all_of(state_posts_, [](const PostType& state_post) { return state_post.empty(); });
}


template <typename P, typename TT>
std::vector<typename DeltaBase<P, TT>::PostType> DeltaBase<P, TT>::renumber_targets(const std::function<State(State)>& target_renumberer) const {
	std::vector<PostType> copied_state_posts;
	copied_state_posts.reserve(num_of_states());
	for (const PostType& state_post : state_posts_) {
		copied_state_posts.emplace_back(renumbered(state_post, target_renumberer));
	}
	return copied_state_posts;
}

template <typename P, typename TT>
typename DeltaBase<P, TT>::PostType& DeltaBase<P, TT>::mutable_state_post(const State q) {
	if (q >= state_posts_.size()) {
		utils::reserve_on_insert(state_posts_, q);
		const size_t new_size{q + 1};
		state_posts_.resize(new_size);
	}
	return state_posts_[q];
}

template <typename P, typename TT>
DeltaBase<P, TT> defragment(const DeltaBase<P, TT>& delta, const BoolVector& is_staying, const std::vector<typename DeltaBase<P, TT>::State>& renaming) {
	DeltaBase<P, TT> result{delta};
	result.defragment(is_staying, renaming);
	return result;
}

template <typename P, typename TT>
DeltaBase<P, TT>& DeltaBase<P, TT>::defragment(const BoolVector& is_staying, const std::vector<State>& renaming) {
	size_t source_new{0};
	for (size_t source_orig{0}, num_of_states{this->num_of_states()}; source_orig < num_of_states; ++source_orig) {
		if (!is_staying[source_orig]) { continue; }
		state_posts_[source_new] = defragmented(state_posts_[source_orig], is_staying, renaming);
		++source_new;
	}
	state_posts_.resize(source_new);
	return *this;
}

template <typename P, typename TT>
bool DeltaBase<P, TT>::operator==(const DeltaBase& other) const {
	const size_t states{std::max(num_of_states(), other.num_of_states())};
	for (size_t source{0}; source < states; ++source) {
		// Using posts_equal rather than operator== because the latter compares only keys, not targets.
		// state_post on nonexistent states returns an empty post, so the relation comparison does not 
		// require the same number of states, only the same transitions.
		if (!posts_equal(state_post(source), other.state_post(source))) { return false; }
	}
	return true;
}

} // namespace mata::posts.

#endif // MATA_CORE_DELTA_TPP
