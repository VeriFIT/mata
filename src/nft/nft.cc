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

std::string Nft::print_to_DOT(const bool ascii) const {
    std::stringstream output;
    print_to_DOT(output, ascii);
    return output.str();
}

void Nft::print_to_DOT(std::ostream &output, const bool ascii) const {
    auto translate_special_symbols = [&](const Symbol symbol) -> std::string {
        if (symbol == EPSILON) {
            return "<eps>";
        }
        if (symbol == DONT_CARE) {
            return "<dcare>";
        }
        return std::to_string(symbol);
    };

    auto to_ascii = [&](const Symbol symbol) {
        // Translate only printable ASCII characters.
        if (symbol < 33) {
            return std::to_string(symbol);
        }
        return "\\'" + std::string(1, static_cast<char>(symbol)) + "\\'";
    };
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

            if (ascii && move.symbol < 128) {
                output << "} [label=\"" << to_ascii(move.symbol) << "\"];" << std::endl;
            } else {
                output << "} [label=\"" << translate_special_symbols(move.symbol) << "\"];" << std::endl;
            }
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
        output << "%LevelsCnt " << num_of_levels << std::endl;
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

void Nft::make_one_level_aut(const utils::OrdVector<Symbol> &dont_care_symbol_replacements, const JumpMode jump_mode) {
    const bool dcare_for_dcare = dont_care_symbol_replacements == utils::OrdVector<Symbol>({ DONT_CARE });
    std::vector<Transition> transitions_to_del;
    std::vector<Transition> transitions_to_add;

    auto add_inner_transitions = [&](State src, Symbol symbol, State trg) {
        if (symbol == DONT_CARE && !dcare_for_dcare) {
            for (const Symbol replace_symbol : dont_care_symbol_replacements) {
                transitions_to_add.emplace_back( src, replace_symbol, trg );
            }
        } else {
            transitions_to_add.emplace_back( src, symbol, trg );
        }
    };

    Level src_lvl, trg_lvl, diff_lvl;
    for (const auto &transition : delta.transitions()) {
        src_lvl = levels[transition.source];
        trg_lvl = levels[transition.target];
        diff_lvl = (trg_lvl == 0) ? (static_cast<Level>(num_of_levels) - src_lvl) : trg_lvl - src_lvl;

        if (diff_lvl == 1 && transition.symbol == DONT_CARE && !dcare_for_dcare) {
            transitions_to_del.push_back(transition);
            for (const Symbol replace_symbol : dont_care_symbol_replacements) {
                transitions_to_add.emplace_back( transition.source, replace_symbol, transition.target );
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
            Level pre_trg_lvl = (trg_lvl == 0) ? (static_cast<Level>(num_of_levels) - 1) : (trg_lvl - 1);
            for (; inner_src_lvl < pre_trg_lvl; inner_src_lvl++, inner_trg_lvl++) {
                inner_trg = add_state();
                levels[inner_trg] = inner_trg_lvl;

                add_inner_transitions(inner_src, jump_mode == JumpMode::AppendDontCares ? DONT_CARE : transition.symbol, inner_trg);
                inner_src = inner_trg;
            }

            // The last iteration connecting last inner state with the original target state.
            add_inner_transitions(inner_src, jump_mode == JumpMode::AppendDontCares ? DONT_CARE : transition.symbol, transition.target);
        }
    }

    for (const Transition &transition : transitions_to_add) {
        delta.add(transition);
    }
    for (const Transition &transition : transitions_to_del) {
        delta.remove(transition);
    }
}

Nft Nft::get_one_level_aut(const utils::OrdVector<Symbol> &dont_care_symbol_replacements, const JumpMode jump_mode) const {
    Nft result{ *this };
    // HACK. Works only for automata without levels.
    if (result.levels.size() != result.num_of_states()) {
        return result;
    }
    result.make_one_level_aut(dont_care_symbol_replacements, jump_mode);
    return result;
}

void Nft::get_one_level_aut(Nft& result, const utils::OrdVector<Symbol> &dont_care_symbol_replacements, const JumpMode jump_mode) const {
    result = get_one_level_aut(dont_care_symbol_replacements, jump_mode);
}

Nft& Nft::operator=(Nft&& other) noexcept {
    if (this != &other) {
        mata::nfa::Nfa::operator=(other);
        levels = std::move(other.levels);
        num_of_levels = other.num_of_levels;
    }
    return *this;
}

State Nft::add_state() {
    const State state{ Nfa::add_state() };
    levels.set(state);
    return state;
}

State Nft::add_state(const State state) {
    levels.set(state);
    return Nfa::add_state(state);
}

State Nft::add_state_with_level(const Level level) {
    const State state{ Nfa::add_state() };
    levels.set(state, level);
    return state;
}

State Nft::add_state_with_level(const State state, const Level level) {
    levels.set(state, level);
    return Nfa::add_state(state);
}

State Nft::insert_word(const State source, const Word &word, const State target) {
    assert(0 < num_of_levels);
    const size_t num_of_states_orig{ num_of_states() };
    assert(source < num_of_states_orig);
    assert(target < num_of_states_orig);
    assert(levels[source] == levels[target]);

    const State first_new_state = num_of_states_orig;
    const State word_tgt = Nfa::insert_word(source, word, target);
    const size_t num_of_states_after = num_of_states();
    const Level src_lvl = levels[source];

    Level lvl = (num_of_levels == 1 ) ? src_lvl : (src_lvl + 1) % static_cast<Level>(num_of_levels);
    State state{ first_new_state };
    for (; state < num_of_states_after; state++, lvl = (lvl + 1) % static_cast<Level>(num_of_levels)){
        add_state_with_level(state, lvl);
    }

    assert(levels[word_tgt] == 0 || levels[num_of_states_after - 1] < levels[word_tgt]);

    return word_tgt;
}

State Nft::insert_word(const State source, const Word& word) {
    assert(source < levels.size());
    return insert_word(source, word, add_state_with_level(levels[source]));
}

State Nft::insert_word_by_parts(const State source, const std::vector<Word> &word_parts_on_levels, const State target) {
    assert(0 < num_of_levels);
    assert(word_parts_on_levels.size() == num_of_levels);
    assert(source < num_of_states());
    assert(target < num_of_states());
    assert(source < levels.size());
    assert(levels[source] == levels[target]);
    const Level from_to_level{ levels[source] };

    if (num_of_levels == 1) {
        return Nft::insert_word(source, word_parts_on_levels[0], target);
    }

    std::vector<mata::Word::const_iterator> word_part_it_v(num_of_levels);
    for (Level lvl{ from_to_level }, i{ 0 }; i < static_cast<Level>(num_of_levels); ++i) {
        word_part_it_v[lvl] = word_parts_on_levels[lvl].begin();
        lvl = (lvl + 1) % static_cast<Level>(num_of_levels);
    }

    // This function retrieves the next symbol from a word part at a specified level and advances the corresponding iterator.
    // Returns EPSILON when the iterator reaches the end of the word part.
    auto get_next_symbol = [&](Level lvl) {
        if (word_part_it_v[lvl] == word_parts_on_levels[lvl].end()) {
            return EPSILON;
        }
        return *(word_part_it_v[lvl]++);
    };

    // Add transition source --> inner_state.
    Level inner_lvl = (num_of_levels == 1 ) ? 0 : (from_to_level + 1) % static_cast<Level>(num_of_levels);
    State inner_state = add_state_with_level(inner_lvl);
    delta.add(source, get_next_symbol(from_to_level), inner_state);

    // Add transition inner_state --> inner_state
    State prev_state = inner_state;
    Level prev_lvl = inner_lvl;
    const size_t max_word_part_len = std::max_element(
        word_parts_on_levels.begin(),
        word_parts_on_levels.end(),
        [](const Word& a, const Word& b) { return a.size() < b.size(); }
    )->size();
    const size_t word_total_len = num_of_levels * max_word_part_len;
    if (word_total_len != 0) {
        for (size_t symbol_idx{ 1 }; symbol_idx < word_total_len - 1; symbol_idx++) {
            inner_lvl = (prev_lvl + 1) % static_cast<Level>(num_of_levels);
            inner_state = add_state_with_level(inner_lvl);
            delta.add(prev_state, get_next_symbol(prev_lvl), inner_state);
            prev_state = inner_state;
            prev_lvl = inner_lvl;
        }
    }
    // Add transition inner_state --> target.
    delta.add(prev_state, get_next_symbol(prev_lvl), target);
    return target;
}

State Nft::insert_word_by_parts(const State source, const std::vector<Word> &word_parts_on_levels) {
   assert(source < levels.size());
   return insert_word_by_parts(source, word_parts_on_levels, add_state_with_level(levels[source]));
}

void Nft::insert_identity(const State state, const std::vector<Symbol> &symbols, const JumpMode jump_mode) {
    for (const Symbol symbol : symbols) {
        insert_identity(state, symbol, jump_mode);
    }
}

void Nft::insert_identity(const State state, const Symbol symbol, const JumpMode jump_mode) {
    // TODO(nft): Evaluate the performance difference between adding a jump transition and inserting a transition for each level.
    if (jump_mode == JumpMode::RepeatSymbol) {
        delta.add(state, symbol, state);
    } else {
        insert_word(state, Word(num_of_levels, symbol), state);
    }
}

void Nft::clear() {
    mata::nfa::Nfa::clear();
    levels.clear();
}

bool Nft::is_identical(const Nft& aut) const {
    return num_of_levels == aut.num_of_levels && levels == aut.levels && mata::nfa::Nfa::is_identical(aut);
}

Levels& Levels::set(State state, Level level) {
    if (size() <= state) { resize(state + 1, DEFAULT_LEVEL); }
    (*this)[state] = level;
    return *this;
}
