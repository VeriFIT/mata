/* nfa.cc -- operations for NFA
 */

#include <algorithm>
#include <list>
#include <optional>
#include <iterator>
#include <fstream>
#include <string>

// MATA headers
#include "mata/alphabet.hh"
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

std::vector<State> Nfa::distances_to_final() const {
    return revert(*this).distances_from_initial();
}

Run Nfa::get_shortest_accepting_run_from_state(State q, const std::vector<State>& distances_to_final) const {
    Run result{{}, {q}};
    while (!final[q]) {
        for (Move move : delta[q].moves()) {
            if (distances_to_final[move.target] < distances_to_final[q]) {
                result.word.push_back(move.symbol);
                result.path.push_back(move.target);
                q = move.target;
                break;
            }
        }
    }
    return result;
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

bool Nfa::is_flat() const {
    bool flat = true;

    mata::nfa::Nfa::TarjanDiscoverCallback callback {};
    callback.scc_discover = [&](const std::vector<mata::nfa::State>& scc, const std::vector<mata::nfa::State>& tarjan_stack) -> bool {
        (void)tarjan_stack;

        for(const mata::nfa::State& st : scc) {
            bool one_input_visited = false;
            for (const mata::nfa::SymbolPost& sp : this->delta[st]) {
                for (const mata::nfa::State& tgt : scc) {
                    if(sp.targets.contains(tgt)) {
                        if(one_input_visited) {
                            flat = false;
                            return true;
                        }
                        one_input_visited = true;
                    }
                }
            }
        }
        return false;
    };

    tarjan_scc_discover(callback);
    return flat;
}

std::string Nfa::print_to_dot(const bool decode_ascii_chars, const bool use_intervals, const int max_label_length) const {
    std::stringstream output;
    print_to_dot(output, decode_ascii_chars, use_intervals, max_label_length);
    return output.str();
}

void Nfa::print_to_dot(std::ostream &output, const bool decode_ascii_chars, const bool use_intervals, const int max_label_length) const {
    auto to_ascii = [&](const Symbol symbol) -> std::string {
        // Translate only printable ASCII characters.
        if (symbol < 33 || symbol >= 127) {
            return "<" + std::to_string(symbol) + ">";
        }
        switch (symbol) {
            case '"':     return "\\\"";
            case '\\':    return "\\\\";
            default:      return std::string(1, static_cast<char>(symbol));
        }
    };

    auto translate_symbol = [&](const Symbol symbol) -> std::string {
        switch (symbol) {
            case EPSILON:      return "<eps>";
            default:           break;
        }
        if (decode_ascii_chars) {
            return to_ascii(symbol);
        }
        return std::to_string(symbol);
    };

    auto vec_of_symbols_to_string = [&](const OrdVector<Symbol>& symbols) {
        std::string result;
        for (const Symbol& symbol: symbols) {
            result += translate_symbol(symbol) + ",";
        }
        result.pop_back(); // Remove last comma
        return result;
    };

    auto vec_of_symbols_to_string_with_intervals = [&](const OrdVector<Symbol>& symbols) {
        std::string result;

        std::vector<std::pair<Symbol, Symbol>> intervals;
        auto symbols_it = symbols.begin();
        std::pair<Symbol, Symbol> interval{*symbols_it, *symbols_it};
        ++symbols_it;
        for (; symbols_it != symbols.end(); ++symbols_it) {
            if (*symbols_it == interval.second + 1) {
                interval.second = *symbols_it;
            } else {
                intervals.push_back(interval);
                interval = {*symbols_it, *symbols_it};
            }
        }
        intervals.push_back(interval);

        for (const auto& interval: intervals) {
            const size_t interval_size = interval.second - interval.first + 1;
            if (interval_size == 1) {
                result += translate_symbol(interval.first) + ",";
            } else if (interval_size == 2) {
                result += translate_symbol(interval.first) + "," + translate_symbol(interval.second) + ",";
            } else {
                result += "[" + translate_symbol(interval.first) + "-" + translate_symbol(interval.second) + "],";
            }
        }

        result.pop_back(); // Remove last comma
        return result;
    };

    BoolVector is_state_drawn(num_of_states(), false);
    output << "digraph finiteAutomaton {" << std::endl
                 << "node [shape=circle];" << std::endl;

    // Double circle for final states
    for (State final_state: final) {
        is_state_drawn[final_state] = true;
        output << final_state << " [shape=doublecircle];" << std::endl;
    }

    // Print transitions
    const size_t delta_size = delta.num_of_states();
    for (State source = 0; source != delta_size; ++source) {
        std::unordered_map<State, OrdVector<Symbol>> tgt_symbols_map;
        for (const SymbolPost &move: delta[source]) {
            is_state_drawn[source] = true;
            for (State target: move.targets) {
                is_state_drawn[target] = true;
                tgt_symbols_map[target].insert(move.symbol);
            }
        }
        for (const auto& [target, symbols]: tgt_symbols_map) {
            if (max_label_length == 0) {
                output << source << " -> " << target << ";" << std::endl;
                continue;
            }

            std::string label = (use_intervals) ? vec_of_symbols_to_string_with_intervals(symbols) : vec_of_symbols_to_string(symbols);
            std::string on_hover_label = utils::replace_all(utils::replace_all(label, "<", "&lt;"), ">", "&gt;");
            bool is_shortened = false;
            if (max_label_length > 0 && label.length() > static_cast<size_t>(max_label_length)) {
                label.replace(static_cast<size_t>(max_label_length), std::string::npos, "...");
                is_shortened = true;
            }

            if (is_shortened) {
                output << source << " -> " << target << " [label=\"" << label << "\", tooltip=\"" << on_hover_label << "\"];" << std::endl;
            } else {
                output << source << " -> " << target << " [label=\"" << label << "\"];" << std::endl;
            }
        }
    }

    // Circle for isolated states with no transitions
    for (State state{ 0 }; state < is_state_drawn.size(); ++state) {
        if (!is_state_drawn[state]) {
            output << state << " [shape=circle];" << std::endl;
        }
    }

    // Arrow for initial states
    output << "node [shape=none, label=\"\"];" << std::endl;
    for (State init_state: initial) {
        output << "i" << init_state << " -> " << init_state << ";" << std::endl;
    }

    output << "}" << std::endl;
}

void Nfa::print_to_dot(const std::string& filename, const bool decode_ascii_chars, const bool use_intervals, const int max_label_length) const {
    std::ofstream output(filename);
    if (!output) {
        throw std::ios_base::failure("Failed to open file: " + filename);
    }
    print_to_dot(output, decode_ascii_chars, use_intervals, max_label_length);
}

std::string Nfa::print_to_mata(const Alphabet* alphabet) const {
    std::stringstream output;
    print_to_mata(output, alphabet);
    return output.str();
}

void Nfa::print_to_mata(std::ostream &output, const Alphabet* alphabet) const {
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
        output << "q" << trans.source << " "
        << ((alphabet != nullptr) ? alphabet->reverse_translate_symbol(trans.symbol) : std::to_string(trans.symbol))
        << " q" << trans.target << std::endl;
    }
}

void Nfa::print_to_mata(const std::string& filename, const Alphabet* alphabet) const {
    std::ofstream output(filename);
    if (!output) {
        throw std::ios_base::failure("Failed to open file: " + filename);
    }
    print_to_mata(output, alphabet);
}

Nfa Nfa::get_one_letter_aut(Symbol abstract_symbol) const {
    Nfa digraph{num_of_states(), initial, final };
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

StateSet Nfa::post(const StateSet& states, const Symbol symbol, const EpsilonClosureOpt epsilon_closure_opt) const {
    auto get_epsilon_closure = [&](const StateSet& states) {
        StateSet closure{ states };
        std::queue<State> worklist;
        for (const State state: states) {
            worklist.push(state);
        }
        while (!worklist.empty()) {
            const State state = worklist.front();
            worklist.pop();
            auto move_it{ delta[state].find(EPSILON) };
            if (move_it != delta[state].end()) {
                for (const State target: move_it->targets) {
                    if (!closure.contains(target)) {
                        closure.insert(target);
                        worklist.push(target);
                    }
                }
            }
        }
        return closure;
    };

    StateSet res{};

    // If the symbol is EPSILON, we can stay in the same state.
    if (symbol == EPSILON && epsilon_closure_opt != EpsilonClosureOpt::NONE) {
        res = states;
    }

    if (delta.empty()) {
        return res;
    }

    StateSet from_states = states;
    if (epsilon_closure_opt == EpsilonClosureOpt::BEFORE) {
        // Before making the step using the symbol, we compute the epsilon closure.
        from_states = get_epsilon_closure(states);
    }

    // Now, we can make the step using the symbol.
    for (const State state: from_states) {
        const StatePost& post{ delta[state] };
        const auto move_it{ post.find(symbol) };
        if (move_it != post.end()) {
            res.insert(move_it->targets);
        }
    }

    if (epsilon_closure_opt == EpsilonClosureOpt::AFTER) {
        // We need to compute the epsilon closure of the resulting states.
        res = get_epsilon_closure(res);
    }

    return res;
}

 Nfa& Nfa::unify_initial(const bool force_new_state) {
    if (!force_new_state && (initial.empty() || initial.size() == 1)) { return *this; }

    const State new_initial_state{ add_state() };
    for (const State orig_initial_state: initial) {
        const StatePost& state_post{ delta.state_post(orig_initial_state) };
        for (const auto& symbol_post: state_post) {
            for (const State target: symbol_post.targets) {
                delta.add(new_initial_state, symbol_post.symbol, target);
            }
        }
        if (final[orig_initial_state]) { final.insert(new_initial_state); }
    }

    initial.clear();
    initial.insert(new_initial_state);
    return *this;
}

Nfa& Nfa::unify_final(const bool force_new_state) {
    if (!force_new_state && (final.empty() || final.size() == 1)) { return *this; }

    const State new_final_state{ add_state() };
    for (const auto& orig_final_state: final) {
        const auto transitions_to{ delta.get_transitions_to(orig_final_state) };
        for (const auto& transition: transitions_to) {
            delta.add(transition.source, transition.symbol, new_final_state);
        }
        if (initial[orig_final_state]) { initial.insert(new_final_state); }
    }

    final.clear();
    final.insert(new_final_state);
    return *this;
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

State Nfa::insert_word(const State source, const Word &word, const State target) {
    assert(!word.empty());
    assert(source < num_of_states());
    assert(target < num_of_states());

    const size_t word_len = word.size();
    if (word_len == 1) {
        delta.add(source, word[0], target);
        return target;
    }

    // Add transition source --> inner_state.
    State inner_state = add_state();
    delta.add(source, word[0], inner_state);

    // Add transitions inner_state --> inner_state
    State prev_state = inner_state;
    for (size_t idx{ 1 }; idx < word_len - 1; idx++) {
        inner_state = add_state();
        delta.add(prev_state, word[idx], inner_state);
        prev_state = inner_state;
    }

    // Add transition inner_state --> target
    delta.add(prev_state, word[word_len - 1], target);
    return target;
}

State Nfa::insert_word(const State source, const Word &word) { return insert_word(source, word, add_state()); }

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

Nfa& Nfa::unite_nondet_with(const mata::nfa::Nfa& aut) {
    const size_t num_of_states{ this->num_of_states() };
    const size_t aut_num_of_states{ aut.num_of_states() };
    const size_t new_num_of_states{ num_of_states + aut_num_of_states };

    if (this == &aut) {
        return *this;
    }

    if (final.empty() || initial.empty()) { *this = aut; return *this; }
    if (aut.final.empty() || aut.initial.empty()) { return *this; }

    this->delta.reserve(new_num_of_states);
    // Allocate space for initial and final states from 'this' which might be missing in Delta.
    this->delta.allocate(num_of_states);

    auto renumber_states = [&](State st) {
        return st + num_of_states;
    };
    this->delta.append(aut.delta.renumber_targets(renumber_states));

    // Set accepting states.
    this->final.reserve(new_num_of_states);
    for(const State& aut_fin: aut.final) {
        this->final.insert(renumber_states(aut_fin));
    }
    // Set initial states.
    this->initial.reserve(new_num_of_states);
    for(const State& aut_ini: aut.initial) {
        this->initial.insert(renumber_states(aut_ini));
    }

    return *this;
}

Nfa Nfa::decode_utf8() const {
    Nfa result{ num_of_states(), { initial }, { final } };
    BoolVector used(num_of_states(), false);
    std::stack<State> worklist;

    // Pushes a set of states to the worklist and marks them as used.
    auto push_state_set = [&](const StateSet& set) {
        for (State state: set) {
            if (used[state]) {
                continue;
            }
            worklist.push(state);
            used[state] = true;
        }
    };

    // Adds a symbol_post to the state_post.
    // If the transition sequence is deterministic, we can use emplace_back
    // because symbols are discovered in ascending order. However, in cases
    // of nondeterministic sequences, we must use insert to ensure proper ordering.
    // For example, consider the sequences 0xC8 0x80 and 0xC8 0x88.
    // Based solely on the first byte (0xC8), we cannot determine which sequence
    // will result in the higher number.
    auto add_to_state_post = [&](StatePost &state_post, const SymbolPost& symbol_post, const bool is_nondet) {
        if (is_nondet) {
            state_post.insert(symbol_post);
        } else {
            state_post.emplace_back(std::move(symbol_post));
        }
    };

    // UTF-8 Byte Patterns:
    // U+0000   to U+007F  : 0xxxxxxx
    // U+0080   to U+07FF  : 110xxxxx 10xxxxxx
    // U+0800   to U+FFFF  : 1110xxxx 10xxxxxx 10xxxxxx
    // U+010000 to U+10FFFF: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    // NOTE: Due to the nature of RE2, the automaton language can contain unexpected (invalid)
    //       UTF-8 sequences, such as 11000000 10000000 (U+0300). Because of that,
    //       we need to check if the decoded symbol is within the valid range of Unicode code points.
    push_state_set(StateSet{this->initial});
    while (!worklist.empty()) {
        State q1 = worklist.top();
        StatePost &q1_state_post = result.delta.mutable_state_post(q1);
        worklist.pop();
        // 1st Byte
        for (const SymbolPost &sp1: this->delta[q1]) {
            const Symbol s1 = sp1.symbol;
            if ((s1 & 0x80) == 0x00) {
                q1_state_post.emplace_back(SymbolPost{s1, sp1.targets});;
                push_state_set(sp1.targets);
                continue;
            }
            // 2nd Byte
            const bool is_nondet1 = sp1.targets.size() > 1;
            for (const State q2: sp1.targets) {
                for (const SymbolPost &sp2: this->delta[q2]) {
                    const Symbol s2 = sp2.symbol;
                    if ((s1 & 0xE0) == 0xC0) {
                        assert((s2 & 0xC0) == 0x80);
                        const Symbol symbol = ((s1 & 0x1F) << 6) | (s2 & 0x3F);
                        if (symbol < 0x80) {
                            continue;   // Invalid UTF-8 sequence
                        }
                        assert(symbol <= 0x7FF);
                        add_to_state_post(q1_state_post, SymbolPost{symbol, sp2.targets}, is_nondet1);
                        push_state_set(sp2.targets);
                        continue;
                    }
                    // 3rd Byte
                    const bool is_nondet2 = is_nondet1 || sp2.targets.size() > 1;
                    for (const State q3: sp2.targets) {
                        for (const SymbolPost &sp3: this->delta[q3]) {
                            const Symbol s3 = sp3.symbol;
                            if ((s1 & 0xF0) == 0xE0) {
                                assert((s3 & 0xC0) == 0x80);
                                const Symbol symbol = ((s1 & 0x0F) << 12) | ((s2 & 0x3F) << 6) | (s3 & 0x3F);
                                if (symbol < 0x800) {
                                    continue;   // Invalid UTF-8 sequence
                                }
                                assert(symbol <= 0xFFFF);
                                add_to_state_post(q1_state_post, SymbolPost{symbol, sp3.targets}, is_nondet2);
                                push_state_set(sp3.targets);
                                continue;
                            }
                            // 4th Byte
                            const bool is_nondet3 = is_nondet2 || sp3.targets.size() > 1;
                            for (const State q4: sp3.targets) {
                                for (const SymbolPost &sp4: this->delta[q4]) {
                                    const Symbol s4 = sp4.symbol;
                                    assert((s1 & 0xF8) == 0xF0);
                                    assert((s4 & 0xC0) == 0x80);
                                    const Symbol symbol = ((s1 & 0x07) << 18) | ((s2 & 0x3F) << 12) | ((s3 & 0x3F) << 6) | (s4 & 0x3F);
                                    if (symbol < 0x10000 || symbol > 0x10FFFF) {
                                        continue;   // Invalid UTF-8 sequence
                                    }
                                    add_to_state_post(q1_state_post, SymbolPost{symbol, sp4.targets}, is_nondet3);
                                    push_state_set(sp4.targets);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return result;
}
