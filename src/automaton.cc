/** @file
 * @brief Structural, language-agnostic operations shared by all automata in Mata.
 */

#include <algorithm>
#include <deque>

#include "mata/automaton.hh"
#include "mata/utils/sparse-set.hh"

using namespace mata;
using namespace mata::utils;

using mata::nfa::Delta;
using mata::nfa::State;
using mata::nfa::StateSet;
using mata::nfa::SuccessorCursor;

using StateBoolArray = std::vector<bool>; ///< Bool array for states in the automaton.

namespace {
/**
 * Compute reachability of states considering only specified states.
 *
 * @param[in] aut Automaton to compute reachability for.
 * @param[in] states_to_consider State to consider as potentially reachable. If @c std::nullopt is used, all states
 *  are considered as potentially reachable.
 * @return Bool array for reachable states (from initial states): true for reachable, false for unreachable states.
 */
StateBoolArray reachable_states(
	const Automaton& aut, const std::optional<const StateBoolArray>& states_to_consider = std::nullopt
) {
	std::vector<State> worklist{};
	StateBoolArray reachable(aut.num_of_states(), false);
	for (const State state : aut.initial) {
		if (!states_to_consider.has_value() || states_to_consider.value()[state]) {
			worklist.push_back(state);
			reachable.at(state) = true;
		}
	}

	while (!worklist.empty()) {
		const State state{worklist.back()};
		worklist.pop_back();
		aut.delta.for_each_successor(state, [&](const State target_state) {
			if (!reachable[target_state] &&
				(!states_to_consider.has_value() || states_to_consider.value()[target_state])) {
				worklist.push_back(target_state);
				reachable[target_state] = true;
			}
		});
	}
	return reachable;
}

// A structure to store metadata related to each state/node during the computation
// of useful states. It contains Tarjan's metadata and the state of the
// iteration through the successors.
struct TarjanNodeData {
	SuccessorCursor::const_iterator current_successor_it{};
	// index of a node (corresponds to the time of discovery)
	unsigned long index{0};
	// index of a lower node in the same SCC
	unsigned long lowlink{0};
	// was the node already initialized (=the initial phase of the Tarjan's recursive call was executed)
	bool initilized{false};
	// is node on Tarjan's stack?
	bool on_stack{false};

	TarjanNodeData() = default;

	TarjanNodeData(const State q, const Delta& delta, const unsigned long index)
		: current_successor_it(delta.successor_cursor(q).begin()),
		  index(index),
		  lowlink(index),
		  initilized(true),
		  on_stack(true) {}
};
} // anonymous namespace

Automaton& Automaton::operator=(Automaton&& other) noexcept {
	if (this != &other) {
		delta = std::move(other.delta);
		initial = std::move(other.initial);
		final = std::move(other.final);
	}
	return *this;
}

State Automaton::add_state() {
	const size_t num_of_states{this->num_of_states()};
	delta.allocate(num_of_states + 1);
	return num_of_states;
}

State Automaton::add_state(const State state) {
	if (state >= delta.num_of_states()) { delta.allocate(state + 1); }
	return state;
}

size_t Automaton::num_of_states() const {
	return std::max({initial.domain_size(), final.domain_size(), delta.num_of_states()});
}

void Automaton::clear() {
	delta.clear();
	initial.clear();
	final.clear();
}

Automaton Automaton::reverted() const {
	Automaton result{};

	const size_t num_of_states{this->num_of_states()};
	result.delta.allocate(num_of_states);

	for (State source_state{0}; source_state < num_of_states; ++source_state) {
		delta.for_each_move(source_state, [&](const Symbol symbol, const State target_state) {
			result.delta.add(target_state, symbol, source_state);
		});
	}

	result.initial = final;
	result.final = initial;

	return result;
}

StateSet Automaton::get_reachable_states(const std::function<bool(State)>& filter) const {
	const StateBoolArray reachable_bool_array{reachable_states(*this)};

	StateSet reachable_states{};
	const size_t num_of_states{this->num_of_states()};
	for (State state{0}; state < num_of_states; ++state) {
		if (reachable_bool_array[state] && (not filter || filter(state))) { reachable_states.insert(state); }
	}
	return reachable_states;
}

StateSet Automaton::get_terminating_states() const { return reverted().get_reachable_states(); }

std::vector<State> Automaton::distances_from_initial() const {
	std::vector<State> distances(num_of_states() + 1, nfa::Limits::max_state);
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
		delta.for_each_successor(src, [&](const State target) {
			if (!visited[target]) {
				visited[target] = true;
				distances[target] = distances[src] + 1;
				que.push_back(target);
			}
		});
	}

	return distances;
}

std::vector<State> Automaton::distances_to_final() const { return reverted().distances_from_initial(); }

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
void Automaton::tarjan_scc_discover(
	const TarjanDiscoverCallback& callback,
	const std::optional<std::reference_wrapper<const SparseSet<State>>> initial_states
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
			const State act_succ = *act_state_data.current_successor_it;
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
			next_state = *act_state_data.current_successor_it;
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

BoolVector Automaton::get_useful_states(
	const std::optional<std::reference_wrapper<const SparseSet<State>>> initial_states,
	const std::optional<std::reference_wrapper<const SparseSet<State>>> final_states
) const {
	BoolVector useful(this->num_of_states(), false);
	bool final_scc = false;

	const SparseSet<State>& used_initial_states{initial_states.value_or(initial)};
	const SparseSet<State>& used_final_states{final_states.value_or(this->final)};

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

bool Automaton::has_no_accepting_path() const {
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

bool Automaton::is_acyclic() const {
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
