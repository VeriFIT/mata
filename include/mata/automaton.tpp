/** @file
 * @brief Template member definitions for @c mata::Automaton.
 */

#ifndef MATA_AUTOMATON_TPP_
#define MATA_AUTOMATON_TPP_

#include <algorithm>
#include <list>
#include <map>
#include <tuple>
#include <unordered_set>

#include "mata/utils/assert.hh"
#include "mata/utils/ord-vector.hh"

namespace mata {

template <typename Self> bool Automaton::is_identical(this const Self& self, const Self& other) {
	if (utils::OrdVector<nfa::State>(self.initial) != utils::OrdVector<nfa::State>(other.initial)) { return false; }
	if (utils::OrdVector<nfa::State>(self.final) != utils::OrdVector<nfa::State>(other.final)) { return false; }
	return self.delta == other.delta;
}

template <typename Self> Self& Automaton::trim(this Self& self, nfa::StateRenaming* state_renaming) {
#ifdef _STATIC_STRUCTURES_
	BoolVector useful_states{self.get_useful_states()};
	useful_states.clear();
	useful_states = self.get_useful_states();
#else
	const BoolVector useful_states{self.get_useful_states()};
#endif
	return self.trim_impl(useful_states, state_renaming);
}

template <typename Self> bool Automaton::is_lang_empty(this const Self& self, nfa::Run* const cex) {
	// TODO: hot fix for performance reasons for TACAS.
	//  Perhaps make the get_useful_states return a witness on demand somehow.
	if (!cex) { return self.has_no_accepting_path(); }

	std::list<nfa::State> worklist(self.initial.begin(), self.initial.end());
	std::unordered_set<nfa::State> processed(self.initial.begin(), self.initial.end());

	// 'paths[s] == t' denotes that state 's' was accessed from state 't',
	// 'paths[s] == s' means that 's' is an initial state
	std::map<nfa::State, nfa::State> paths;
	// Initialize paths.
	for (const nfa::State s : worklist) { paths[s] = s; }

	while (!worklist.empty()) {
		nfa::State state{worklist.front()};
		worklist.pop_front();

		if (self.final[state]) {
			cex->path.clear();
			cex->path.push_back(state);
			while (paths[state] != state) {
				state = paths[state];
				cex->path.push_back(state);
			}
			std::ranges::reverse(cex->path);
			// The structural search above only finds the path; reading it as a word is leaf-specific (flat for an
			//  NFA, level-aware for an NFT). `self.get_word_for_path()` is not declared on `Automaton` at all --
			//  it resolves once `Self` is known, at instantiation, so this always calls the leaf's own version.
			cex->word = self.get_word_for_path(*cex).first.word;
			return false;
		}

		if (self.delta.empty()) { continue; }

		self.delta.for_each_successor(state, [&](const nfa::State target) {
			bool inserted;
			std::tie(std::ignore, inserted) = processed.insert(target);
			if (inserted) {
				worklist.push_back(target);
				// Also set that tgt_state was accessed from state.
				paths[target] = state;
			} else {
				MATA_ASSERT(utils::haskey(paths, target)); /* Invariant. */
			}
		});
	} // while (!worklist.empty()).
	return true;
} // is_lang_empty().

template <typename Self>
Self& Automaton::trim_impl(this Self& self, const BoolVector& useful_states, nfa::StateRenaming* state_renaming) {
	const size_t useful_states_size{useful_states.size()};
	std::vector<nfa::State> renaming(useful_states_size);
	for (nfa::State new_state{0}, orig_state{0}; orig_state < useful_states_size; ++orig_state) {
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
			nfa::State expected_new_state{0};
			for (nfa::State orig_state{0}; orig_state < useful_states_size; ++orig_state) {
				if (useful_states[orig_state]) {
					if (renaming[orig_state] != expected_new_state) { return false; }
					++expected_new_state;
				}
			}
			return true;
		}(),
		"Automaton::trim_impl: useful states must be renamed densely in ascending order."
	);

	auto is_state_useful = [&](const nfa::State q) { return q < useful_states_size && useful_states[q]; };
	self.initial.filter(is_state_useful);
	self.final.filter(is_state_useful);
	auto rename_state = [&](const nfa::State q) { return renaming[q]; };
	self.initial.rename(rename_state);
	self.final.rename(rename_state);
	self.initial.truncate();
	self.final.truncate();
	if (state_renaming != nullptr) {
		state_renaming->clear();
		state_renaming->reserve(useful_states_size);
		for (nfa::State q{0}; q < useful_states_size; ++q) {
			if (useful_states[q]) { (*state_renaming)[q] = renaming[q]; }
		}
	}
	return self;
}

} // namespace mata.

#endif // MATA_AUTOMATON_TPP_
