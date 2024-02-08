/* lvlfa.cc -- operations for NFA
 */

#include <algorithm>
#include <list>
#include <optional>
#include <iterator>

// MATA headers
#include "mata/utils/sparse-set.hh"
#include "mata/lvlfa/lvlfa.hh"
#include "mata/lvlfa/algorithms.hh"
#include <mata/simlib/explicit_lts.hh>

using namespace mata::utils;
using namespace mata::lvlfa;
using mata::Symbol;
using mata::Word;
using mata::BoolVector;

using StateBoolArray = std::vector<bool>; ///< Bool array for states in the automaton.

const std::string mata::lvlfa::TYPE_NFA = "LVLFA";


Lvlfa& Lvlfa::trim(StateRenaming* state_renaming) {


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

    // Specific for levels
    //////////////////////
    State move_index{ 0 };
    std::erase_if(levels,
         [&](Level&) -> bool {
             State prev{ move_index };
             ++move_index;
             return !useful_states[prev];
         }
    );
    //////////////////////

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

std::string Lvlfa::print_to_DOT() const {
    std::stringstream output;
    print_to_DOT(output);
    return output.str();
}

void Lvlfa::print_to_DOT(std::ostream &output) const {
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
    output << "forcelabels=true;" << std::endl;
    for (State s{ 0 }; s < levels.size(); s++) {
        output << s << " [label=\"" << s << ":" << levels[s] << "\"];" << std::endl;
    }
    for (State init_state: initial) {
        output << "i" << init_state << " -> " << init_state << ";" << std::endl;
    }

    output << "}" << std::endl;
}

std::string Lvlfa::print_to_mata() const {
    std::stringstream output;
    print_to_mata(output);
    return output.str();
}

void Lvlfa::print_to_mata(std::ostream &output) const {
    output << "@LVLFA-explicit" << std::endl
           << "%Alphabet-auto" << std::endl;
           // TODO should be this, but we cannot parse %Alphabet-numbers yet
           //<< "%Alphabet-numbers" << std::endl;

    if (!initial.empty()) {
        output << "%Initial";
        for (State init_state : initial) {
            output << " q" << init_state;
        }
        output << std::endl;
    }

    if (!final.empty()) {
        output << "%Final";
        for (State final_state : final) {
            output << " q" << final_state;
        }
        output << std::endl;
    }

    if (!levels.empty()) {
        BoolVector live_states(num_of_states(), false);
        for (const State &s : initial) {
            live_states[s] = true;
        }
        for (const State &s : final) {
            live_states[s] = true;
        }
        for (const Transition &trans: delta.transitions()) {
            live_states[trans.source] = true;
            live_states[trans.target] = true;
        }
        output << "%Levels";
        for (State s{ 0 }; s < num_of_states(); s++) {
            if (live_states[s]) {
                output << " " << "q" << s << ":" << levels[s];
            }
        }
        output << std::endl;
        output << "%LevelsCnt " << levels_cnt << std::endl;
    }

    for (const Transition& trans: delta.transitions()) {
        output << "q" << trans.source << " " << trans.symbol << " q" << trans.target << std::endl;
    }
}

Lvlfa Lvlfa::get_one_letter_aut(Symbol abstract_symbol) const {
    return Lvlfa(mata::nfa::Nfa::get_one_letter_aut(abstract_symbol));
}

void Lvlfa::get_one_letter_aut(Lvlfa& result) const {
    result = get_one_letter_aut();
}

Lvlfa& Lvlfa::operator=(Lvlfa&& other) noexcept {
    if (this != &other) {
        delta = std::move(other.delta);
        initial = std::move(other.initial);
        final = std::move(other.final);
        levels = std::move(other.levels);
        levels_cnt = other.levels_cnt;
        alphabet = other.alphabet;
        attributes = std::move(other.attributes);
        other.alphabet = nullptr;
    }
    return *this;
}

State Lvlfa::add_state() {
    levels.push_back(0);
    return mata::nfa::Nfa::add_state();
}

State Lvlfa::add_state(State state) {
    levels.push_back(0);
    return mata::nfa::Nfa::add_state(state);
}

void Lvlfa::clear() {
    mata::nfa::Nfa::clear();
    levels.clear();
}

bool Lvlfa::is_identical(const Lvlfa& aut) const {
    return levels_cnt == aut.levels_cnt && levels == aut.levels && mata::nfa::Nfa::is_identical(aut);
}
