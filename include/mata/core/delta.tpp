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
typename DeltaBase<P, TT>::Successors DeltaBase<P, TT>::get_successors(const State state) const {
	return state_post(state).get_successors();
}

template <typename P, typename TT>
typename DeltaBase<P, TT>::KeyedSuccessors DeltaBase<P, TT>::get_successors(const State state, const Key<0> symbol) const {
	return state_post(state).get_successors(symbol);
}

template <typename P, typename TT>
std::vector<typename DeltaBase<P, TT>::TransitionType> DeltaBase<P, TT>::get_transitions_to(const State state_to) const {
	std::vector<TransitionType> transitions_to_state{};
	const size_t num_of_states{this->num_of_states()};
	for (State state_from{0}; state_from < num_of_states; ++state_from) {
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

	if (P& state_transitions{self().state_posts_[source]};
		state_transitions.empty() || state_transitions.back().symbol < symbol) {
		state_transitions.emplace_back(symbol, target);
	} else {
		// We need to insert into the middle of the vector.
		if (const auto symbol_transitions{state_transitions.find(typename P::Entry{symbol})};
			symbol_transitions != state_transitions.end()) {
			symbol_transitions->insert(target);
		} else {
			// Add transition to a new Move struct with symbol yet unused on transitions from state_from.
			state_transitions.insert(typename P::Entry{symbol, target});
		}
	}
}

template <typename D, typename P>
void KeyedAccess<D, P, std::index_sequence<0>>::add(const State source, const Key symbol, const Nested& targets) {
	// @p targets may be borrowed from this relation, and no copy is needed: resizing moves the posts, which leaves their
	//  entries in place (see mata::posts::PostLike), and a new entry copies @p targets before it is inserted.
	if (targets.empty()) { return; }
	if constexpr (std::same_as<Target, State>) {
		// Sorted, so the last target is the largest state.
		self().resize_for_states(source, last_target(targets));
	} else {
		// A target may not be sorted.
		State max_state{0};
		for (const Target& target : targets) { max_state = std::max(max_state, D::state_of(target)); }
		self().resize_for_states(source, max_state);
	}

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

	return symbol_transitions->targets.contains(target);
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
	// A key can be left with no targets under it, so it is a target that is looked for, not a key.
	return std::ranges::none_of(state_posts_, [](const PostType& state_post) {
		return any_target(state_post, [](const Target&) { return true; });
	});
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
		const size_t new_size{num_of_states_for_(q)};
		state_posts_.resize(new_size);
	}
	return state_posts_[q];
}

template <typename P, typename TT>
DeltaBase<P, TT> defragment(const DeltaBase<P, TT>& delta, const BoolVector& is_staying, const std::vector<typename DeltaBase<P, TT>::State>& renaming) {
	// Built from the staying posts only, instead of copying the whole relation and then filtering the copy.
	DeltaBase<P, TT> result{};
	const size_t num_of_states{delta.num_of_states()};
	result.reserve(static_cast<size_t>(std::count(is_staying.begin(), is_staying.begin() + static_cast<std::ptrdiff_t>(std::min(num_of_states, is_staying.size())), true)));
	for (size_t source{0}; source < num_of_states; ++source) {
		if (!is_staying[source]) { continue; }
		result.emplace_back(defragmented(delta.state_post(static_cast<typename DeltaBase<P, TT>::State>(source)), is_staying, renaming));
	}
	return result;
}

template <typename P, typename TT>
DeltaBase<P, TT>& DeltaBase<P, TT>::defragment(const BoolVector& is_staying, const std::vector<State>& renaming) {
	size_t source_new{0};
	for (size_t source_orig{0}, num_of_states{this->num_of_states()}; source_orig < num_of_states; ++source_orig) {
		if (!is_staying[source_orig]) { continue; }
		// Filtered and renamed in place: no post is rebuilt, so nothing is allocated.
		defragment_in_place(state_posts_[source_orig], is_staying, renaming);
		if (source_new != source_orig) { state_posts_[source_new] = std::move(state_posts_[source_orig]); }
		++source_new;
	}
	state_posts_.resize(source_new);
	return *this;
}

template <typename P, typename TT>
bool DeltaBase<P, TT>::operator==(const DeltaBase& other) const {
	const size_t states{std::max(num_of_states(), other.num_of_states())};
	for (State source{0}; source < states; ++source) {
		// Using posts_equal rather than operator== because the latter compares only keys, not targets.
		// state_post on nonexistent states returns an empty post, so the relation comparison does not 
		// require the same number of states, only the same transitions.
		if (!posts_equal(state_post(source), other.state_post(source))) { return false; }
	}
	return true;
}

} // namespace mata::posts.

#endif // MATA_CORE_DELTA_TPP
