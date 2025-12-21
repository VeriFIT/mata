/** @file
 * @brief Methods for NFTs.
 */

#include <algorithm>
#include <cassert>
#include <optional>
#include <fstream>
#include <string>
#include <utility>
#include <ranges>

#include "mata/utils/sparse-set.hh"
#include "mata/nfa/builder.hh"
#include "mata/nft/nft.hh"


using namespace mata::utils;
using namespace mata::nft;
using mata::Symbol;
using mata::Word;
using mata::BoolVector;

using StateBoolArray = std::vector<bool>; ///< Bool array for states in the automaton.

constexpr std::string mata::nft::TYPE_NFT = "NFT";

Levels::Levels(const size_t num_of_levels, const size_t count, const Level value)
    : super(count, value), num_of_levels{ num_of_levels } {
    assert(value < num_of_levels && "Levels::Levels: level is out of range of the set number of levels.");
}

Levels::Levels(const size_t num_of_levels, const std::initializer_list<Level> levels)
    : super{ levels }, num_of_levels{ num_of_levels } {
    assert(check_levels_in_range_() && "Levels::Levels: level is out of range of the set number of levels.");
}

Levels::Levels(const size_t num_of_levels, const iterator first, const iterator last)
    : super{ first, last }, num_of_levels{ num_of_levels } {
    assert(check_levels_in_range_() && "Levels::Levels: level is out of range of the set number of levels.");
}

Levels& Levels::operator=(const std::initializer_list<Level> other) {
    super::operator=(other);
    num_of_levels = num_of_levels_used().value_or(DEFAULT_NUM_OF_LEVELS);
    return *this;
}

Levels& Levels::operator=(const std::vector<Level>& levels) {
    if (this != &levels) {
        super::operator=(levels);
        num_of_levels = num_of_levels_used().value_or(DEFAULT_NUM_OF_LEVELS);
    }
    return *this;
}

Levels& Levels::operator=(std::vector<Level>&& levels) {
    if (this != &levels) {
        super::operator=(std::move(levels));
        num_of_levels = num_of_levels_used().value_or(DEFAULT_NUM_OF_LEVELS);
    }
    return *this;
}

Levels& Levels::set(const State state, const Level level) {
    assert(level < num_of_levels && "Levels::set: level is out of range of the set number of levels.");
    if (size() <= state) { resize(state + 1, DEFAULT_LEVEL); }
    (*this)[state] = level;
    return *this;
}

Levels& Levels::set(const std::vector<Level>& levels) {
    assign(levels.begin(), levels.end());
    assert(check_levels_in_range_() && "Levels::set: level is out of range of the set number of levels.");
    return *this;
}

Levels& Levels::set(std::vector<Level>&& levels) {
    const auto num_of_levels_orig{ num_of_levels };
    *this = std::move(levels);
    num_of_levels = num_of_levels_orig;
    assert(check_levels_in_range_() && "Levels::set: level is out of range of the set number of levels.");
    return *this;
}

void Levels::append(const Levels& levels_vector) {
    reserve(size() + levels_vector.size());
    std::ranges::copy(levels_vector, std::back_inserter(*this));
    assert(check_levels_in_range_() && "Levels::append: level is out of range of the set number of levels.");
}

std::vector<Level> Levels::get_levels_of(const SparseSet<State>& states) const {
    std::vector<Level> result;
    result.reserve(states.size());
    for (const State state : states) { result.push_back((*this)[state]); }
    return result;
}

std::vector<Level> Levels::get_levels_of(const StateSet& states) const {
    std::vector<Level> result;
    result.reserve(states.size());
    for (const State state : states) { result.push_back((*this)[state]); }
    return result;
}

std::optional<Level> Levels::get_minimal_level_of(const StateSet& states, Ordering::Compare levels_ordering) const {
    if (states.empty()) { return std::nullopt; }
    auto levels = states | std::views::transform([&](const State& state) { return (*this)[state]; });
    return std::ranges::min(levels, std::move(levels_ordering));
}

std::optional<Level> Levels::get_minimal_next_level_of(const StateSet& states) const {
    return get_minimal_level_of(states, Ordering::Next);
}

bool Levels::can_follow(const Level source_level, const Level target_level) {
    return source_level < target_level || target_level == 0;
}

bool Levels::can_follow_for_states(const State source, const State target) const {
    return can_follow((*this)[source], (*this)[target]);
}

size_t Nft::num_of_states_with_level(const Level level) const { return levels.count(level); }

Nft& Nft::trim(StateRenaming* state_renaming) {
    #ifdef _STATIC_STRUCTURES_
    BoolVector useful_states{ useful_states() };
    useful_states.clear();
    useful_states = useful_states();
    #else
    const BoolVector useful_states{ get_useful_states() };
    #endif
    const size_t useful_states_size{ useful_states.size() };
    std::vector<State> renaming(useful_states_size);
    for (State new_state{ 0 }, orig_state{ 0 }; orig_state < useful_states_size; ++orig_state) {
        if (useful_states[orig_state]) {
            renaming[orig_state] = new_state;
            ++new_state;
        }
    }

    delta.defragment(useful_states, renaming);

    auto is_state_useful = [&](const State q) { return q < useful_states.size() && useful_states[q]; };
    initial.filter(is_state_useful);
    final.filter(is_state_useful);

    // Specific for levels
    State move_index{ 0 };
    levels.erase(
        std::ranges::remove_if(
            levels,
            [&](Level&) -> bool {
                const State prev{ move_index };
                ++move_index;
                return !useful_states[prev];
            }
        ).begin(),
        levels.end()
    );

    auto rename_state = [&](const State q) { return renaming[q]; };
    initial.rename(rename_state);
    final.rename(rename_state);
    initial.truncate();
    final.truncate();
    if (state_renaming != nullptr) {
        state_renaming->clear();
        state_renaming->reserve(useful_states_size);
        for (State q{ 0 }; q < useful_states_size; ++q) {
            if (useful_states[q]) { (*state_renaming)[q] = renaming[q]; }
        }
    }
    return *this;
}

void Nft::remove_epsilon(const Symbol epsilon) { *this = nft::remove_epsilon(*this, epsilon); }

std::string Nft::print_to_dot(
    const bool decode_ascii_chars, const bool use_intervals, const int max_label_length,
    const Alphabet* alphabet) const {
    std::stringstream output;
    print_to_dot(output, decode_ascii_chars, use_intervals, max_label_length, alphabet);
    return output.str();
}

void Nft::print_to_dot(
    std::ostream& output, const bool decode_ascii_chars, const bool use_intervals, const int max_label_length,
    const Alphabet* alphabet) const {
    auto to_ascii = [&](const Symbol symbol) -> std::string {
        // Translate only printable ASCII characters.
        if (symbol < 33 || symbol >= 127) { return "<" + std::to_string(symbol) + ">"; }
        switch (symbol) {
            case '"': return "\\\"";
            case '\\': return "\\\\";
            default: return std::string(1, static_cast<char>(symbol));
        }
    };

    auto translate_symbol = [&](const Symbol symbol) -> std::string {
        switch (symbol) {
            case EPSILON: return "<eps>";
            default: break;
        }
        if (decode_ascii_chars) { return to_ascii(symbol); } else if (alphabet != nullptr) {
            return alphabet->reverse_translate_symbol(symbol);
        } else if (this->alphabet != nullptr) { return this->alphabet->reverse_translate_symbol(symbol); } else {
            return std::to_string(symbol);
        }
    };

    auto vec_of_symbols_to_string = [&](const OrdVector<Symbol>& symbols) {
        std::string result;
        for (const Symbol& symbol : symbols) { result += translate_symbol(symbol) + ","; }
        result.pop_back(); // Remove last comma
        return result;
    };

    auto vec_of_symbols_to_string_with_intervals = [&](const OrdVector<Symbol>& symbols) {
        std::string result;

        const auto intervals{
            [&]() {
                std::vector<std::pair<Symbol, Symbol>> intervals_val;
                auto symbols_it = symbols.begin();
                std::pair<Symbol, Symbol> interval{ *symbols_it, *symbols_it };
                ++symbols_it;
                for (; symbols_it != symbols.end(); ++symbols_it) {
                    if (*symbols_it == interval.second + 1) { interval.second = *symbols_it; } else {
                        intervals_val.push_back(interval);
                        interval = { *symbols_it, *symbols_it };
                    }
                }
                intervals_val.push_back(interval);
                return intervals_val;
            }()
        };

        for (const auto& [symbol_from, symbol_to] : intervals) {
            if (const size_t interval_size{ symbol_to - symbol_from + 1 }; interval_size == 1) {
                result += translate_symbol(symbol_from) + ",";
            } else if (interval_size == 2) {
                result += translate_symbol(symbol_from) + "," + translate_symbol(symbol_to) + ",";
            } else { result += "[" + translate_symbol(symbol_from) + "-" + translate_symbol(symbol_to) + "],"; }
        }

        result.pop_back(); // Remove last comma
        return result;
    };


    BoolVector is_state_drawn(num_of_states(), false);
    output << "digraph finiteAutomaton {" << std::endl
            << "node [shape=circle];" << std::endl;

    // Double circle for final states
    for (const State final_state : final) {
        is_state_drawn[final_state] = true;
        output << final_state << " [shape=doublecircle];" << std::endl;
    }

    // Print transitions
    const size_t delta_size = delta.num_of_states();
    for (State source = 0; source != delta_size; ++source) {
        std::unordered_map<State, OrdVector<Symbol>> tgt_symbols_map;
        for (const SymbolPost& move : delta[source]) {
            is_state_drawn[source] = true;
            for (State target : move.targets) {
                is_state_drawn[target] = true;
                tgt_symbols_map[target].insert(move.symbol);
            }
        }
        for (const auto& [target, symbols] : tgt_symbols_map) {
            if (max_label_length == 0) {
                output << source << " -> " << target << ";" << std::endl;
                continue;
            }

            std::string label = use_intervals
                                    ? vec_of_symbols_to_string_with_intervals(symbols)
                                    : vec_of_symbols_to_string(symbols);
            std::string on_hover_label = replace_all(replace_all(label, "<", "&lt;"), ">", "&gt;");
            bool is_shortened = false;
            if (max_label_length > 0 && label.length() > static_cast<size_t>(max_label_length)) {
                label.replace(static_cast<size_t>(max_label_length), std::string::npos, "...");
                is_shortened = true;
            }

            if (is_shortened) {
                output << source << " -> " << target << " [label=\"" << label << "\", tooltip=\"" << on_hover_label <<
                        "\"];" << std::endl;
            } else { output << source << " -> " << target << " [label=\"" << label << "\"];" << std::endl; }
        }
    }

    // Circle for isolated states with no transitions
    for (State state{ 0 }; state < is_state_drawn.size(); ++state) {
        if (!is_state_drawn[state]) { output << state << " [shape=circle];" << std::endl; }
    }

    // Levels for each state
    output << "node [shape=none, label=\"\"];" << std::endl;
    output << "forcelabels=true;" << std::endl;
    for (State s{ 0 }; s < levels.size(); s++) {
        output << s << " [label=\"" << s << ":" << levels[s] << "\"];" << std::endl;
    }

    // Arrow for initial states
    for (const State init_state : initial) { output << "i" << init_state << " -> " << init_state << ";" << std::endl; }

    output << "}" << std::endl;
}

void Nft::print_to_dot(
    const std::string& filename, const bool decode_ascii_chars, const bool use_intervals, const int max_label_length,
    const Alphabet* alphabet) const {
    std::ofstream output(filename);
    if (!output) { throw std::ios_base::failure("Failed to open file: " + filename); }
    print_to_dot(output, decode_ascii_chars, use_intervals, max_label_length, alphabet);
}

std::string Nft::print_to_mata() const {
    std::stringstream output;
    print_to_mata(output);
    return output.str();
}

void Nft::print_to_mata(std::ostream& output) const {
    output << "@NFT-explicit" << std::endl
            << "%Alphabet-auto" << std::endl;
    // TODO should be this, but we cannot parse %Alphabet-numbers yet
    //<< "%Alphabet-numbers" << std::endl;

    if (!initial.empty()) {
        output << "%Initial";
        for (const State init_state : initial) { output << " q" << init_state; }
        output << std::endl;
    }

    if (!final.empty()) {
        output << "%Final";
        for (const State final_state : final) { output << " q" << final_state; }
        output << std::endl;
    }

    if (!levels.empty()) {
        BoolVector live_states(num_of_states(), false);
        for (const State& s : initial) { live_states[s] = true; }
        for (const State& s : final) { live_states[s] = true; }
        for (const Transition& trans : delta.transitions()) {
            live_states[trans.source] = true;
            live_states[trans.target] = true;
        }
        output << "%Levels";
        for (State s{ 0 }; s < num_of_states(); s++) {
            if (live_states[s]) { output << " " << "q" << s << ":" << levels[s]; }
        }
        output << std::endl;
    }
    output << "%LevelsNum " << levels.num_of_levels << std::endl;

    for (const Transition& trans : delta.transitions()) {
        output << "q" << trans.source << " "
                << ((alphabet != nullptr)
                        ? alphabet->reverse_translate_symbol(trans.symbol)
                        : ((this->alphabet != nullptr)
                               ? this->alphabet->reverse_translate_symbol(trans.symbol)
                               : std::to_string(trans.symbol)))
                << " q" << trans.target << std::endl;
    }
}

void Nft::print_to_mata(const std::string& filename) const {
    std::ofstream output(filename);
    if (!output) { throw std::ios_base::failure("Failed to open file: " + filename); }
    print_to_mata(output);
}

Nft Nft::get_one_letter_aut(const std::set<Level>& levels_to_keep, const Symbol abstract_symbol) const {
    Nft one_symbol_transducer{ with_levels(levels, num_of_states(), initial, final) };
    for (nfa::State source_state = 0; source_state < delta.num_of_states(); ++source_state) {
        nfa::StatePost& state_post_of_new_source_state = one_symbol_transducer.delta.mutable_state_post(source_state);
        if (levels_to_keep.contains(levels[source_state])) {
            state_post_of_new_source_state = delta[source_state];
        } else {
            nfa::SymbolPost new_transition{ abstract_symbol };
            for (const nfa::SymbolPost& symbol_post : delta[source_state]) {
                if (symbol_post.symbol == EPSILON) { state_post_of_new_source_state.insert(symbol_post); } else {
                    new_transition.targets.insert(symbol_post.targets);
                }
            }
            if (!new_transition.targets.empty()) { state_post_of_new_source_state.insert(new_transition); }
        }
    }
    return one_symbol_transducer;
}

void Nft::get_one_letter_aut(Nft& result) const { result = get_one_letter_aut(); }

StateSet Nft::post(const StateSet& states, const Symbol symbol, const EpsilonClosureOpt epsilon_closure_opt) const {
    std::cerr << "Warning: Nft::post uses Nfa::post, which is not designed for NFT's jump transitions" << std::endl;
    return Nfa::post(states, symbol, epsilon_closure_opt);
}

void Nft::unwind_jumps_inplace(
    const OrdVector<Symbol>& dont_care_symbol_replacements, const JumpMode jump_mode) {
    const bool dont_care_for_dont_care = dont_care_symbol_replacements == utils::OrdVector<Symbol>({ DONT_CARE });
    std::vector<Transition> transitions_to_del;
    std::vector<Transition> transitions_to_add;

    auto add_inner_transitions = [&](State src, Symbol symbol, State trg) {
        if (symbol == DONT_CARE && !dont_care_for_dont_care) {
            for (const Symbol replace_symbol : dont_care_symbol_replacements) {
                transitions_to_add.emplace_back(src, replace_symbol, trg);
            }
        } else { transitions_to_add.emplace_back(src, symbol, trg); }
    };

    for (const auto& transition : delta.transitions()) {
        const Level src_lvl = levels[transition.source];
        const Level trg_lvl = levels[transition.target];

        if (const Level diff_lvl = (trg_lvl == 0)
                                       ? (static_cast<Level>(levels.num_of_levels) - src_lvl)
                                       : trg_lvl - src_lvl;
            diff_lvl == 1 && transition.symbol == DONT_CARE && !dont_care_for_dont_care) {
            transitions_to_del.push_back(transition);
            for (const Symbol replace_symbol : dont_care_symbol_replacements) {
                transitions_to_add.emplace_back(transition.source, replace_symbol, transition.target);
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
            for (const Level pre_trg_lvl = (trg_lvl == 0)
                                               ? (static_cast<Level>(levels.num_of_levels) - 1)
                                               : (trg_lvl - 1);
                 inner_src_lvl < pre_trg_lvl; inner_src_lvl++, inner_trg_lvl++) {
                inner_trg = add_state();
                levels[inner_trg] = inner_trg_lvl;

                add_inner_transitions(
                    inner_src, jump_mode == JumpMode::AppendDontCares ? DONT_CARE : transition.symbol, inner_trg
                );
                inner_src = inner_trg;
            }

            // The last iteration connecting last inner state with the original target state.
            add_inner_transitions(
                inner_src, jump_mode == JumpMode::AppendDontCares ? DONT_CARE : transition.symbol, transition.target
            );
        }
    }

    for (const Transition& transition : transitions_to_add) { delta.add(transition); }
    for (const Transition& transition : transitions_to_del) { delta.remove(transition); }
}

Nft Nft::unwind_jumps(const OrdVector<Symbol>& dont_care_symbol_replacements, const JumpMode jump_mode) const {
    Nft result{ *this };
    // HACK. Works only for automata without levels.
    if (result.levels.size() != result.num_of_states()) { return result; }
    result.unwind_jumps_inplace(dont_care_symbol_replacements, jump_mode);
    return result;
}

void Nft::unwind_jumps(
    Nft& result, const OrdVector<Symbol>& dont_care_symbol_replacements, const JumpMode jump_mode) const {
    result = unwind_jumps(dont_care_symbol_replacements, jump_mode);
}

Nft& Nft::operator=(Nft&& other) noexcept {
    if (this != &other) {
        Nfa::operator=(other);
        levels = std::move(other.levels);
        levels.num_of_levels = other.levels.num_of_levels;
    }
    return *this;
}


Nft& Nft::operator=(const Nfa& other) noexcept {
    if (this != &other) {
        Nfa::operator=(other);
        levels = Levels(num_of_states(), DEFAULT_LEVEL);
        levels.num_of_levels = 1;
    }
    return *this;
}

Nft& Nft::operator=(Nfa&& other) noexcept {
    if (this != &other) {
        Nfa::operator=(std::move(other));
        levels = Levels(num_of_states(), DEFAULT_LEVEL);
        levels.num_of_levels = 1;
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

State Nft::insert_word(const State source, const Word& word, const State target) {
    // Ensure both states exist in the NFT.
    add_state(std::max(source, target));
    if (levels[source] != levels[target]) {
        throw std::invalid_argument{ "Inserting word between source and target states with different levels." };
    }

    const State first_new_state = num_of_states();
    const State word_target = Nfa::insert_word(source, word, target);
    const size_t num_of_states_after = num_of_states();
    const Level source_level = levels[source];

    Level lvl = (levels.num_of_levels == 1)
                    ? source_level
                    : (source_level + 1) % static_cast<Level>(levels.num_of_levels);
    for (State state{ first_new_state }; state < num_of_states_after;
         ++state, lvl = (lvl + 1) % static_cast<Level>(levels.num_of_levels)) { add_state_with_level(state, lvl); }

    assert(levels[word_target] == 0 || levels[num_of_states_after - 1] < levels[word_target]);

    return word_target;
}

State Nft::insert_word(const State source, const Word& word) {
    if (num_of_states() <= source) { add_state(source); }
    return insert_word(source, word, add_state_with_level(levels[source]));
}

State Nft::insert_word_by_levels(
    const State source, const std::vector<Word>& word_parts_on_levels, const State target) {
    assert(word_parts_on_levels.size() == levels.num_of_levels);
    assert(source < num_of_states());
    assert(target < num_of_states());
    assert(source < levels.size());
    assert(target < levels.size());
    assert(levels[source] == levels[target]);
    const Level from_to_level{ levels[source] };

    if (levels.num_of_levels == 1) { return insert_word(source, word_parts_on_levels[0], target); }

    std::vector<Word::const_iterator> word_part_it_v(levels.num_of_levels);
    for (Level lvl{ from_to_level }, i{ 0 }; i < static_cast<Level>(levels.num_of_levels); ++i) {
        word_part_it_v[lvl] = word_parts_on_levels[lvl].begin();
        lvl = (lvl + 1) % static_cast<Level>(levels.num_of_levels);
    }

    // This function retrieves the next symbol from a word part at a specified level and advances the corresponding iterator.
    // Returns EPSILON when the iterator reaches the end of the word part.
    auto get_next_symbol = [&](const Level lvl) {
        if (word_part_it_v[lvl] == word_parts_on_levels[lvl].end()) { return EPSILON; }
        return *(word_part_it_v[lvl]++);
    };

    // Add transition source --> inner_state.
    Level inner_lvl = (levels.num_of_levels == 1) ? 0 : (from_to_level + 1) % static_cast<Level>(levels.num_of_levels);
    State inner_state = add_state_with_level(inner_lvl);
    delta.add(source, get_next_symbol(from_to_level), inner_state);

    // Add transition inner_state --> inner_state
    State prev_state = inner_state;
    Level prev_lvl = inner_lvl;
    const size_t max_word_part_len = std::ranges::max_element(
        word_parts_on_levels,
        [](const Word& a, const Word& b) { return a.size() < b.size(); }
    )->size();
    if (const size_t word_total_len = levels.num_of_levels * max_word_part_len; word_total_len != 0) {
        for (size_t symbol_idx{ 1 }; symbol_idx < word_total_len - 1; symbol_idx++) {
            inner_lvl = (prev_lvl + 1) % static_cast<Level>(levels.num_of_levels);
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

State Nft::insert_word_by_levels(const State source, const std::vector<Word>& word_parts_on_levels) {
    assert(source < levels.size());
    return insert_word_by_levels(source, word_parts_on_levels, add_state_with_level(levels[source]));
}

State Nft::add_transition(const State source, const std::vector<Symbol>& symbols, const State target) {
    return insert_word(source, symbols, target);
}

State Nft::add_transition(const State source, const std::vector<Symbol>& symbols) {
    return insert_word(source, symbols);
}

Nft& Nft::insert_identity(const State state, const std::vector<Symbol>& symbols, const JumpMode jump_mode) {
    for (const Symbol symbol : symbols) { insert_identity(state, symbol, jump_mode); }
    return *this;
}

Nft& Nft::insert_identity(const State state, const Alphabet* alphabet, const JumpMode jump_mode) {
    for (const Symbol symbol : alphabet->get_alphabet_symbols()) { insert_identity(state, symbol, jump_mode); }
    return *this;
}

Nft& Nft::insert_identity(const State state, const Symbol symbol, const JumpMode jump_mode) {
    (void) jump_mode;
    // TODO(nft): Evaluate the performance difference between adding a jump transition and inserting a transition for each level.
    // FIXME(nft): Allow symbol jump transitions?
    //    if (jump_mode == JumpMode::RepeatSymbol) {
    //        delta.add(state, symbol, state);
    //        insert_word(state, Word(levels.num_of_levels, symbol), state);
    //    } else {
    insert_word(state, Word(levels.num_of_levels, symbol), state);
    //    }
    return *this;
}

bool Nft::contains_jump_transitions() const {
    if (levels.num_of_levels == 1) { return false; }

    for (const Transition& transition : delta.transitions()) {
        const Level src_level{ levels[transition.source] };
        Level tgt_level{ levels[transition.target] };
        if (tgt_level == 0) {
            // we want to check if the difference between src and tgt levels is at most 1 modulo levels.num_of_levels
            tgt_level = tgt_level + static_cast<Level>(levels.num_of_levels);
        }
        if (tgt_level - src_level != 1) { return true; }
    }
    return false;
}

void Nft::clear() {
    Nfa::clear();
    levels.clear();
}

bool Nft::is_identical(const Nft& aut) const {
    return levels.num_of_levels == aut.levels.num_of_levels && levels == aut.levels &&
           Nfa::is_identical(aut);
}


mata::nfa::Nfa Nft::to_nfa_update_copy(
    const OrdVector<Symbol>& dont_care_symbol_replacements, const JumpMode jump_mode) const {
    return unwind_jumps(dont_care_symbol_replacements, jump_mode).to_nfa_copy();
}

mata::nfa::Nfa Nft::to_nfa_update_move(
    const OrdVector<Symbol>& dont_care_symbol_replacements, const JumpMode jump_mode) {
    unwind_jumps_inplace(dont_care_symbol_replacements, jump_mode);
    return to_nfa_move();
}

Nft Nft::apply(
    const Nfa& nfa, const Level level_to_apply_on, const bool project_out_applied_level,
    const JumpMode jump_mode) const {
    return compose(Nft{ nfa }, *this, 0, level_to_apply_on, project_out_applied_level, jump_mode);
}

Nft Nft::apply(
    const Word& word, const Level level_to_apply_on, const bool project_out_applied_level,
    const JumpMode jump_mode) const {
    return apply(nfa::builder::create_single_word_nfa(word), level_to_apply_on, project_out_applied_level, jump_mode);
}

bool Nft::make_complete(
    const Alphabet* const alphabet,
    const OrdVector<Symbol>& epsilons,
    const std::optional<std::vector<State>>& sink_states) {
    return make_complete(get_symbols_to_work_with(*this, alphabet), epsilons, sink_states);
}

bool Nft::make_complete(
    const OrdVector<Symbol>& symbols,
    const OrdVector<Symbol>& epsilons,
    const std::optional<std::vector<State>>& sink_states) {
    const size_t num_of_states_orig{ this->num_of_states() };
    if (num_of_states_orig == 0) { return false; }

    const std::vector<State> sinks{
        [&] {
            auto sinks_val{ sink_states.value_or(std::vector<State>{}) };
            if (sinks_val.empty()) {
                for (Level level{ 0 }; level < levels.num_of_levels; ++level) {
                    sinks_val.push_back(add_state_with_level(level));
                }
            }
            assert(
                sinks_val.size() == levels.num_of_levels &&
                "Nft::make_complete: sink_states size must be equal to num_of_levels."
            );

            assert( // NOLINT(*-assert-side-effect)
                [&]{
                    for (Level level{ 0 }; level < levels.num_of_levels; ++level) {
                        const State sink_state{ sinks_val[level] };
                        if (sink_state >= this->num_of_states() || levels[sink_state] != level) {
                            return false;
                        }
                    }
                    return true;
                }() && "Nft::make_complete: sink_states must have correct levels and exist in the NFT."
            );
            return sinks_val;
        }()
    };

    auto next_level = [&](const Level level) {
        return (levels.num_of_levels == 1) ? level : (level + 1) % static_cast<Level>(levels.num_of_levels);
    };

    // Add missing transitions from original states to sink states.
    OrdVector<Symbol> used_symbols{};
    bool transition_added{ false };
    for (State state{ 0 }; state < num_of_states_orig; ++state) {
        used_symbols.clear();
        for (const SymbolPost& symbol_post : delta[state]) { used_symbols.insert(symbol_post.symbol); }

        auto add_transitions_to_sinks = [&](const auto& symbols_to_add) {
            for (const Symbol symbol : symbols_to_add) {
                delta.add(state, symbol, sinks[next_level(levels[state])]);
                transition_added = true;
            }
        };
        add_transitions_to_sinks(symbols.difference(used_symbols));
        add_transitions_to_sinks(epsilons.difference(used_symbols));
    }

    // Add loops on sink states (from sink to a sink of the next level).
    if (transition_added && num_of_states_orig < this->num_of_states()) {
        auto add_transitions_between_sinks = [&](const auto& symbols_to_add) {
            for (Level level{ 0 }; level < levels.num_of_levels; ++level) {
                for (const Symbol symbol : symbols_to_add) {
                    delta.add(sinks[level], symbol, sinks[next_level(level)]);
                }
            }
        };
        add_transitions_between_sinks(symbols);
        add_transitions_between_sinks(epsilons);
    }

    return transition_added;
}
