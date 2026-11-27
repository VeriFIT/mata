/** @file
 * @brief Member definitions of @c mata::posts::DeltaBase.
 */

#ifndef MATA_CORE_DELTA_TPP
#define MATA_CORE_DELTA_TPP

#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace mata::posts {

template <typename P, typename TT>
typename DeltaBase<P, TT>::PostType::const_iterator
	DeltaBase<P, TT>::epsilon_symbol_posts(const State state, const Key<0> epsilon) const {
	return epsilon_symbol_posts(state_post(state), epsilon);
}

template <typename P, typename TT>
typename DeltaBase<P, TT>::PostType::const_iterator
	DeltaBase<P, TT>::epsilon_symbol_posts(const PostType& state_post, const Key<0> epsilon) {
	if (state_post.empty()) { return state_post.end(); }
	// Fast path: when nothing can sort above the relation's own epsilon, an entry carrying it can
	//  only be the last one, and it is also the *smallest* epsilon because it is the only possible one.
	if constexpr (Reserved<0>::epsilon_is_greatest) {
		if (epsilon == Reserved<0>::min_epsilon) {
			if (const auto& back = state_post.back(); back.key() == epsilon) { return std::prev(state_post.end()); }
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
typename DeltaBase<P, TT>::KeyedSuccessors
	DeltaBase<P, TT>::get_successors(const State state, const Key<0> symbol) const {
	return state_post(state).get_successors(symbol);
}

template <typename P, typename TT>
std::vector<typename DeltaBase<P, TT>::TransitionType>
	DeltaBase<P, TT>::get_transitions_to(const State state_to) const {
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
		self().note_added(1);
	} else {
		// We need to insert into the middle of the vector.
		if (const auto symbol_transitions{state_transitions.find(typename P::Entry{symbol})};
			symbol_transitions != state_transitions.end()) {
			if (symbol_transitions->insert(target)) { self().note_added(1); }
		} else {
			// Add transition to a new Move struct with symbol yet unused on transitions from state_from.
			state_transitions.insert(typename P::Entry{symbol, target});
			self().note_added(1);
		}
	}
}

template <typename D, typename P>
void KeyedAccess<D, P, std::index_sequence<0>>::add(const State source, const Key symbol, const Nested& targets) {
	// @p targets may be borrowed from this relation, and no copy is needed: resizing moves the posts, which leaves
	// their
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

	// Read before any insert: @p targets may be borrowed from the entry that grows.
	const size_t num_of_targets{targets.size()};
	if (P& state_transitions{self().state_posts_[source]}; state_transitions.empty()) {
		state_transitions.insert({symbol, targets});
		self().note_added(num_of_targets);
	} else if (state_transitions.back().symbol < symbol) {
		state_transitions.insert({symbol, targets});
		self().note_added(num_of_targets);
	} else {
		// We need to insert into the middle of the vector.
		if (const auto symbol_transitions{state_transitions.find(symbol)};
			symbol_transitions != state_transitions.end()) {
			const size_t old_size{symbol_transitions->nested().size()};
			symbol_transitions->insert(targets);
			self().note_added(symbol_transitions->nested().size() - old_size);
		} else {
			// Add transition to a new Move struct with symbol yet unused on transitions from state_from.
			state_transitions.insert(typename P::Entry{symbol, targets});
			self().note_added(num_of_targets);
		}
	}
}

template <typename D, typename P>
void KeyedAccess<D, P, std::index_sequence<0>>::remove(const State source, const Key symbol, TargetArg target) {
	if (source >= self().state_posts_.size()) { return; }

	if (P& state_transitions{self().state_posts_[source]}; state_transitions.empty()) {
		throw std::invalid_argument(
			"TransitionType [" + utils::format_or_unprintable(source) + ", " + utils::format_or_unprintable(symbol) +
			", " + utils::format_or_unprintable(D::state_of(target)) + "] does not exist."
		);
	} else if (state_transitions.back().symbol < symbol) {
		throw std::invalid_argument(
			"TransitionType [" + utils::format_or_unprintable(source) + ", " + utils::format_or_unprintable(symbol) +
			", " + utils::format_or_unprintable(D::state_of(target)) + "] does not exist."
		);
	} else {
		if (const auto symbol_transitions{state_transitions.find(symbol)};
			symbol_transitions == state_transitions.end()) {
			throw std::invalid_argument(
				"TransitionType [" + utils::format_or_unprintable(source) + ", " +
				utils::format_or_unprintable(symbol) + ", " + utils::format_or_unprintable(D::state_of(target)) +
				"] does not exist."
			);
		} else {
			if (symbol_transitions->erase(target)) { self().note_removed(1); }
			if (symbol_transitions->empty()) { self().state_posts_[source].erase(*symbol_transitions); }
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

template <typename P, typename TT> size_t DeltaBase<P, TT>::num_of_transitions() const {
	const auto count_by_scan{[this] {
		size_t count{0};
		for (const PostType& state_post : state_posts_) { count += count_targets(state_post); }
		return count;
	}};
	// Concurrent const callers may all recount; they store the same value, and the release publishes it.
	if (count_dirty_.load(std::memory_order_acquire)) {
		const size_t count{count_by_scan()};
		cached_transition_count_.store(count, std::memory_order_relaxed);
		count_dirty_.store(false, std::memory_order_release);
		return count;
	}
	const size_t count{cached_transition_count_.load(std::memory_order_relaxed)};
	// Catches a missed count update, or an edit through a `mutable_state_post()` reference kept past a query.
	MATA_EXPENSIVE_ASSERT(count == count_by_scan());
	return count;
}

template <typename P, typename TT>
std::vector<typename DeltaBase<P, TT>::PostType>
	DeltaBase<P, TT>::renumber_targets(const std::function<State(State)>& target_renumberer) const {
	std::vector<PostType> copied_state_posts;
	copied_state_posts.reserve(num_of_states());
	for (const PostType& state_post : state_posts_) {
		copied_state_posts.emplace_back(renumbered(state_post, target_renumberer));
	}
	return copied_state_posts;
}

template <typename P, typename TT>
typename DeltaBase<P, TT>::PostType& DeltaBase<P, TT>::mutable_state_post(const State q) {
	// Edits through the returned reference cannot be seen here.
	mark_count_dirty();
	if (q >= state_posts_.size()) {
		utils::reserve_on_insert(state_posts_, q);
		const size_t new_size{num_of_states_for_(q)};
		state_posts_.resize(new_size);
	}
	return state_posts_[q];
}

template <typename P, typename TT>
DeltaBase<P, TT> defragment(
	const DeltaBase<P, TT>& delta,
	const BoolVector& is_staying,
	const std::vector<typename DeltaBase<P, TT>::State>& renaming
) {
	// Built from the staying posts only, instead of copying the whole relation and then filtering the copy.
	DeltaBase<P, TT> result{};
	const size_t num_of_states{delta.num_of_states()};
	result.reserve(
		static_cast<size_t>(std::count(
			is_staying.begin(),
			is_staying.begin() + static_cast<std::ptrdiff_t>(std::min(num_of_states, is_staying.size())), true
		))
	);
	for (size_t source{0}; source < num_of_states; ++source) {
		if (!is_staying[source]) { continue; }
		result.emplace_back(
			defragmented(delta.state_post(static_cast<typename DeltaBase<P, TT>::State>(source)), is_staying, renaming)
		);
	}
	return result;
}

template <typename P, typename TT>
DeltaBase<P, TT>& DeltaBase<P, TT>::defragment(const BoolVector& is_staying, const std::vector<State>& renaming) {
	// Drops the transitions of the leaving states without counting them, so the next query recounts.
	mark_count_dirty();
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

template <typename P, typename TT> bool DeltaBase<P, TT>::operator==(const DeltaBase& other) const {
	const size_t states{std::max(num_of_states(), other.num_of_states())};
	for (State source{0}; source < states; ++source) {
		// Using posts_equal rather than operator== because the latter compares only keys, not targets.
		// state_post on nonexistent states returns an empty post, so the relation comparison does not
		// require the same number of states, only the same transitions.
		if (!posts_equal(state_post(source), other.state_post(source))) { return false; }
	}
	return true;
}
/// @name Post and epsilon closure with reusable buffers, generic in the key arity.
///@{
/// Empty slot of the visited table; no relation can reach it, as that would take max_state + 1 state posts.
template <typename State> constexpr State NO_STATE{std::numeric_limits<State>::max()};

/// Slot of @p state in the open-addressed @p table (power-of-two size, at most half full): the slot holding
///  @p state, or the empty slot where it belongs.
template <typename State> size_t slot_of(const std::vector<State>& table, const State state) {
	const auto shift{std::numeric_limits<uint64_t>::digits - std::countr_zero(table.size())};
	const size_t mask{table.size() - 1};
	size_t slot{static_cast<size_t>((static_cast<uint64_t>(state) * 0x9E'37'79'B9'7F'4A'7C'15ULL) >> shift)};
	while (table[slot] != NO_STATE<State> && table[slot] != state) { slot = (slot + 1) & mask; }
	return slot;
}

/// Appends to @p closure every state reachable from it over @p epsilons that it does not hold yet. Visited states
/// are
///  kept in a hash table sized to the closure, so the cost follows the closure, not the number of states of the
///  delta.
template <typename P, typename TT>
void close_over_epsilons(
	const DeltaBase<P, TT>& delta,
	const std::vector<typename DeltaBase<P, TT>::Key<0>>& epsilons,
	std::vector<typename DeltaBase<P, TT>::State>& closure,
	PostWorkspace& workspace
) {
	using State = typename DeltaBase<P, TT>::State;
	using Key = typename DeltaBase<P, TT>::Key<0>;
	using Target = typename DeltaBase<P, TT>::Target;
	std::vector<State>& visited{workspace.visited};
	std::vector<State>& worklist{workspace.worklist};
	const auto rebuild_visited{[&](const size_t num_of_slots) {
		visited.assign(num_of_slots, NO_STATE<State>);
		for (const State state : closure) { visited[slot_of(visited, state)] = state; }
	}};
	rebuild_visited(std::bit_ceil(std::max<size_t>(16, 2 * closure.size())));
	worklist.assign(closure.begin(), closure.end());
	// An empty @p epsilons means the relation's own smallest epsilon, as at the default argument of
	//  @c epsilon_symbol_posts().
	std::vector<Key> effective_epsilons{epsilons};
	constexpr auto default_epsilon{posts::PostAt<P, 0>::type::Reserved::min_epsilon};
	if (effective_epsilons.empty()) { effective_epsilons.push_back(default_epsilon); }
	while (!worklist.empty()) {
		const State state{worklist.back()};
		worklist.pop_back();
		const typename DeltaBase<P, TT>::PostType& state_post{delta.state_post(state)};
		for (const Key epsilon : effective_epsilons) {
			const auto symbol_post_it{DeltaBase<P, TT>::epsilon_symbol_posts(state_post, epsilon)};
			if (symbol_post_it == state_post.end()) { continue; }
			visit_targets(symbol_post_it->nested(), [&](const Target& target) {
				const size_t slot{slot_of(visited, target)};
				if (visited[slot] == target) { return; }
				visited[slot] = target;
				closure.push_back(target);
				worklist.push_back(target);
				if (2 * closure.size() > visited.size()) { rebuild_visited(2 * visited.size()); }
			});
		}
	}
}

template <typename P, typename TT>
std::vector<typename DeltaBase<P, TT>::State>& DeltaBase<P, TT>::gather_post(
	const std::vector<State>& states,
	const Key<0> key,
	const EpsilonClosureOpt epsilon_closure_opt,
	const std::vector<Key<0>>& epsilons,
	PostWorkspace& workspace
) const {
	using State = DeltaBase<P, TT>::State;
	if (closes_before(epsilon_closure_opt)) {
		workspace.sources.assign(states.begin(), states.end());
		close_over_epsilons(*this, epsilons, workspace.sources, workspace);
	}
	const std::vector<State>& sources{closes_before(epsilon_closure_opt) ? workspace.sources : states};

	std::vector<State>& gathered{workspace.gathered};
	gathered.clear();
	// Stepping over epsilon with a closure requested means the source states are reachable as well.
	if (key == Reserved<0>::min_epsilon && (closes_before(epsilon_closure_opt) || closes_after(epsilon_closure_opt))) {
		gathered.assign(states.begin(), states.end());
	}
	for (const State state : sources) {
		const PostType& state_post{this->state_post(state)};
		const auto symbol_post_it{state_post.find(key)};
		if (symbol_post_it == state_post.end()) { continue; }
		visit_targets(symbol_post_it->nested(), [&](const Target& target) { gathered.push_back(state_of(target)); });
	}
	if (closes_after(epsilon_closure_opt)) { close_over_epsilons(*this, epsilons, gathered, workspace); }
	return gathered;
}

template <typename P, typename TT>
std::vector<typename DeltaBase<P, TT>::State> DeltaBase<P, TT>::epsilon_closure(
	const std::vector<State>& states, const std::vector<Key<0>>& epsilons, PostWorkspace& workspace
) const {
	std::vector<State>& closure{workspace.gathered};
	closure.assign(states.begin(), states.end());
	close_over_epsilons(*this, epsilons, closure, workspace);
	return closure;
}
///@}

} // namespace mata::posts.

#endif // MATA_CORE_DELTA_TPP
