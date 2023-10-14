/* nfa-segmentation.cc -- Segmentation of NFAs
 */

#include "mata/nfa/strings.hh"

using namespace mata::nfa;
using namespace mata::strings;

void seg_nfa::Segmentation::process_state_depth_pair(const StateDepthTuple& state_depth_pair,
                                                     std::deque<StateDepthTuple>& worklist) {
    auto outgoing_post{ automaton.delta[state_depth_pair.state] };
    for (const SymbolPost& outgoing_move: outgoing_post) {
        if (this->epsilons.find(outgoing_move.symbol) != this->epsilons.end()) {
            handle_epsilon_transitions(state_depth_pair, outgoing_move, worklist);
        } else { // Handle other transitions.
            add_transitions_to_worklist(state_depth_pair, outgoing_move, worklist);
        }
    }
}

void seg_nfa::Segmentation::handle_epsilon_transitions(const StateDepthTuple& state_depth_pair,
                                                       const SymbolPost& move,
                                                       std::deque<StateDepthTuple>& worklist)
{
    /// TODO: Maybe we don't need to keep the transitions in both structures
    this->epsilon_depth_transitions.insert(std::make_pair(state_depth_pair.depth, std::vector<Transition>{}));
    this->eps_depth_trans_map.insert({ state_depth_pair.depth, {{state_depth_pair.state, std::vector<Transition>{}}} });

    std::map<Symbol, unsigned> visited_eps_aux(state_depth_pair.eps);
    visited_eps_aux[move.symbol]++;

    for (State target_state: move.targets)
    {
        // TODO: Use vector indexed by depths instead of a map.
        this->epsilon_depth_transitions[state_depth_pair.depth].emplace_back(
                state_depth_pair.state, move.symbol, target_state
        );
        this->eps_depth_trans_map[state_depth_pair.depth][state_depth_pair.state].emplace_back(
             state_depth_pair.state, move.symbol, target_state
        );
        worklist.push_back({ target_state, state_depth_pair.depth + 1, visited_eps_aux });
        this->visited_eps[target_state] = visited_eps_aux;
    }
}

void seg_nfa::Segmentation::add_transitions_to_worklist(const StateDepthTuple& state_depth_pair,
                                                        const SymbolPost& move,
                                                        std::deque<StateDepthTuple>& worklist)
{
    for (State target_state: move.targets)
    {
        worklist.push_back(StateDepthTuple{ target_state, state_depth_pair.depth, state_depth_pair.eps });
        this->visited_eps[target_state] = state_depth_pair.eps;
    }
}

std::deque<seg_nfa::Segmentation::StateDepthTuple> seg_nfa::Segmentation::initialize_worklist() const
{
    std::deque<StateDepthTuple> worklist{};
    std::map<Symbol, unsigned> def_eps_map{};
    for(const Symbol& e : this->epsilons) {
        def_eps_map[e] = 0;
    }

    for (State state: automaton.initial)
    {
        worklist.push_back(StateDepthTuple{ state, 0, def_eps_map });
    }
    return worklist;
}

std::unordered_map<State, bool> seg_nfa::Segmentation::initialize_visited_map() const
{
    std::unordered_map<State, bool> visited{};
    const size_t state_num = automaton.num_of_states();
    for (State state{ 0 }; state < state_num; ++state)
    {
        visited[state] = false;
    }
    return visited;
}

void seg_nfa::Segmentation::split_aut_into_segments()
{
    segments_raw = { epsilon_depth_transitions.size() + 1, automaton };
    remove_inner_initial_and_final_states();

    // Construct segment automata.
    std::unique_ptr<const std::vector<Transition>> depth_transitions{};
    for (size_t depth{ 0 }; depth < epsilon_depth_transitions.size(); ++depth)
    {
        // Split the left segment from automaton into a new segment.
        depth_transitions = std::make_unique<const std::vector<Transition>>(epsilon_depth_transitions[depth]);
        for (const auto& transition: *depth_transitions)
        {
            update_current_segment(depth, transition);
            update_next_segment(depth, transition);
        }
    }
}

void seg_nfa::Segmentation::remove_inner_initial_and_final_states() {
    const auto segments_begin{ segments_raw.begin() };
    const auto segments_end{ segments_raw.end() };
    for (auto iter{ segments_begin }; iter != segments_end; ++iter) {
        if (iter != segments_begin) {
            iter->initial.clear();
        }
        if (iter + 1 != segments_end) {
            iter->final.clear();
        }
    }
}

void seg_nfa::Segmentation::update_current_segment(const size_t current_depth, const Transition& transition)
{
    assert(this->epsilons.find(transition.symbol) != this->epsilons.end());
    assert(segments_raw[current_depth].delta.contains(transition.source, transition.symbol, transition.target));

    segments_raw[current_depth].final.insert(transition.source);
    // we need to remove this transition so that the language of the current segment does not accept too much
    segments_raw[current_depth].delta.remove(transition);
}

void seg_nfa::Segmentation::update_next_segment(const size_t current_depth, const Transition& transition)
{
    const size_t next_depth = current_depth + 1;

    assert(this->epsilons.find(transition.symbol) != this->epsilons.end());
    assert(segments_raw[next_depth].delta.contains(transition.source, transition.symbol, transition.target));

    // we do not need to remove epsilon transitions in current_depth from the next segment (or the
    // segments after) as the initial states are after these transitions
    segments_raw[next_depth].initial.insert(transition.target);
}

const std::vector<Nfa>& seg_nfa::Segmentation::get_segments()
{
    if (segments.empty()) {
        get_untrimmed_segments();
        for (auto& seg_aut: segments_raw) { segments.push_back(nfa::Nfa{ seg_aut }.trim()); }
    }

    return segments;
}

const std::vector<Nfa>& seg_nfa::Segmentation::get_untrimmed_segments()
{
    if (segments_raw.empty()) { split_aut_into_segments(); }

    return segments_raw;
}

void seg_nfa::Segmentation::compute_epsilon_depths()
{
    std::unordered_map<State, bool> visited{ initialize_visited_map() };
    std::deque<StateDepthTuple> worklist{ initialize_worklist() };

    while (!worklist.empty())
    {
        StateDepthTuple state_depth_pair{ worklist.front() };
        worklist.pop_front();

        if (!visited[state_depth_pair.state])
        {
            visited[state_depth_pair.state] = true;
            process_state_depth_pair(state_depth_pair, worklist);
        }
    }
}
