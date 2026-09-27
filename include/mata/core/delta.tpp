/** @file
 * @brief Member definitions of @c mata::posts::DeltaBase.
 *
 * Included at the end of @c mata/core/delta.hh. The bodies live in a header rather than in
 *  `src/core/delta.cc` because a third party instantiating the relation over its own key or target
 *  types needs them; the in-tree depth-2 instantiation is kept out of every translation unit by the
 *  @c extern template declarations in @c mata/relation.hh.
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
	//  only be the last one, and it is also the *smallest* epsilon because it is the only possible
	//  one. That makes the answer an O(1) look at the back instead of a search -- but only under
	//  both conditions. With a reserved tail wider than one key, or an epsilon that is not the
	//  greatest key, there can be entries above @p epsilon and the back is then the wrong entry;
	//  hence the descriptor is asked rather than the constant assumed. @see mata::ReservedKeys.
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

template <typename P, typename TT>
void DeltaBase<P, TT>::add(const State source, Key<0> symbol, TargetArg target)
	requires(P::key_arity == 1)
{
	resize_for_states(source, state_of(target));

	if (PostType& state_transitions{state_posts_[source]}; state_transitions.empty()) {
		state_transitions.insert({symbol, target});
	} else if (state_transitions.back().symbol < symbol) {
		state_transitions.insert({symbol, target});
	} else {
		if (const auto symbol_transitions{state_transitions.find(Entry{symbol})};
			symbol_transitions != state_transitions.end()) {
			// Add transition with symbol already used on transitions from state_from.
			symbol_transitions->insert(target);
		} else {
			// Add transition to a new Move struct with symbol yet unused on transitions from state_from.
			const Entry new_symbol_transitions{symbol, target};
			state_transitions.insert(new_symbol_transitions);
		}
	}
}

template <typename P, typename TT>
void DeltaBase<P, TT>::add(const State source, const Key<0> symbol, const Nested& targets)
	requires(P::key_arity == 1)
{
	if (targets.empty()) { return; }
	resize_for_states(source, targets.back());

	if (PostType& state_transitions{state_posts_[source]}; state_transitions.empty()) {
		state_transitions.insert({symbol, targets});
	} else if (state_transitions.back().symbol < symbol) {
		state_transitions.insert({symbol, targets});
	} else {
		if (const auto symbol_transitions{state_transitions.find(symbol)};
			symbol_transitions != state_transitions.end()) {
			// Add transition with symbolOnTransition already used on transitions from state_from.
			symbol_transitions->insert(targets);

		} else {
			// Add transition to a new Move struct with symbol yet unused on transitions from state_from.
			// Move new_symbol_transitions{ symbol, states };
			state_transitions.insert(Entry{symbol, targets});
		}
	}
}

template <typename P, typename TT>
void DeltaBase<P, TT>::remove(const State source, const Key<0> symbol, TargetArg target)
	requires(P::key_arity == 1)
{
	if (source >= state_posts_.size()) { return; }

	if (PostType& state_transitions{state_posts_[source]}; state_transitions.empty()) {
		throw std::invalid_argument(
			"TransitionType [" + utils::format_or_unprintable(source) + ", " + utils::format_or_unprintable(symbol) + ", " + utils::format_or_unprintable(state_of(target)) +
			"] does not exist."
		);
	} else if (state_transitions.back().symbol < symbol) {
		throw std::invalid_argument(
			"TransitionType [" + utils::format_or_unprintable(source) + ", " + utils::format_or_unprintable(symbol) + ", " + utils::format_or_unprintable(state_of(target)) +
			"] does not exist."
		);
	} else {
		if (const auto symbol_transitions{state_transitions.find(symbol)};
			symbol_transitions == state_transitions.end()) {
			throw std::invalid_argument(
				"TransitionType [" + utils::format_or_unprintable(source) + ", " + utils::format_or_unprintable(symbol) + ", " +
				utils::format_or_unprintable(state_of(target)) + "] does not exist."
			);
		} else {
			symbol_transitions->erase(target);
			if (symbol_transitions->empty()) { state_posts_[source].erase(*symbol_transitions); }
		}
	}
}

template <typename P, typename TT>
bool DeltaBase<P, TT>::contains(const State source, const Key<0> symbol, TargetArg target) const
	requires(P::key_arity == 1)
{ // {{{
	if (state_posts_.empty()) { return false; }
	if (state_posts_.size() <= source) { return false; }

	const PostType& tl = state_posts_[source];
	if (tl.empty()) { return false; }
	const auto symbol_transitions{tl.find(Entry{symbol})};
	if (symbol_transitions == tl.cend()) { return false; }

	return symbol_transitions->targets.find(target) != symbol_transitions->targets.end();
}

template <typename P, typename TT>
size_t DeltaBase<P, TT>::num_of_transitions() const {
	size_t number_of_transitions{0};
	for (const PostType& state_post : state_posts_) { number_of_transitions += count_targets(state_post); }
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
DeltaBase<P, TT> defragment(const DeltaBase<P, TT>& delta, const BoolVector& is_staying,
                    const std::vector<typename DeltaBase<P, TT>::State>& renaming) {
	// One implementation, not two: the free function is the member applied to a copy.
	DeltaBase<P, TT> result{delta};
	result.defragment(is_staying, renaming);
	return result;
}

template <typename P, typename TT>
DeltaBase<P, TT>& DeltaBase<P, TT>::defragment(const BoolVector& is_staying, const std::vector<State>& renaming) {
	// Each post's own job is in `defragmented()`; this one owns only the outer index -- drop the
	//  sources that go, and compact the rest down.
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
	// Post by post, over the union of the two state spaces. `state_post()` yields the shared empty
	//  post out of range, so a relation with trailing empty posts compares equal to one without --
	//  the same meaning the old transition-by-transition comparison had, without needing a depth-2
	//  transition iterator to express it. Note `posts_equal` and not `!=`: an entry's operator==
	//  compares only its key, so `!=` would ignore differing targets entirely.
	const size_t states{std::max(num_of_states(), other.num_of_states())};
	for (size_t source{0}; source < states; ++source) {
		if (!posts_equal(state_post(source), other.state_post(source))) { return false; }
	}
	return true;
}

} // namespace mata::posts.

#endif // MATA_CORE_DELTA_TPP
