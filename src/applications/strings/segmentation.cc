/* nfa-segmentation.cc -- Segmentation of NFAs
 */

#include "mata/applications/strings.hh"
#include "mata/utils/assert.hh"

using namespace mata::nfa;
using namespace mata::applications::strings;

void seg_nfa::Segmentation::process_state_depth_pair(
	const StateDepthTuple& state_depth_pair, std::deque<StateDepthTuple>& worklist
) {
	for (auto outgoing_post{automaton_.delta[state_depth_pair.state]};
		 const SymbolPost& outgoing_move : outgoing_post) {
		if (this->epsilons_.contains(outgoing_move.symbol)) {
			handle_epsilon_transitions(state_depth_pair, outgoing_move, worklist);
		} else { // Handle other transitions.
			add_transitions_to_worklist(state_depth_pair, outgoing_move, worklist);
		}
	}
}

void seg_nfa::Segmentation::handle_epsilon_transitions(
	const StateDepthTuple& state_depth_pair, const SymbolPost& move, std::deque<StateDepthTuple>& worklist
) {
	const EpsilonDepth depth{state_depth_pair.depth};
	if (depth >= epsilon_transitions_by_depth_.size()) { epsilon_transitions_by_depth_.resize(depth + 1); }
	std::vector<Transition>& depth_transitions{epsilon_transitions_by_depth_[depth]};
	// Each state is expanded once and all its ε-moves are handled in a row, so its ε-transitions are contiguous.
	EpsilonTransitionSpan& span{epsilon_transitions_of_state_[state_depth_pair.state]};
	if (span.begin == span.end) {
		span = {.depth = depth, .begin = depth_transitions.size(), .end = depth_transitions.size()};
	}
	MATA_ASSERT(span.depth == depth);
	MATA_ASSERT(span.end == depth_transitions.size());

	std::map<Symbol, unsigned> visited_eps_aux(state_depth_pair.eps);
	visited_eps_aux[move.symbol]++;

	for (const State target_state : move.targets) {
		depth_transitions.emplace_back(state_depth_pair.state, move.symbol, target_state);
		worklist.push_back({.state = target_state, .depth = depth + 1, .eps = visited_eps_aux});
		this->visited_eps_[target_state] = visited_eps_aux;
	}
	span.end = depth_transitions.size();
}

void seg_nfa::Segmentation::add_transitions_to_worklist(
	const StateDepthTuple& state_depth_pair, const SymbolPost& move, std::deque<StateDepthTuple>& worklist
) {
	for (const State target_state : move.targets) {
		worklist.push_back({.state = target_state, .depth = state_depth_pair.depth, .eps = state_depth_pair.eps});
		this->visited_eps_[target_state] = state_depth_pair.eps;
	}
}

std::deque<seg_nfa::Segmentation::StateDepthTuple> seg_nfa::Segmentation::initialize_worklist() const {
	std::deque<StateDepthTuple> worklist{};
	std::map<Symbol, unsigned> def_eps_map{};
	for (const Symbol& e : this->epsilons_) { def_eps_map[e] = 0; }

	for (const State state : automaton_.initial) { worklist.push_back(StateDepthTuple{state, 0, def_eps_map}); }
	return worklist;
}

std::unordered_map<State, bool> seg_nfa::Segmentation::initialize_visited_map() const {
	std::unordered_map<State, bool> visited{};
	const size_t state_num = automaton_.num_of_states();
	for (State state{0}; state < state_num; ++state) { visited[state] = false; }
	return visited;
}

void seg_nfa::Segmentation::split_aut_into_segments() {
	const size_t num_of_segments{epsilon_transitions_by_depth_.size() + 1};
	segments_raw_.clear();
	segments_raw_.reserve(num_of_segments);
	BoolVector reached(automaton_.num_of_states(), false);
	std::vector<State> worklist{};
	for (EpsilonDepth depth{0}; depth < num_of_segments; ++depth) {
		utils::SparseSet<State> initial{};
		if (depth == 0) {
			initial = automaton_.initial;
		} else {
			initial = utils::SparseSet<State>(static_cast<State>(automaton_.initial.domain_size()));
			for (const Transition& transition : epsilon_transitions_by_depth_[depth - 1]) {
				initial.insert(transition.target);
			}
		}
		utils::SparseSet<State> final{};
		if (depth + 1 == num_of_segments) {
			final = automaton_.final;
		} else {
			final = utils::SparseSet<State>(static_cast<State>(automaton_.final.domain_size()));
			for (const Transition& transition : epsilon_transitions_by_depth_[depth]) {
				final.insert(transition.source);
			}
		}

		// Copy only the part reachable from the segment's initial states, without the ε-transitions of this depth
		//  (they lead to the next segment). States out of reach keep empty posts, so state ids stay those of the
		//  automaton.
		Delta delta(automaton_.delta.num_of_states());
		std::ranges::fill(reached, false);
		for (const State state : initial) {
			if (!reached[state]) {
				reached[state] = true;
				worklist.push_back(state);
			}
		}
		while (!worklist.empty()) {
			const State state{worklist.back()};
			worklist.pop_back();
			const StatePost& state_post{automaton_.delta[state]};
			if (state_post.empty()) { continue; }
			StatePost& segment_state_post{delta.mutable_state_post(state)};
			// All ε-transitions of a state have the same depth.
			if (!get_epsilon_transitions(state).empty() && epsilon_transitions_of_state_[state].depth == depth) {
				segment_state_post.reserve(state_post.size());
				for (const SymbolPost& symbol_post : state_post) {
					if (!epsilons_.contains(symbol_post.symbol)) { segment_state_post.push_back(symbol_post); }
				}
			} else {
				segment_state_post = state_post;
			}
			for (const SymbolPost& symbol_post : segment_state_post) {
				for (const State target : symbol_post.targets) {
					if (!reached[target]) {
						reached[target] = true;
						worklist.push_back(target);
					}
				}
			}
		}
		Nfa& segment{
			segments_raw_.emplace_back(std::move(delta), std::move(initial), std::move(final), automaton_.alphabet)
		};
		segment.attributes = automaton_.attributes;
	}
}

const std::vector<Nfa>& seg_nfa::Segmentation::get_segments() {
	if (segments_.empty()) {
		get_untrimmed_segments();
		segments_.reserve(segments_raw_.size());
		for (const auto& seg_aut : segments_raw_) { segments_.push_back(nfa::trim(seg_aut)); }
	}

	return segments_;
}

const std::vector<Nfa>& seg_nfa::Segmentation::get_untrimmed_segments() {
	if (segments_raw_.empty()) { split_aut_into_segments(); }

	return segments_raw_;
}

void seg_nfa::Segmentation::compute_epsilon_depths() {
	epsilon_transitions_of_state_.resize(automaton_.num_of_states());
	std::unordered_map<State, bool> visited{initialize_visited_map()};
	std::deque<StateDepthTuple> worklist{initialize_worklist()};

	while (!worklist.empty()) {
		StateDepthTuple state_depth_pair{worklist.front()};
		worklist.pop_front();

		if (!visited[state_depth_pair.state]) {
			visited[state_depth_pair.state] = true;
			process_state_depth_pair(state_depth_pair, worklist);
		}
	}
}

std::span<const Transition> seg_nfa::Segmentation::get_epsilon_transitions(const State state) const {
	if (state >= epsilon_transitions_of_state_.size()) { return {}; }
	const EpsilonTransitionSpan& span{epsilon_transitions_of_state_[state]};
	if (span.begin == span.end) { return {}; }
	return std::span{epsilon_transitions_by_depth_[span.depth]}.subspan(span.begin, span.end - span.begin);
}

const seg_nfa::Segmentation::EpsilonDepthTransitions& seg_nfa::Segmentation::get_epsilon_depths() const {
	return epsilon_depth_transitions_.get([this](EpsilonDepthTransitions& depths) {
		for (EpsilonDepth depth{0}; depth < epsilon_transitions_by_depth_.size(); ++depth) {
			depths.emplace(depth, epsilon_transitions_by_depth_[depth]);
		}
	});
}

const seg_nfa::Segmentation::EpsilonDepthTransitionMap& seg_nfa::Segmentation::get_epsilon_depth_trans_map() const {
	return eps_depth_trans_map_.get([this](EpsilonDepthTransitionMap& map) {
		for (State state{0}; state < epsilon_transitions_of_state_.size(); ++state) {
			const std::span<const Transition> transitions{get_epsilon_transitions(state)};
			if (transitions.empty()) { continue; }
			map[epsilon_transitions_of_state_[state].depth].emplace(
				state, std::vector<Transition>{transitions.begin(), transitions.end()}
			);
		}
	});
}
