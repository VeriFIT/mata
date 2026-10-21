/** @file
 * @brief Operations on NFAs.
 */

#include <algorithm>
#include <deque>
#include <limits>
#include <list>
#include <optional>
#include <queue>
#include <unordered_set>

// MATA headers
#include <ranges>
#include <span>

#include "mata/nfa/algorithms.hh"
#include "mata/nfa/delta.hh"
#include "mata/nfa/nfa.hh"
#include "mata/utils/sparse-set.hh"
#include <mata/simlib/explicit_lts.hh>
#include <mata/utils/assert.hh>

using std::tie;

using namespace mata::utils;
using namespace mata::nfa;
using mata::Symbol;

using StateBoolArray = std::vector<bool>; ///< Bool array for states in the automaton.

namespace {
/**
 * @brief Forward direct simulation: @c get(p, q) is true iff @c q simulates @c p.
 *
 * A final state may only be simulated by a final state. That used to be encoded by a self-loop over a
 *  symbol the automaton does not use, which needed a search for such a symbol and threw when the whole
 *  @c Symbol range was taken. The same constraint is the initial partition `{non-final, final}` with the
 *  block relation `final(B) -> final(B')`, which the engine accepts directly and does not have to refine
 *  the single block `{Q}` down to.
 */
Simlib::Util::BinaryRelation compute_fw_direct_simulation(const Nfa& aut) {
	const size_t state_num{aut.num_of_states()};
	Simlib::ExplicitLTS lts_for_simulation(state_num);

	for (const Transition& transition : aut.delta.transitions()) {
		lts_for_simulation.add_transition(transition.source, transition.symbol, transition.target);
	}
	lts_for_simulation.init();

	std::vector<size_t> non_final_states{};
	std::vector<size_t> final_states{};
	for (State state{0}; state < state_num; ++state) {
		(aut.final[state] ? final_states : non_final_states).push_back(state);
	}

	// The engine rejects empty blocks, so a block only enters the partition when it has a state.
	std::vector<std::vector<size_t>> partition{};
	size_t non_final_block{0};
	size_t final_block{0};
	if (!non_final_states.empty()) {
		non_final_block = partition.size();
		partition.push_back(std::move(non_final_states));
	}
	if (!final_states.empty()) {
		final_block = partition.size();
		partition.push_back(std::move(final_states));
	}

	Simlib::Util::BinaryRelation relation{partition.size(), false};
	for (size_t block{0}; block < partition.size(); ++block) { relation.set(block, block, true); }
	if (partition.size() == 2) { relation.set(non_final_block, final_block, true); }

	return lts_for_simulation.compute_simulation(partition, relation, state_num);
}

/// Incoming transitions `(source, symbol)` of each state of the result of residual reduction "with", recorded when
///  a transition is added. A removed transition leaves its entry behind, so users check the delta.
class Predecessors {
  public:
	void add(Nfa& nfa, const State source, const Symbol symbol, const State target) {
		nfa.delta.add(source, symbol, target);
		record(source, symbol, target);
	}

	void record(const State source, const Symbol symbol, const State target) {
		if (target >= predecessors_.size()) { predecessors_.resize(target + 1); }
		predecessors_[target].emplace_back(source, symbol);
	}

	/// The transitions to @p target present in @p nfa, each once; forgets the recorded entries.
	std::vector<std::pair<State, Symbol>> take_transitions_to(const Nfa& nfa, const State target) {
		if (target >= predecessors_.size()) { return {}; }
		std::vector<std::pair<State, Symbol>> transitions{std::move(predecessors_[target])};
		predecessors_[target].clear();
		std::ranges::sort(transitions);
		const auto [first, last]{std::ranges::unique(transitions)};
		transitions.erase(first, last);
		std::erase_if(transitions, [&](const std::pair<State, Symbol>& transition) {
			const StatePost& state_post{nfa.delta[transition.first]};
			const auto symbol_post{state_post.find(transition.second)};
			return symbol_post == state_post.end() || !symbol_post->targets.contains(target);
		});
		return transitions;
	}

  private:
	std::vector<std::vector<std::pair<State, Symbol>>> predecessors_{};
};

void remove_covered_state(const StateSet& covering_set, const State remove, Nfa& nfa, Predecessors& predecessors) {
	// Remove all transitions from the covered state.
	nfa.delta.mutable_state_post(remove).clear();

	// Redirect transitions to the covered state to the covering set instead.
	for (const auto& [source, symbol] : predecessors.take_transitions_to(nfa, remove)) {
		// transfer transitions from covered state to covering set
		for (const State switch_target : covering_set) { predecessors.add(nfa, source, symbol, switch_target); }
		nfa.delta.remove(source, symbol, remove);
	}

	// check final and initial states
	nfa.final.erase(remove);
	if (nfa.initial.contains(remove)) {
		nfa.initial.erase(remove);
		for (const State new_init : covering_set) { nfa.initial.insert(new_init); }
	}
}

void check_covered_and_covering(
	std::vector<StateSet>& covering_states, // covering sets for each state
	std::vector<StateSet>& covering_indexes, // indexes of covering states
	std::unordered_map<StateSet, State>& covered, // map of covered states
	std::unordered_map<StateSet, State>& subset_map, // map of non-covered states
	const State target_res,
	const StateSet& targets_orig, // current state to check
	Nfa& result,
	Predecessors& predecessors
) {
	auto it = subset_map.begin();

	// initiate with empty StateSets
	covering_states.emplace_back();
	covering_indexes.emplace_back();

	while (it != subset_map.end()) { // goes through all found states
		if (it->first.is_subset_of(targets_orig)) {
			// check if targets_orig is covered
			// if so add covering state to its covering StateSet

			covering_states[target_res].insert(it->first);
			covering_indexes[target_res].insert(it->second);
		} else if (targets_orig.is_subset_of(it->first)) {
			// check if state in map is covered
			// if so add covering state to its covering StateSet

			covering_states[it->second].insert(targets_orig);
			covering_indexes[it->second].insert(target_res);

			// check is some already existing state that had a new covering state added turned fully covered
			if (it->first == covering_states[it->second]) {
				// if any covered state is in the covering set of newly turned covered state,
				// then it has to be replaced by its covering set
				//
				// same applies for any covered state, if it contains newly turned state in theirs
				// covering set, then it has to be updated
				State erase_state = it->second; // covered state to remove
				for (const auto& state : covered | std::views::values) {
					if (covering_indexes[state].contains(erase_state)) {
						covering_indexes[state].erase(erase_state);
						covering_indexes[state].insert(covering_indexes[erase_state]);
					}
					if (covering_indexes[erase_state].contains(state)) {
						covering_indexes[erase_state].erase(state);
						covering_indexes[erase_state].insert(covering_indexes[state]);
					}
				}

				// remove covered state from the automaton, replace with covering set
				remove_covered_state(covering_indexes[erase_state], erase_state, result, predecessors);

				auto temp = it++;
				// move state from subset_map to covered
				auto transfer = subset_map.extract(temp);
				covered.insert(std::move(transfer));
				continue; // skip increasing map pointer
			}
		}
		++it;
	}
}

/// Covered states of residual reduction "after" in removal order, with their covering states.
struct CoveredStates {
	std::vector<State> order{};
	std::vector<StateSet> covering_sets{};
};

/**
 * Redirect all transitions and initial states from the covered states to their covering states and drop the covered
 *  states' outgoing transitions and finality, as if remove_covered_state() was called for each in order.
 *
 * A covering set holds only states not covered at removal time, so states covered later are resolved first.
 */
void redirect_covered_states(const CoveredStates& covered_states, Nfa& nfa) {
	const size_t num_of_covered{covered_states.order.size()};
	if (num_of_covered == 0) { return; }
	constexpr size_t NOT_COVERED{std::numeric_limits<size_t>::max()};
	std::vector<size_t> covered_index(nfa.num_of_states(), NOT_COVERED);
	for (size_t i{0}; i < num_of_covered; ++i) { covered_index[covered_states.order[i]] = i; }

	std::vector<StateSet> resolved(num_of_covered);
	for (size_t i{num_of_covered}; i-- > 0;) {
		for (const State covering : covered_states.covering_sets[i]) {
			const size_t covering_index{covered_index[covering]};
			if (covering_index == NOT_COVERED) {
				resolved[i].insert(covering);
			} else {
				resolved[i].insert(resolved[covering_index]);
			}
		}
	}

	const auto is_covered{[&](const State state) { return covered_index[state] != NOT_COVERED; }};
	for (State source{0}; source < nfa.delta.num_of_states(); ++source) {
		if (is_covered(source)) {
			nfa.delta.mutable_state_post(source).clear();
			continue;
		}
		const StatePost& state_post{nfa.delta[source]};
		if (std::ranges::none_of(state_post, [&](const SymbolPost& symbol_post) {
				return std::ranges::any_of(symbol_post.targets, is_covered);
			})) {
			continue;
		}
		for (SymbolPost& symbol_post : nfa.delta.mutable_state_post(source)) {
			if (std::ranges::none_of(symbol_post.targets, is_covered)) { continue; }
			StateSet targets{};
			for (const State target : symbol_post.targets) {
				if (!is_covered(target)) { targets.push_back(target); }
			}
			for (const State target : symbol_post.targets) {
				if (is_covered(target)) { targets.insert(resolved[covered_index[target]]); }
			}
			symbol_post.targets = std::move(targets);
		}
	}

	for (size_t i{0}; i < num_of_covered; ++i) {
		const State state{covered_states.order[i]};
		nfa.final.erase(state);
		if (nfa.initial.contains(state)) {
			nfa.initial.erase(state);
			for (const State covering : covered_states.covering_sets[i]) { nfa.initial.insert(covering); }
		}
	}
}

/**
 * Coverage analysis of residual reduction "after" over the macrostates ordered from the largest.
 *
 * A macrostate is covered when it is the union of the uncovered smaller macrostates it contains. Membership in the
 *  tested macrostate and in the union so far is marked by stamps over the states of the determinized NFA, so a
 *  subset test costs at most the size of the candidate and a union test the total size of the candidates.
 */
class ResidualCoverage {
  public:
	ResidualCoverage(
		std::vector<const StateSet*> macrostates, std::vector<State> result_states, const size_t num_of_nfa_states
	)
		: macrostates_{std::move(macrostates)},
		  result_states_{std::move(result_states)},
		  covered_(macrostates_.size(), false),
		  visited_(macrostates_.size(), false),
		  member_stamp_(num_of_nfa_states),
		  union_stamp_(num_of_nfa_states) {}

	CoveredStates run() {
		const size_t num_of_macrostates{macrostates_.size()};
		for (size_t i{0}; i < num_of_macrostates - 1; ++i) {
			if (macrostates_[i]->size() == 1) { break; } // Singletons are never covered.
			if (visited_[i]) { continue; }
			visited_[i] = true;
			try_cover(i, std::views::iota(i + 1, num_of_macrostates));
		}
		return std::move(covered_states_);
	}

  private:
	std::vector<const StateSet*> macrostates_;
	std::vector<State> result_states_;
	std::vector<bool> covered_;
	std::vector<bool> visited_;
	std::vector<size_t> member_stamp_;
	std::vector<size_t> union_stamp_;
	size_t stamp_{0};
	CoveredStates covered_states_{};

	/// Cover macrostate @p index by its uncovered subsets among @p candidates (ascending indexes after @p index),
	///  first trying to cover each of them by its successors in that list.
	template <class Candidates> void try_cover(const size_t index, const Candidates& candidates) {
		std::vector<size_t> covering_indexes{};
		if (!find_covering(index, candidates, covering_indexes)) { return; }
		const size_t covering_size{covering_indexes.size() - 1};
		for (size_t k{0}; k < covering_size; ++k) {
			const size_t candidate{covering_indexes[k]};
			if (macrostates_[candidate]->size() == 1) { break; }
			if (visited_[candidate]) { continue; }
			visited_[candidate] = true;
			try_cover(candidate, std::span<const size_t>{covering_indexes}.subspan(k + 1));
		}

		StateSet covering_set{};
		for (const size_t covering_index : covering_indexes) {
			if (!covered_[covering_index]) { covering_set.insert(result_states_[covering_index]); }
		}
		covered_states_.order.push_back(result_states_[index]);
		covered_states_.covering_sets.push_back(std::move(covering_set));
		covered_[index] = true;
	}

	/// Collect the uncovered @p candidates which are subsets of macrostate @p index into @p covering_indexes and
	///  return whether their union is the macrostate.
	template <class Candidates>
	bool find_covering(const size_t index, const Candidates& candidates, std::vector<size_t>& covering_indexes) {
		++stamp_;
		const StateSet& macrostate{*macrostates_[index]};
		for (const State state : macrostate) { member_stamp_[state] = stamp_; }
		size_t num_of_united{0};
		for (const size_t candidate : candidates) {
			if (covered_[candidate]) { continue; }
			const StateSet& candidate_macrostate{*macrostates_[candidate]};
			if (!std::ranges::all_of(candidate_macrostate, [&](const State state) {
					return member_stamp_[state] == stamp_;
				})) {
				continue;
			}
			covering_indexes.push_back(candidate);
			for (const State state : candidate_macrostate) {
				if (union_stamp_[state] != stamp_) {
					union_stamp_[state] = stamp_;
					++num_of_united;
				}
			}
		}
		return num_of_united == macrostate.size();
	}
};
} // namespace

std::ostream& std::operator<<(std::ostream& os, const mata::nfa::Transition& trans) { // {{{
	const std::string result = "(" + std::to_string(trans.source) + ", " + std::to_string(trans.symbol) + ", " +
							   std::to_string(trans.target) + ")";
	return os << result;
}

bool mata::nfa::Nfa::make_complete(const Alphabet* const alphabet, const std::optional<State> sink_state) {
	return make_complete(get_symbols_to_work_with(alphabet), sink_state);
}

bool mata::nfa::Nfa::make_complete(const OrdVector<Symbol>& symbols, const std::optional<State> sink_state) {
	if (symbols.empty()) { return false; }

	bool transition_added{false};
	const size_t num_of_states{this->num_of_states()};
	const State sink_state_val{sink_state.value_or(num_of_states)};
	const auto symbols_begin{symbols.begin()};
	const auto symbols_end{symbols.end()};

	// Scratch buffer for merging an incomplete state post. The swap below hands its storage over to the state and
	//  takes the storage of the state in exchange, so a later, larger state post can still reallocate. The win is
	//  appending the merged state post instead of inserting every missing symbol into the middle of the old one.
	StatePost completed_post{};
	for (State state{0}; state < num_of_states; ++state) {
		// Probed through the constant accessor: 'mutable_state_post()' allocates the posts of all states up to
		//  'state' (the delta may be shorter than the automaton), so a state that turns out to be complete must
		//  not reach it.
		const StatePost& post{delta[state]};

		// Both the state post and @p symbols are sorted by symbol, so a single merge decides whether the post
		//  contains all of @p symbols. Comparing sizes would not decide it: a post may hold symbols outside
		//  @p symbols (e.g., EPSILON), which complete nothing and are only skipped over here.
		auto symbols_it{symbols_begin};
		auto post_it{post.begin()};
		const auto post_end{post.end()};
		while (symbols_it != symbols_end) {
			while (post_it != post_end && post_it->symbol < *symbols_it) { ++post_it; }
			if (post_it == post_end || post_it->symbol != *symbols_it) { break; } // '*symbols_it' is missing.
			++post_it;
			++symbols_it;
		}
		if (symbols_it == symbols_end) { continue; } // The state is already complete.
		transition_added = true;
		// Upper bound on the missing symbols: everything from the first missing one on. 'post' dangles from here,
		//  'mutable_state_post()' may reallocate the state posts.
		const size_t num_of_missing_at_most{static_cast<size_t>(symbols_end - symbols_it)};

		// Rebuild the state post as the merge of the original symbol posts with the missing symbols. Both sequences
		//  are sorted, so the merged state post is created by appending only, with no sorted insert in the middle.
		StatePost& state_post{delta.mutable_state_post(state)};
		completed_post.clear();
		completed_post.reserve(state_post.size() + num_of_missing_at_most);
		symbols_it = symbols_begin;
		for (SymbolPost& symbol_post : state_post) {
			while (symbols_it != symbols_end && *symbols_it < symbol_post.symbol) {
				completed_post.emplace_back(*symbols_it, sink_state_val);
				++symbols_it;
			}
			if (symbols_it != symbols_end && *symbols_it == symbol_post.symbol) { ++symbols_it; }
			completed_post.emplace_back(std::move(symbol_post));
		}
		for (; symbols_it != symbols_end; ++symbols_it) { completed_post.emplace_back(*symbols_it, sink_state_val); }
		std::swap(state_post, completed_post);
	}

	if (transition_added && num_of_states <= sink_state_val) {
		// 'sink_state_val' is at least 'num_of_states', hence beyond every allocated state post: the sink post is
		//  empty and the self-loops over the sorted @p symbols are appended in order, without the search and the
		//  shifting that 'Delta::add()' pays per symbol.
		StatePost& sink_post{delta.mutable_state_post(sink_state_val)};
		MATA_ASSERT(sink_post.empty(), "Nfa::make_complete: the post of a fresh sink state must be empty.");
		sink_post.reserve(symbols.size());
		for (const Symbol symbol : symbols) { sink_post.emplace_back(symbol, sink_state_val); }
	}

	return transition_added;
}

// TODO: based on the comments inside, this function needs to be rewritten in a more optimal way.
Nfa mata::nfa::remove_epsilon(const Nfa& aut, const Symbol epsilon) {
	// cannot use multimap, because it can contain multiple occurrences of (a -> a), (a -> a)
	std::unordered_map<State, StateSet> epsilon_closure;

	// TODO: grossly inefficient
	// first we compute the epsilon closure
	const size_t num_of_states{aut.num_of_states()};
	for (State i{0}; i < num_of_states; ++i) {
		for (const auto& trans : aut.delta[i]) {
			const auto [it, inserted] = epsilon_closure.insert({i, {i}});
			if (trans.symbol == epsilon) {
				StateSet& closure = it->second;
				// TODO: Fix possibly insert to OrdVector. Create list already ordered, then merge (do not need to
				// resize each time);
				closure.insert(trans.targets);
			}
		}
	}

	bool changed = true;
	while (changed) { // Compute the fixpoint.
		changed = false;
		for (State i = 0; i < num_of_states; ++i) {
			const StatePost& post{aut.delta[i]};
			// TODO: make faster if default epsilon
			if (const auto eps_move_it{post.find(epsilon)}; eps_move_it != post.end()) {
				StateSet& src_eps_cl = epsilon_closure[i];
				for (const State tgt : eps_move_it->targets) {
					const StateSet& tgt_eps_cl = epsilon_closure[tgt];
					for (const State st : tgt_eps_cl) {
						if (src_eps_cl.count(st) == 0) {
							changed = true;
							break;
						}
					}
					src_eps_cl.insert(tgt_eps_cl);
				}
			}
		}
	}

	// Construct the automaton without epsilon transitions.
	Nfa result{Delta{}, aut.initial, aut.final, aut.alphabet};
	for (const auto& [state, closure_states] : epsilon_closure) {
		for (const State eps_cl_state : closure_states) {
			if (aut.final[eps_cl_state]) { result.final.insert(state); }
			for (const SymbolPost& move : aut.delta[eps_cl_state]) {
				if (move.symbol == epsilon) { continue; }
				// TODO: this could be done more efficiently if we had a better add method
				for (const State tgt_state : move.targets) { result.delta.add(state, move.symbol, tgt_state); }
			}
		}
	}
	return result;
}

Nfa mata::nfa::fragile_revert(const Nfa& aut) {
	const size_t num_of_states{aut.num_of_states()};

	Nfa result(num_of_states);

	result.initial = aut.final;
	result.final = aut.initial;

	// Compute non-epsilon symbols.
	OrdVector<Symbol> symbols = aut.delta.get_used_symbols();
	if (symbols.empty()) { return result; }
	if (symbols.back() == EPSILON) { symbols.pop_back(); }
	// size of the "used alphabet", i.e. max symbol+1 or 0
	const Symbol alphabet_size = (symbols.empty()) ? 0 : (symbols.back() + 1);

#ifdef _STATIC_STRUCTURES_
	// STATIC DATA STRUCTURES:
	//  Not sure that it works ideally, whether the space for the inner vectors stays there.
	static std::vector<std::vector<State>> sources;
	static std::vector<std::vector<State>> targets;
	static std::vector<State> e_sources;
	static std::vector<State> e_targets;
	if (alphabet_size > sources.size()) {
		sources.resize(alphabet_size);
		targets.resize(alphabet_size);
	}

	e_sources.clear();
	e_targets.clear();

	// WHEN ONLY MAX SYMBOL IS COMPUTED
	//  for (int i = 0;i<alphasize;i++) {
	//      for (int i = 0;i<alphasize;i++) {
	//          if (!sources[i].empty())
	//          {
	//              sources[i].resize(0);
	//              targets[i].resize(0);
	//          }
	//      }
	//  }

	// WHEN ALL SYMBOLS ARE COMPUTED
	for (Symbol symbol : symbols) {
		if (!sources[symbol].empty()) {
			sources[symbol].clear();
			targets[symbol].clear();
		}
	}
#else
	// NORMAL, NON-STATIC DATA STRUCTURES:
	// All transition of delta are to be copied here, into two arrays of transition sources and targets indexed by the
	//  transition symbol.
	// There is a special treatment for epsilon, since we want the arrays to be only as long as the largest symbol in
	// the automaton, and epsilon is the maximum (so we don't want to have the maximum array length whenever epsilon is
	// present).
	std::vector<std::vector<State>> sources(alphabet_size);
	std::vector<std::vector<State>> targets(alphabet_size);
	std::vector<State> e_sources;
	std::vector<State> e_targets;
#endif

	// Copy all transition with non-e symbols to the arrays of sources and targets indexed by symbols.
	// Targets and sources of e-transitions go to the special place.
	// Important: since we are going through delta in order of sources, the sources arrays are all ordered.
	for (State source_state{0}; source_state < num_of_states; ++source_state) {
		for (const SymbolPost& move : aut.delta[source_state]) {
			if (move.symbol == EPSILON) {
				for (const State target_state : move.targets) {
					// reserve_on_insert(e_sources);
					e_sources.push_back(source_state);
					// reserve_on_insert(e_targets);
					e_targets.push_back(target_state);
				}
			} else {
				for (const State target_state : move.targets) {
					// reserve_on_insert(sources[move.symbol]);
					sources[move.symbol].push_back(source_state);
					// reserve_on_insert(targets[move.symbol]);
					targets[move.symbol].push_back(target_state);
				}
			}
		}
	}

	// Now make the delta of the reversed automaton.
	// Important: since sources are ordered, when adding them as targets, we can just push them back.
	result.delta.reserve(num_of_states);

	// adding non-e transitions
	for (const Symbol symbol : symbols) {
		for (size_t i{0}; i < sources[symbol].size(); ++i) {
			const State tgt_state = sources[symbol][i];
			const State src_state = targets[symbol][i];
			StatePost& src_post = result.delta.mutable_state_post(src_state);
			if (src_post.empty() || src_post.back().symbol != symbol) { src_post.push_back(SymbolPost(symbol)); }
			src_post.back().push_back(tgt_state);
		}
	}

	// adding e-transitions
	for (size_t i{0}; i < e_sources.size(); ++i) {
		const State tgt_state = e_sources[i];
		const State src_state = e_targets[i];
		StatePost& src_post = result.delta.mutable_state_post(src_state);
		if (src_post.empty() || src_post.back().symbol != EPSILON) { src_post.push_back(SymbolPost(EPSILON)); }
		src_post.back().push_back(tgt_state);
	}

	// sorting the targets
	// Hm I don't know why I put this here, but it should not be needed ...
	// for (State q = 0, states_num = result.delta.post_size(); q<states_num;++q) {
	//     for (auto m = result.delta.get_mutable_post(q).begin(); m != result.delta.get_mutable_post(q).end(); ++m) {
	//         sort_and_rmdupl(m->targets);
	//     }
	// }

	return result;
}

Nfa mata::nfa::simple_revert(const Nfa& aut) {
	Nfa result;
	result.clear();

	const size_t num_of_states{aut.num_of_states()};
	result.delta.allocate(num_of_states);

	for (State source_state{0}; source_state < num_of_states; ++source_state) {
		for (const SymbolPost& transition : aut.delta[source_state]) {
			for (const State target_state : transition.targets) {
				result.delta.add(target_state, transition.symbol, source_state);
			}
		}
	}

	result.initial = aut.final;
	result.final = aut.initial;

	return result;
}

// not so great, can be removed
Nfa mata::nfa::somewhat_simple_revert(const Nfa& aut) {
	const size_t num_of_states{aut.num_of_states()};

	Nfa result(num_of_states);

	result.initial = aut.final;
	result.final = aut.initial;

	for (State source_state{0}; source_state < num_of_states; ++source_state) {
		for (const SymbolPost& transition : aut.delta[source_state]) {
			for (const State target_state : transition.targets) {
				StatePost& post = result.delta.mutable_state_post(target_state);
				// auto move = std::find(post.begin(),post.end(),Move(transition.symbol));
				if (auto move = post.find(SymbolPost(transition.symbol)); move == post.end()) {
					// post.push_back(Move(transition.symbol,sourceState));
					post.insert(SymbolPost(transition.symbol, source_state));
				} else {
					move->push_back(source_state);
				}
				// move->insert(sourceState);
			}
		}
	}

	// sorting the targets
	for (State q = 0, states_num = static_cast<State>(result.delta.num_of_states()); q < states_num; ++q) {
		// Post & post = result.delta.get_mutable_post(q);
		// utils::sort_and_rmdupl(post);
		for (SymbolPost& m : result.delta.mutable_state_post(q)) { m.targets.sort_and_rmdupl(); }
	}

	return result;
}

Nfa mata::nfa::revert(const Nfa& aut) {
	// The counting sort of Delta::reverted is much faster than the insertion-based variants below
	//  (kept for benchmarking) and does not allocate any symbol-indexed arrays as fragile_revert does.
	Nfa result{};
	result.delta = aut.delta.reverted(aut.num_of_states());
	result.initial = aut.final;
	result.final = aut.initial;
	return result;
}

bool mata::nfa::Nfa::is_deterministic() const {
	if (initial.size() != 1) { return false; }
	if (delta.empty()) { return true; }

	const size_t aut_size{num_of_states()};
	for (State i = 0; i < aut_size; ++i) {
		for (const auto& symbol_post : delta[i]) {
			if (symbol_post.num_of_targets() != 1) { return false; }
		}
	}
	return true;
}

bool mata::nfa::Nfa::is_minimal() const {
	if (!is_deterministic()) { return false; }
	if (final.empty()) { return num_of_states() == 1 && delta.num_of_transitions() == 0; }
	if (get_useful_states().count() != num_of_states()) { return false; }
	// Hopcroft merges exactly the equivalent states of a trimmed DFA.
	return algorithms::minimize_hopcroft(*this).num_of_states() == num_of_states();
}

bool Nfa::is_complete(const Alphabet* const alphabet) const { return is_complete(get_symbols_to_work_with(alphabet)); }

bool mata::nfa::Nfa::is_complete(const OrdVector<Symbol>& symbols) const {
	// TODO: make a general function for traversal over reachable states that can be shared by other functions?
	// Breadth-first search over the reachable states, with a vector used as a FIFO queue and a byte array in place
	//  of a hash set of visited states. Both a state post and @p symbols are sorted by symbol, so a state is decided
	//  by a single two-pointer merge instead of one binary search per outgoing symbol.
	// No `delta.empty()` guard: `delta[state]` of an empty or unallocated state post already yields nothing, while
	//  `Delta::empty()` walks state posts until it finds a nonempty one and so costs O(|Q|) per worklist item.
	std::vector<State> worklist(initial.begin(), initial.end());
	std::vector<char> visited(num_of_states(), 0);
	const auto visit = [&visited](const State state) -> bool {
		if (state >= visited.size()) { visited.resize(state + 1, 0); }
		if (visited[state] != 0) { return false; }
		visited[state] = 1;
		return true;
	};
	for (const State state : initial) { visit(state); }

	const auto symbols_end{symbols.end()};
	for (size_t head{0}; head < worklist.size(); ++head) {
		const StatePost& state_post{delta[worklist[head]]};
		auto symbol_post_it{state_post.begin()};
		const auto symbol_post_end{state_post.end()};
		auto symbol_it{symbols.begin()};
		// A transition symbol outside @p symbols throws, and the throw takes precedence over a symbol of @p symbols
		//  missing in the same state. The whole state post is therefore scanned before `false` is returned.
		bool is_symbol_missing{false};
		while (symbol_post_it != symbol_post_end) {
			if (symbol_it == symbols_end || symbol_post_it->symbol < *symbol_it) {
				throw std::runtime_error(
					std::to_string(__func__) + ": encountered a symbol that is not in the provided alphabet"
				);
			}
			if (*symbol_it < symbol_post_it->symbol) { // The symbol has no transition from this state.
				is_symbol_missing = true;
				++symbol_it;
				continue;
			}
			for (const State target : symbol_post_it->targets) {
				if (visit(target)) { worklist.push_back(target); }
			}
			++symbol_post_it;
			++symbol_it;
		}
		if (is_symbol_missing || symbol_it != symbols_end) { return false; }
	}

	return true;
}

std::pair<Run, bool> mata::nfa::Nfa::get_word_for_path(const Run& run) const {
	if (run.path.empty()) { return {{}, true}; }

	Run word;
	State cur = run.path[0];
	for (size_t i = 1; i < run.path.size(); ++i) {
		const State new_st = run.path[i];
		bool found = false;
		// `delta[cur]` already yields nothing for an empty or unallocated state post, see `is_complete()`.
		for (const auto& symbol_map : this->delta[cur]) {
			for (const State st : symbol_map.targets) {
				if (st == new_st) {
					word.word.push_back(symbol_map.symbol);
					found = true;
					break;
				}
			}
			if (found) { break; }
		}
		if (!found) { return {{}, false}; }
		cur = new_st; // update current state
	}
	return {word, true};
}

// TODO: this is not efficient
StateSet mata::nfa::Nfa::read_word(const Run& run, const bool use_epsilon) const {
	StateSet current_post(this->initial);

	if (use_epsilon) {
		// Compute epsilon closure from the initial states.
		current_post = this->post(current_post, EPSILON, EpsilonClosureOpt::Before);
	}

	const EpsilonClosureOpt epsilon_closure_opt = use_epsilon ? EpsilonClosureOpt::After : EpsilonClosureOpt::None;
	for (const Symbol sym : run.word) {
		current_post = this->post(current_post, sym, epsilon_closure_opt);
		if (current_post.empty()) { return current_post; }
	}

	return current_post;
}

std::optional<State> mata::nfa::Nfa::read_word_det(const Run& run) const {
	if (initial.empty()) { return std::nullopt; }
	State current_post = *initial.begin();

	for (const Symbol sym : run.word) {
		const SmallStateSet& successors = delta.get_successors(current_post, sym);
		if (successors.empty()) { return std::nullopt; }
		current_post = *successors.begin();
	}

	return current_post;
}

// TODO: this is not efficient
bool Nfa::is_in_lang(const Run& run, const bool use_epsilon, const bool match_prefix) const {
	if (!use_epsilon && !match_prefix && initial.size() == 1) {
		// Scalar walk: while every visited symbol post has at most one target, the run is unique. On the first
		//  nondeterministic post, the subset walk continues from the state reached, keeping the answer exact.
		State state{*initial.begin()};
		size_t prefix_len{0};
		const size_t word_len{run.word.size()};
		for (; prefix_len < word_len; ++prefix_len) {
			const SmallStateSet& successors{delta.get_successors(state, run.word[prefix_len])};
			if (successors.empty()) { return false; }
			if (successors.size() > 1) { break; }
			state = successors.front();
		}
		if (prefix_len == word_len) { return final.contains(state); }
		StateSet current_post{state};
		for (; prefix_len < word_len; ++prefix_len) {
			current_post = post(current_post, run.word[prefix_len]);
			if (current_post.empty()) { return false; }
		}
		return final.intersects_with(current_post);
	}
	if (match_prefix) {
		StateSet current_post(this->initial);

		if (use_epsilon) {
			// Compute epsilon closure from the initial states.
			current_post = this->post(current_post, EPSILON, EpsilonClosureOpt::Before);
		}

		const EpsilonClosureOpt epsilon_closure_opt = use_epsilon ? EpsilonClosureOpt::After : EpsilonClosureOpt::None;
		for (const Symbol sym : run.word) {
			if (this->final.intersects_with(current_post)) { return true; }
			current_post = this->post(current_post, sym, epsilon_closure_opt);
			if (current_post.empty()) { return false; }
		}

		return this->final.intersects_with(current_post);
	}

	return this->final.intersects_with(read_word(run, use_epsilon));
}

Nfa mata::nfa::algorithms::minimize_brzozowski(const Nfa& aut) {
	// compute the minimal deterministic automaton, Brzozovski algorithm
	return determinize(revert(determinize(revert(aut))));
}

Nfa mata::nfa::algorithms::minimize_auto(const Nfa& aut) {
	// Brzozowski determinizes twice, so it can blow up exponentially even on an input that already is a trimmed
	//  DFA; Hopcroft is O(a*n*log(n)) on such an input and supports partial transition functions, so the
	//  determinization is only needed when the input is not deterministic.
	Nfa deterministic{aut.is_deterministic() ? aut : determinize(aut)};
	deterministic.trim();
	if (deterministic.num_of_states() == 0) {
		// Trimming an automaton with an empty language removes every state. Return the canonical automaton of the
		//  empty language instead, so that the result is a usable automaton with a single non-final initial state.
		Nfa empty_language{1};
		empty_language.initial.insert(0);
		return empty_language;
	}
	return minimize_hopcroft(deterministic);
}

Nfa mata::nfa::minimize(const Nfa& aut, const ParameterMap& params) {
	decltype(algorithms::minimize_auto)* algo = algorithms::minimize_auto;
	if (!haskey(params, "algorithm")) {
		throw std::runtime_error(
			std::to_string(__func__) +
			" requires setting the \"algorithm\" key in the \"params\" argument; "
			"received: " +
			std::to_string(params)
		);
	}

	if (const std::string& str_algo = params.at("algorithm"); "auto" == str_algo) { /* default */
	} else if ("brzozowski" == str_algo) {
		algo = algorithms::minimize_brzozowski;
	} else if ("hopcroft" == str_algo) {
		algo = algorithms::minimize_hopcroft;
	} else {
		throw std::runtime_error(
			std::to_string(__func__) + " received an unknown value of the \"algorithm\" key: " + str_algo
		);
	}

	return algo(aut);
}

// Anonymous namespace for the Hopcroft minimization algorithm.
namespace {
template <typename T>
/**
 * A class for partitionable sets of elements. All elements are stored in a contiguous vector.
 * Elements from the same set are contiguous in the vector. Auxiliary data (indices, positions, etc.) are
 * stored in separate vectors.
 */
class RefinablePartition {
  public:
	static_assert(std::is_unsigned_v<T>, "T must be an unsigned type.");
	static const T no_more_elements = std::numeric_limits<T>::max();
	static constexpr size_t no_split = std::numeric_limits<size_t>::max();
	size_t num_of_sets; ///< The number of sets in the partition.
	std::vector<size_t> set_idx; ///< For each element, tells the index of the set it belongs to.

  private:
	std::vector<T> elems_; ///< Contains all elements in an order such that the elements of the same set are contiguous.
	std::vector<T> location_; ///< Contains the location of each element in the elements vector.
	std::vector<size_t> first_; ///< Contains the index of the first element of each set in the elements vector.
	std::vector<size_t> end_; ///< Contains the index after the last element of each set in the elements vector.
	std::vector<size_t>
		mid_; ///< Contains the index after the last element of the first half of the set in the elements vector.

  public:
	/**
	 * @brief Construct a new Refinable Partition for equivalence classes of states.
	 *
	 * @param num_of_states The number of states.
	 */
	explicit RefinablePartition(const size_t num_of_states)
		: num_of_sets(1),
		  set_idx(num_of_states),
		  elems_(num_of_states),
		  location_(num_of_states),
		  first_(num_of_states),
		  end_(num_of_states),
		  mid_(num_of_states) {
		// Initially, all states are in the same equivalence class.
		first_[0] = mid_[0] = 0;
		end_[0] = num_of_states;
		for (State e = 0; e < num_of_states; ++e) {
			elems_[e] = location_[e] = e;
			set_idx[e] = 0;
		}
	}

	/**
	 * @brief Construct a new Refinable Partition for sets of splitters (transitions incoming to
	 *  equivalence classes under a common symbol). Initially, the partition has n sets, where n is the alphabet size.
	 *
	 * @param delta The transition function.
	 */
	explicit RefinablePartition(const Delta& delta)
		: num_of_sets(0),
		  set_idx(),
		  elems_(),
		  location_(),
		  first_(),
		  end_(),
		  mid_() {
		size_t num_of_transitions = 0;
		std::vector<size_t> counts;
		std::unordered_map<Symbol, size_t> symbol_map;

		// Transitions are grouped by symbols using counting sort in time O(m).
		// Count the number of elements and the number of sets.
		for (const auto& trans : delta.transitions()) {
			if (const Symbol a{trans.symbol}; !symbol_map.contains(a)) {
				symbol_map[a] = num_of_sets++;
				counts.push_back(1);
			} else {
				++counts[symbol_map[a]];
			}
			++num_of_transitions;
		}

		// Initialize data structures.
		elems_.resize(num_of_transitions);
		location_.resize(num_of_transitions);
		set_idx.resize(num_of_transitions);
		first_.resize(num_of_transitions);
		end_.resize(num_of_transitions);
		mid_.resize(num_of_transitions);

		// Compute set indices.
		// Use (mid - 1) as an index for the insertion.
		first_[0] = 0;
		end_[0] = counts[0];
		mid_[0] = end_[0];
		for (size_t i = 1; i < num_of_sets; ++i) {
			first_[i] = end_[i - 1];
			end_[i] = first_[i] + counts[i];
			mid_[i] = end_[i];
		}

		// Fill the sets from the back.
		// Mid, decremented before use, is used as an index for the next element.
		size_t trans_idx = 0; // Index of the transition in the (flattened) delta.
		for (const auto& trans : delta.transitions()) {
			const Symbol a = trans.symbol;
			const size_t a_idx = symbol_map[a];
			const size_t trans_loc = mid_[a_idx] - 1;
			mid_[a_idx] = trans_loc;
			elems_[trans_loc] = trans_idx;
			location_[trans_idx] = trans_loc;
			set_idx[trans_idx] = a_idx;
			++trans_idx;
		}
	}

	RefinablePartition(const RefinablePartition& other)
		: num_of_sets(other.num_of_sets),
		  set_idx(other.set_idx),
		  elems_(other.elems_),
		  location_(other.location_),
		  first_(other.first_),
		  end_(other.end_),
		  mid_(other.mid_) {}

	RefinablePartition(RefinablePartition&& other) noexcept
		: num_of_sets(other.num_of_sets),
		  set_idx(std::move(other.set_idx)),
		  elems_(std::move(other.elems_)),
		  location_(std::move(other.location_)),
		  first_(std::move(other.first_)),
		  end_(std::move(other.end_)),
		  mid_(std::move(other.mid_)) {}

	RefinablePartition& operator=(const RefinablePartition& other) {
		if (this != &other) {
			elems_ = other.elems_;
			set_idx = other.set_idx;
			location_ = other.location_;
			first_ = other.first_;
			end_ = other.end_;
			mid_ = other.mid_;
			num_of_sets = other.num_of_sets;
		}
		return *this;
	}

	RefinablePartition& operator=(RefinablePartition&& other) noexcept {
		if (this != &other) {
			elems_ = std::move(other.elems_);
			set_idx = std::move(other.set_idx);
			location_ = std::move(other.location_);
			first_ = std::move(other.first_);
			end_ = std::move(other.end_);
			mid_ = std::move(other.mid_);
			num_of_sets = other.num_of_sets;
		}
		return *this;
	}

	/**
	 * @brief Get the number of elements across all sets.
	 *
	 * @return The number of elements.
	 */
	inline size_t size() const { return elems_.size(); }

	/**
	 * @brief Get the size of the set.
	 *
	 * @param s The set index.
	 * @return The size of the set.
	 */
	inline size_t size_of_set(const size_t s) const { return end_[s] - first_[s]; }

	/**
	 * @brief Get the first element of the set.
	 *
	 * @param s The set index.
	 * @return The first element of the set.
	 */
	inline T get_first(const size_t s) const { return elems_[first_[s]]; }

	/**
	 * @brief Get the next element of the set.
	 *
	 * @param e The element.
	 * @return The next element of the set or NO_MORE_ELEMENTS if there is no next element.
	 */
	inline T get_next(const T e) const {
		if (location_[e] + 1 >= end_[set_idx[e]]) { return no_more_elements; }
		return elems_[location_[e] + 1];
	}

	/**
	 * @brief Mark the element and move it to the first half of the set.
	 *
	 * @param e The element.
	 */
	void mark(const T e) {
		const size_t e_set = set_idx[e];
		const size_t e_loc = location_[e];
		const size_t e_set_mid = mid_[e_set];
		if (e_loc >= e_set_mid) {
			elems_[e_loc] = elems_[e_set_mid];
			location_[elems_[e_loc]] = static_cast<T>(e_loc);
			elems_[e_set_mid] = e;
			location_[e] = static_cast<T>(e_set_mid);
			mid_[e_set] = e_set_mid + 1;
		}
	}

	/**
	 * @brief Test if the set has no marked elements.
	 *
	 * @param s The set index.
	 * @return True if the set has no marked elements, false otherwise.
	 */
	inline bool has_no_marks(const size_t s) const { return mid_[s] == first_[s]; }

	/**
	 * @brief Split the set into two sets according to the marked elements (the mid).
	 * The first set name remains the same.
	 *
	 * @param s The set index.
	 * @return The new set index.
	 */
	size_t split(const size_t s) {
		if (mid_[s] == end_[s]) {
			// If no elements were marked, move the mid to the end (no split needed).
			mid_[s] = first_[s];
		}
		if (mid_[s] == first_[s]) {
			// If all or no elements were marked, there is no need to split the set.
			return no_split;
		} else {
			// Split the set according to the mid.
			first_[num_of_sets] = first_[s];
			mid_[num_of_sets] = first_[s];
			end_[num_of_sets] = mid_[s];
			first_[s] = mid_[s];
			for (size_t l = first_[num_of_sets]; l < end_[num_of_sets]; ++l) { set_idx[elems_[l]] = num_of_sets; }
			return num_of_sets++;
		}
	}
};
} // namespace

Nfa mata::nfa::algorithms::minimize_hopcroft(const Nfa& dfa_trimmed) {
	// `empty()` stops at the first non-empty state post. It differs from `num_of_transitions() == 0` only on a symbol
	//  post without targets, which the required `trim()` drops.
	if (dfa_trimmed.delta.empty()) {
		// The automaton is trivially minimal.
		return Nfa{dfa_trimmed};
	}
	MATA_ASSERT(dfa_trimmed.is_deterministic());
	MATA_ASSERT(dfa_trimmed.get_useful_states().count() == dfa_trimmed.num_of_states());

	// Initialize equivalence classes. The initial partition is {Q}.
	RefinablePartition<State> brp(dfa_trimmed.num_of_states());
	// Initialize splitters. A splitter is a set of transitions
	// over a common symbol incoming to an equivalence class. Initially,
	// the partition has m splitters, where m is the alphabet size.
	RefinablePartition<size_t> trp(dfa_trimmed.delta);

	// Initialize vector of incoming transitions for each state. Transitions
	// are represented only by their indices in the (flattened) delta.
	// Initialize mapping from transition index to its source state.
	std::vector<State> trans_source_map(trp.size());
	std::vector<std::vector<size_t>> incoming_trans_idxs(brp.size(), std::vector<size_t>());
	size_t trans_idx = 0;
	for (const auto& trans : dfa_trimmed.delta.transitions()) {
		trans_source_map[trans_idx] = trans.source;
		incoming_trans_idxs[trans.target].push_back(trans_idx);
		++trans_idx;
	}

	// Worklists for the Hopcroft algorithm.
	std::stack<size_t> unready_splitters; // Splitters that will be used in the backpropagation.
	std::stack<size_t> touched_blocks; // Blocks (equivalence classes) touched during backpropagation.
	std::stack<size_t>
		touched_splitters; // Splitters touched (in the split_block function) as a result of backpropagation.

	/**
	 * @brief Split the block (equivalence class) according to the marked states.
	 *
	 * @param b The block index.
	 */
	auto split_block = [&](const size_t b) {
		// touched_splitters has been moved to a higher scope to avoid multiple constructions/destructions.
		MATA_ASSERT(touched_splitters.empty());
		size_t b_prime = brp.split(b); // One block will keep the old name 'b'.
		if (b_prime == RefinablePartition<size_t>::no_split) {
			// All or no states in the block were marked (touched) during the backpropagation.
			return;
		}
		// According to the Hopcroft's algorithm, it is sufficient to work only with the smaller block.
		if (brp.size_of_set(b) < brp.size_of_set(b_prime)) { b_prime = b; }
		// Split the transitions of the splitters according to the new partitioning.
		// Transitions in one splitter must have the same symbol and go to the same block.
		for (State q = brp.get_first(b_prime); q != RefinablePartition<State>::no_more_elements; q = brp.get_next(q)) {
			for (const size_t trans_index : incoming_trans_idxs[q]) {
				if (const size_t splitter_idx = trp.set_idx[trans_index]; trp.has_no_marks(splitter_idx)) {
					touched_splitters.push(splitter_idx);
				}
				// Mark the transition in the splitter and move it to the first half of the set.
				trp.mark(trans_index);
			}
		}
		// Refine all splitters where some transitions were marked.
		while (!touched_splitters.empty()) {
			const size_t splitter_idx = touched_splitters.top();
			touched_splitters.pop();
			if (const size_t splitter_pime = trp.split(splitter_idx);
				splitter_pime != RefinablePartition<size_t>::no_split) {
				// Use the new splitter for further refinement of the equivalence classes.
				unready_splitters.push(splitter_pime);
			}
		}
	};

	// Use all splitters for the initial refinement.
	for (size_t splitter_idx = 0; splitter_idx < trp.num_of_sets; ++splitter_idx) {
		unready_splitters.push(splitter_idx);
	}

	// In the first refinement, we split the equivalence classes according to the final states.
	for (const State q : dfa_trimmed.final) { brp.mark(q); }
	split_block(0);

	// Main loop of the Hopcroft's algorithm.
	while (!unready_splitters.empty()) {
		const size_t splitter_idx = unready_splitters.top();
		unready_splitters.pop();
		// Backpropagation.
		// Fire back all transitions of the splitter. (Transitions over the same
		// symbol that go to the same block.) Mark the source states of these transitions.
		for (size_t trans_index = trp.get_first(splitter_idx);
			 trans_index != RefinablePartition<size_t>::no_more_elements; trans_index = trp.get_next(trans_index)) {
			const State q = trans_source_map[trans_index];
			if (const size_t b_prime = brp.set_idx[q]; brp.has_no_marks(b_prime)) { touched_blocks.push(b_prime); }
			brp.mark(q);
		}
		// Try to split the blocks touched during the backpropagation.
		// The block will be split only if some states (not all) were touched (marked).
		while (!touched_blocks.empty()) {
			const size_t b = touched_blocks.top();
			touched_blocks.pop();
			split_block(b);
		}
	}

	// Construct the minimized automaton using equivalence classes (BRP).
	MATA_ASSERT(dfa_trimmed.initial.size() == 1);
	Nfa result(brp.num_of_sets, {static_cast<State>(brp.set_idx[*dfa_trimmed.initial.begin()])}, {});
	for (State block_idx = 0; block_idx < brp.num_of_sets; ++block_idx) {
		const State q = brp.get_first(block_idx);
		if (dfa_trimmed.final.contains(q)) { result.final.insert(block_idx); }
		StatePost& mut_state_post = result.delta.mutable_state_post(block_idx);
		for (const SymbolPost& symbol_post : dfa_trimmed.delta[q]) {
			MATA_ASSERT(symbol_post.targets.size() == 1);
			const auto target{static_cast<State>(brp.set_idx[*symbol_post.targets.begin()])};
			mut_state_post.push_back(SymbolPost{symbol_post.symbol, target});
		}
	}

	return result;
}

Nfa mata::nfa::product(
	const Nfa& lhs,
	const Nfa& rhs,
	const ProductFinalStateCondition final_condition,
	const Symbol first_epsilon,
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>>* prod_map
) {
	std::function<bool(const State, const State)> is_product_state_final_func;
	if (final_condition == ProductFinalStateCondition::Or) {
		if (lhs.initial.empty() || lhs.final.empty()) { return rhs; }
		if (rhs.initial.empty() || rhs.final.empty()) { return lhs; }
		is_product_state_final_func = [&](const State lhs_state, const State rhs_state) {
			return lhs.final.contains(lhs_state) || rhs.final.contains(rhs_state);
		};
	} else if (final_condition == ProductFinalStateCondition::And) {
		if (lhs.initial.empty() || lhs.final.empty()) { return Nfa{}; }
		if (rhs.initial.empty() || rhs.final.empty()) { return Nfa{}; }
		is_product_state_final_func = [&](const State lhs_state, const State rhs_state) {
			return lhs.final.contains(lhs_state) && rhs.final.contains(rhs_state);
		};
	} else {
		throw std::runtime_error(std::to_string(__func__) + " received an unknown value of the \"final_condition\"");
	}

	return algorithms::product(lhs, rhs, std::move(is_product_state_final_func), first_epsilon, prod_map);
}

Nfa mata::nfa::intersection(
	const Nfa& lhs,
	const Nfa& rhs,
	const Symbol first_epsilon,
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>>* prod_map
) {
	return product(lhs, rhs, ProductFinalStateCondition::And, first_epsilon, prod_map);
}

Nfa mata::nfa::union_nondet(const Nfa& lhs, const Nfa& rhs) {
	Nfa result{lhs};
	result.unite_nondet_with(rhs);
	return result;
}

Nfa mata::nfa::union_det_complete(const Nfa& lhs, const Nfa& rhs) {
	if (!lhs.is_deterministic() || !rhs.is_deterministic()) {
		throw std::invalid_argument{"union_det_complete requires deterministic automata."};
	}
	if (!lhs.is_complete() || !rhs.is_complete()) {
		throw std::invalid_argument{"union_det_complete requires complete automata."};
	}
	return product(lhs, rhs, ProductFinalStateCondition::Or, EPSILON);
}

Nfa mata::nfa::union_nondet(Nfa&& lhs, const Nfa& rhs) {
	// Aliased operands: snapshot `rhs` before `lhs` is consumed.
	if (&lhs == &rhs) { return union_nondet(std::move(lhs), Nfa{rhs}); }
	Nfa result{std::move(lhs)};
	result.unite_nondet_with(rhs);
	return result;
}

Nfa mata::nfa::union_nondet(Nfa&& lhs, Nfa&& rhs) {
	// Aliased operands: snapshot `rhs` before `lhs` is consumed.
	if (&lhs == &rhs) { return union_nondet(std::move(lhs), Nfa{rhs}); }
	Nfa result{std::move(lhs)};
	result.unite_nondet_with(std::move(rhs));
	return result;
}

Simlib::Util::BinaryRelation mata::nfa::algorithms::compute_relation(const Nfa& aut, const ParameterMap& params) {
	if (!haskey(params, "relation")) {
		throw std::runtime_error(
			std::to_string(__func__) +
			" requires setting the \"relation\" key in the \"params\" argument; "
			"received: " +
			std::to_string(params)
		);
	}
	if (!haskey(params, "direction")) {
		throw std::runtime_error(
			std::to_string(__func__) +
			" requires setting the \"direction\" key in the \"params\" argument; "
			"received: " +
			std::to_string(params)
		);
	}

	const std::string& relation = params.at("relation");
	if (const std::string& direction = params.at("direction"); "simulation" == relation && direction == "forward") {
		return compute_fw_direct_simulation(aut);
	} else {
		throw std::runtime_error(
			std::to_string(__func__) + " received an unknown value of the \"relation\" key: " + relation
		);
	}
}

Nfa mata::nfa::reduce(const Nfa& aut, StateRenaming* state_renaming, const ParameterMap& params) {
	if (!haskey(params, "algorithm")) {
		throw std::runtime_error(
			std::to_string(__func__) +
			" requires setting the \"algorithm\" key in the \"params\" argument; "
			"received: " +
			std::to_string(params)
		);
	}

	Nfa result;
	std::unordered_map<State, State> reduced_state_map;
	if (const std::string& algorithm = params.at("algorithm"); "simulation" == algorithm) {
		result = algorithms::reduce_simulation(aut, reduced_state_map);
	} else if ("residual" == algorithm) {
		// Throw early before any work if state_renaming is requested: residual reduces by prime residuals
		// (double-determinized macrostates), not by equivalence classes, so no original→reduced mapping exists.
		if (state_renaming != nullptr) {
			throw std::runtime_error(
				"mata::nfa::reduce() with algorithm=\"residual\" does not support state_renaming; "
				"the result states are prime residuals, not equivalence classes of input states. "
				"Call reduce() without requesting a state_renaming, or use algorithm=\"simulation\" instead."
			);
		}

		// reduce type either 'after' or 'with' creation of residual automaton
		if (!haskey(params, "type")) {
			throw std::runtime_error(
				std::to_string(__func__) +
				" requires setting the \"type\" key in the \"params\" argument; "
				"received: " +
				std::to_string(params)
			);
		}
		// forward or backward canonical residual automaton
		if (!haskey(params, "direction")) {
			throw std::runtime_error(
				std::to_string(__func__) +
				" requires setting the \"direction\" key in the \"params\" argument; "
				"received: " +
				std::to_string(params)
			);
		}

		const std::string& residual_type = params.at("type");
		const std::string& residual_direction = params.at("direction");

		// Use a dummy map since reduce_residual requires one but we won't use it.
		std::unordered_map<State, State> dummy_map;
		result = algorithms::reduce_residual(aut, dummy_map, residual_type, residual_direction);
	} else if ("sat" == algorithm) {
		// The result is constructed anew, its states do not correspond to the states of the original automaton.
		result = algorithms::reduce_sat(
			aut, haskey(params, "type") ? params.at("type") : "nfa", haskey(params, "solver") ? params.at("solver") : ""
		);
	} else if ("qbf" == algorithm) {
		result = algorithms::reduce_qbf(aut, haskey(params, "solver") ? params.at("solver") : "");
	} else {
		throw std::runtime_error(
			std::to_string(__func__) + " received an unknown value of the \"algorithm\" key: " + algorithm
		);
	}

	if (state_renaming) {
		state_renaming->clear();
		*state_renaming = reduced_state_map;
	}
	return result;
}

Nfa mata::nfa::determinize(
	const Nfa& aut,
	std::unordered_map<StateSet, State>* subset_map,
	std::optional<std::function<bool(const Nfa&, const State, const StateSet&)>> macrostate_discover,
	const OrdVector<Symbol>* const restrict_to_symbols
) {
	// DFA fast path: a scalar depth-first renumbering reproduces the numbering of the generic subset
	//  construction below exactly (same stack discipline, same increasing-symbol visit order) without
	//  hashing one-element macrostates.
	if (aut.is_deterministic()) {
		std::unordered_map<StateSet, State> subset_map_local{};
		if (subset_map == nullptr) { subset_map = &subset_map_local; }
		Nfa result{};
		const State initial_res{result.add_state()};
		result.initial.insert(initial_res);
		const State initial_orig{*aut.initial.begin()};
		if (aut.final.contains(initial_orig)) { result.final.insert(initial_res); }
		(*subset_map)[StateSet{initial_orig}] = initial_res;
		if (aut.delta.empty()) { return result; }
		if (macrostate_discover.has_value() && !(*macrostate_discover)(result, initial_res, StateSet{initial_orig})) {
			return result;
		}
		constexpr State kUnseen{std::numeric_limits<State>::max()};
		std::vector<State> renaming(aut.num_of_states(), kUnseen);
		renaming[initial_orig] = initial_res;
		std::vector<State> worklist{initial_orig};
		while (!worklist.empty()) {
			const State state_orig{worklist.back()};
			worklist.pop_back();
			const State state_res{renaming[state_orig]};
			for (const SymbolPost& symbol_post : aut.delta[state_orig]) {
				if (symbol_post.targets.empty()) { continue; }
				if (restrict_to_symbols != nullptr && !haskey(*restrict_to_symbols, symbol_post.symbol)) { continue; }
				const State target_orig{symbol_post.targets.front()};
				State target_res{renaming[target_orig]};
				const bool target_is_new{target_res == kUnseen};
				if (target_is_new) {
					target_res = result.add_state();
					renaming[target_orig] = target_res;
					(*subset_map)[StateSet{target_orig}] = target_res;
					if (aut.final.contains(target_orig)) { result.final.insert(target_res); }
					worklist.push_back(target_orig);
				}
				result.delta.mutable_state_post(state_res).insert(SymbolPost(symbol_post.symbol, target_res));
				if (target_is_new && macrostate_discover.has_value() &&
					!(*macrostate_discover)(result, target_res, StateSet{target_orig})) {
					return result;
				}
			}
		}
		return result;
	}

	Nfa result{};
	// assuming all sets targets are non-empty
	// Macrostates are owned by the subset map; its nodes never move, so the worklist points to their keys.
	std::vector<std::pair<State, const StateSet*>> worklist{};
	std::unordered_map<StateSet, State> subset_map_local{};
	if (subset_map == nullptr) { subset_map = &subset_map_local; }

	const State initial_state_res{result.add_state()};
	result.initial.insert(initial_state_res);
	const StateSet& initial_states_orig{
		subset_map->insert_or_assign(StateSet{aut.initial}, initial_state_res).first->first
	};

	if (aut.final.intersects_with(initial_states_orig)) { result.final.insert(initial_state_res); }
	worklist.emplace_back(initial_state_res, &initial_states_orig);
	if (aut.delta.empty()) { return result; }
	if (macrostate_discover.has_value() && !(*macrostate_discover)(result, initial_state_res, initial_states_orig)) {
		return result;
	}

	using Iterator = mata::utils::OrdVector<SymbolPost>::const_iterator;
	SynchronizedExistentialSymbolPostIterator synchronized_iterator;

	while (!worklist.empty()) {
		const auto [state_res, states_orig_ptr]{worklist.back()};
		worklist.pop_back();
		const StateSet& states_orig{*states_orig_ptr};
		if (states_orig.empty()) {
			// This should not happen assuming all sets targets are non-empty.
			break;
		}

		// add moves of S to the sync ex iterator
		synchronized_iterator.reset();
		for (State q : states_orig) { push_back(synchronized_iterator, aut.delta[q]); }

		while (synchronized_iterator.advance()) {
			// extract post from the synchronized_iterator iterator
			const std::vector<Iterator>& symbol_posts = synchronized_iterator.get_current();
			Symbol current_symbol = (*symbol_posts.begin())->symbol;
			// Skipping before the targets are unified keeps the macrostates reachable only over a foreign symbol
			//  from ever being constructed.
			if (restrict_to_symbols != nullptr && !haskey(*restrict_to_symbols, current_symbol)) { continue; }

			// One hash per macrostate; an existing key is left untouched, so a known macrostate is not copied.
			const auto [target_it, inserted]{subset_map->try_emplace(synchronized_iterator.unify_targets(), 0)};
			if (inserted) {
				target_it->second = result.add_state();
				if (aut.final.intersects_with(target_it->first)) { result.final.insert(target_it->second); }
				worklist.emplace_back(target_it->second, &target_it->first);
			}
			const State target_res{target_it->second};
			result.delta.mutable_state_post(state_res).insert(SymbolPost(current_symbol, target_res));
			if (inserted && macrostate_discover.has_value() &&
				!(*macrostate_discover)(result, target_res, target_it->first)) {
				return result;
			}
		}
	}
	return result;
}

Nfa mata::nfa::as_dfa(const Nfa& aut) { return aut.is_deterministic() ? aut : determinize(aut); }

Nfa mata::nfa::as_dfa(Nfa&& aut) {
	if (aut.is_deterministic()) { return std::move(aut); }
	return determinize(aut);
}

std::ostream& std::operator<<(std::ostream& os, const Nfa& nfa) {
	nfa.print_to_mata(os);
	return os;
}

void mata::nfa::Nfa::fill_alphabet(OnTheFlyAlphabet& alphabet_to_fill) const {
	for (const StatePost& state_post : this->delta) {
		for (const SymbolPost& symbol_post : state_post) {
			alphabet_to_fill.update_next_symbol_value(symbol_post.symbol);
			alphabet_to_fill.try_add_new_symbol(std::to_string(symbol_post.symbol), symbol_post.symbol);
		}
	}
}

std::shared_ptr<const mata::Alphabet> mata::nfa::Nfa::resolve_alphabet(const Alphabet* const alphabet) const {
	if (alphabet != nullptr) {
		// Non-owning: `alphabet` is borrowed from the caller (e.g., a stack-local object), so wrap it with a
		//  no-op deleter instead of an owning shared_ptr, which would erroneously delete it once this shared_ptr's
		//  refcount reaches zero.
		return std::shared_ptr<const Alphabet>(alphabet, [](const Alphabet*) {});
	}
	if (this->alphabet != nullptr) { return this->alphabet; }
	return {std::make_shared<mata::EnumAlphabet>(EnumAlphabet{delta.get_used_symbols()})};
}

OrdVector<Symbol> mata::nfa::Nfa::get_symbols_to_work_with(const Alphabet* const alphabet) const {
	return mata::nfa::get_symbols_to_work_with(*this, alphabet);
}

mata::OnTheFlyAlphabet mata::nfa::create_alphabet(const std::vector<std::reference_wrapper<const Nfa>>& nfas) {
	mata::OnTheFlyAlphabet alphabet{};
	for (const auto& nfa : nfas) { nfa.get().fill_alphabet(alphabet); }
	return alphabet;
}

mata::OnTheFlyAlphabet mata::nfa::create_alphabet(const std::vector<std::reference_wrapper<Nfa>>& nfas) {
	mata::OnTheFlyAlphabet alphabet{};
	for (const auto& nfa : nfas) { nfa.get().fill_alphabet(alphabet); }
	return alphabet;
}

mata::OnTheFlyAlphabet mata::nfa::create_alphabet(const std::vector<const Nfa*>& nfas) {
	mata::OnTheFlyAlphabet alphabet{};
	for (const Nfa* const nfa : nfas) { nfa->fill_alphabet(alphabet); }
	return alphabet;
}

mata::OnTheFlyAlphabet mata::nfa::create_alphabet(const std::vector<Nfa*>& nfas) {
	mata::OnTheFlyAlphabet alphabet{};
	for (const Nfa* const nfa : nfas) { nfa->fill_alphabet(alphabet); }
	return alphabet;
}

Run mata::nfa::encode_word(const Alphabet* alphabet, const std::vector<std::string>& input) {
	return {.word = alphabet->translate_word(input)};
}

namespace {
/// Distance of a state from which no final state is reachable.
constexpr size_t NO_FINAL_REACHABLE{std::numeric_limits<size_t>::max()};

/// How far every state is from acceptance, which lets the word search skip branches that cannot be accepted.
struct FinalDistances {
	/// The length of a shortest word leading from each state to a final state, or @c NO_FINAL_REACHABLE.
	std::vector<size_t> distances;
	/// Whether each state has a transition to a state from which a final state is reachable.
	std::vector<bool> extendable;
};

FinalDistances get_final_distances(const mata::nfa::Nfa& nfa) {
	const size_t num_of_states{nfa.num_of_states()};
	std::vector<std::vector<mata::nfa::State>> predecessors(num_of_states);
	for (mata::nfa::State source{0}; source < num_of_states; ++source) {
		for (const mata::nfa::SymbolPost& symbol_post : nfa.delta[source]) {
			for (const mata::nfa::State target : symbol_post.targets) { predecessors[target].push_back(source); }
		}
	}

	FinalDistances result{
		.distances = std::vector<size_t>(num_of_states, NO_FINAL_REACHABLE),
		.extendable = std::vector<bool>(num_of_states, false)
	};
	std::deque<mata::nfa::State> queue;
	for (const mata::nfa::State final_state : nfa.final) {
		result.distances[final_state] = 0;
		queue.push_back(final_state);
	}
	while (!queue.empty()) {
		const mata::nfa::State state{queue.front()};
		queue.pop_front();
		for (const mata::nfa::State predecessor : predecessors[state]) {
			// Every state reaching a final state is dequeued once, so this marks every extendable state.
			result.extendable[predecessor] = true;
			if (result.distances[predecessor] != NO_FINAL_REACHABLE) { continue; }
			result.distances[predecessor] = result.distances[state] + 1;
			queue.push_back(predecessor);
		}
	}
	return result;
}

/// Collect the state posts of every state of @p macrostate into @p sync_iterator, which then yields the symbols
///  enabled in @p macrostate in an increasing order, each together with the states it leads to.
void sync_macrostate_posts(
	const mata::nfa::Nfa& nfa,
	const mata::nfa::StateSet& macrostate,
	mata::nfa::SynchronizedExistentialSymbolPostIterator& sync_iterator
) {
	sync_iterator.reset(macrostate.size());
	for (const mata::nfa::State state : macrostate) { mata::utils::push_back(sync_iterator, nfa.delta[state]); }
}

/// The search behind @c Nfa::get_words() and @c Nfa::get_words_lazy(). A search node is a macrostate (the set of
///  states reachable by the current word), determinized on the fly, so every word is visited once however many
///  accepting paths spell it, and states from which no final state is reachable are dropped from it. The search
///  is depth-first in lexicographic order over one shared word, with one synchronized iterator per depth.
/// With a @p length_bound, the search visits only the words that can still be completed to an accepted word of at
///  most that length and reports only the accepted words of exactly that length, recording whether it cut off any
///  longer word: a caller raising the bound one by one gets every word in shortlex order and knows when to stop.
class WordSearch {
  public:
	WordSearch(
		const mata::nfa::Nfa& nfa,
		const FinalDistances& final_distances,
		const size_t max_length,
		const std::optional<size_t> length_bound
	)
		: nfa_{nfa},
		  final_distances_{final_distances},
		  max_length_{max_length},
		  length_bound_{length_bound} {}

	/// The next accepted word, or @c nullptr once the search is exhausted. Valid until the next call.
	const mata::Word* next() {
		if (!started_) {
			started_ = true;
			if (keep_live_states(mata::nfa::StateSet(nfa_.initial), 0) && enter()) { return &word_; }
		}
		while (num_of_frames_ > 0) {
			mata::nfa::SynchronizedExistentialSymbolPostIterator& sync_iterator{sync_iterators_[num_of_frames_ - 1]};
			word_.resize(num_of_frames_ - 1);
			if (!sync_iterator.advance()) {
				--num_of_frames_;
				continue;
			}
			if (!keep_live_states(sync_iterator.unify_targets(), num_of_frames_)) { continue; }
			word_.push_back(sync_iterator.get_current_minimum()->symbol);
			if (enter()) { return &word_; }
		}
		return nullptr;
	}

	/// Whether a word longer than the length bound was cut off, so a larger bound can find more words.
	bool cut_off() const { return cut_off_; }

  private:
	/// Keep in @c macrostate_ the states of @p states that can still lead to an accepted word within the length
	///  bound after a word of @p length. Return whether any is left.
	bool keep_live_states(const mata::nfa::StateSet& states, const size_t length) {
		macrostate_.clear();
		for (const mata::nfa::State state : states) {
			const size_t distance{final_distances_.distances[state]};
			if (distance == NO_FINAL_REACHABLE) { continue; }
			if (length_bound_.has_value() && distance > length_bound_.value() - length) {
				cut_off_ = true;
				continue;
			}
			macrostate_.push_back(state);
		}
		return !macrostate_.empty();
	}

	/// Enter the search node of @c word_ and @c macrostate_. Return whether @c word_ is to be reported.
	bool enter() {
		const size_t length{word_.size()};
		if (length < max_length_) {
			if (length_bound_.has_value() && length == length_bound_.value()) {
				// Every successor is longer than the bound; just record whether there is one.
				for (const mata::nfa::State state : macrostate_) {
					if (final_distances_.extendable[state]) { cut_off_ = true; }
				}
			} else {
				// The deque keeps the references of the shallower frames valid while the deeper frames are appended.
				if (sync_iterators_.size() <= num_of_frames_) { sync_iterators_.emplace_back(); }
				sync_macrostate_posts(nfa_, macrostate_, sync_iterators_[num_of_frames_]);
				++num_of_frames_;
			}
		}
		return nfa_.final.intersects_with(macrostate_) &&
			   (!length_bound_.has_value() || length == length_bound_.value());
	}

	const mata::nfa::Nfa& nfa_;
	const FinalDistances& final_distances_;
	const size_t max_length_;
	const std::optional<size_t> length_bound_;
	/// The word of the current search node; one symbol per transition, so its length is the depth of the node.
	mata::Word word_{};
	/// The macrostate of the node being entered.
	mata::nfa::StateSet macrostate_{};
	/// One synchronized iterator per depth, reused across sibling branches; the first @c num_of_frames_ are active.
	std::deque<mata::nfa::SynchronizedExistentialSymbolPostIterator> sync_iterators_{};
	size_t num_of_frames_{0};
	bool started_{false};
	bool cut_off_{false};
};
} // namespace

std::set<mata::Word> mata::nfa::Nfa::get_words(const size_t max_length) const {
	std::set<mata::Word> result;
	const FinalDistances final_distances{get_final_distances(*this)};
	WordSearch search{*this, final_distances, max_length, std::nullopt};
	while (const Word* word{search.next()}) { result.insert(*word); }
	return result;
}

#if MATA_HAS_GENERATOR_SUPPORT
std::generator<mata::Word> mata::nfa::Nfa::get_words_lazy(const std::optional<size_t> max_length) const {
	// A plain depth-first search could descend into an infinite branch forever and never yield the words beside it,
	//  so the words are searched by increasing length (iterative deepening), each length in its own bounded search.
	const size_t length_max{max_length.value_or(std::numeric_limits<size_t>::max())};
	const FinalDistances final_distances{get_final_distances(*this)};
	for (size_t length{0};; ++length) {
		WordSearch search{*this, final_distances, length_max, length};
		while (const Word* word{search.next()}) { co_yield *word; }
		if (!search.cut_off() || length >= length_max) { co_return; }
	}
}
#endif // MATA_HAS_GENERATOR_SUPPORT

OrdVector<Symbol> mata::nfa::get_symbols_to_work_with(const Nfa& nfa, const mata::Alphabet* const shared_alphabet) {
	return nfa.resolve_alphabet(shared_alphabet)->get_alphabet_symbols();
}

std::optional<mata::Word> Nfa::get_word(const std::optional<Symbol> first_epsilon) const {
	if (initial.empty() || final.empty()) { return std::nullopt; }
	if (initial.intersects_with(final)) { return Word{}; }

	/// Current state state post iterator, its end iterator, and iterator in the current symbol post to target states.
	std::vector<std::tuple<StatePost::const_iterator, StatePost::const_iterator, SmallStateSet::const_iterator>>
		worklist{};
	std::vector<bool> searched(num_of_states(), false);
	bool final_found{};
	for (const State initial_state : initial) {
		if (searched[initial_state]) { continue; }
		searched[initial_state] = true;

		const StatePost& initial_state_post{delta[initial_state]};
		auto initial_symbol_post_it{initial_state_post.cbegin()};
		auto initial_symbol_post_end{initial_state_post.cend()};
		// Initialize the worklist for the given @p initial_state with the first valid transition.
		while (worklist.empty() && initial_symbol_post_it != initial_state_post.cend()) {
			auto initial_target_it{initial_symbol_post_it->targets.cbegin()};
			do {
				if (searched[*initial_target_it]) {
					++initial_target_it;
				} else {
					worklist.emplace_back(initial_symbol_post_it, initial_symbol_post_end, initial_target_it);
					break;
				}
			} while (initial_target_it != initial_symbol_post_it->targets.cend());
			if (initial_target_it == initial_symbol_post_it->targets.cend()) { ++initial_symbol_post_it; }
		}

		// Try to recursively walk through the NFA transitions from the current @p initial_state until a final state is
		//  encountered or all states reachable from @p initial_state are searched without finding any final state.
		while (!worklist.empty()) {
			// Using references to iterators to be able to increment the top-most element in the worklist in place.
			auto& [state_post_it, state_post_end, targets_it]{worklist.back()};
			if (state_post_it != state_post_end) {
				if (targets_it != state_post_it->targets.cend()) {
					if (searched[*targets_it]) {
						++targets_it;
						continue;
					}
					if (final.contains(*targets_it)) {
						final_found = true;
						break;
					}
					searched[*targets_it] = true;
					const StatePost& state_post{delta[*targets_it]};
					if (!state_post.empty()) {
						auto new_state_post_it{state_post.cbegin()};
						auto new_targets_it{new_state_post_it->cbegin()};
						worklist.emplace_back(new_state_post_it, state_post.cend(), new_targets_it);
					} else {
						++targets_it;
					}
				} else { // targets_it == state_post_it->targets.cend().
					++state_post_it;
					if (state_post_it != state_post_end) { targets_it = state_post_it->cbegin(); }
				}
			} else { // state_post_it == state_post_end.
				worklist.pop_back();
				if (!worklist.empty()) {
					auto& [_prev_state_post_it, _prev_state_post_end, prev_targets_it]{worklist.back()};
					++prev_targets_it;
				}
			}
		}
		if (final_found) { break; }
	}

	if (!final_found) { return std::nullopt; }
	Word word;
	word.reserve(worklist.size());
	for (const auto& [symbol_post_it, symbol_post_end, _targets_it] : worklist) {
		if (!first_epsilon.has_value() || symbol_post_it->symbol < first_epsilon) {
			word.push_back(symbol_post_it->symbol);
		}
	}
	return word;
}

std::optional<mata::Word> Nfa::get_shortest_word(const std::optional<Symbol> first_epsilon) const {
	if (initial.empty() || final.empty()) { return std::nullopt; }

	// Predecessor state and the symbol used to reach the current state.
	struct Predecessor {
		mata::nfa::State prev;
		mata::Symbol sym;
	};

	// Resulting word
	const size_t num_of_states = this->num_of_states();

	std::queue<mata::nfa::State> worklist;
	std::vector<int> dist(num_of_states, -1);
	std::vector<Predecessor> pred(num_of_states);

	// Initialize BFS from all initial states
	for (mata::nfa::State init_state : this->initial) {
		dist[init_state] = 0;
		worklist.push(init_state);

		if (this->final.contains(init_state)) {
			return mata::Word{}; // empty word accepted
		}
	}

	// Standard BFS loop
	while (!worklist.empty()) {
		// Get the next state in the queue and remove it
		mata::nfa::State curr_state = worklist.front();
		worklist.pop();

		// Calculate the distance for any newly discovered states from this node
		int next_dist = dist[curr_state] + 1;

		// Iterate outgoing transitions
		for (const auto& sym_post : this->delta[curr_state]) {
			mata::Symbol transition_sym = sym_post.symbol;

			// Iterate over all destination states for this specific symbol
			for (mata::nfa::State target_state : sym_post.targets) {
				// Skip if the state has already been visited
				if (dist[target_state] != -1) { continue; }

				dist[target_state] = next_dist; // Mark state as visited
				pred[target_state] = {curr_state, transition_sym}; // Store made transition

				// Check if the newly reached state is an accepting state
				if (this->final.contains(target_state)) {
					mata::Word res;
					// reconstruct shortest word
					for (mata::nfa::State backtrack_state = target_state; dist[backtrack_state] > 0;
						 backtrack_state = pred[backtrack_state].prev) {
						Symbol next_symbol = pred[backtrack_state].sym;
						if (!first_epsilon.has_value() || next_symbol < first_epsilon) { res.push_back(next_symbol); }
					}
					// Reverse the backtracked word
					std::ranges::reverse(res);
					return res;
				}

				// If 'target_state' is not a final state, push it to the queue to explore its transitions later
				worklist.push(target_state);
			}
		}
	}

	return std::nullopt; // language empty
}

std::optional<mata::Word> Nfa::get_word_from_complement(const Alphabet* alphabet) const {
	if (are_disjoint(initial, final)) { return Word{}; }

	std::vector<std::unordered_map<StateSet, State>::const_pointer> worklist{};
	std::unordered_map<StateSet, State> subset_map{};

	Nfa nfa_complete{};
	const State sink_state{nfa_complete.add_state()};
	nfa_complete.final.insert(sink_state);
	const State new_initial{nfa_complete.add_state()};
	nfa_complete.initial.insert(new_initial);
	auto subset_map_it{subset_map.emplace(initial, new_initial).first};
	worklist.emplace_back(subset_map_it.operator->());

	using Iterator = mata::utils::OrdVector<SymbolPost>::const_iterator;
	SynchronizedExistentialSymbolPostIterator synchronized_iterator{};

	const utils::OrdVector<Symbol> symbols{get_symbols_to_work_with(alphabet)};
	const auto symbols_end{symbols.end()};
	bool continue_complementation{true};
	while (continue_complementation && !worklist.empty()) {
		const auto curr_state_set_it{worklist.back()};
		const State macrostate{curr_state_set_it->second};
		const StateSet& curr_state_set{curr_state_set_it->first};
		worklist.pop_back();

		synchronized_iterator.reset();
		for (const State orig_state : curr_state_set) {
			mata::utils::push_back(synchronized_iterator, delta[orig_state]);
		}
		bool sync_it_advanced{synchronized_iterator.advance()};
		auto symbols_it{symbols.begin()};
		while (sync_it_advanced || symbols_it != symbols_end) {
			if (!sync_it_advanced) {
				MATA_ASSERT(symbols_it != symbols_end);
				// There are no more transitions from the 'orig_states' but there is a symbol from the 'symbols'. Make
				//  the complemented NFA complete by adding a transition to a sink state. We can now return the access
				//  word for the sink state.
				nfa_complete.delta.add(macrostate, *symbols_it, sink_state);
				continue_complementation = false;
				break;
			}
			MATA_ASSERT(sync_it_advanced);
			const std::vector<Iterator>& orig_symbol_posts{synchronized_iterator.get_current()};
			const Symbol symbol_advanced_to{(*orig_symbol_posts.begin())->symbol};
			StateSet orig_targets{synchronized_iterator.unify_targets()};

			if (symbols_it != symbols_end && symbol_advanced_to == *symbols_it) {
				State target_macrostate;
				// Continue with the determinization of the NFA.
				if (const auto target_macrostate_it = subset_map.find(orig_targets);
					target_macrostate_it != subset_map.end()) {
					target_macrostate = target_macrostate_it->second;
				} else {
					target_macrostate = nfa_complete.add_state();
					if (!final.intersects_with(orig_targets)) {
						nfa_complete.final.insert(target_macrostate);
						continue_complementation = false;
					}
					subset_map_it = subset_map.emplace(std::move(orig_targets), target_macrostate).first;
					worklist.emplace_back(subset_map_it.operator->());
				}
				nfa_complete.delta.add(macrostate, symbol_advanced_to, target_macrostate);
				if (!continue_complementation) { break; }
				++symbols_it;
			} else if (symbols_it == symbols_end || symbol_advanced_to < *symbols_it) {
				// The symbol on the transitions is not in the requested alphabet. The symbol does not belong to the
				//  complemented language over the alphabet either way; skip it.
			} else {
				MATA_ASSERT(symbol_advanced_to > *symbols_it);
				// There are more transitions from the 'orig_states', but there is a missing transition over
				//  '*symbols_it'. Make the complemented NFA complete by adding a transition to a sink state. We can now
				//  return the access word for the sink state.
				nfa_complete.delta.add(macrostate, *symbols_it, sink_state);
				continue_complementation = false;
				break;
			}

			sync_it_advanced = synchronized_iterator.advance();
		}
	}
	return nfa_complete.get_word();
}

Nfa mata::nfa::lang_difference(
	const Nfa& nfa_included,
	const Nfa& nfa_excluded,
	std::optional<
		std::function<bool(const Nfa&, const Nfa&, const StateSet&, const StateSet&, const State, const Nfa&)>>
		macrostate_discover
) {
	std::unordered_set<StateSet> subset_set_included{};
	std::unordered_set<StateSet> subset_set_excluded{};
	using SubsetMacrostatePair =
		std::pair<std::unordered_set<StateSet>::const_pointer, std::unordered_set<StateSet>::const_pointer>;
	using SubsetMacrostateMap = std::unordered_map<
		SubsetMacrostatePair, State,
		mata::utils::PairHash<
			std::unordered_set<StateSet>::const_pointer, std::unordered_set<StateSet>::const_pointer>>;
	SubsetMacrostateMap subset_macrostate_map{};
	std::vector<SubsetMacrostateMap::const_pointer> worklist{};

	// '{}' represents a sink state that is always final in the complement.
	const auto subset_set_excluded_sink_ptr{subset_set_excluded.insert(StateSet{}).first.operator->()};

	Nfa nfa_lang_difference{};
	const State new_initial{nfa_lang_difference.add_state()};
	nfa_lang_difference.initial.insert(new_initial);
	if (nfa_included.final.intersects_with(nfa_included.initial) &&
		!nfa_excluded.final.intersects_with(nfa_excluded.initial)) {
		nfa_lang_difference.final.insert(new_initial);
	}
	auto subset_set_included_ptr{subset_set_included.emplace(nfa_included.initial).first.operator->()};
	auto subset_set_excluded_ptr{subset_set_excluded.emplace(nfa_excluded.initial).first.operator->()};
	auto subset_macrostate_map_ptr{
		subset_macrostate_map.emplace(std::make_pair(subset_set_included_ptr, subset_set_excluded_ptr), new_initial)
			.first.operator->()
	};
	worklist.emplace_back(subset_macrostate_map_ptr);
	if (macrostate_discover.has_value() && !(*macrostate_discover)(
											   nfa_included, nfa_excluded, *subset_set_included_ptr,
											   *subset_set_excluded_ptr, new_initial, nfa_lang_difference
										   )) {
		return nfa_lang_difference;
	}

	using Iterator = mata::utils::OrdVector<SymbolPost>::const_iterator;
	SynchronizedExistentialSymbolPostIterator synchronized_iterator_included{};
	SynchronizedExistentialSymbolPostIterator synchronized_iterator_excluded{};

	while (!worklist.empty()) {
		const auto curr_macrostate_ptr{worklist.back()};
		worklist.pop_back();
		const auto curr_state_set_included_ptr{curr_macrostate_ptr->first.first};
		const auto curr_state_set_excluded_ptr{curr_macrostate_ptr->first.second};
		const StateSet& curr_state_set_included{*curr_state_set_included_ptr};
		const StateSet& curr_state_set_excluded{*curr_state_set_excluded_ptr};
		const State macrostate{curr_macrostate_ptr->second};

		synchronized_iterator_included.reset();
		synchronized_iterator_excluded.reset();
		for (const State orig_state : curr_state_set_included) {
			mata::utils::push_back(synchronized_iterator_included, nfa_included.delta[orig_state]);
		}
		for (const State orig_state : curr_state_set_excluded) {
			mata::utils::push_back(synchronized_iterator_excluded, nfa_excluded.delta[orig_state]);
		}
		bool sync_it_included_advanced{synchronized_iterator_included.advance()};
		bool sync_it_excluded_advanced{false};
		while (sync_it_included_advanced) {
			const std::vector<Iterator>& orig_symbol_posts{synchronized_iterator_included.get_current()};
			const Symbol symbol_advanced_to{(*orig_symbol_posts.begin())->symbol};
			StateSet orig_targets_included{synchronized_iterator_included.unify_targets()};
			sync_it_excluded_advanced = synchronized_iterator_excluded.synchronize_with(symbol_advanced_to);
			StateSet orig_targets_excluded{
				sync_it_excluded_advanced ? synchronized_iterator_excluded.unify_targets() : StateSet{}
			};
			const bool final_included_intersects_targets{nfa_included.final.intersects_with(orig_targets_included)};
			const bool final_excluded_intersects_targets{nfa_excluded.final.intersects_with(orig_targets_excluded)};
			subset_set_included_ptr = subset_set_included.insert(std::move(orig_targets_included)).first.operator->();
			subset_set_excluded_ptr = subset_set_excluded.insert(std::move(orig_targets_excluded)).first.operator->();
			const auto [target_macrostate_it, macrostate_inserted]{subset_macrostate_map.emplace(
				std::make_pair(subset_set_included_ptr, subset_set_excluded_ptr), nfa_lang_difference.num_of_states()
			)};
			subset_macrostate_map_ptr = target_macrostate_it.operator->();
			const State target_macrostate{target_macrostate_it->second};
			nfa_lang_difference.delta.add(macrostate, symbol_advanced_to, target_macrostate);
			if (macrostate_inserted) {
				// 'sync_it_excluded_advanced' is true iff there is a transition in the excluded NFA over the symbol
				//  'symbol_advanced_to'. If sync_it_excluded_advanced == false, the complement of the excluded NFA will
				//  have a transition over 'symbol_advanced_to' to a "sink state" which is a final state in the
				//  complement, and therefore must always be final in the language difference.
				if (final_included_intersects_targets) {
					if (subset_set_excluded_ptr == subset_set_excluded_sink_ptr ||
						(sync_it_excluded_advanced && !final_excluded_intersects_targets)) {
						nfa_lang_difference.final.insert(target_macrostate);
					}
				}
				if (macrostate_discover.has_value() &&
					!(*macrostate_discover)(
						nfa_included, nfa_excluded, *subset_set_included_ptr, *subset_set_excluded_ptr,
						target_macrostate, nfa_lang_difference
					)) {
					return nfa_lang_difference;
				}

				worklist.emplace_back(subset_macrostate_map_ptr);
			}
			sync_it_included_advanced = synchronized_iterator_included.advance();
		}
	}
	return nfa_lang_difference;
}

std::optional<mata::Word> mata::nfa::get_word_from_lang_difference(const Nfa& nfa_included, const Nfa& nfa_excluded) {
	return lang_difference(
			   nfa_included, nfa_excluded,
			   [](const Nfa& nfa_included, const Nfa& nfa_excluded, const StateSet& macrostate_included_state_set,
				  const StateSet& macrostate_excluded_state_set, const State macrostate,
				  const Nfa& nfa_lang_difference) {
				   (void) nfa_included, (void) nfa_excluded;
				   (void) macrostate_included_state_set, (void) macrostate_excluded_state_set;
				   (void) macrostate;
				   return nfa_lang_difference.final.empty();
			   }
	).get_word();
}

Nfa mata::nfa::algorithms::reduce_simulation(const Nfa& aut, StateRenaming& state_renaming) {
	const auto sim_relation =
		algorithms::compute_relation(aut, ParameterMap{{"relation", "simulation"}, {"direction", "forward"}});

	auto sim_relation_symmetric = sim_relation;
	sim_relation_symmetric.restrict_to_symmetric();
	std::vector<size_t> quot_proj;
	sim_relation_symmetric.get_quotient_projection(quot_proj);

	return quotient_by_simulation(aut, sim_relation, quot_proj, nullptr, &state_renaming);
}

Nfa mata::nfa::algorithms::quotient_by_simulation(
	const Nfa& aut,
	const Simlib::Util::BinaryRelation& sim_relation,
	const std::vector<size_t>& quot_proj,
	const BoolVector* states,
	StateRenaming* state_renaming
) {
	const size_t num_of_states = aut.num_of_states();
	const auto is_kept = [&](const State q) { return states == nullptr || (*states)[q]; };

	// Kept representative of each class, indexed by the class's smallest state quot_proj[q], which may itself be
	//  outside of the kept states.
	std::vector<State> representative(num_of_states, Limits::max_state);
	// The reduced state of each kept state.
	std::vector<State> reduced(num_of_states, Limits::max_state);

	Nfa result;
	// Number the classes in the order of their smallest kept states, as reduce_simulation() does on the part alone.
	for (State q = 0; q < num_of_states; ++q) {
		if (!is_kept(q)) { continue; }
		if (State& q_repr = representative[quot_proj[q]]; q_repr == Limits::max_state) {
			q_repr = q;
			reduced[q] = result.add_state();
		} else {
			reduced[q] = reduced[q_repr];
		}
	}

	for (State q = 0; q < num_of_states; ++q) {
		if (!is_kept(q)) { continue; }
		const State q_class_state = reduced[q];

		if (aut.initial[q]) { // if a symmetric class contains initial state, then the whole class should be initial
			result.initial.insert(q_class_state);
		}

		if (representative[quot_proj[q]] ==
			q) { // we process only transitions starting from the representative state, this is enough for simulation
			for (const auto& q_trans : aut.delta.state_post(q)) {
				const StateSet representatives_of_states_to = [&] {
					StateSet state_set;
					for (const auto s : q_trans.targets) {
						MATA_ASSERT(is_kept(s));
						state_set.insert(representative[quot_proj[s]]);
					}
					return state_set;
				}();

				// get the class states of those representatives that are not simulated by another representative in
				// representatives_of_states_to
				StateSet representatives_class_states;
				for (const State s : representatives_of_states_to) {
					bool is_state_important = true; // if true, we need to keep the transition from q to s
					for (const State p : representatives_of_states_to) {
						if (s != p && sim_relation.get(s, p)) { // if p (different from s) simulates s
							is_state_important = false; // as p simulates s, the transition from q to s is not important
														// to keep, as it is subsumed in transition from q to p
							break;
						}
					}
					if (is_state_important) { representatives_class_states.insert(reduced[s]); }
				}

				// add the transition 'q_class_state-q_trans.symbol->representatives_class_states' at the end of
				// transition list of transitions starting from q_class_state as the q_trans.symbol should be the
				// largest symbol we saw (as we iterate trough getTransitionsFromState(q) which is ordered)
				result.delta.mutable_state_post(q_class_state)
					.insert(SymbolPost(q_trans.symbol, representatives_class_states));
			}

			if (aut.final[q]) { // if q is final, then all states in its class are final => we make q_class_state final
				result.final.insert(q_class_state);
			}
		}
	}

	if (state_renaming != nullptr) {
		state_renaming->clear();
		state_renaming->reserve(num_of_states);
		for (State q = 0; q < num_of_states; ++q) {
			if (is_kept(q)) { (*state_renaming)[q] = reduced[q]; }
		}
	}
	return result;
}

Nfa mata::nfa::algorithms::reduce_residual(
	const Nfa& nfa, StateRenaming& state_renaming, const std::string& type, const std::string& direction
) {
	Nfa back_determinized = nfa;
	Nfa result;

	if (direction != "forward" && direction != "backward") {
		throw std::runtime_error(
			std::to_string(__func__) + " received an unknown value of the \"direction\" key: " + direction
		);
	}

	// forward canonical residual automaton is firstly backward determinized and
	// then the residual construction is done forward, for backward residual automaton
	// is it the opposite, so the automaton is reverted once more before and after
	// construction, however the first two reversion negate each other out
	if (direction == "forward") { back_determinized = revert(back_determinized); }
	back_determinized = revert(determinize(back_determinized)); // Backward determinization.

	// state_renaming is passed but not used: residual reduction is checked at the caller (reduce())
	// to reject any state_renaming request before performing any work.

	// two different implementations of the same algorithm, for type "after" the
	// residual automaton and removal of covering states is done after the final
	// determinization if finished, for type "with" this residual construction is
	// done during the last determinization, both types had similar results in
	// effectivity, their output is almost the same expect the transitions, those
	// may slightly differ, but number of states is the same for both types
	if (type == "with") {
		result = reduce_residual_with(back_determinized);
	} else if (type == "after") {
		result = reduce_residual_after(back_determinized);
	} else {
		throw std::runtime_error(std::to_string(__func__) + " received an unknown value of the \"type\" key: " + type);
	}

	if (direction == "backward") { result = revert(result); }

	result.trim();
	return result;
}

Nfa mata::nfa::algorithms::reduce_residual_with(const Nfa& nfa) {
	Nfa result;

	// assuming all sets targets are non-empty
	std::vector<std::pair<State, StateSet>> worklist;
	std::unordered_map<StateSet, State> subset_map;

	std::vector<StateSet> covering_states; // check covering set
	std::vector<StateSet> covering_indexes; // indexes of covering macrostates
	std::unordered_map<StateSet, State> covered; // map of covered states for transferring new transitions
	Predecessors predecessors{};

	result.clear();
	const StateSet initials_orig = StateSet(nfa.initial);
	const State initial_res = result.add_state();
	result.initial.insert(initial_res);

	if (nfa.final.intersects_with(initials_orig)) { result.final.insert(initial_res); }
	worklist.emplace_back(initial_res, initials_orig);

	(subset_map)[mata::utils::OrdVector<State>(initials_orig)] = initial_res;
	covering_states.emplace_back();
	covering_indexes.emplace_back();

	if (nfa.delta.empty()) { return result; }

	using Iterator = OrdVector<SymbolPost>::const_iterator;
	SynchronizedExistentialSymbolPostIterator synchronized_iterator;

	while (!worklist.empty()) {
		const auto [state_res, states_orig] = std::move(worklist.back());
		worklist.pop_back();
		if (states_orig.empty()) {
			// This should not happen assuming all sets targets are non-empty.
			break;
		}

		// add moves of S to the sync ex iterator
		synchronized_iterator.reset();
		for (State q : states_orig) { mata::utils::push_back(synchronized_iterator, nfa.delta[q]); }

		while (synchronized_iterator.advance()) {
			bool add = false; // check whether to add transitions

			// extract post from the synchronized_iterator iterator
			const std::vector<Iterator>& moves = synchronized_iterator.get_current();
			Symbol current_symbol = (*moves.begin())->symbol;
			StateSet targets = synchronized_iterator.unify_targets(); // new state unify

			auto existing_target_res = subset_map.find(targets); // check if state was already discovered
			State target_res;
			if (existing_target_res != subset_map.end()) { // already visited state
				target_res = existing_target_res->second;
				add = true;
			} else if ((existing_target_res = covered.find(targets)) != covered.end()) {
				target_res = existing_target_res->second;
			} else { // add new state
				target_res = result.add_state();
				check_covered_and_covering(
					covering_states, covering_indexes, covered, subset_map, target_res, targets, result, predecessors
				);

				if (targets != covering_states[target_res]) { // new state is not covered, replace transitions
					subset_map[mata::utils::OrdVector<State>(targets)] = target_res; // add to map

					if (nfa.final.intersects_with(targets)) { // add to final
						result.final.insert(target_res);
					}

					worklist.emplace_back(target_res, targets);
					add = true;

				} else { // new state is covered
					covered[mata::utils::OrdVector<State>(targets)] = target_res;
				}
			}

			if (covered.contains(states_orig)) {
				continue; // skip generating any transitions as the source state was covered right now
			}

			if (add) {
				result.delta.mutable_state_post(state_res).insert(SymbolPost(current_symbol, target_res));
				predecessors.record(state_res, current_symbol, target_res);
			} else {
				for (State switch_target : covering_indexes[target_res]) {
					predecessors.add(result, state_res, current_symbol, switch_target);
				}
			}
		}
	}

	return result;
}

Nfa mata::nfa::algorithms::reduce_residual_after(const Nfa& nfa) {
	std::unordered_map<StateSet, State> subset_map{};
	Nfa result;
	result = determinize(nfa, &subset_map);

	// Ordered by size from the largest; equal sizes keep the iteration order of the map.
	std::vector<std::pair<const StateSet*, State>> macrostates{};
	macrostates.reserve(subset_map.size());
	for (const auto& [state_set, state] : subset_map) { macrostates.emplace_back(&state_set, state); }
	std::ranges::stable_sort(macrostates, std::ranges::greater{}, [](const auto& macrostate) {
		return macrostate.first->size();
	});
	std::vector<const StateSet*> macrostate_sets{};
	std::vector<State> result_states{};
	macrostate_sets.reserve(macrostates.size());
	result_states.reserve(macrostates.size());
	for (const auto& [state_set, state] : macrostates) {
		macrostate_sets.push_back(state_set);
		result_states.push_back(state);
	}

	// Coverage depends only on the macrostates; redirect all transitions in one pass at the end.
	redirect_covered_states(
		ResidualCoverage{std::move(macrostate_sets), std::move(result_states), nfa.num_of_states()}.run(), result
	);
	return result;
}
