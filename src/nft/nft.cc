/* nft.cc -- operations for NFA
 */

#include <algorithm>
#include <list>
#include <optional>
#include <iterator>
#include <fstream>
#include <string>

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


size_t Nft::num_of_states_with_level(const Level level) const {
    return levels.count(level);
}

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

void Nft::remove_epsilon(Symbol epsilon) {
    *this = mata::nft::remove_epsilon(*this, epsilon);
}

std::string Nft::print_to_dot(const bool decode_ascii_chars, const bool use_intervals, const int max_label_length) const {
    std::stringstream output;
    print_to_dot(output, decode_ascii_chars, use_intervals, max_label_length);
    return output.str();
}

void Nft::print_to_dot(std::ostream &output, const bool decode_ascii_chars, const bool use_intervals, const int max_label_length) const {
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

    // Levels for each state
    output << "node [shape=none, label=\"\"];" << std::endl;
    output << "forcelabels=true;" << std::endl;
    for (State s{ 0 }; s < levels.size(); s++) {
        output << s << " [label=\"" << s << ":" << levels[s] << "\"];" << std::endl;
    }

    // Arrow for initial states
    for (State init_state: initial) {
        output << "i" << init_state << " -> " << init_state << ";" << std::endl;
    }

    output << "}" << std::endl;
}

void Nft::print_to_dot(const std::string& filename,  const bool decode_ascii_chars, const bool use_intervals, const int max_label_length) const {
    std::ofstream output(filename);
    if (!output) {
        throw std::ios_base::failure("Failed to open file: " + filename);
    }
    print_to_dot(output, decode_ascii_chars, use_intervals, max_label_length);
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
    }
    output << "%LevelsNum " << num_of_levels << std::endl;

    for (const Transition& trans: delta.transitions()) {
        output << "q" << trans.source << " " << trans.symbol << " q" << trans.target << std::endl;
    }
}

void Nft::print_to_mata(const std::string& filename) const {
    std::ofstream output(filename);
    if (!output) {
        throw std::ios_base::failure("Failed to open file: " + filename);
    }
    print_to_mata(output);
}

Nft Nft::get_one_letter_aut(const std::set<Level>& levels_to_keep, Symbol abstract_symbol) const {
    mata::nft::Nft one_symbol_transducer{num_of_states(), initial, final, levels, num_of_levels};
    for (mata::nfa::State source_state = 0; source_state < delta.num_of_states(); ++source_state) {
        mata::nfa::StatePost& state_post_of_new_source_state = one_symbol_transducer.delta.mutable_state_post(source_state);
        if (levels_to_keep.contains(levels[source_state])) {
            state_post_of_new_source_state = delta[source_state];
        } else {
            mata::nfa::SymbolPost new_transition{abstract_symbol};
            for (const mata::nfa::SymbolPost& symbol_post : delta[source_state]) {
                if (symbol_post.symbol == mata::nft::EPSILON) {
                    state_post_of_new_source_state.insert(symbol_post);
                } else {
                    new_transition.targets.insert(symbol_post.targets);
                }
            }
            if (!new_transition.targets.empty()) {
                state_post_of_new_source_state.insert(new_transition);
            }
        }
    }
    return one_symbol_transducer;
}

void Nft::get_one_letter_aut(Nft& result) const {
    result = get_one_letter_aut();
}

StateSet Nft::post(const StateSet& states, const Symbol symbol, const EpsilonClosureOpt epsilon_closure_opt) const {
    std::cerr << "Warning: Nft::post uses Nfa::post, which is not designed for NFT's jump transitions" << std::endl;
    return nfa::Nfa::post(states, symbol, epsilon_closure_opt);
}

void Nft::unwind_jumps_inplace(const utils::OrdVector<Symbol> &dont_care_symbol_replacements, const JumpMode jump_mode) {
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

Nft Nft::unwind_jumps(const utils::OrdVector<Symbol> &dont_care_symbol_replacements, const JumpMode jump_mode) const {
    Nft result{ *this };
    // HACK. Works only for automata without levels.
    if (result.levels.size() != result.num_of_states()) {
        return result;
    }
    result.unwind_jumps_inplace(dont_care_symbol_replacements, jump_mode);
    return result;
}

void Nft::unwind_jumps(Nft& result, const utils::OrdVector<Symbol> &dont_care_symbol_replacements, const JumpMode jump_mode) const {
    result = unwind_jumps(dont_care_symbol_replacements, jump_mode);
}

Nft& Nft::operator=(Nft&& other) noexcept {
    if (this != &other) {
        mata::nfa::Nfa::operator=(other);
        levels = std::move(other.levels);
        num_of_levels = other.num_of_levels;
    }
    return *this;
}


Nft& Nft::operator=(const mata::nfa::Nfa& other) noexcept {
    if (this != &other) {
        mata::nfa::Nfa::operator=(other);
        levels = Levels(num_of_states(), DEFAULT_LEVEL);
        num_of_levels = 1;
    }
    return *this;
}

Nft& Nft::operator=(mata::nfa::Nfa&& other) noexcept {
    if (this != &other) {
        mata::nfa::Nfa::operator=(other);
        levels = Levels(num_of_states(), DEFAULT_LEVEL);
        num_of_levels = 1;
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
    // Ensure both states exist in the NFT.
    add_state(std::max(source, target));
    if (levels[source] != levels[target]) {
        throw std::invalid_argument{ "Inserting word between source and target states with different levels." };
    }

    const State first_new_state = num_of_states();
    const State word_target = Nfa::insert_word(source, word, target);
    const size_t num_of_states_after = num_of_states();
    const Level source_level = levels[source];

    Level lvl = (num_of_levels == 1) ? source_level : (source_level + 1) % static_cast<Level>(num_of_levels);
    for (State state{ first_new_state }; state < num_of_states_after;
         ++state, lvl = (lvl + 1) % static_cast<Level>(num_of_levels)) { add_state_with_level(state, lvl); }

    assert(levels[word_target] == 0 || levels[num_of_states_after - 1] < levels[word_target]);

    return word_target;
}

State Nft::insert_word(const State source, const Word& word) {
    if (num_of_states() <= source) { add_state(source); }
    return insert_word(source, word, add_state_with_level(levels[source]));
}

State Nft::insert_word_by_parts(const State source, const std::vector<Word> &word_parts_on_levels, const State target) {
    assert(word_parts_on_levels.size() == num_of_levels);
    assert(source < num_of_states());
    assert(target < num_of_states());
    assert(source < levels.size());
    assert(target < levels.size());
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

State Nft::add_transition(const State source, const std::vector<Symbol>& symbols, const State target) {
    return insert_word(source, symbols, target);
}
State Nft::add_transition(const State source, const std::vector<Symbol>& symbols) {
    return insert_word(source, symbols);
}

Nft& Nft::insert_identity(const State state, const std::vector<Symbol> &symbols, const JumpMode jump_mode) {
    for (const Symbol symbol: symbols) { insert_identity(state, symbol, jump_mode); }
    return *this;
}

Nft& Nft::insert_identity(const State state, const Alphabet* alphabet, const JumpMode jump_mode) {
    for (const Symbol symbol: alphabet->get_alphabet_symbols()) { insert_identity(state, symbol, jump_mode); }
    return *this;
}

Nft& Nft::insert_identity(const State state, const Symbol symbol, const JumpMode jump_mode) {
    (void)jump_mode;
    // TODO(nft): Evaluate the performance difference between adding a jump transition and inserting a transition for each level.
    // FIXME(nft): Allow symbol jump transitions?
//    if (jump_mode == JumpMode::RepeatSymbol) {
//        delta.add(state, symbol, state);
//        insert_word(state, Word(num_of_levels, symbol), state);
//    } else {
        insert_word(state, Word(num_of_levels, symbol), state);
//    }
    return *this;
}

bool Nft::contains_jump_transitions() {
    if (num_of_levels == 1) { return false; }

    for (const Transition& transition : delta.transitions()) {
        Level src_level = levels[transition.source];
        Level tgt_level = levels[transition.target];
        if (tgt_level == 0) {
            // we want to check if the difference between src and tgt levels is at most 1 modulo num_of_levels
            tgt_level = tgt_level + static_cast<Level>(num_of_levels);
        }
        if (tgt_level - src_level != 1) {
            return true;
        }
    }
    return false;
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

mata::nfa::Nfa Nft::to_nfa_update_copy(
    const utils::OrdVector<Symbol>& dont_care_symbol_replacements, const JumpMode jump_mode) const {
    return unwind_jumps(dont_care_symbol_replacements, jump_mode).to_nfa_copy();
}

mata::nfa::Nfa Nft::to_nfa_update_move(
    const utils::OrdVector<Symbol>& dont_care_symbol_replacements, const JumpMode jump_mode) {
    unwind_jumps_inplace(dont_care_symbol_replacements, jump_mode);
    return to_nfa_move();
}
