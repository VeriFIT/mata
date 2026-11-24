/** @file
 * @brief Iterative Tarjan SCC traversal over any relation, shared by @c mata::AutomatonBase and leaf algorithms.
 */

#ifndef MATA_CORE_TARJAN_HH
#define MATA_CORE_TARJAN_HH

#include <algorithm>
#include <cstddef>
#include <functional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <iterator>

#include "mata/utils/sparse-set.hh"

namespace mata::internal {

/// Calls @p callback unless it is @c nullptr or an empty @c std::function. Returns true iff it asks to stop.
template <typename Callback, typename... Args> bool call_callback(const Callback& callback, const Args&... args) {
	if constexpr (std::is_same_v<Callback, std::nullptr_t>) {
		return false;
	} else {
		if constexpr (std::is_constructible_v<bool, const Callback&>) {
			if (!static_cast<bool>(callback)) { return false; }
		}
		if constexpr (std::is_void_v<std::invoke_result_t<const Callback&, const Args&...>>) {
			callback(args...);
			return false;
		} else {
			return callback(args...);
		}
	}
}

template <typename StateDiscover, typename SuccDiscover, typename SccDiscover, typename SccStateDiscover>
struct TarjanCallbacks {
	StateDiscover state_discover_{};
	SuccDiscover succ_state_discover_{};
	SccDiscover scc_discover_{};
	SccStateDiscover scc_state_discover_{};
};

template <typename Delta> struct TarjanNodeData {
	using State = typename Delta::State;
	using SuccessorIterator =
		decltype(std::declval<const Delta&>().successor_cursor(std::declval<State>()).begin());
	SuccessorIterator current_successor_it{};
	unsigned long index{0};
	unsigned long lowlink{0};
	bool initialized{false};
	bool on_stack{false};

	TarjanNodeData() = default;
	TarjanNodeData(const State state, const Delta& delta, const unsigned long index)
		: current_successor_it{delta.successor_cursor(state).begin()},
		  index{index},
		  lowlink{index},
		  initialized{true},
		  on_stack{true} {}
};

/**
 * Run the non-recursive Tarjan traversal over @p delta starting at @p initial_states.
 *
 * The SCC callback receives its members in Tarjan-stack order (bottom to top, i.e., the reverse of the order in which
 *  they are popped) and the remaining Tarjan stack. Both spans are valid only for the duration of that callback.
 *  @c nullptr callbacks are skipped at compile time. The traversal stops when a state- or SCC-level callback returns
 *  true.
 */
template <typename Delta, typename Callbacks>
void tarjan_scc_discover(
	const Delta& delta,
	const size_t num_of_states,
	const utils::SparseSet<typename Delta::State>& initial_states,
	Callbacks&& callbacks
) {
	using State = typename Delta::State;
	std::vector<TarjanNodeData<Delta>> node_info(num_of_states);
	std::vector<State> program_stack{};
	std::vector<State> tarjan_stack{};
	program_stack.reserve(num_of_states);
	tarjan_stack.reserve(num_of_states);
	unsigned long index_cnt{0};
	for (const State state : initial_states) { program_stack.push_back(state); }

	while (!program_stack.empty()) {
		const State act_state{program_stack.back()};
		TarjanNodeData<Delta>& act_state_data{node_info[act_state]};
		if (act_state_data.initialized && !act_state_data.on_stack) {
			program_stack.pop_back();
			continue;
		}
		if (!act_state_data.initialized) {
			act_state_data = TarjanNodeData<Delta>(act_state, delta, index_cnt++);
			tarjan_stack.push_back(act_state);
			if (call_callback(callbacks.state_discover_, act_state)) { return; }
		} else {
			const State act_succ{Delta::state_of(*act_state_data.current_successor_it)};
			act_state_data.lowlink = std::min(act_state_data.lowlink, node_info[act_succ].lowlink);
			++act_state_data.current_successor_it;
		}

		bool rec_call{false};
		for (; act_state_data.current_successor_it != std::default_sentinel; ++act_state_data.current_successor_it) {
			const State next_state{Delta::state_of(*act_state_data.current_successor_it)};
			call_callback(callbacks.succ_state_discover_, act_state, next_state);
			if (!node_info[next_state].initialized) {
				program_stack.push_back(next_state);
				rec_call = true;
				break;
			}
			if (node_info[next_state].on_stack) {
				act_state_data.lowlink = std::min(act_state_data.lowlink, node_info[next_state].index);
			}
		}
		if (rec_call) { continue; }

		if (act_state_data.lowlink == act_state_data.index) {
			// The SCC is the top of the stack down to act_state; search from the top to stay O(|SCC|).
			const auto act_state_it{std::find(tarjan_stack.rbegin(), tarjan_stack.rend(), act_state)};
			const size_t stack_pos{static_cast<size_t>(tarjan_stack.rend() - act_state_it) - 1};
			for (size_t i{tarjan_stack.size()}; i > stack_pos; --i) {
				const State state{tarjan_stack[i - 1]};
				node_info[state].on_stack = false;
				call_callback(callbacks.scc_state_discover_, state);
			}
			const std::span<const State> stack_view{tarjan_stack};
			if (call_callback(callbacks.scc_discover_, stack_view.subspan(stack_pos), stack_view.first(stack_pos))) {
				return;
			}
			tarjan_stack.resize(stack_pos);
		}
		program_stack.pop_back();
	}
}

} // namespace mata::internal

#endif // MATA_CORE_TARJAN_HH
