/** @file
 * @brief Member definitions for @c mata::AutomatonBase.
 *
 * Included at the end of @c mata/core/automaton.hh. Every definition here is a walk over @c delta,
 *  @c initial and @c final, reaching successors only through the @c mata::DeltaLike interface.
 *
 * @note The bodies live in a header, rather than in `src/core/automaton.cc`, because a third party
 *  instantiating @c AutomatonBase over its own relation needs them. The one in-tree instantiation
 *  is kept out of every translation unit by the @c extern template declaration in the header.
 */

#ifndef MATA_CORE_AUTOMATON_TPP_
#define MATA_CORE_AUTOMATON_TPP_

#include <algorithm>
#include <deque>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <tuple>
#include <utility>
#include <unordered_set>

#include "mata/utils/assert.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/utils/sparse-set.hh"

namespace mata {

template <DeltaLike D> AutomatonBase<D>& AutomatonBase<D>::operator=(AutomatonBase&& other) noexcept {
	if (this != &other) {
		delta = std::move(other.delta);
		initial = std::move(other.initial);
		final = std::move(other.final);
	}
	return *this;
}

template <DeltaLike D> typename AutomatonBase<D>::State AutomatonBase<D>::add_state() {
	const size_t num_of_states{this->num_of_states()};
	delta.allocate(num_of_states + 1);
	return num_of_states;
}

template <DeltaLike D> typename AutomatonBase<D>::State AutomatonBase<D>::add_state(const State state) {
	if (state >= delta.num_of_states()) { delta.allocate(state + 1); }
	return state;
}

template <DeltaLike D> size_t AutomatonBase<D>::num_of_states() const {
	return std::max({initial.domain_size(), final.domain_size(), delta.num_of_states()});
}

template <DeltaLike D> void AutomatonBase<D>::clear() {
	delta.clear();
	initial.clear();
	final.clear();
}

namespace detail {
/// Write one walked-out move back with its source and target exchanged, keys untouched.
///
/// The move arrives from @c for_each_move as `(keys..., target)`; @c insert_target wants the target
///  first and the keys last, so the pack is re-ordered through a tuple. That is the whole of it —
///  there is no shape change and no variadic @c add.
template <typename Delta, typename State, typename Move, size_t... Keys>
void insert_reversed(Delta& delta, const State source, const Move& move, std::index_sequence<Keys...>) {
	const auto& target{std::get<sizeof...(Keys)>(move)};
	posts::insert_target(delta.mutable_state_post(Delta::state_of(target)), source, std::get<Keys>(move)...);
}
} // namespace detail.

template <DeltaLike D>
AutomatonBase<D> AutomatonBase<D>::reverted() const {
	AutomatonBase result{};

	const size_t num_of_states{this->num_of_states()};
	result.delta.allocate(num_of_states);

	for (State source_state{0}; source_state < num_of_states; ++source_state) {
		// Every move arrives as `(keys..., target)`, at any arity. Reverting exchanges the source and
		//  the target and leaves the keys in the same order, so nothing here inspects a key -- a
		//  relation whose keys are not symbols, or which has more than one of them, works unchanged.
		delta.for_each_move(source_state, [&](const auto&... move) {
			static_assert(sizeof...(move) == D::key_arity + 1, "a move is one key per level, then a target");
			detail::insert_reversed(
				result.delta, source_state, std::forward_as_tuple(move...),
				std::make_index_sequence<sizeof...(move) - 1>{}
			);
		});
	}

	result.initial = final;
	result.final = initial;

	return result;
}

template <DeltaLike D>
typename AutomatonBase<D>::StateBoolArray
AutomatonBase<D>::reachable_states_(const std::optional<const StateBoolArray>& states_to_consider) const {
	std::vector<State> worklist{};
	StateBoolArray reachable(num_of_states(), false);
	for (const State state : initial) {
		if (!states_to_consider.has_value() || states_to_consider.value()[state]) {
			worklist.push_back(state);
			reachable.at(state) = true;
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

template <DeltaLike D>
typename AutomatonBase<D>::StateSet
AutomatonBase<D>::get_reachable_states(const std::function<bool(State)>& filter) const {
	const StateBoolArray reachable_bool_array{reachable_states_()};

	StateSet reachable_states{};
	const size_t num_of_states{this->num_of_states()};
	for (State state{0}; state < num_of_states; ++state) {
		if (reachable_bool_array[state] && (not filter || filter(state))) { reachable_states.insert(state); }
	}
	return reachable_states;
}

template <DeltaLike D>
typename AutomatonBase<D>::StateSet AutomatonBase<D>::get_terminating_states() const {
	return reverted().get_reachable_states();
}

template <DeltaLike D> std::vector<typename AutomatonBase<D>::State> AutomatonBase<D>::distances_from_initial() const {
	// Spelled off @c State rather than through @c mata::Limits, which is tied to @c mata::State.
	std::vector<State> distances(num_of_states() + 1, std::numeric_limits<State>::max());
	BoolVector visited(num_of_states() + 1, false);
	std::deque<State> que;

	for (State qi : initial) {
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
	return reverted().distances_from_initial();
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
	std::vector<TarjanNodeData> node_info(this->num_of_states());
	std::vector<State> program_stack;
	std::vector<State> tarjan_stack;
	unsigned long index_cnt = 0;

	for (const State& q0 : (initial_states.value_or(this->initial)).get()) { program_stack.push_back(q0); }

	while (!program_stack.empty()) {
		State act_state = program_stack.back();
		TarjanNodeData& act_state_data = node_info[act_state];

		// if a node is initialized and is not on stack --> skip it; this state was
		// already processed (=this state is initial and was reachable from another initial).
		if (act_state_data.initilized && !act_state_data.on_stack) {
			program_stack.pop_back();
			continue;
		}

		// node has not been initialized yet --> corresponds to the first call of strongconnect(act_state)
		if (!act_state_data.initilized) {
			// initialize node
			act_state_data = TarjanNodeData(act_state, this->delta, index_cnt++);
			tarjan_stack.push_back(act_state);

			if (callback.state_discover && callback.state_discover(act_state)) { return; }
		} else { // return from the recursive call
			const State act_succ = D::state_of(*act_state_data.current_successor_it);
			act_state_data.lowlink = std::min(act_state_data.lowlink, node_info[act_succ].lowlink);
			// act_succ is the state that caused the recursive call. Move on to the next successor.
			++act_state_data.current_successor_it;
		}

		// iterate through outgoing edges
		State next_state;
		// rec_call simulates call of the strongconnect. Since c++ cannot do continue over
		// multiple loops, we use rec_call to jump to the main loop
		bool rec_call = false;
		for (; act_state_data.current_successor_it != std::default_sentinel; ++act_state_data.current_successor_it) {
			next_state = D::state_of(*act_state_data.current_successor_it);
			if (callback.succ_state_discover) { callback.succ_state_discover(act_state, next_state); }
			if (!node_info[next_state].initilized) { // recursive call
				program_stack.push_back(next_state);
				rec_call = true;
				break;
			} else if (node_info[next_state].on_stack) {
				act_state_data.lowlink = std::min(act_state_data.lowlink, node_info[next_state].index);
			}
		}
		if (rec_call) { continue; }

		// check if we have the root of an SCC
		if (act_state_data.lowlink == act_state_data.index) {
			State st;
			std::vector<State> scc;
			do {
				st = tarjan_stack.back();
				tarjan_stack.pop_back();
				node_info[st].on_stack = false;

				if (callback.scc_state_discover) { callback.scc_state_discover(st); }
				scc.push_back(st);
			} while (st != act_state);
			if (callback.scc_discover && callback.scc_discover(scc, tarjan_stack)) { return; }
		}
		// all successors have been processed, we can remove act_state from the program stack
		program_stack.pop_back();
	}
}

template <DeltaLike D>
BoolVector AutomatonBase<D>::get_useful_states(
	const std::optional<std::reference_wrapper<const utils::SparseSet<State>>> initial_states,
	const std::optional<std::reference_wrapper<const utils::SparseSet<State>>> final_states
) const {
	BoolVector useful(this->num_of_states(), false);
	bool final_scc = false;

	const utils::SparseSet<State>& used_initial_states{initial_states.value_or(initial)};
	const utils::SparseSet<State>& used_final_states{final_states.value_or(this->final)};

	TarjanDiscoverCallback callback{};
	callback.state_discover = [&](const State state) -> bool {
		if (used_final_states.contains(state)) { useful[state] = true; }
		return false;
	};
	callback.scc_discover = [&](const std::vector<State>& scc, const std::vector<State>& tarjan_stack) -> bool {
		if (final_scc) {
			// Propagate usefulness to the closed SCC.
			for (const State& st : scc) { useful[st] = true; }
			// Propagate usefulness to predecessors in @p tarjan_stack.
			for (auto state_it{tarjan_stack.rbegin()}, state_it_end{tarjan_stack.rend()}; state_it != state_it_end;
				 ++state_it) {
				if (useful[*state_it]) { break; }
				useful[*state_it] = true;
			}
		}
		final_scc = false;
		return false;
	};
	callback.scc_state_discover = [&](const State state) {
		if (useful[state]) { final_scc = true; }
	};
	callback.succ_state_discover = [&](const State act_state, const State next_state) {
		if (useful[next_state]) { useful[act_state] = true; }
	};

	tarjan_scc_discover(callback, used_initial_states);
	return useful;
}

template <DeltaLike D> bool AutomatonBase<D>::find_accepting_path_(std::vector<State>& path) const {
	std::list<State> worklist(initial.begin(), initial.end());
	std::unordered_set<State> processed(initial.begin(), initial.end());

	// 'paths[s] == t' denotes that state 's' was accessed from state 't',
	// 'paths[s] == s' means that 's' is an initial state
	std::map<State, State> paths;
	for (const State s : worklist) { paths[s] = s; }

	while (!worklist.empty()) {
		State state{worklist.front()};
		worklist.pop_front();

		if (final[state]) {
			path.clear();
			path.push_back(state);
			while (paths[state] != state) {
				state = paths[state];
				path.push_back(state);
			}
			std::ranges::reverse(path);
			return true;
		}

		if (delta.empty()) { continue; }

		delta.for_each_successor(state, [&](const Target& target) {
			const State target_state{D::state_of(target)};
			bool inserted;
			std::tie(std::ignore, inserted) = processed.insert(target_state);
			if (inserted) {
				worklist.push_back(target_state);
				paths[target_state] = state;
			} else {
				MATA_ASSERT(utils::haskey(paths, target_state)); /* Invariant. */
			}
		});
	}
	return false;
}

template <DeltaLike D> bool AutomatonBase<D>::has_no_accepting_path_scc_() const {
	bool accepting_state = false;

	TarjanDiscoverCallback callback{};
	callback.state_discover = [&](const State state) -> bool {
		if (this->final.contains(state)) {
			accepting_state = true;
			return true;
		}
		return false;
	};

	tarjan_scc_discover(callback);
	return !accepting_state;
}

template <DeltaLike D> bool AutomatonBase<D>::is_acyclic() const {
	bool acyclic = true;

	TarjanDiscoverCallback callback{};
	callback.scc_discover = [&](const std::vector<State>& scc, const std::vector<State>& tarjan_stack) -> bool {
		(void) tarjan_stack;
		if (scc.size() > 1) {
			acyclic = false;
			return true;
		} else { // check for self-loops
			if (delta.has_self_loop(scc[0])) {
				acyclic = false;
				return true;
			}
		}
		return false;
	};

	tarjan_scc_discover(callback);
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
#ifdef _STATIC_STRUCTURES_
	BoolVector useful_states{self.get_useful_states()};
	useful_states.clear();
	useful_states = self.get_useful_states();
#else
	const BoolVector useful_states{self.get_useful_states()};
#endif
	return self.trim_impl(useful_states, state_renaming);
}


template <DeltaLike D>
template <typename Self>
Self& AutomatonBase<D>::trim_impl(this Self& self, const BoolVector& useful_states, StateRenaming* state_renaming) {
	const size_t useful_states_size{useful_states.size()};
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
