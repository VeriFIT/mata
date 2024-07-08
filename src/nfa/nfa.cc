/* nfa.cc -- operations for NFA
 */

#include <algorithm>
#include <list>
#include <optional>
#include <iterator>

// MATA headers
#include "mata/utils/sparse-set.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nfa/algorithms.hh"
#include <mata/simlib/explicit_lts.hh>

using namespace mata::utils;
using namespace mata::nfa;
using mata::Symbol;
using mata::Word;
using mata::BoolVector;

using StateBoolArray = std::vector<bool>; ///< Bool array for states in the automaton.

const std::string mata::nfa::TYPE_NFA = "NFA";

const State Limits::min_state;
const State Limits::max_state;
const Symbol Limits::min_symbol;
const Symbol Limits::max_symbol;

namespace {
    /**
     * Compute reachability of states considering only specified states.
     *
     * @param[in] nfa NFA to compute reachability for.
     * @param[in] states_to_consider State to consider as potentially reachable. If @c std::nullopt is used, all states
     *  are considered as potentially reachable.
     * @return Bool array for reachable states (from initial states): true for reachable, false for unreachable states.
     */
    StateBoolArray reachable_states(const Nfa& nfa,
                                       const std::optional<const StateBoolArray>& states_to_consider = std::nullopt) {
        std::vector<State> worklist{};
        StateBoolArray reachable(nfa.num_of_states(), false);
        for (const State state: nfa.initial) {
            if (!states_to_consider.has_value() || states_to_consider.value()[state]) {
                worklist.push_back(state);
                reachable.at(state) = true;
            }
        }

        State state;
        while (!worklist.empty()) {
            state = worklist.back();
            worklist.pop_back();
            for (const SymbolPost& move: nfa.delta[state]) {
                for (const State target_state: move.targets) {
                    if (!reachable[target_state] &&
                        (!states_to_consider.has_value() || states_to_consider.value()[target_state])) {
                        worklist.push_back(target_state);
                        reachable[target_state] = true;
                    }
                }
            }
        }
        return reachable;
    }
}

void Nfa::remove_epsilon(const Symbol epsilon)
{
    *this = mata::nfa::remove_epsilon(*this, epsilon);
}

StateSet Nfa::get_reachable_states() const {
    StateBoolArray reachable_bool_array{ reachable_states(*this) };

    StateSet reachable_states{};
    const size_t num_of_states{ this->num_of_states() };
    for (State original_state{ 0 }; original_state < num_of_states; ++original_state)
    {
        if (reachable_bool_array[original_state])
        {
            reachable_states.insert(original_state);
        }
    }

    return reachable_states;
}

StateSet Nfa::get_terminating_states() const
{
    return revert(*this).get_reachable_states();
}

std::vector<State> Nfa::distances_from_initial() const {
    std::vector<State> distances(num_of_states()+1, Limits::max_state);
    BoolVector visited(num_of_states()+1, false);
    std::deque<State> que;

    for (State qi: initial) {
        visited[qi] = true;
        distances[qi] = 0;
        que.push_back(qi);
    }

    while (!que.empty()) {
        State src = que.front();
        que.pop_front();
        for (Move move : delta[src].moves()) {
            if (!visited[move.target]) {
                visited[move.target] = true;
                distances[move.target] = distances[src] +1;
                que.push_back(move.target);
            }
        }
    }

    return distances;
}

Nfa& Nfa::trim(StateRenaming* state_renaming) {
#ifdef _STATIC_STRUCTURES_
    BoolVector useful_states{ useful_states() };
    useful_states.clear();
    useful_states = useful_states();
#else
    BoolVector useful_states{ get_useful_states() };
#endif
    const size_t useful_states_size{ useful_states.size() };
    std::vector<State> renaming(useful_states_size);
    for(State new_state{ 0 }, orig_state{ 0 }; orig_state < useful_states_size; ++orig_state) {
        if (useful_states[orig_state]) {
            renaming[orig_state] = new_state;
            ++new_state;
        }
    }

    delta.defragment(useful_states, renaming);

    auto is_state_useful = [&](State q){return q < useful_states.size() && useful_states[q];};
    initial.filter(is_state_useful);
    final.filter(is_state_useful);
    auto rename_state = [&](State q){return renaming[q];};
    initial.rename(rename_state);
    final.rename(rename_state);
    initial.truncate();
    final.truncate();
    if (state_renaming != nullptr) {
        state_renaming->clear();
        state_renaming->reserve(useful_states_size);
        for (State q{ 0 }; q < useful_states_size; ++q) {
            if (useful_states[q]) {
                (*state_renaming)[q] = renaming[q];
            }
        }
    }
    return *this;
}

namespace {
    // A structure to store metadata related to each state/node during the computation
    // of useful states. It contains Tarjan's metadata and the state of the
    // iteration through the successors.
    struct TarjanNodeData {
        StatePost::Moves::const_iterator current_move{};
        StatePost::Moves::const_iterator end_move{};
        // index of a node (corresponds to the time of discovery)
        unsigned long index{ 0 };
        // index of a lower node in the same SCC
        unsigned long lowlink{ 0 };
        // was the node already initialized (=the initial phase of the Tarjan's recursive call was executed)
        bool initilized{ false };
        // is node on Tarjan's stack?
        bool on_stack{ false };

        TarjanNodeData() = default;

        TarjanNodeData(State q, const Delta& delta, unsigned long index)
            : index(index), lowlink(index), initilized(true), on_stack(true) {
            const StatePost::Moves moves{ delta[q].moves() };
            current_move = moves.begin();
            end_move = moves.end();
        };
    };
};

/**
 * @brief This function employs non-recursive version of Tarjan's algorithm for finding SCCs
 * (see https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm, in particular strongconnect(v))
 * The method saturates a bool vector @p reached_and_reaching in a way that reached_and_reaching[i] = true iff
 * the state i is useful at the end. To break the recursiveness, we use @p program_stack simulating
 * the program stack during the recursive calls of strongconnect(v) (see the wiki).
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
void Nfa::tarjan_scc_discover(const TarjanDiscoverCallback& callback) const {
    std::vector<TarjanNodeData> node_info(this->num_of_states());
    std::vector<State> program_stack;
    std::vector<State> tarjan_stack;
    unsigned long index_cnt = 0;

    for(const State& q0 : initial) {
        program_stack.push_back(q0);
    }

    while(!program_stack.empty()) {
        State act_state = program_stack.back();
        TarjanNodeData& act_state_data = node_info[act_state];

        // if a node is initialized and is not on stack --> skip it; this state was
        // already processed (=this state is initial and was reachable from another initial).
        if(act_state_data.initilized && !act_state_data.on_stack) {
            program_stack.pop_back();
            continue;
        }

        // node has not been initialized yet --> corresponds to the first call of strongconnect(act_state)
        if(!act_state_data.initilized) {
            // initialize node
            act_state_data = TarjanNodeData(act_state, this->delta, index_cnt++);
            tarjan_stack.push_back(act_state);

            if(callback.state_discover && callback.state_discover(act_state)) {
                return;
            }
        } else { // return from the recursive call
            State act_succ = act_state_data.current_move->target;
            act_state_data.lowlink = std::min(act_state_data.lowlink, node_info[act_succ].lowlink);
            // act_succ is the state that cased the recursive call. Move to another successor.
            ++act_state_data.current_move;
        }

        // iterate through outgoing edges
        State next_state;
        // rec_call simulates call of the strongconnect. Since c++ cannot do continue over
        // multiple loops, we use rec_call to jump to the main loop
        bool rec_call = false;
        for(; act_state_data.current_move != act_state_data.end_move; ++act_state_data.current_move) {
            next_state = act_state_data.current_move->target;
            if(callback.succ_state_discover) {
                callback.succ_state_discover(act_state, next_state);
            }
            if(!node_info[next_state].initilized) { // recursive call
                program_stack.push_back(next_state);
                rec_call = true;
                break;
            } else if(node_info[next_state].on_stack) {
                act_state_data.lowlink = std::min(act_state_data.lowlink, node_info[next_state].index);
            }
        }
        if(rec_call) continue;

        // check if we have the root of a SCC
        if(act_state_data.lowlink == act_state_data.index) {
            State st;
            std::vector<State> scc;
            do {
                st = tarjan_stack.back();
                tarjan_stack.pop_back();
                node_info[st].on_stack = false;

                if(callback.scc_state_discover) {
                    callback.scc_state_discover(st);
                }
                scc.push_back(st);
            } while(st != act_state);
            if(callback.scc_discover && callback.scc_discover(scc, tarjan_stack)) {
                return;
            }
        }
        // all successors have been processed, we can remove act_state from the program stack
        program_stack.pop_back();
    }
}

BoolVector Nfa::get_useful_states() const {
    BoolVector useful(this->num_of_states(), false);
    bool final_scc = false;

    TarjanDiscoverCallback callback {};
    callback.state_discover = [&](State state) -> bool {
        if(this->final.contains(state)) {
            useful[state] = true;
        }
        return false;
    };
    callback.scc_discover = [&](const std::vector<State>& scc, const std::vector<State>& tarjan_stack) -> bool {
        if(final_scc) {
            // Propagate usefulness to the closed SCC.
            for(const State& st: scc) { useful[st] = true; }
            // Propagate usefulness to predecessors in @p tarjan_stack.
            for (auto state_it{ tarjan_stack.rbegin() }, state_it_end{ tarjan_stack.rend() };
                    state_it != state_it_end; ++state_it) {
                if (useful[*state_it]) { break; }
                useful[*state_it] = true;
            }
        }
        final_scc = false;
        return false;
    };
    callback.scc_state_discover = [&](State state) {
        if(useful[state]) {
            final_scc = true;
        }
    };
    callback.succ_state_discover = [&](State act_state, State next_state) {
        if(useful[next_state]) {
            useful[act_state] = true;
        }
    };

    tarjan_scc_discover(callback);
    return useful;
}

bool Nfa::is_lang_empty_scc() const {
    bool accepting_state = false;

    TarjanDiscoverCallback callback {};
    callback.state_discover = [&](State state) -> bool {
        if(this->final.contains(state)) {
            accepting_state = true;
            return true;
        }
        return false;
    };

    tarjan_scc_discover(callback);
    return !accepting_state;
}

bool Nfa::is_acyclic() const {
    bool acyclic = true;

    TarjanDiscoverCallback callback {};
    callback.scc_discover = [&](const std::vector<State>& scc, const std::vector<State>& tarjan_stack) -> bool {
        (void)tarjan_stack;
        if(scc.size() > 1) {
            acyclic = false;
                return true;
            } else { // check for self-loops
                State st = scc[0];
                for(const auto& sp : this->delta[st]) {
                    if(sp.targets.find(st) != sp.targets.end()) {
                        acyclic = false;
                        return true;
                    }
                }
            }
            return false;
    };

    tarjan_scc_discover(callback);
    return acyclic;
}

std::string Nfa::print_to_DOT() const {
    std::stringstream output;
    print_to_DOT(output);
    return output.str();
}

void Nfa::print_to_DOT(std::ostream &output) const {
    output << "digraph finiteAutomaton {" << std::endl
                 << "node [shape=circle];" << std::endl;

    for (State final_state: final) {
        output << final_state << " [shape=doublecircle];" << std::endl;
    }

    const size_t delta_size = delta.num_of_states();
    for (State source = 0; source != delta_size; ++source) {
        for (const SymbolPost &move: delta[source]) {
            output << source << " -> {";
            for (State target: move.targets) {
                output << target << " ";
            }
            output << "} [label=" << move.symbol << "];" << std::endl;
        }
    }

    output << "node [shape=none, label=\"\"];" << std::endl;
    for (State init_state: initial) {
        output << "i" << init_state << " -> " << init_state << ";" << std::endl;
    }

    output << "}" << std::endl;
}

std::string Nfa::print_to_mata() const {
    std::stringstream output;
    print_to_mata(output);
    return output.str();
}

void Nfa::print_to_mata(std::ostream &output) const {
    output << "@NFA-explicit" << std::endl
           << "%Alphabet-auto" << std::endl;
           // TODO should be this, but we cannot parse %Alphabet-numbers yet
           //<< "%Alphabet-numbers" << std::endl;

    if (!initial.empty()) {
        output << "%Initial";
        for (State init_state: initial) {
            output << " q" << init_state;
        }
        output << std::endl;
    }

    if (!final.empty()) {
        output << "%Final";
        for (State final_state: final) {
            output << " q" << final_state;
        }
        output << std::endl;
    }

    for (const Transition& trans: delta.transitions()) {
        output << "q" << trans.source << " " << trans.symbol << " q" << trans.target << std::endl;
    }
}

Nfa Nfa::get_one_letter_aut(Symbol abstract_symbol) const {
    Nfa digraph{num_of_states(), StateSet(initial), StateSet(final) };
    // Add directed transitions for digraph.
    for (const Transition& transition: delta.transitions()) {
        // Directly try to add the transition. Finding out whether the transition is already in the digraph
        //  only iterates through transition relation again.
        digraph.delta.add(transition.source, abstract_symbol, transition.target);
    }
    return digraph;
}

void Nfa::get_one_letter_aut(Nfa& result) const {
    result = get_one_letter_aut();
}

StateSet Nfa::post(const StateSet& states, const Symbol& symbol) const {
    StateSet res{};
    if (delta.empty()) {
        return res;
    }

    for (const State state: states) {
        const StatePost& post{ delta[state] };
        // TODO: This does not handle epsilons.
        const auto move_it{ post.find(symbol) };
        if (move_it != post.end()) {
            res.insert(move_it->targets);
        }
    }
    return res;
}

 void Nfa::unify_initial() {
    if (initial.empty() || initial.size() == 1) { return; }
    const State new_initial_state{add_state() };
    for (const State orig_initial_state: initial) {
        const StatePost& moves{ delta.state_post(orig_initial_state) };
        for (const auto& transitions: moves) {
            for (const State state_to: transitions.targets) {
                delta.add(new_initial_state, transitions.symbol, state_to);
            }
        }
        if (final[orig_initial_state]) { final.insert(new_initial_state); }
    }
    initial.clear();
    initial.insert(new_initial_state);
}

void Nfa::unify_final() {
    if (final.empty() || final.size() == 1) { return; }
    const State new_final_state{ add_state() };
    for (const auto& orig_final_state: final) {
        const auto transitions_to{ delta.get_transitions_to(orig_final_state) };
        for (const auto& transitions: transitions_to) {
            delta.add(transitions.source, transitions.symbol, new_final_state);
        }
        if (initial[orig_final_state]) { initial.insert(new_final_state); }
    }
    final.clear();
    final.insert(new_final_state);
}

Nfa& Nfa::operator=(Nfa&& other) noexcept {
    if (this != &other) {
        delta = std::move(other.delta);
        initial = std::move(other.initial);
        final = std::move(other.final);
        alphabet = other.alphabet;
        attributes = std::move(other.attributes);
        other.alphabet = nullptr;
    }
    return *this;
}

State Nfa::add_state() {
    const size_t num_of_states{ this->num_of_states() };
    delta.allocate(num_of_states + 1);
    return num_of_states;
}

State Nfa::add_state(State state) {
    if (state >= delta.num_of_states()) {
        delta.allocate(state + 1);
    }
    return state;
}

size_t Nfa::num_of_states() const {
    return std::max({
        static_cast<size_t>(initial.domain_size()),
        static_cast<size_t>(final.domain_size()),
        static_cast<size_t>(delta.num_of_states())
    });
}

void Nfa::clear() {
    delta.clear();
    initial.clear();
    final.clear();
}

bool Nfa::is_identical(const Nfa& aut) const {
    if (utils::OrdVector<State>(initial) != utils::OrdVector<State>(aut.initial)) {
        return false;
    }
    if (utils::OrdVector<State>(final) != utils::OrdVector<State>(aut.final)) {
        return false;
    }
    return delta == aut.delta;
}

Nfa& Nfa::complement_deterministic(const OrdVector<Symbol>& symbols, std::optional<State> sink_state) {
    const State sink{ sink_state.value_or(num_of_states()) };
    if (initial.empty()) { // The automaton has no reachable states (accepting an empty language).
        // Insert a single initial sink state.
        initial.insert(sink);
    }
    make_complete(symbols, sink);
    swap_final_nonfinal();
    return *this;
}
