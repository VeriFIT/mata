/** @file
 * @brief Template member definitions for @c mata::Automaton.
 */

#ifndef MATA_CORE_AUTOMATON_TPP_
#define MATA_CORE_AUTOMATON_TPP_

#include <algorithm>
#include <list>
#include <map>
#include <tuple>
#include <unordered_set>

#include "mata/utils/assert.hh"
#include "mata/utils/ord-vector.hh"

namespace mata {

template <typename Self> bool Automaton::is_identical(this const Self& self, const Self& other) {
	if (utils::OrdVector<State>(self.initial) != utils::OrdVector<State>(other.initial)) { return false; }
	if (utils::OrdVector<State>(self.final) != utils::OrdVector<State>(other.final)) { return false; }
	return self.delta == other.delta;
}

template <typename Self> Self& Automaton::trim(this Self& self, StateRenaming* state_renaming) {
#ifdef _STATIC_STRUCTURES_
	BoolVector useful_states{self.get_useful_states()};
	useful_states.clear();
	useful_states = self.get_useful_states();
#else
	const BoolVector useful_states{self.get_useful_states()};
#endif
	return self.trim_impl(useful_states, state_renaming);
}


template <typename Self>
Self& Automaton::trim_impl(this Self& self, const BoolVector& useful_states, StateRenaming* state_renaming) {
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
		"Automaton::trim_impl: useful states must be renamed densely in ascending order."
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
