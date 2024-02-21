/* nft.cc -- operations for NFA
 */

#include <algorithm>
#include <list>
#include <optional>
#include <iterator>

// MATA headers
#include "mata/utils/sparse-set.hh"
#include "mata/nft/nft.hh"
#include "mata/nft/algorithms.hh"
#include <mata/simlib/explicit_lts.hh>

using namespace mata::utils;
using namespace mata::nft;
using mata::Symbol;
using mata::Word;
using mata::BoolVector;

using StateBoolArray = std::vector<bool>; ///< Bool array for states in the automaton.

const std::string mata::nft::TYPE_NFT = "NFT";


Nft& Nft::trim(StateRenaming* state_renaming) {

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

std::string Nft::print_to_DOT() const {
    std::stringstream output;
    print_to_DOT(output);
    return output.str();
}

void Nft::print_to_DOT(std::ostream &output) const {
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

std::string Nft::print_to_mata() const {
    std::stringstream output;
    print_to_mata(output);
    return output.str();
}

void Nft::print_to_mata(std::ostream &output) const {
    output << "@NFT-explicit" << std::endl
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

Nft Nft::get_one_letter_aut(Symbol abstract_symbol) const {
    return Nft(mata::nfa::Nfa::get_one_letter_aut(abstract_symbol));
}

void Nft::get_one_letter_aut(Nft& result) const {
    result = get_one_letter_aut();
}

void Nft::make_one_level_aut(const utils::OrdVector<Symbol> &dcare_replacements) {
    bool dcare_for_dcare = dcare_replacements == utils::OrdVector<Symbol>({ DONT_CARE });
    std::vector<Transition> transitions_to_del;
    std::vector<Transition> transitions_to_add;

    auto add_inner_transitions = [&](State src, Symbol symbol, State trg) {
        if (symbol == DONT_CARE && !dcare_for_dcare) {
            for (const Symbol replace_symbol : dcare_replacements) {
                transitions_to_add.push_back({ src, replace_symbol, trg });
            }
        } else {
            transitions_to_add.push_back({ src, symbol, trg });
        }
    };

    Level src_lvl, trg_lvl, diff_lvl;
    for (const auto &transition : delta.transitions()) {
        src_lvl = levels[transition.source];
        trg_lvl = levels[transition.target];
        diff_lvl = (trg_lvl == 0) ? (levels_cnt - src_lvl) : trg_lvl - src_lvl;

        if (diff_lvl == 1 && transition.symbol == DONT_CARE && !dcare_for_dcare) {
            transitions_to_del.push_back(transition);
            for (const Symbol replace_symbol : dcare_replacements) {
                transitions_to_add.push_back({ transition.source, replace_symbol, transition.target });
            }
        } else if (diff_lvl > 1) {
            transitions_to_del.push_back(transition);
            State inner_src = transition.source;
            Level inner_src_lvl = src_lvl;

            // The first iteration connecting original source state with inner state.
            State inner_trg = add_state();
            Level inner_trg_lvl = src_lvl + 1;
            levels[inner_trg] = inner_trg_lvl;
            add_inner_transitions(inner_src, transition.symbol, inner_trg);
            inner_src = inner_trg;
            inner_src_lvl++;
            inner_trg_lvl++;

            // Iterations 1 to n-1 connecting inner states.
            Level pre_trg_lvl = (trg_lvl == 0) ? (levels_cnt - 1) : (trg_lvl - 1);
            for (; inner_src_lvl < pre_trg_lvl; inner_src_lvl++, inner_trg_lvl++) {
                inner_trg = add_state();
                levels[inner_trg] = inner_trg_lvl;
                add_inner_transitions(inner_src, DONT_CARE, inner_trg);
                inner_src = inner_trg;
            }

            // The last iteration connecting last inner state with the original target state.
            add_inner_transitions(inner_src, DONT_CARE, transition.target);
        }
    }

    for (const Transition &transition : transitions_to_add) {
        delta.add(transition);
    }
    for (const Transition &transition : transitions_to_del) {
        delta.remove(transition);
    }
}

Nft Nft::get_one_level_aut(const utils::OrdVector<Symbol> &dcare_replacements) const {
    Nft result{ *this };

    // TODO(nft): Create a class for LEVELS with overloaded getter and setter.
    // HACK. Works only for automata without levels.
    if (result.levels.size() != result.num_of_states()) {
        return result;
    }

    result.make_one_level_aut(dcare_replacements);
    return result;
}

void Nft::get_one_level_aut(Nft& result, const utils::OrdVector<Symbol> &dcare_replacements) const {
    result = get_one_level_aut(dcare_replacements);
}

Nft& Nft::operator=(Nft&& other) noexcept {
    if (this != &other) {
        mata::nfa::Nfa::operator=(other);
        levels = std::move(other.levels);
        levels_cnt = other.levels_cnt;
    }
    return *this;
}

State Nft::add_state() {
    const size_t required_capacity{ num_of_states() + 1 };
    if (levels.size() < required_capacity) {
        levels.resize(required_capacity, DEFAULT_LEVEL);
    }
    return mata::nfa::Nfa::add_state();
}

State Nft::add_state(const State state) {
    const size_t required_capacity{ state + 1 };
    if (levels.size() < required_capacity) {
        levels.resize(required_capacity, DEFAULT_LEVEL);
    }
    return mata::nfa::Nfa::add_state(state);
}

State Nft::add_state_with_level(const Level level) {
    const State state{ add_state() };
    levels[state] = level;
    return state;
}

State Nft::add_state_with_level(const State state, const Level level) {
    add_state(state);
    levels[state] = level;
    return state;
}

void Nft::clear() {
    mata::nfa::Nfa::clear();
    levels.clear();
}

bool Nft::is_identical(const Nft& aut) const {
    return levels_cnt == aut.levels_cnt && levels == aut.levels && mata::nfa::Nfa::is_identical(aut);
}
