/** @file
 * @brief Implementation of the @c mata::nfa::Delta class and related functions.
 *
 * This file contains the implementation of the Delta class, which represents the transition relation of
 *  a non-deterministic finite automaton (NFA). It includes methods for adding, removing, and querying transitions, as
 *  well as iterating over transitions.
 */

#include "mata/nfa/delta.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nfa/types.hh"
#include "mata/utils/assert.hh"
#include "mata/utils/sparse-set.hh"

#include <algorithm>
#include <functional>
#include <iterator>
#include <queue>
#include <utility>
#include <stdexcept>

using namespace mata::utils;
using namespace mata::nfa;
using mata::Symbol;

using StateBoolArray = std::vector<bool>; ///< Bool array for states in the automaton.

SymbolPost& SymbolPost::operator=(SymbolPost&& rhs) noexcept {
	if (*this != rhs) {
		symbol = rhs.symbol;
		targets = std::move(rhs.targets);
	}
	return *this;
}

bool SymbolPost::insert(const State s) {
	if (targets.empty() || targets.back() < s) {
		targets.push_back(s);
		return true;
	}
	// Find the place where to put the element (if not present).
	// Insert to OrdVector without the searching of a proper position inside insert(const Key&x).
	if (const auto it = std::ranges::lower_bound(targets, s); it == targets.end() || *it != s) {
		targets.insert(it, s);
		return true;
	}
	return false;
}

size_t SymbolPost::insert(const StateSet& states) {
	if (states.empty()) { return 0; }
	const size_t old_size{targets.size()};
	if (targets.empty()) {
		targets = states;
		return states.size();
	}
	if (targets.back() < states.front()) { // The sets are disjoint and in order: append without any comparison.
		targets.reserve(targets.size() + states.size());
		for (const State s : states) { targets.push_back(s); }
		return states.size();
	}
	StateSet merged{};
	StateSet::set_union(targets, states, merged);
	targets = std::move(merged);
	return targets.size() - old_size;
}

StatePost::const_iterator Delta::epsilon_symbol_posts(const State state, const Symbol epsilon) const {
	return epsilon_symbol_posts(state_post(state), epsilon);
}

StatePost::const_iterator Delta::epsilon_symbol_posts(const StatePost& state_post, const Symbol epsilon) {
	if (!state_post.empty()) {
		if (epsilon == EPSILON) {
			if (const auto& back = state_post.back(); back.symbol == epsilon) { return std::prev(state_post.end()); }
		} else {
			return state_post.find(SymbolPost(epsilon));
		}
	}
	return state_post.end();
}

StateSet StatePost::get_successors() const {
	StateSet successors;
	for (const SymbolPost& symbol_post : *this) { successors.insert(symbol_post.targets); }
	return successors;
}

const StateSet& StatePost::get_successors(const Symbol symbol) const {
	const auto symbol_post_it = find(symbol);
	if (symbol_post_it == this->end()) {
		static StateSet empty_set{};
		return empty_set;
	}
	return symbol_post_it->targets;
}

StateSet Delta::get_successors(const State state) const { return state_post(state).get_successors(); }

const StateSet& Delta::get_successors(const State state, const Symbol symbol) const {
	return state_post(state).get_successors(symbol);
}

namespace {
/// Collects the epsilon closure of the states gathered in @p workspace into its own gathered buffer.
void close_gathered(const Delta& delta, const std::vector<Symbol>& epsilons, PostWorkspace& workspace) {
	std::vector<State>& gathered{workspace.gathered()};
	std::vector<State>& queue{workspace.queue()};
	queue.assign(gathered.begin(), gathered.end());
	while (!queue.empty()) {
		const State state{queue.back()};
		queue.pop_back();
		const StatePost& state_post{delta[state]};
		for (const Symbol epsilon : epsilons) {
			const auto symbol_post_it{state_post.find(epsilon)};
			if (symbol_post_it == state_post.end()) { continue; }
			for (const State target : symbol_post_it->targets) {
				if (workspace.mark(target)) {
					gathered.push_back(target);
					queue.push_back(target);
				}
			}
		}
	}
}
} // namespace

StateSet Delta::epsilon_closure(
	const StateSet& states, const std::vector<Symbol>& epsilons, PostWorkspace& workspace
) const {
	workspace.start(num_of_states());
	for (const State state : states) {
		if (workspace.mark(state)) { workspace.gathered().push_back(state); }
	}
	close_gathered(*this, epsilons, workspace);
	return StateSet{workspace.gathered()};
}

StateSet Delta::post(
	const StateSet& states,
	const Symbol symbol,
	const EpsilonClosureOpt epsilon_closure_opt,
	const std::vector<Symbol>& epsilons,
	PostWorkspace& workspace
) const {
	// Stepping over epsilon with a closure requested means the source states are reachable as well.
	const bool stays_via_epsilon{symbol == EPSILON && epsilon_closure_opt != EpsilonClosureOpt::None};

	workspace.start(num_of_states());
	std::vector<State>& gathered{workspace.gathered()};
	for (const State state : states) {
		if (workspace.mark(state)) { gathered.push_back(state); }
	}
	if (closes_before(epsilon_closure_opt)) { close_gathered(*this, epsilons, workspace); }

	// The sources are read out before the buffer is reused for the targets.
	const std::vector<State> sources{gathered};
	workspace.start(num_of_states());
	if (stays_via_epsilon) {
		for (const State state : states) {
			if (workspace.mark(state)) { gathered.push_back(state); }
		}
	}
	for (const State state : sources) {
		const StatePost& state_post{(*this)[state]};
		const auto symbol_post_it{state_post.find(symbol)};
		if (symbol_post_it == state_post.end()) { continue; }
		for (const State target : symbol_post_it->targets) {
			if (workspace.mark(target)) { gathered.push_back(target); }
		}
	}
	if (closes_after(epsilon_closure_opt)) { close_gathered(*this, epsilons, workspace); }
	return StateSet{gathered};
}

StateSet
	Delta::get_successors(const State state, const Symbol symbol, const EpsilonClosureOpt epsilon_closure_opt) const {
	PostWorkspace workspace{};
	return post(StateSet{state}, symbol, epsilon_closure_opt, {EPSILON}, workspace);
}

std::vector<Transition> Delta::get_transitions_to(const State state_to) const {
	std::vector<Transition> transitions_to_state{};
	const size_t num_of_states{this->num_of_states()};
	for (State state_from{0}; state_from < num_of_states; ++state_from) {
		for (const SymbolPost& state_from_move : state_post(state_from)) {
			if (const auto target_state{state_from_move.targets.find(state_to)};
				target_state != state_from_move.targets.end()) {
				transitions_to_state.emplace_back(state_from, state_from_move.symbol, state_to);
			}
		}
	}
	return transitions_to_state;
}

std::vector<Transition> Delta::get_transitions_between(const State state_from, const State state_to) const {
	std::vector<Transition> transitions_between{};
	for (const SymbolPost& symbol_post : state_post(state_from)) {
		if (const auto state_to_find_it = symbol_post.targets.find(state_to);
			state_to_find_it != symbol_post.targets.end()) {
			transitions_between.emplace_back(state_from, symbol_post.symbol, state_to);
		}
	}
	return transitions_between;
}

void Delta::add(const State source, Symbol symbol, const State target) {
	++mutation_epoch_;
	resize_for_states(source, target);

	StatePost& state_transitions{state_posts_[source]};
	if (state_transitions.empty() || state_transitions.back().symbol < symbol) {
		state_transitions.insert({symbol, target});
		note_added(1);
		return;
	}
	if (const auto symbol_transitions{state_transitions.find(SymbolPost{symbol})};
		symbol_transitions != state_transitions.end()) {
		// Add transition with symbol already used on transitions from state_from.
		if (symbol_transitions->insert(target)) { note_added(1); }
	} else {
		// Add transition to a new Move struct with symbol yet unused on transitions from state_from.
		state_transitions.insert(SymbolPost{symbol, target});
		note_added(1);
	}
}

void Delta::add(const State source, const Symbol symbol, const StateSet& targets) {
	++mutation_epoch_;
	if (targets.empty()) { return; }
	resize_for_states(source, targets.back());

	StatePost& state_transitions{state_posts_[source]};
	if (state_transitions.empty() || state_transitions.back().symbol < symbol) {
		state_transitions.insert({symbol, targets});
		note_added(targets.size());
		return;
	}
	if (const auto symbol_transitions{state_transitions.find(symbol)}; symbol_transitions != state_transitions.end()) {
		// Add transition with symbolOnTransition already used on transitions from state_from.
		note_added(symbol_transitions->insert(targets));
	} else {
		// Add transition to a new Move struct with symbol yet unused on transitions from state_from.
		state_transitions.insert(SymbolPost{symbol, targets});
		note_added(targets.size());
	}
}

void Delta::add(const State source, SymbolPost&& symbol_post) {
	++mutation_epoch_;
	if (symbol_post.targets.empty()) { return; }
	resize_for_states(source, symbol_post.targets.back());

	StatePost& state_post{state_posts_[source]};
	const size_t added{symbol_post.num_of_targets()};
	if (state_post.empty() || state_post.back().symbol < symbol_post.symbol) {
		state_post.insert(std::move(symbol_post));
		note_added(added);
		return;
	}
	if (const auto existing{state_post.find(symbol_post)}; existing != state_post.end()) {
		// One linear merge of two sorted target sets, instead of a search and a shift per target.
		note_added(existing->targets.insert(symbol_post.targets));
		return;
	}
	state_post.insert(std::move(symbol_post));
	note_added(added);
}

void Delta::add(std::vector<Transition>&& transitions) {
	// The per-symbol-post adds below keep the counter exact; the bulk path installs it explicitly.
	if (transitions.empty()) { return; }
	std::ranges::sort(transitions, [](const Transition& lhs, const Transition& rhs) {
		return std::tie(lhs.source, lhs.symbol, lhs.target) < std::tie(rhs.source, rhs.symbol, rhs.target);
	});
	const auto duplicates{std::ranges::unique(transitions)};
	transitions.erase(duplicates.begin(), duplicates.end());

	const bool build_from_scratch{empty() && num_of_states() == 0};
	DeltaBuilder builder{build_from_scratch ? transitions.back().source + 1 : 0};
	size_t index{0};
	const size_t size{transitions.size()};
	while (index < size) {
		const State source{transitions[index].source};
		if (build_from_scratch) { builder.begin_state(source); }
		while (index < size && transitions[index].source == source) {
			const Symbol symbol{transitions[index].symbol};
			if (build_from_scratch) {
				builder.begin_symbol(symbol);
				while (index < size && transitions[index].source == source && transitions[index].symbol == symbol) {
					builder.push_sorted_target(transitions[index].target);
					++index;
				}
				builder.finish_symbol();
			} else {
				SymbolPost symbol_post{symbol};
				while (index < size && transitions[index].source == source && transitions[index].symbol == symbol) {
					symbol_post.targets.push_back(transitions[index].target);
					++index;
				}
				add(source, std::move(symbol_post));
			}
		}
		if (build_from_scratch) { builder.finish_state(); }
	}
	if (build_from_scratch) {
		*this = builder.finish();
		// Every record is unique after the sort and the deduplication above, so the count is exact.
		cached_transition_count_ = size;
		count_dirty_ = false;
	}
}

void Delta::remove(const State source, const Symbol symbol, const State target) {
	++mutation_epoch_;
	if (source >= state_posts_.size()) { return; }

	StatePost& state_transitions{state_posts_[source]};
	if (state_transitions.empty()) {
		throw std::invalid_argument(
			"Transition [" + std::to_string(source) + ", " + std::to_string(symbol) + ", " + std::to_string(target) +
			"] does not exist."
		);
	}
	if (state_transitions.back().symbol < symbol) {
		throw std::invalid_argument(
			"Transition [" + std::to_string(source) + ", " + std::to_string(symbol) + ", " + std::to_string(target) +
			"] does not exist."
		);
	}
	const auto symbol_transitions{state_transitions.find(symbol)};
	if (symbol_transitions == state_transitions.end()) {
		throw std::invalid_argument(
			"Transition [" + std::to_string(source) + ", " + std::to_string(symbol) + ", " + std::to_string(target) +
			"] does not exist."
		);
	}
	if (symbol_transitions->erase(target)) { note_removed(1); }
	if (symbol_transitions->empty()) { state_posts_[source].erase(*symbol_transitions); }
}

bool Delta::contains(const State source, const Symbol symbol, const State target) const { // {{{
	if (state_posts_.empty()) { return false; }
	if (state_posts_.size() <= source) { return false; }

	const StatePost& tl = state_posts_[source];
	if (tl.empty()) { return false; }
	const auto symbol_transitions{tl.find(SymbolPost{symbol})};
	if (symbol_transitions == tl.cend()) { return false; }

	return symbol_transitions->targets.contains(target);
}

bool Delta::contains(const Transition& transition) const {
	return contains(transition.source, transition.symbol, transition.target);
}

size_t Delta::num_of_transitions() const {
	if (count_dirty_) {
		// Recompute from scratch
		cached_transition_count_ = 0;
		for (const StatePost& state_post : state_posts_) {
			for (const SymbolPost& symbol_post : state_post) {
				cached_transition_count_ += symbol_post.num_of_targets();
			}
		}
		count_dirty_ = false;
	}
	return cached_transition_count_;
}

bool Delta::empty() const { return num_of_transitions() == 0; }

Delta::Transitions::const_iterator::const_iterator(const Delta& delta) : delta_{&delta} {
	const size_t post_size = delta_->num_of_states();
	for (size_t i = 0; i < post_size; ++i) {
		if (!(*delta_)[i].empty()) {
			current_state_ = i;
			state_post_it_ = (*delta_)[i].begin();
			symbol_post_it_ = state_post_it_->targets.begin();
			transition_.source = current_state_;
			transition_.symbol = state_post_it_->symbol;
			transition_.target = *symbol_post_it_;
			return;
		}
	}

	// No transition found, delta contains only empty state posts.
	is_end_ = true;
}

Delta::Transitions::const_iterator::const_iterator(const Delta& delta, const State current_state)
	: delta_{&delta},
	  current_state_{current_state} {
	const size_t post_size = delta_->num_of_states();
	for (State source{current_state_}; source < post_size; ++source) {
		if (const StatePost& state_post{delta_->state_post(source)}; !state_post.empty()) {
			current_state_ = source;
			state_post_it_ = state_post.begin();
			symbol_post_it_ = state_post_it_->targets.begin();
			transition_.source = current_state_;
			transition_.symbol = state_post_it_->symbol;
			transition_.target = *symbol_post_it_;
			return;
		}
	}

	// No transition found, delta from the current state contains only empty state posts.
	is_end_ = true;
}

Delta::Transitions::const_iterator& Delta::Transitions::const_iterator::operator++() {
	MATA_ASSERT(delta_->begin() != delta_->end());

	++symbol_post_it_;
	if (symbol_post_it_ != state_post_it_->targets.end()) {
		transition_.target = *symbol_post_it_;
		return *this;
	}

	++state_post_it_;
	if (state_post_it_ != (*delta_)[current_state_].cend()) {
		symbol_post_it_ = state_post_it_->targets.begin();
		transition_.symbol = state_post_it_->symbol;
		transition_.target = *symbol_post_it_;
		return *this;
	}

	const size_t state_posts_size{delta_->num_of_states()};
	do { // Skip empty posts.
		++current_state_;
	} while (current_state_ < state_posts_size && (*delta_)[current_state_].empty());
	if (current_state_ >= state_posts_size) {
		is_end_ = true;
		return *this;
	}

	const StatePost& state_post{(*delta_)[current_state_]};
	state_post_it_ = state_post.begin();
	symbol_post_it_ = state_post_it_->targets.begin();

	transition_.source = current_state_;
	transition_.symbol = state_post_it_->symbol;
	transition_.target = *symbol_post_it_;

	return *this;
}

Delta::Transitions::const_iterator Delta::Transitions::const_iterator::operator++(int) {
	const Delta::Transitions::const_iterator tmp{*this};
	++(*this);
	return tmp;
}

bool Delta::Transitions::const_iterator::operator==(const Delta::Transitions::const_iterator& other) const {
	if (is_end_ && other.is_end_) { return true; }
	if ((is_end_ && !other.is_end_) || (!is_end_ && other.is_end_)) { return false; }
	return current_state_ == other.current_state_ && state_post_it_ == other.state_post_it_ &&
		   symbol_post_it_ == other.symbol_post_it_;
}

std::vector<StatePost> Delta::renumber_targets(const std::function<State(State)>& target_renumberer) const {
	std::vector<StatePost> copied_state_posts;
	copied_state_posts.reserve(num_of_states());
	for (const StatePost& state_post : state_posts_) {
		StatePost copied_state_post;
		copied_state_post.reserve(state_post.size());
		for (const SymbolPost& symbol_post : state_post) {
			StateSet copied_targets;
			copied_targets.reserve(symbol_post.num_of_targets());
			for (const State& state : symbol_post.targets) { copied_targets.push_back(target_renumberer(state)); }
			copied_state_post.push_back(SymbolPost(symbol_post.symbol, copied_targets));
		}
		copied_state_posts.emplace_back(copied_state_post);
	}
	return copied_state_posts;
}


Delta& Delta::append_shifted(const Delta& other, State offset) {
	if (this == &other) {
		// Self-append: take a snapshot to avoid reading moved-from state.
		return append_shifted(Delta{other}, offset);
	}
	
	// Check for overflow: offset + other.num_of_states() must fit in State.
	if (offset > Limits::max_state - other.num_of_states()) {
		throw std::overflow_error(
			"Delta::append_shifted: offset (" + std::to_string(offset) + ") + other.num_of_states() (" +
			std::to_string(other.num_of_states()) + ") exceeds max_state"
		);
	}
	
	// Allocate empty posts up to offset if needed.
	if (num_of_states() < offset) {
		allocate(offset);
	}
	
	// For each state post in other, shift targets by offset and append.
	for (const StatePost& other_post : other.state_posts_) {
		StatePost shifted_post;
		shifted_post.reserve(other_post.size());
		for (const SymbolPost& other_symbol_post : other_post) {
			StateSet shifted_targets;
			shifted_targets.reserve(other_symbol_post.num_of_targets());
			for (const State& target : other_symbol_post.targets) {
				shifted_targets.push_back(target + offset);
			}
			shifted_post.push_back(SymbolPost(other_symbol_post.symbol, shifted_targets));
		}
		state_posts_.push_back(shifted_post);
	}
	
	return *this;
}

Delta& Delta::append_shifted(Delta&& other, State offset) {
	if (this == &other) {
		// Self-append: take a snapshot of other before consuming.
		Delta snapshot{other};
		other.clear();
		return append_shifted(snapshot, offset);
	}
	
	// Check for overflow: offset + other.num_of_states() must fit in State.
	if (offset > Limits::max_state - other.num_of_states()) {
		throw std::overflow_error(
			"Delta::append_shifted: offset (" + std::to_string(offset) + ") + other.num_of_states() (" +
			std::to_string(other.num_of_states()) + ") exceeds max_state"
		);
	}
	
	// Allocate empty posts up to offset if needed.
	if (num_of_states() < offset) {
		allocate(offset);
	}
	
	// For each state post in other, shift targets by offset and move.
	for (StatePost& other_post : other.state_posts_) {
		StatePost shifted_post;
		shifted_post.reserve(other_post.size());
		for (SymbolPost& other_symbol_post : other_post) {
			// Create shifted targets by transforming the original targets.
			// We cannot mutate the StateSet directly, so we build a new one.
			StateSet shifted_targets;
			shifted_targets.reserve(other_symbol_post.targets.size());
			for (const State& target : other_symbol_post.targets) {
				shifted_targets.push_back(target + offset);
			}
			shifted_post.push_back(SymbolPost(other_symbol_post.symbol, std::move(shifted_targets)));
		}
		state_posts_.push_back(std::move(shifted_post));
	}
	
	// Clear other to leave it valid but empty.
	other.state_posts_.clear();
	
	return *this;
}





StatePost& Delta::mutable_state_post(const State source) {
	if (source >= state_posts_.size()) {
		utils::reserve_on_insert(state_posts_, source);
		const size_t new_size{source + 1};
		state_posts_.resize(new_size);
	}

	++mutation_epoch_;
	count_dirty_ = true; // Raw edits invalidate the count
	return state_posts_[source];
}

Delta mata::nfa::defragment(const Delta& delta, const BoolVector& is_staying, const std::vector<State>& renaming) {
	auto filter_rename_symbol_post = [&](const SymbolPost& symbol_post) {
		SymbolPost new_symbol_post{symbol_post.symbol};
		for (const State& target : symbol_post.targets) {
			if (!is_staying[target]) { continue; }
			new_symbol_post.push_back(renaming[target]);
		}
		return new_symbol_post;
	};
	auto filter_rename_state_post = [&](const StatePost& state_post,
										const std::function<SymbolPost(const SymbolPost&)>& transform_symbol_post) {
		StatePost result{};
		for (const SymbolPost& symbol_post : state_post) {
			SymbolPost new_symbol_post = transform_symbol_post(symbol_post);
			if (new_symbol_post.empty()) { continue; }
			result.push_back(std::move(new_symbol_post));
		}
		return result;
	};

	Delta delta_defragmented{};
	for (State source{0}; source < delta.num_of_states(); ++source) {
		if (!is_staying[source]) { continue; }
		delta_defragmented.emplace_back(filter_rename_state_post(delta[source], filter_rename_symbol_post));
	}
	return delta_defragmented;
}

Delta& Delta::defragment(const BoolVector& is_staying, const std::vector<State>& renaming) {
	++mutation_epoch_;
	count_dirty_ = true;
	size_t source_new{0};
	for (size_t source_orig{0}, num_of_states{this->num_of_states()}; source_orig < num_of_states; ++source_orig) {
		if (!is_staying[source_orig]) { continue; } // Skip source states not staying.
		StatePost& state_post = state_posts_[source_orig];
		for (auto state_post_it{state_post.begin()}; state_post_it != state_post.end();) {
			StateSet& targets{state_post_it->targets};
			targets.erase_if([&is_staying](const State& target) { return !is_staying[target]; });
			targets.rename(renaming);
			if (targets.empty()) {
				state_post_it = state_post.erase(state_post_it);
			} else {
				++state_post_it;
			}
		}
		// Move the filtered state post to the new position, if needed.
		if (source_new != source_orig) { state_posts_[source_new] = std::move(state_post); }
		++source_new;
	}
	// Resize to remove filtered-out state posts.
	state_posts_.resize(source_new);
	return *this;
}

bool Delta::operator==(const Delta& other) const {
	const Delta::Transitions this_transitions{transitions()};
	Delta::Transitions::const_iterator this_transitions_it{this_transitions.begin()};
	const Delta::Transitions::const_iterator this_transitions_end{mata::nfa::Delta::Transitions::end()};
	const Delta::Transitions other_transitions{other.transitions()};
	Delta::Transitions::const_iterator other_transitions_it{other_transitions.begin()};
	const Delta::Transitions::const_iterator other_transitions_end{mata::nfa::Delta::Transitions::end()};
	while (this_transitions_it != this_transitions_end) {
		if (other_transitions_it == other_transitions_end || *this_transitions_it != *other_transitions_it) {
			return false;
		}
		++this_transitions_it;
		++other_transitions_it;
	}
	return other_transitions_it == other_transitions_end;
}

/// Returns an iterator to the smallest epsilon, or end() if there is no epsilon
/// Searches from the end of the vector of SymbolPosts, since epsilons are at the end and they are typically few,
/// mostly 1.
StatePost::const_iterator StatePost::first_epsilon_it(const Symbol first_epsilon) const {
	const auto end_it = cend();
	auto it = end_it;
	while (it != begin()) {
		--it;
		if (it->symbol < first_epsilon) { // is it a normal symbol already?
			return it + 1; // Return the previous position, the smallest epsilon or end().
		}
	}

	if (it != end_it && it->symbol >= first_epsilon) {
		// The special case when begin is the smallest epsilon (since the while loop ended before the step back)
		return it;
	}
	return end_it;
}

StatePost::Moves::const_iterator::const_iterator(
	const StatePost::const_iterator symbol_post_it, const StatePost::const_iterator symbol_post_end
)
	: symbol_post_it_{symbol_post_it},
	  symbol_post_end_{symbol_post_end} {
	if (symbol_post_it_ == symbol_post_end_) {
		is_end_ = true;
		return;
	}

	move_.symbol = symbol_post_it_->symbol;
	target_it_ = symbol_post_it_->targets.cbegin();
	move_.target = *target_it_;
}

StatePost::Moves::const_iterator::const_iterator(const StatePost& state_post)
	: symbol_post_it_{state_post.begin()},
	  symbol_post_end_{state_post.end()} {
	if (symbol_post_it_ == symbol_post_end_) {
		is_end_ = true;
		return;
	}

	move_.symbol = symbol_post_it_->symbol;
	target_it_ = symbol_post_it_->targets.cbegin();
	move_.target = *target_it_;
}

StatePost::Moves::const_iterator& StatePost::Moves::const_iterator::operator++() {
	++target_it_;
	if (target_it_ != symbol_post_it_->targets.end()) {
		move_.target = *target_it_;
		return *this;
	}

	// Iterate over to the next symbol post, which can be either an end iterator, or symbol post whose
	//  symbol <= symbol_post_end_.
	++symbol_post_it_;
	if (symbol_post_it_ == symbol_post_end_) {
		is_end_ = true;
		return *this;
	}
	// The current symbol post is valid (not equal symbol_post_end_).
	move_.symbol = symbol_post_it_->symbol;
	target_it_ = symbol_post_it_->targets.begin();
	move_.target = *target_it_;
	return *this;
}

StatePost::Moves::const_iterator StatePost::Moves::const_iterator::operator++(int) {
	const StatePost::Moves::const_iterator tmp{*this};
	++(*this);
	return tmp;
}

bool StatePost::Moves::const_iterator::operator==(const StatePost::Moves::const_iterator& other) const {
	if (is_end_ && other.is_end_) { return true; }
	if ((is_end_ && !other.is_end_) || (!is_end_ && other.is_end_)) { return false; }
	return symbol_post_it_ == other.symbol_post_it_ && target_it_ == other.target_it_ &&
		   symbol_post_end_ == other.symbol_post_end_;
}

size_t StatePost::num_of_moves() const {
	size_t counter{0};
	for (const SymbolPost& symbol_post : *this) { counter += symbol_post.num_of_targets(); }
	return counter;
}

StatePost::Moves& StatePost::Moves::operator=(StatePost::Moves&& other) noexcept {
	if (&other != this) {
		symbol_post_it_ = other.symbol_post_it_;
		symbol_post_end_ = other.symbol_post_end_;
	}
	return *this;
}

StatePost::Moves& StatePost::Moves::operator=(const Moves& other) noexcept {
	if (&other != this) {
		symbol_post_it_ = other.symbol_post_it_;
		symbol_post_end_ = other.symbol_post_end_;
	}
	return *this;
}

StatePost::Moves StatePost::moves(
	const StatePost::const_iterator symbol_post_it, const StatePost::const_iterator symbol_post_end
) const {
	return {symbol_post_it, symbol_post_end};
}

StatePost::Moves StatePost::moves_epsilons(const Symbol first_epsilon) const {
	return {first_epsilon_it(first_epsilon), cend()};
}

StatePost::Moves StatePost::moves_symbols(const Symbol last_symbol) const {
	if (last_symbol == EPSILON) { throw std::runtime_error("Using default epsilon as a last symbol to iterate over."); }
	return {cbegin(), first_epsilon_it(last_symbol + 1)};
}

StatePost::Moves::const_iterator StatePost::Moves::begin() const { return {symbol_post_it_, symbol_post_end_}; }

StatePost::Moves::const_iterator StatePost::Moves::end() { return const_iterator{}; }

Delta::Transitions Delta::transitions() const { return Transitions{this}; }

Delta::Transitions::const_iterator Delta::Transitions::begin() const { return const_iterator{*delta_}; }
Delta::Transitions::const_iterator Delta::Transitions::end() { return const_iterator{}; }

StatePost::Moves::Moves(const StatePost::const_iterator symbol_post_it, const StatePost::const_iterator symbol_post_end)
	: symbol_post_it_{symbol_post_it},
	  symbol_post_end_{symbol_post_end} {}

void Delta::add_symbols_to(OnTheFlyAlphabet& target_alphabet) const {
	const size_t aut_num_of_states{num_of_states()};
	for (mata::nfa::State state{0}; state < aut_num_of_states; ++state) {
		for (const SymbolPost& move : state_post(state)) {
			target_alphabet.update_next_symbol_value(move.symbol);
			target_alphabet.try_add_new_symbol(std::to_string(move.symbol), move.symbol);
		}
	}
}

OrdVector<Symbol> Delta::get_used_symbols() const {
	// TODO: look at the variants in profiling (there are tests in tests-nfa-profiling.cc),
	//  for instance figure out why NumberPredicate and OrdVector are slow,
	//  try also with _STATIC_DATA_STRUCTURES_, it changes things.

	// below are different variant, with different data structures for accumulating symbols,
	// that then must be converted to an OrdVector
	// measured are times with "mata::nfa::get_used_symbols speed, harder", "[.profiling]" now on line 104 of
	// nfa-profiling.cc

	// WITH VECTOR (4.434 s)
	return get_used_symbols_vec();

	// WITH SET (26.5 s)
	// auto from_set = get_used_symbols_set();
	// return utils::OrdVector<Symbol> (from_set .begin(),from_set.end());

	// WITH NUMBER PREDICATE (4.857s) (NP removed)
	// return utils::OrdVector(get_used_symbols_np().get_elements());

	// WITH SPARSE SET (haven't tried)
	// return utils::OrdVector<State>(get_used_symbols_sps());

	// WITH BOOL VECTOR (error !!!!!!!):
	// return utils::OrdVector<Symbol>(utils::NumberPredicate<Symbol>(get_used_symbols_bv()));

	// WITH BOOL VECTOR (1.9s): (The fastest, it seems.)
	//  However, it will try to allocate a vector indexed by the symbols. If there are epsilons in the automaton,
	//   for example, the bool vector implementation will implode.
	//  std::vector<bool> bv{ get_used_symbols_bv() };
	//  utils::OrdVector<Symbol> ov{};
	//  const size_t bv_size{ bv.size() };
	//  for (Symbol i{ 0 }; i < bv_size; ++i) { if (bv[i]) { ov.push_back(i); } }
	//  return ov;

	/// WITH BOOL VECTOR, DIFFERENT VARIANT? (1.9s):
	// std::vector<bool> bv = get_used_symbols_bv();
	// utils::OrdVector<Symbol> ov{};
	// ov.reserve(static_cast<size_t>(std::count(bv.begin(), bv.end(), true)));
	// const size_t bv_size{ bv.size() };
	// for (Symbol i = 0; i < bv_size; i++) {
	//     if (bv[i]) {
	//         ov.push_back(i);
	//     }
	// }
	// return ov;

	// WITH CHAR VECTOR (should be the fastest, haven't tried in this branch):
	// BEWARE: failing in one noodlificatoin test ("Simple automata -- epsilon result") ... strange
	//  BoolVector chv = get_used_symbols_chv();
	//  utils::OrdVector<Symbol> ov;
	//  for(Symbol i = 0;i<chv.size();i++)
	//     if (chv[i]) {
	//         ov.push_back(i);
	//     }
	//  return ov;
}

// Other versions, maybe an interesting experiment with speed of data structures.
// Returns symbols appearing in Delta, pushes back to vector and then sorts
mata::utils::OrdVector<Symbol> Delta::get_used_symbols_vec() const {
#ifdef _STATIC_STRUCTURES_
	static std::vector<Symbol> symbols{};
	symbols.clear();
#else
	std::vector<Symbol> symbols{};
#endif
	for (const StatePost& state_post : state_posts_) {
		for (const SymbolPost& symbol_post : state_post) {
			utils::reserve_on_insert(symbols);
			symbols.push_back(symbol_post.symbol);
		}
	}
	utils::OrdVector<Symbol> sorted_symbols(symbols);
	return sorted_symbols;
}

// returns symbols appearing in Delta, inserts to a std::set
std::set<Symbol> Delta::get_used_symbols_set() const {
	// static should prevent reallocation, seems to speed things up a little
#ifdef _STATIC_STRUCTURES_
	static std::set<Symbol> symbols;
	symbols.clear();
#else
	static std::set<Symbol> symbols{};
#endif
	for (const StatePost& state_post : state_posts_) {
		for (const SymbolPost& symbol_post : state_post) { symbols.insert(symbol_post.symbol); }
	}
	return symbols;
	// utils::OrdVector<Symbol>  sorted_symbols(symbols.begin(),symbols.end());
	// return sorted_symbols;
}

// returns symbols appearing in Delta, adds to NumberPredicate,
// Seems to be the fastest option, but could have problems with large maximum symbols
mata::utils::SparseSet<Symbol> Delta::get_used_symbols_sps() const {
#ifdef _STATIC_STRUCTURES_
	// static seems to speed things up a little
	static utils::SparseSet<Symbol> symbols(64);
	symbols.clear();
#else
	utils::SparseSet<Symbol> symbols(64);
#endif
	// symbols.dont_track_elements();
	for (const StatePost& state_post : state_posts_) {
		for (const SymbolPost& symbol_post : state_post) { symbols.insert(symbol_post.symbol); }
	}
	// TODO: is it necessary to return ordered vector? Would the number predicate suffice?
	return symbols;
}

// returns symbols appearing in Delta, adds to NumberPredicate,
// Seems to be the fastest option, but could have problems with large maximum symbols
std::vector<bool> Delta::get_used_symbols_bv() const {
#ifdef _STATIC_STRUCTURES_
	// static seems to speed things up a little
	static std::vector<bool> symbols(64, false);
	symbols.clear();
#else
	std::vector<bool> symbols(64, false);
#endif
	// symbols.dont_track_elements();
	for (const StatePost& state_post : state_posts_) {
		for (const SymbolPost& symbol_post : state_post) {
			if (const size_t capacity{symbol_post.symbol + 1}; symbols.size() < capacity) { symbols.resize(capacity); }
			symbols[symbol_post.symbol] = true;
		}
	}
	return symbols;
}

mata::BoolVector Delta::get_used_symbols_chv() const {
#ifdef _STATIC_STRUCTURES_
	// static seems to speed things up a little
	static BoolVector symbols(64, false);
	symbols.clear();
#else
	BoolVector symbols(64, false);
#endif
	// symbols.dont_track_elements();
	for (const StatePost& state_post : state_posts_) {
		for (const SymbolPost& symbol_post : state_post) {
			if (const size_t capacity{symbol_post.symbol + 1}; symbols.size() < capacity) {
				symbols.resize(capacity * 2);
			}
			symbols[symbol_post.symbol] = true;
		}
	}
	// TODO: is it necessary to return ordered vector? Would the number predicate suffice?
	return symbols;
}

Symbol Delta::get_max_symbol() const {
	Symbol max{0};
	for (const StatePost& state_post : state_posts_) {
		for (const SymbolPost& symbol_post : state_post) { max = std::max(symbol_post.symbol, max); }
	}
	return max;
}

StateSet SynchronizedExistentialSymbolPostIterator::unify_targets() const {
	// TODO: decide which version performs the best.

	if (!is_synchronized()) { return {}; }

	StateSet unified_targets{};

	// Version with synchronized iterator.
	// static utils::SynchronizedExistentialIterator<StateSet::const_iterator> sync_iterator;
	// sync_iterator.reset();
	// size_t all_targets_size{ 0 };
	// const std::vector<StatePost::const_iterator>& current_symbol_post_its{ this->get_current() };
	// sync_iterator.reserve(current_symbol_post_its.size());
	// for (const auto symbol_post_it: current_symbol_post_its) {
	//     sync_iterator.push_back(symbol_post_it->cbegin(), symbol_post_it->cend());
	//     all_targets_size += symbol_post_it->num_of_targets();
	// }
	// unified_targets.reserve(all_targets_size);
	// while (sync_iterator.advance()) { unified_targets.push_back(*sync_iterator.get_current_minimum()); }

	// Version with set union.
	// for (const auto& symbol_post_it: get_current()) {
	//     unified_targets.insert(symbol_post_it->targets);
	// }

	// Version with priority queue.
	using TargetSetBeginEndPair = std::pair<StateSet::const_iterator, StateSet::const_iterator>;
	auto compare = [](const auto& a, const auto& b) { return *(a.first) > *(b.first); };
	std::priority_queue<TargetSetBeginEndPair, std::vector<TargetSetBeginEndPair>, decltype(compare)> queue(compare);
	for (const StatePost::const_iterator& symbol_post_it : get_current()) {
		queue.emplace(symbol_post_it->cbegin(), symbol_post_it->cend());
	}
	unified_targets.reserve(32);
	while (!queue.empty()) {
		auto item = queue.top();
		queue.pop();
		if (unified_targets.empty() || unified_targets.back() != *(item.first)) {
			unified_targets.push_back(*(item.first));
		}
		if (++item.first != item.second) { queue.emplace(item); }
	}

	return unified_targets;
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
