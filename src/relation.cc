/** @file
 * @brief Instantiations of the shipped relation and automaton templates.
 */

#include "mata/relation.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/sparse-set.hh"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <functional>
#include <iterator>
#include <list>
#include <queue>
#include <ranges>
#include <utility>

using namespace mata::utils;
using namespace mata;

/// @name Instantiations on (State, Symbol, SmallStateSet) relation and automaton.
/// @{
template class mata::posts::detail::PostEntryCore<mata::Symbol, mata::SmallStateSet, mata::ReservedKeys<mata::Symbol>>;
template class mata::posts::PostEntry<mata::Symbol, mata::SmallStateSet>;
template class mata::posts::Post<mata::SymbolPost>;
template class mata::posts::PostMoves<mata::StatePost::Base>;

template class mata::posts::KeyedAccess<mata::Delta::Base, mata::StatePost>;
template class mata::posts::DeltaBase<mata::StatePost, mata::TransitionTraits>;
template class mata::posts::DeltaTransitions<mata::Delta::Base>;

template class mata::AutomatonBase<mata::Delta>;
/// @}

namespace {
/// Empty slot of the visited table; no relation can reach it, as that would take max_state + 1 state posts.
constexpr State NO_STATE{Limits::max_state};

/// Slot of @p state in the open-addressed @p table (power-of-two size, at most half full): the slot holding
///  @p state, or the empty slot where it belongs.
size_t slot_of(const std::vector<State>& table, const State state) {
	const auto shift{std::numeric_limits<uint64_t>::digits - std::countr_zero(table.size())};
	const size_t mask{table.size() - 1};
	size_t slot{static_cast<size_t>((static_cast<uint64_t>(state) * 0x9E'37'79'B9'7F'4A'7C'15ULL) >> shift)};
	while (table[slot] != NO_STATE && table[slot] != state) { slot = (slot + 1) & mask; }
	return slot;
}

/// Appends to @p closure every state reachable from it over @p epsilons that it does not hold yet. Visited states are
///  kept in a hash table sized to the closure, so the cost follows the closure, not the number of states of @p delta.
void close_over_epsilons(
	const Delta& delta, const std::vector<Symbol>& epsilons, std::vector<State>& closure, PostWorkspace& workspace
) {
	std::vector<State>& visited{workspace.visited};
	std::vector<State>& worklist{workspace.worklist};
	const auto rebuild_visited{[&](const size_t num_of_slots) {
		visited.assign(num_of_slots, NO_STATE);
		for (const State state : closure) { visited[slot_of(visited, state)] = state; }
	}};
	rebuild_visited(std::bit_ceil(std::max<size_t>(16, 2 * closure.size())));
	worklist.assign(closure.begin(), closure.end());
	while (!worklist.empty()) {
		const State state{worklist.back()};
		worklist.pop_back();
		const StatePost& state_post{delta[state]};
		for (const Symbol epsilon : epsilons) {
			const auto symbol_post_it{Delta::epsilon_symbol_posts(state_post, epsilon)};
			if (symbol_post_it == state_post.end()) { continue; }
			for (const State target : symbol_post_it->targets) {
				const size_t slot{slot_of(visited, target)};
				if (visited[slot] == target) { continue; }
				visited[slot] = target;
				closure.push_back(target);
				worklist.push_back(target);
				if (2 * closure.size() > visited.size()) { rebuild_visited(2 * visited.size()); }
			}
		}
	}
}

/// The targets of @p states over @p symbol, with the closures of @p epsilon_closure_opt, gathered into @p workspace.
std::vector<State>& gather_post(
	const Delta& delta,
	const StateSet& states,
	const Symbol symbol,
	const EpsilonClosureOpt epsilon_closure_opt,
	const std::vector<Symbol>& epsilons,
	PostWorkspace& workspace
) {
	if (closes_before(epsilon_closure_opt)) {
		workspace.sources.assign(states.begin(), states.end());
		close_over_epsilons(delta, epsilons, workspace.sources, workspace);
	}
	const std::vector<State>& sources{closes_before(epsilon_closure_opt) ? workspace.sources : states.to_vector()};

	std::vector<State>& gathered{workspace.gathered};
	gathered.clear();
	// Stepping over epsilon with a closure requested means the source states are reachable as well.
	if (symbol == EPSILON && epsilon_closure_opt != EpsilonClosureOpt::None) {
		gathered.assign(states.begin(), states.end());
	}
	for (const State state : sources) {
		const StatePost& state_post{delta[state]};
		const auto symbol_post_it{state_post.find(symbol)};
		if (symbol_post_it == state_post.end()) { continue; }
		gathered.insert(gathered.end(), symbol_post_it->targets.begin(), symbol_post_it->targets.end());
	}
	if (closes_after(epsilon_closure_opt)) { close_over_epsilons(delta, epsilons, gathered, workspace); }
	return gathered;
}

/// Shift every target of @p state_post by @p offset. The shift is monotone, so the target sets stay sorted.
void shift_state_post(StatePost& state_post, const State offset) {
	for (SymbolPost& symbol_post : state_post) {
		for (State& target : symbol_post.targets) { target += offset; }
	}
}

void check_append_shifted_range(const State offset, const size_t other_num_of_states) {
	if (static_cast<size_t>(Limits::max_state) - offset < other_num_of_states) {
		throw std::overflow_error(
			"Delta::append_shifted: offset (" + std::to_string(offset) + ") + other.num_of_states() (" +
			std::to_string(other_num_of_states) + ") exceeds max_state"
		);
	}
}
} // namespace

StateSet Delta::epsilon_closure(
	const StateSet& states, const std::vector<Symbol>& epsilons, PostWorkspace& workspace
) const {
	std::vector<State>& closure{workspace.gathered};
	closure.assign(states.begin(), states.end());
	close_over_epsilons(*this, epsilons, closure, workspace);
	return StateSet{closure};
}

StateSet Delta::post(
	const StateSet& states,
	const Symbol symbol,
	const EpsilonClosureOpt epsilon_closure_opt,
	const std::vector<Symbol>& epsilons,
	PostWorkspace& workspace
) const {
	return StateSet{gather_post(*this, states, symbol, epsilon_closure_opt, epsilons, workspace)};
}

StateSet Delta::post_adopt_buffer(
	const StateSet& states,
	const Symbol symbol,
	const EpsilonClosureOpt epsilon_closure_opt,
	const std::vector<Symbol>& epsilons,
	PostWorkspace& workspace
) const {
	return StateSet{std::move(gather_post(*this, states, symbol, epsilon_closure_opt, epsilons, workspace))};
}

StateSet Delta::get_successors(const State state, const Symbol symbol, const EpsilonClosureOpt epsilon_closure_opt) const {
	PostWorkspace workspace{};
	return post(StateSet{state}, symbol, epsilon_closure_opt, {EPSILON}, workspace);
}

void Delta::add(const State source, const Symbol symbol, const std::span<const State> targets) {
	if (targets.empty()) { return; }
	// `targets` may refer into this state post (e.g. `get_successors()` of another symbol), and an insert of a new
	//  symbol post relocates the symbol posts: read everything needed from it before inserting.
	resize_for_states(source, targets.back());
	StatePost& state_post{state_posts_[source]};
	if (state_post.empty() || state_post.back().symbol < symbol) {
		state_post.emplace_back(symbol, targets);
		return;
	}
	if (const auto symbol_post_it{state_post.find(symbol)}; symbol_post_it != state_post.end()) {
		symbol_post_it->insert(targets);
	} else {
		state_post.insert(SymbolPost{symbol, targets});
	}
}

void Delta::add(const State source, SymbolPost&& symbol_post) {
	if (symbol_post.targets.empty()) { return; }
	resize_for_states(source, symbol_post.targets.back());
	StatePost& state_post{state_posts_[source]};
	if (state_post.empty() || state_post.back().symbol < symbol_post.symbol) {
		state_post.emplace_back(std::move(symbol_post));
		return;
	}
	if (const auto existing{state_post.find(symbol_post.symbol)}; existing != state_post.end()) {
		// One linear merge of two sorted target sets, instead of a search and a shift per target.
		existing->insert(symbol_post.targets.span());
		return;
	}
	state_post.insert(std::move(symbol_post));
}

// Taken by rvalue reference to be sorted and deduplicated in place.
// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
void Delta::add(std::vector<Transition>&& transitions) {
	if (transitions.empty()) { return; }
	const auto by_all{[](const Transition& lhs, const Transition& rhs) {
		return std::tie(lhs.source, lhs.symbol, lhs.target) < std::tie(rhs.source, rhs.symbol, rhs.target);
	}};
	if (!empty()) {
		std::ranges::sort(transitions, by_all);
		const auto duplicates{std::ranges::unique(transitions)};
		transitions.erase(duplicates.begin(), duplicates.end());
		size_t index{0};
		const size_t size{transitions.size()};
		while (index < size) {
			const State source{transitions[index].source};
			const Symbol symbol{transitions[index].symbol};
			size_t run_end{index + 1};
			while (run_end < size && transitions[run_end].source == source && transitions[run_end].symbol == symbol) {
				++run_end;
			}
			SymbolPost symbol_post{symbol};
			symbol_post.targets.reserve(run_end - index); // Final form: inline, or one block of exact capacity.
			for (; index < run_end; ++index) { symbol_post.targets.push_back(transitions[index].target); }
			add(source, std::move(symbol_post));
		}
		return;
	}

	// A relation without transitions, as readers leave it after adding the states, is built anew, keeping its states.
	//  The transitions are bucketed by source in linear time, so that only the few transitions of each source get sorted.
	const size_t num_of_states_before{num_of_states()};
	const State max_source{std::ranges::max_element(transitions, {}, &Transition::source)->source};
	const size_t num_of_sources{num_of_states_for(max_source)};
	// Counted at the next source, so that placing the transitions leaves each source at the end of its bucket.
	std::vector<size_t> bucket_ends(num_of_sources + 1, 0);
	for (const Transition& transition : transitions) { ++bucket_ends[static_cast<size_t>(transition.source) + 1]; }
	std::partial_sum(bucket_ends.begin(), bucket_ends.end(), bucket_ends.begin());
	std::vector<Transition> by_source(transitions.size());
	for (const Transition& transition : transitions) {
		by_source[bucket_ends[static_cast<size_t>(transition.source)]++] = transition;
	}
	// Released before the posts take as much memory again.
	transitions.clear();
	transitions.shrink_to_fit();

	DeltaBuilder builder{std::max(num_of_states_before, num_of_sources)};
	size_t begin{0};
	for (size_t source{0}; source < num_of_sources; ++source) {
		const size_t end{bucket_ends[source]};
		if (begin == end) { continue; }
		std::sort(
			by_source.begin() + static_cast<std::ptrdiff_t>(begin),
			by_source.begin() + static_cast<std::ptrdiff_t>(end), [](const Transition& lhs, const Transition& rhs) {
				return std::tie(lhs.symbol, lhs.target) < std::tie(rhs.symbol, rhs.target);
			}
		);
		builder.begin_state(to_state(source));
		size_t num_of_symbols{1};
		for (size_t index{begin + 1}; index < end; ++index) {
			if (by_source[index].symbol != by_source[index - 1].symbol) { ++num_of_symbols; }
		}
		builder.reserve_symbols(num_of_symbols);
		size_t index{begin};
		while (index < end) {
			const Symbol symbol{by_source[index].symbol};
			builder.begin_symbol(symbol);
			builder.push_sorted_target(by_source[index].target);
			for (++index; index < end && by_source[index].symbol == symbol; ++index) {
				// Duplicates are skipped.
				if (by_source[index].target != by_source[index - 1].target) {
					builder.push_sorted_target(by_source[index].target);
				}
			}
			builder.finish_symbol();
		}
		builder.finish_state();
		begin = end;
	}
	*this = builder.finish();
	if (num_of_states() < num_of_states_before) { allocate(num_of_states_before); }
}

Delta& Delta::append_shifted(const Delta& other, const State offset) {
	// Self-append: one controlled snapshot, then the consuming path.
	if (this == &other) { return append_shifted(Delta{other}, offset); }
	check_append_shifted_range(offset, other.num_of_states());
	if (num_of_states() < offset) { state_posts_.resize(offset); }
	state_posts_.reserve(state_posts_.size() + other.state_posts_.size());
	for (const StatePost& other_post : other.state_posts_) {
		StatePost shifted_post{other_post};
		shift_state_post(shifted_post, offset);
		state_posts_.push_back(std::move(shifted_post));
	}
	return *this;
}

// The posts of 'other' are moved out one by one below.
// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
Delta& Delta::append_shifted(Delta&& other, const State offset) {
	if (this == &other) { return append_shifted(Delta{other}, offset); }
	check_append_shifted_range(offset, other.num_of_states());
	if (num_of_states() < offset) { state_posts_.resize(offset); }
	state_posts_.reserve(state_posts_.size() + other.state_posts_.size());
	for (StatePost& other_post : other.state_posts_) {
		StatePost shifted_post{std::move(other_post)};
		shift_state_post(shifted_post, offset);
		state_posts_.push_back(std::move(shifted_post));
	}
	other.clear();
	return *this;
}

Delta Delta::reverted(const size_t num_of_states) const {
	// A target must never lie outside the state domain, but nothing enforces it, and building the result by
	//  repeated `add(target, symbol, source)` would have grown it to such a target; stay faithful to that.
	size_t num_of_reversed_states{num_of_states};
	for (const StatePost& state_post : state_posts_) {
		for (const SymbolPost& symbol_post : state_post) {
			// The targets are sorted, so the last one is the largest.
			if (!symbol_post.targets.empty() && num_of_reversed_states <= symbol_post.targets.back()) {
				num_of_reversed_states = static_cast<size_t>(symbol_post.targets.back()) + 1;
			}
		}
	}

	// Count the incoming transitions of each state in the offset of the following state,
	//  then prefix-sum the counts into the start of the slice of each target.
	std::vector<size_t> offsets(num_of_reversed_states + 1, 0);
	for (const StatePost& state_post : state_posts_) {
		for (const SymbolPost& symbol_post : state_post) {
			for (const State target : symbol_post.targets) { ++offsets[static_cast<size_t>(target) + 1]; }
		}
	}
	for (size_t target{0}; target < num_of_reversed_states; ++target) { offsets[target + 1] += offsets[target]; }

	struct ReversedEdge {
		Symbol symbol;
		State source;
		bool operator<(const ReversedEdge& other) const {
			return symbol < other.symbol || (symbol == other.symbol && source < other.source);
		}
		bool operator==(const ReversedEdge& other) const = default;
	};

	// Scatter the pairs (symbol, source) into the slice of their target; sources land in increasing order.
	std::vector<ReversedEdge> edges(offsets.back());
	std::vector<size_t> fill_cursors{offsets.begin(), std::prev(offsets.end())};
	for (State source{0}; source < state_posts_.size(); ++source) {
		for (const SymbolPost& symbol_post : state_posts_[source]) {
			for (const State target : symbol_post.targets) {
				edges[fill_cursors[target]++] = ReversedEdge{.symbol = symbol_post.symbol, .source = source};
			}
		}
	}

	Delta result{};
	result.reserve(num_of_reversed_states);
	for (size_t target{0}; target < num_of_reversed_states; ++target) {
		// Sorting the slice by (symbol, source) yields exactly the state post of the reversed state: symbols
		//  in ascending order, sources sorted and, after the dedup, unique.
		const auto slice_first{edges.begin() + static_cast<std::ptrdiff_t>(offsets[target])};
		const auto slice_end{edges.begin() + static_cast<std::ptrdiff_t>(offsets[target + 1])};
		std::sort(slice_first, slice_end);
		const auto slice_last{std::unique(slice_first, slice_end)};
		StatePost state_post{};
		for (auto run_first{slice_first}; run_first != slice_last;) {
			auto run_last{run_first};
			while (run_last != slice_last && run_last->symbol == run_first->symbol) { ++run_last; }
			SymbolPost symbol_post{run_first->symbol};
			symbol_post.targets.reserve(static_cast<size_t>(run_last - run_first));
			for (auto edge_it{run_first}; edge_it != run_last; ++edge_it) {
				symbol_post.targets.push_back(edge_it->source);
			}
			state_post.push_back(std::move(symbol_post));
			run_first = run_last;
		}
		result.emplace_back(std::move(state_post));
	}
	return result;
}
