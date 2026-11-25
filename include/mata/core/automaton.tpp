/** @file
 * @brief Member definitions for @c mata::AutomatonBase.
 */

#ifndef MATA_CORE_AUTOMATON_TPP_
#define MATA_CORE_AUTOMATON_TPP_

#include <algorithm>
#include <deque>
#include <iterator>
#include <limits>
#include <span>
#include <utility>

#include "mata/utils/assert.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/utils/sparse-set.hh"
#include "mata/core/tarjan.hh"

namespace mata {

template <DeltaLike D>
AutomatonBase<D>& AutomatonBase<D>::operator=(AutomatonBase&& other) noexcept(
	std::is_nothrow_move_assignable_v<D> && std::is_nothrow_move_assignable_v<utils::SparseSet<State>>
) {
	if (this != &other) {
		delta = std::move(other.delta);
		initial = std::move(other.initial);
		final = std::move(other.final);
	}
	return *this;
}

template <DeltaLike D> typename AutomatonBase<D>::State AutomatonBase<D>::add_state() {
	const size_t num_of_states{this->num_of_states()};
	delta.allocate(num_of_states + 1); // Throws when the new state would be the max_state sentinel.
	return static_cast<State>(num_of_states);
}

template <DeltaLike D> typename AutomatonBase<D>::State AutomatonBase<D>::add_state(const State state) {
	if (state == std::numeric_limits<State>::max()) [[unlikely]] {
		throw std::overflow_error("mata: the no-state sentinel cannot become a state");
	}
	if (state >= delta.num_of_states()) { delta.allocate(static_cast<size_t>(state) + 1); }
	return state;
}

template <DeltaLike D> size_t AutomatonBase<D>::num_of_states() const {
	// DeltaLike asks only for a count convertible to size_t; std::max needs one type.
	const size_t num_of_states{
		std::max({initial.domain_size(), final.domain_size(), static_cast<size_t>(delta.num_of_states())})
	};
	// Every algorithm starts here, so this is where a state at the reserved maximum is caught.
	MATA_ASSERT(
		num_of_states <= static_cast<size_t>(std::numeric_limits<State>::max()),
		"the largest State is reserved as \"no state\"; an initial or final state cannot be it"
	);
	return num_of_states;
}

template <DeltaLike D> void AutomatonBase<D>::clear() {
	delta.clear();
	initial.clear();
	final.clear();
}

template <DeltaLike D>
inline typename AutomatonBase<D>::StateBoolArray AutomatonBase<D>::reachable_states_(
	const utils::SparseSet<State>& initial_states, const std::optional<const StateBoolArray>& states_to_consider
) const {
	std::vector<State> worklist{};
	const size_t num_of_states{this->num_of_states()};
	StateBoolArray reachable(num_of_states, false);
	for (const State state : initial_states) {
		if (state < num_of_states && (!states_to_consider.has_value() || states_to_consider.value()[state])) {
			worklist.push_back(state);
			reachable[state] = true;
		}
	}

	while (!worklist.empty()) {
		const State state{worklist.back()};
		worklist.pop_back();
		delta.for_each_successor(state, [&](const Target& target) {
			const State target_state{D::state_of(target)};
			if (!reachable[target_state] &&
				(!states_to_consider.has_value() || states_to_consider.value()[target_state])) {
				worklist.push_back(target_state);
				reachable[target_state] = true;
			}
		});
	}
	return reachable;
}

namespace detail {
/**
 * @brief Label-free predecessor lists of a relation in the CSR (compressed sparse row) layout.
 *
 * The predecessors of a state @c target are the slice `sources[offsets[target]..offsets[target + 1])`. Keys are
 *  not stored: the backward traversals using the index only ask which states can reach a given state.
 */
template <typename State> struct PredecessorIndex {
	/// Start of the predecessor slice of each state; one element longer than the number of indexed states.
	///  The values count edges, which can exceed what a @c State holds, so this has to be @c size_t.
	std::vector<size_t> offsets{};
	/// Sources of all indexed transitions, grouped by their target state.
	std::vector<State> sources{};
};

/**
 * @brief Build the predecessor index of @p delta over @p num_of_states states.
 *
 * Only transitions leaving a state selected by @p source_filter are indexed (all of them without a filter); the others
 *  are skipped both when counting and when filling, so the index costs exactly one entry per kept transition.
 *
 * @param[in] delta Transition relation to index. All its target states have to be smaller than @p num_of_states.
 * @param[in] num_of_states Number of states to index.
 * @param[in] source_filter Bool array selecting the sources whose outgoing transitions are indexed.
 */
template <DeltaLike D>
PredecessorIndex<typename D::State> build_predecessor_index(
	const D& delta, const size_t num_of_states, const BoolVector* const source_filter = nullptr
) {
	using State = typename D::State;
	using Target = typename D::Target;
	PredecessorIndex<State> index{};
	index.offsets.assign(num_of_states + 1, 0);
	const size_t num_of_delta_states{std::min(num_of_states, static_cast<size_t>(delta.num_of_states()))};
	const auto kept = [&](const State source) { return source_filter == nullptr || (*source_filter)[source]; };

	// Count the kept incoming transitions of each state in the offset of the following state.
	for (State source{0}; source < num_of_delta_states; ++source) {
		if (!kept(source)) { continue; }
		delta.for_each_successor(source, [&](const Target& target) {
			MATA_ASSERT(D::state_of(target) < num_of_states);
			++index.offsets[D::state_of(target) + 1];
		});
	}
	// Prefix sums turn the counts into the start of the predecessor slice of each state.
	for (size_t state{0}; state < num_of_states; ++state) { index.offsets[state + 1] += index.offsets[state]; }

	index.sources.resize(index.offsets.back());
	std::vector<size_t> fill_cursors{index.offsets.begin(), std::prev(index.offsets.end())};
	for (State source{0}; source < num_of_delta_states; ++source) {
		if (!kept(source)) { continue; }
		delta.for_each_successor(source, [&](const Target& target) {
			index.sources[fill_cursors[D::state_of(target)]++] = source;
		});
	}
	// Each slice has to be filled exactly up to the start of the next one.
	MATA_ASSERT(std::ranges::equal(
		fill_cursors, std::span<const size_t>{index.offsets}.subspan(1)
	));
	return index;
}
} // namespace detail.

template <DeltaLike D>
typename AutomatonBase<D>::StateSet
AutomatonBase<D>::get_reachable_states(const std::function<bool(State)>& filter) const {
	const StateBoolArray reachable_bool_array{reachable_states_(initial)};

	StateSet reachable_states{};
	const size_t num_of_states{this->num_of_states()};
	for (State state{0}; state < num_of_states; ++state) {
		if (reachable_bool_array[state] && (not filter || filter(state))) { reachable_states.insert(state); }
	}
	return reachable_states;
}

template <DeltaLike D>
typename AutomatonBase<D>::StateSet AutomatonBase<D>::get_terminating_states() const {
	// States that can reach a final state, by a backward search over predecessors. Building the reverted automaton
	//  first costs one sorted insert per transition, while the traversal only needs the predecessor lists.
	const size_t num_of_states{this->num_of_states()};
	const auto pred_index{detail::build_predecessor_index(delta, num_of_states)};

	BoolVector terminating(num_of_states, false);
	std::vector<State> worklist{};
	for (const State state : final) {
		if (state < num_of_states && !terminating[state]) {
			terminating[state] = true;
			worklist.push_back(state);
		}
	}
	while (!worklist.empty()) {
		const State state{worklist.back()};
		worklist.pop_back();
		for (size_t i{pred_index.offsets[state]}; i < pred_index.offsets[state + 1]; ++i) {
			const State predecessor{pred_index.sources[i]};
			if (!terminating[predecessor]) {
				terminating[predecessor] = true;
				worklist.push_back(predecessor);
			}
		}
	}

	StateSet result{};
	for (State state{0}; state < num_of_states; ++state) {
		if (terminating[state]) { result.insert(state); }
	}
	return result;
}

template <DeltaLike D> std::vector<typename AutomatonBase<D>::State> AutomatonBase<D>::distances_from_initial() const {
	// Spelled off @c State rather than through @c mata::Limits, which is tied to @c mata::State.
	std::vector<State> distances(num_of_states() + 1, std::numeric_limits<State>::max());
	BoolVector visited(num_of_states() + 1, false);
	std::deque<State> que;

	for (const State qi : initial) {
		visited[qi] = true;
		distances[qi] = 0;
		que.push_back(qi);
	}

	while (!que.empty()) {
		const State src = que.front();
		que.pop_front();
		delta.for_each_successor(src, [&](const Target& target) {
			const State target_state{D::state_of(target)};
			if (!visited[target_state]) {
				visited[target_state] = true;
				distances[target_state] = distances[src] + 1;
				que.push_back(target_state);
			}
		});
	}

	return distances;
}

template <DeltaLike D>
std::vector<typename AutomatonBase<D>::State> AutomatonBase<D>::distances_to_final() const {
	// A BFS from the final states over the predecessor lists, which is all the reverted automaton would be used for.
	const size_t num_states{this->num_of_states()};
	const auto pred_index{detail::build_predecessor_index(delta, num_states)};

	constexpr State unreached{std::numeric_limits<State>::max()};
	std::vector<State> distances(num_states + 1, unreached);
	std::vector<State> queue;
	queue.reserve(num_states);
	for (const State target_final : final) {
		distances[target_final] = 0;
		queue.push_back(target_final);
	}
	for (size_t head{0}; head < queue.size(); ++head) {
		const State target{queue[head]};
		if (target >= num_states) { continue; }
		for (size_t i{pred_index.offsets[target]}; i < pred_index.offsets[target + 1]; ++i) {
			if (const State predecessor{pred_index.sources[i]}; distances[predecessor] == unreached) {
				distances[predecessor] = distances[target] + 1;
				queue.push_back(predecessor);
			}
		}
	}
	return distances;
}

/**
 * @brief This function employs non-recursive version of Tarjan's algorithm for finding SCCs
 * (see https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm, in particular
 * strongconnect(v)) The method saturates a bool vector @p reached_and_reaching in a way that reached_and_reaching[i] =
 * true iff the state `i` is useful at the end. To break the recursiveness, we use @p program_stack simulating the
 * program stack during the recursive calls of strongconnect(v) (see the wiki).
 *
 * Node data
 *  - lowlink, index, on_stack (the same as from strongconnect(v))
 *  - initialized (flag denoting whether the node started to be processing in strongconnect)
 *  - bunch of iterators allowing to iterate over successors (and store the state of the iteration)
 *
 * Program stack @p program_stack
 *  - contains nodes
 *  - node on the top is being currently processed
 *  - node is removed after it has been completely processed (after the end of strongconnect)
 *
 * Simulation of strongconnect( @p act_state = v )
 *  - if @p act_state is not initialized yet (corresponds to the initial phase of strongconnect), initialize
 *  - if @p act_state has already been initialized (i.e., processing of @p act_state was resumed by a
 *    recursive call, which already finished and we continue in processing of @p act_state ), we set
 *    @p act_state lowlink to min of current lowlink and the current successor @p act_succ of @p act_state.
 *    @p act_succ corresponds to w in strongconnect(v). In particular, in strongconnect(v) we called
 *    strongconnect(w) and now we continue after the return.
 *  - Then, we continue iterating over successors @p next_state of @p act_state:
 *      * if @p next_state is not initialized (corresponds to the first if in strongconnect(v)), we simulate
 *        the recursive call of strongconnect( @p next_state ): we put @p next_state on @p program_stack and
 *        jump to the processing of a new node from @p program_stack (we do not remove @p act_state from program
 *        stack yet).
 *      * otherwise update the lowlink
 *  - The rest corresponds to the last part of strongconnect(v) with a difference that if a node in the closed
 *    SCC if useful, we declare all nodes in the SCC useful and moreover we propagate usefulness also the states
 *    in @p tarjan_stack as it contains states that can reach this closed SCC.
 *
 */
template <DeltaLike D>
void AutomatonBase<D>::tarjan_scc_discover(
	const TarjanDiscoverCallback& callback,
	const std::optional<std::reference_wrapper<const utils::SparseSet<State>>> initial_states
) const {
	// A copy of a std::function copies its target, so a stateful callback would mutate the copy instead of the
	//  caller's object. Wrap with lambdas that forward to the originals, and keep the old skips of empty functions.
	auto state_discover = [&callback](const State state) {
		if (!callback.state_discover) { return false; }
		return callback.state_discover(state);
	};
	auto succ_state_discover = [&callback](const State from, const State to) {
		if (callback.succ_state_discover) { callback.succ_state_discover(from, to); }
	};
	auto scc_discover = [&callback](const std::span<const State> scc, const std::span<const State> remaining_stack) {
		if (!callback.scc_discover) { return false; }
		// Keep the std::vector-based public interface and its pop order (top of the Tarjan stack first). Internal
		//  callers take the spans directly and avoid these copies.
		const std::vector<State> scc_vector{scc.rbegin(), scc.rend()};
		const std::vector<State> stack_vector{remaining_stack.begin(), remaining_stack.end()};
		return callback.scc_discover(scc_vector, stack_vector);
	};
	auto scc_state_discover = [&callback](const State state) {
		if (callback.scc_state_discover) { callback.scc_state_discover(state); }
	};
	const internal::TarjanCallbacks callbacks{
		std::move(state_discover), std::move(succ_state_discover), std::move(scc_discover),
		std::move(scc_state_discover)
	};
	internal::tarjan_scc_discover(delta, this->num_of_states(), initial_states.value_or(this->initial), callbacks);
}

template <DeltaLike D>
BoolVector AutomatonBase<D>::get_useful_states(
	const std::optional<std::reference_wrapper<const utils::SparseSet<State>>> initial_states,
	const std::optional<std::reference_wrapper<const utils::SparseSet<State>>> final_states
) const {
	const size_t num_of_states{this->num_of_states()};
	const utils::SparseSet<State>& used_initial_states{initial_states.value_or(initial)};
	const utils::SparseSet<State>& used_final_states{final_states.value_or(this->final)};

	// Forward pass: find all states reachable from initial states.
	const BoolVector reachable{reachable_states_(used_initial_states)};

	// Build predecessor index for forward-reachable states.
	const auto pred_index{detail::build_predecessor_index(delta, num_of_states, &reachable)};

	// Backward pass: BFS from final states over the predecessor index.
	// Mark only the final states that are forward-reachable.
	BoolVector useful(num_of_states, false);
	std::vector<State> worklist{};
	for (const State state : used_final_states) {
		if (state < num_of_states && reachable[state]) {
			worklist.push_back(state);
			useful[state] = true;
		}
	}

	// BFS backward over predecessors.
	while (!worklist.empty()) {
		const State state{worklist.back()};
		worklist.pop_back();

		// Iterate over predecessors of state.
		const size_t pred_start{pred_index.offsets[state]};
		const size_t pred_end{pred_index.offsets[state + 1]};
		for (size_t pred_idx{pred_start}; pred_idx < pred_end; ++pred_idx) {
			const State pred_state{pred_index.sources[pred_idx]};
			if (!useful[pred_state]) {
				useful[pred_state] = true;
				worklist.push_back(pred_state);
			}
		}
	}

	return useful;
}

template <DeltaLike D> bool AutomatonBase<D>::find_accepting_path_(std::vector<State>& path) const {
	// BFS over a vector queue, so the witness is still shortest in the number of transitions.
	// `predecessors[q] == unvisited` marks an unvisited state, an initial state is its own predecessor.
	constexpr State unvisited{std::numeric_limits<State>::max()};
	const size_t num_states{num_of_states()};
	std::vector<State> worklist{};
	std::vector<State> predecessors(num_states, unvisited);
	for (const State state : initial) {
		if (state < num_states && predecessors[state] == unvisited) {
			predecessors[state] = state;
			worklist.push_back(state);
		}
	}

	for (size_t head{0}; head < worklist.size(); ++head) {
		State state{worklist[head]};
		if (final[state]) {
			path.clear();
			path.push_back(state);
			while (predecessors[state] != state) {
				state = predecessors[state];
				path.push_back(state);
			}
			std::ranges::reverse(path);
			return true;
		}
		// No `delta.empty()` guard: visiting the successors of an empty or unallocated state post does nothing.
		delta.for_each_successor(state, [&](const Target& target) {
			const State target_state{D::state_of(target)};
			if (predecessors[target_state] != unvisited) { return; }
			predecessors[target_state] = state;
			worklist.push_back(target_state);
		});
	}
	return false;
}

template <DeltaLike D> bool AutomatonBase<D>::has_no_accepting_path() const {
	// Reachability only: a breadth-first search that stops at the first final state. Tarjan's SCC discovery used to
	//  answer this and pays for its per-state bookkeeping even when the whole reachable part has to be traversed.
	//  The queue order also keeps the scan of `delta` close to ascending, which a LIFO worklist does not.
	const size_t num_states{num_of_states()};
	std::vector<State> worklist{};
	BoolVector visited(num_states, false);
	for (const State state : initial) {
		if (state < num_states && !visited[state]) {
			visited[state] = true;
			worklist.push_back(state);
		}
	}

	for (size_t head{0}; head < worklist.size(); ++head) {
		const State state{worklist[head]};
		// Once per visited state, not once per incoming transition: `final` is a sparse set, so every lookup is an
		//  indirect read and there are more transitions than states.
		if (final.contains(state)) { return false; }
		delta.for_each_successor(state, [&](const Target& target) {
			const State target_state{D::state_of(target)};
			if (visited[target_state]) { return; }
			visited[target_state] = true;
			worklist.push_back(target_state);
		});
	}
	return true;
}

template <DeltaLike D> bool AutomatonBase<D>::is_acyclic() const {
	bool acyclic = true;
	const internal::TarjanCallbacks callbacks{
		std::nullptr_t{}, std::nullptr_t{},
		[this, &acyclic](const std::span<const State> scc, const std::span<const State> remaining_stack) {
			(void) remaining_stack;
			if (scc.size() > 1 || delta.has_self_loop(scc[0])) {
				acyclic = false;
				return true;
			}
			return false;
		},
		std::nullptr_t{}
	};
	internal::tarjan_scc_discover(delta, this->num_of_states(), this->initial, callbacks);
	return acyclic;
}

template <DeltaLike D>
template <typename Self>
bool AutomatonBase<D>::is_identical(this const Self& self, const Self& other) {
	if (utils::OrdVector<State>(self.initial) != utils::OrdVector<State>(other.initial)) { return false; }
	if (utils::OrdVector<State>(self.final) != utils::OrdVector<State>(other.final)) { return false; }
	return self.delta == other.delta;
}

template <DeltaLike D>
template <typename Self>
Self& AutomatonBase<D>::trim(this Self& self, StateRenaming* state_renaming) {
	const BoolVector useful_states{self.get_useful_states()};
	return self.trim_impl(useful_states, state_renaming);
}


template <DeltaLike D>
template <typename Self>
Self& AutomatonBase<D>::trim_impl(this Self& self, const BoolVector& useful_states, StateRenaming* state_renaming) {
	const size_t useful_states_size{useful_states.size()};
	// Nothing to remove: the renaming would be the identity, so the transitions are left as they are.
	if (std::ranges::all_of(useful_states, [](const auto is_useful) { return is_useful != 0; })) {
		if (state_renaming != nullptr) {
			state_renaming->clear();
			state_renaming->reserve(useful_states_size);
			for (State q{0}; q < useful_states_size; ++q) { (*state_renaming)[q] = q; }
		}
		return self;
	}
	std::vector<State> renaming(useful_states_size);
	for (State new_state{0}, orig_state{0}; orig_state < useful_states_size; ++orig_state) {
		if (useful_states[orig_state]) {
			renaming[orig_state] = new_state;
			++new_state;
		}
	}

	self.delta.defragment(useful_states, renaming);
	// Only useful states have a meaningful entry in `renaming`.
	// Removed states keep the default zero.
	// Check that the useful-state projection is the expected dense, ascending sequence.
	MATA_ASSERT(
		[&] {
			State expected_new_state{0};
			for (State orig_state{0}; orig_state < useful_states_size; ++orig_state) {
				if (useful_states[orig_state]) {
					if (renaming[orig_state] != expected_new_state) { return false; }
					++expected_new_state;
				}
			}
			return true;
		}(),
		"AutomatonBase::trim_impl: useful states must be renamed densely in ascending order."
	);

	auto is_state_useful = [&](const State q) { return q < useful_states_size && useful_states[q]; };
	self.initial.filter(is_state_useful);
	self.final.filter(is_state_useful);
	auto rename_state = [&](const State q) { return renaming[q]; };
	self.initial.rename(rename_state);
	self.final.rename(rename_state);
	self.initial.truncate();
	self.final.truncate();
	if (state_renaming != nullptr) {
		state_renaming->clear();
		state_renaming->reserve(useful_states_size);
		for (State q{0}; q < useful_states_size; ++q) {
			if (useful_states[q]) { (*state_renaming)[q] = renaming[q]; }
		}
	}
	return self;
}

} // namespace mata.

#endif // MATA_CORE_AUTOMATON_TPP_
