/* nfa-strings.hh -- Operations on NFAs for string solving.
 */

#include <utility>

#include "mata/nft/strings.hh"
#include "mata/alphabet.hh"
#include "mata/nft/nft.hh"
#include "mata/nft/builder.hh"

using namespace mata;
using mata::Symbol;
using mata::nfa::SymbolPost;
using mata::nfa::StatePost;
using mata::nfa::Nfa;
using namespace mata::nft;
using mata::nft::State;
using mata::nft::Level;
using mata::nft::Nft;
using nft::strings::ReplaceMode;
using nft::strings::ReluctantReplace;

namespace {
    template<class Sequence>
    bool is_subsequence(const Sequence& subsequence, const Sequence& sequence) {
        assert(subsequence.size() <= sequence.size());
        for (size_t i{ 0 }; const Symbol symbol: subsequence) {
            if (symbol != sequence[i]) { return false; }
            ++i;
        }
        return true;
    }

    void add_end_marker_transitions_from_literal_states(
        const Symbol end_marker, const std::vector<std::pair<State, Word>>& state_word_pairs, Nft& nft) {
        auto state_pair_end{ state_word_pairs.end() };
        State state_lvl0, state_lvl1;
        // Skip the last state-word pair for the whole literal as it must first apply the replacement before accepting the end marker.
        for (auto state_word_pair_it{ state_word_pairs.begin() };
             state_word_pair_it + 1 < state_pair_end; ++state_word_pair_it) {
            const auto& [state, word]{ *state_word_pair_it };
            auto word_it{ word.begin() };
            auto word_end{ word.end() };
            state_lvl0 = state;
            state_lvl1 = nft.add_state_with_level(1);
            nft.delta.add(state_lvl0, end_marker, state_lvl1);
            for (; word_it + 1 < word_end; ++word_it) {
                state_lvl0 = nft.add_state_with_level(0);
                nft.delta.add(state_lvl1, *word_it, state_lvl0);
                state_lvl1 = nft.add_state_with_level(1);
                nft.delta.add(state_lvl0, EPSILON, state_lvl1);
            }
            nft.delta.add(state_lvl1, word_it == word_end ? EPSILON : *word_it, *nft.final.begin());
        }
    }

    void handle_last_symbol_replacement(const Symbol end_marker, const ReplaceMode& replace_mode, Nft& nft,
                                        const utils::OrdVector<Symbol>& alphabet_symbols,
                                        State state_lvl1, Symbol replacement) {
        switch (replace_mode) {
            case ReplaceMode::All: {
                nft.delta.add(state_lvl1, replacement, *nft.initial.begin());
                break;
            }
            case ReplaceMode::Single: {
                State state_lvl0{ nft.add_state_with_level(0) };
                nft.delta.add(state_lvl1, replacement, state_lvl0);
                nft.insert_identity(state_lvl0, alphabet_symbols.to_vector());
                state_lvl1 = nft.add_state_with_level(1);
                nft.delta.add(state_lvl0, end_marker, state_lvl1);
                nft.delta.add(state_lvl1, EPSILON, *nft.final.begin());
                break;
            }
            default: {
                throw std::runtime_error("Unhandled replace mode.");
            }
        }
    }

    void add_replacement_transitions(const Word& replacement, const Symbol end_marker, const ReplaceMode& replace_mode,
                                     const std::vector<std::pair<State, Word>>& state_word_pairs, Nft& nft,
                                     const utils::OrdVector<Symbol>& alphabet_symbols) {
        auto replacement_end{ replacement.end() };
        State state_lvl0{ state_word_pairs.back().first };
        if (replacement.empty()) {
            State state_lvl1{ nft.add_state_with_level(1) };
            nft.delta.add(state_lvl0, EPSILON, state_lvl1);
            handle_last_symbol_replacement(end_marker, replace_mode, nft, alphabet_symbols, state_lvl1, EPSILON);
        }
        for (auto replacement_it{ replacement.begin() }; replacement_it < replacement_end; ++replacement_it) {
            State state_lvl1{ nft.add_state_with_level(1) };
            nft.delta.add(state_lvl0, EPSILON, state_lvl1);
            if (replacement_it + 1 == replacement_end) {
                handle_last_symbol_replacement(end_marker, replace_mode, nft, alphabet_symbols, state_lvl1, *replacement_it);
            } else {
                state_lvl0 = nft.add_state_with_level(0);
                nft.delta.add(state_lvl1, *replacement_it, state_lvl0);
            }
        }
    }

    void add_generic_literal_transitions(const Word& literal, const std::vector<std::pair<State, Word>>& state_word_pairs,
                                    Nft& nft, const utils::OrdVector<Symbol>& alphabet_symbols) {
        const size_t literal_size{ literal.size() };
        Symbol literal_symbol;
        for (size_t i{ 0 }; i < literal_size; ++i) {
            literal_symbol = literal[i];
            const auto& [word_state, subword] = state_word_pairs[i];
            for (Symbol symbol: alphabet_symbols) {
                State target_state{ 0 };
                if (symbol == literal_symbol) { // Handle transition to next subword init_state.
                    State middle{ nft.add_state_with_level(1) };
                    nft.delta.add(word_state, literal_symbol, middle);
                    nft.delta.add(middle, EPSILON, state_word_pairs[i + 1].first);
                } else { // Add back transitions.
                    Word subword_next_symbol = subword;
                    subword_next_symbol.push_back(symbol);
                    const auto subword_next_symbol_begin{ subword_next_symbol.begin() };
                    const auto subword_next_symbol_end{ subword_next_symbol.end() };
                    auto subword_next_symbol_it{ subword_next_symbol_begin };
                    while (subword_next_symbol_it != subword_next_symbol_end) {
                        const Word subsubword{ subword_next_symbol_it, subword_next_symbol_end };
                        if (is_subsequence(subsubword, literal)) {
                            // it...end is a valid literal subvector. Transition should therefore lead to the corresponding
                            //  subvector init_state.
                            target_state = subsubword.size();
                            break;
                        }
                        ++subword_next_symbol_it;
                    }

                    // Output all buffered symbols up until the new buffered content (subsubword).
                    auto subword_next_symbol_it_from_begin = subword_next_symbol.begin();
                    State state_lvl0{ word_state };
                    for (;
                        subword_next_symbol_it_from_begin <
                        subword_next_symbol_it; ++subword_next_symbol_it_from_begin) {
                        State state_lvl1 = nft.add_state_with_level(1);
                        nft.delta.add(state_lvl0, symbol, state_lvl1);
                        symbol = EPSILON;
                        if (subword_next_symbol_it_from_begin + 1 == subword_next_symbol_it) {
                            nft.delta.add(state_lvl1, *subword_next_symbol_it_from_begin,
                                          target_state);
                        } else {
                            state_lvl0 = nft.add_state_with_level(0);
                            nft.delta.add(state_lvl1, *subword_next_symbol_it_from_begin, state_lvl0);
                        }
                    }
                }
            }
        }
    }

    /// Add transitions, optionally add @p source to @p dfa_generic_end_marker.final, and update @p labeling and @p labeling_inv functions.
    void process_source(const nfa::Nfa& regex, const utils::OrdVector<Symbol>& alphabet_symbols, nfa::Nfa& dfa_generic_end_marker,
                        std::map<State, StateSet>& labeling,
                        std::unordered_map<StateSet, State>& labeling_inv, State source,
                        StateSet& source_label, std::vector<State>& worklist) {
        const State generic_initial_state{ *regex.initial.begin() };
        for (const Symbol symbol: alphabet_symbols) {
            StateSet target_label{ generic_initial_state };
            for (const State regex_state: source_label) {
                const StatePost& state_post{ regex.delta[regex_state] };
                auto symbol_post_it{ state_post.find(symbol) };
                if (symbol_post_it == state_post.end()) { continue; }
                target_label.insert(symbol_post_it->targets);
            }
            auto target_it{ labeling_inv.find(target_label) };
            State target;
            if (target_it == labeling_inv.end()) {
                target = dfa_generic_end_marker.add_state();
                labeling.emplace(target, target_label);
                labeling_inv.emplace(target_label, target);
                worklist.push_back(target);
            } else {
                target = target_it->second;
            }
            dfa_generic_end_marker.delta.add(source, symbol, target);
        }
        dfa_generic_end_marker.final.insert(source);
    }
}

Nft mata::nft::strings::create_identity(mata::Alphabet* alphabet, const size_t num_of_levels) {
    if (num_of_levels == 0) { throw std::runtime_error("NFT must have at least one level"); }
    const auto alphabet_symbols{ alphabet->get_alphabet_symbols() };
//    const size_t additional_states_per_symbol_num{ num_of_levels - 1 };
//    const size_t num_of_states{ alphabet_symbols.size() * additional_states_per_symbol_num + 1 };
    Nft nft{ 1, { 0 }, { 0 }, { 0 }, num_of_levels, alphabet };
    nft.insert_identity(0, alphabet_symbols.to_vector());
    return nft;
}

Nft mata::nft::strings::create_identity_with_single_symbol_replace(
    Alphabet* alphabet, const Symbol from_symbol, const Symbol replacement, const ReplaceMode replace_mode) {
    return create_identity_with_single_symbol_replace(alphabet, from_symbol, Word{ replacement }, replace_mode);
}

Nft nft::strings::create_identity_with_single_symbol_replace(Alphabet* alphabet, const Symbol from_symbol,
                                                             const Word& replacement, const ReplaceMode replace_mode) {
    Nft nft{ create_identity(alphabet) };
    if (alphabet->empty()) { throw std::runtime_error("Alphabet does not contain symbol being replaced."); }
    auto symbol_post_to_state_with_replace{ nft.delta.mutable_state_post(0).find(from_symbol) };
    State state_lvl1{ symbol_post_to_state_with_replace->targets.front() };
    nft.delta.mutable_state_post(state_lvl1).clear();
    const auto replacement_end{ replacement.end() };
    auto replacement_it{ replacement.begin() };
    State state_lvl0;
    for (; replacement_it < replacement_end; ++replacement_it) {
        if (replacement_it + 1 == replacement_end) { break; }
        state_lvl0 = nft.add_state_with_level(0);
        nft.delta.add(state_lvl1, *replacement_it, state_lvl0);
        state_lvl1 = nft.add_state_with_level(1);
        nft.delta.add(state_lvl0, EPSILON, state_lvl1);
    }
    switch (replace_mode) {
        case ReplaceMode::All: {
            nft.delta.add(state_lvl1,
                          replacement_it == replacement_end ? EPSILON : *replacement_it,
                          *nft.initial.begin());
            break;
        }
        case ReplaceMode::Single: {
            const State after_replace_state{ nft.add_state_with_level(0) };
            nft.delta.add(state_lvl1,
                          replacement_it == replacement_end ? EPSILON : *replacement_it,
                          after_replace_state);
            nft.insert_identity(after_replace_state, alphabet->get_alphabet_symbols().to_vector());
            nft.final.insert(after_replace_state);
            break;
        }
        default: {
            throw std::runtime_error("Unhandled replace mode.");
        }
    }
    return nft;
}

Nft mata::nft::strings::replace_reluctant_regex(
    const std::string& regex,
    const Word& replacement,
    Alphabet* alphabet,
    ReplaceMode replace_mode,
    Symbol begin_marker
) {
    return replace_reluctant_regex(nfa::builder::create_from_regex(regex), replacement, alphabet, replace_mode, begin_marker);
}

Nft mata::nft::strings::replace_reluctant_regex(
    nfa::Nfa regex,
    const Word& replacement,
    Alphabet* alphabet,
    ReplaceMode replace_mode,
    Symbol begin_marker
) {
    return ReluctantReplace::replace_regex(std::move(regex), replacement, alphabet, replace_mode, begin_marker);
}

nfa::Nfa ReluctantReplace::end_marker_dfa(nfa::Nfa regex) {
    if (!regex.is_deterministic()) { regex = determinize(regex); }
    State new_final;
    for (State orig_final: regex.final) {
        new_final = regex.add_state();
        regex.final.insert(new_final);
        regex.final.erase(orig_final);
        StatePost::Moves orig_moves{ regex.delta[orig_final].moves() };
        std::vector<Move> moves{ orig_moves.begin(), orig_moves.end() };
        for (const Move& move: moves) {
            regex.delta.remove(orig_final, move.symbol, move.target);
            regex.delta.add(new_final, move.symbol, move.target);
        }
        regex.delta.add(orig_final, EPSILON, new_final);
    }
    return regex;
}

Nft ReluctantReplace::marker_nft(const nfa::Nfa& marker_dfa, Symbol marker) {
    Nft dft_marker{ nft::builder::from_nfa_with_levels_zero(marker_dfa) };
    const size_t dft_marker_num_of_states{ dft_marker.num_of_states() };
    for (State source{ 0 }; source < dft_marker_num_of_states; ++source) {
        for (const Move& move: dft_marker.delta[source].moves_epsilons()) {
            const State marker_state{ dft_marker.add_state() };
            dft_marker.levels.resize(marker_state + 1);
            dft_marker.levels[marker_state] = 1;
            SymbolPost& symbol_post{ *dft_marker.delta.mutable_state_post(source).find(move.symbol) };
            symbol_post.targets.erase(move.target);
            symbol_post.targets.insert(marker_state);
            dft_marker.delta.add(marker_state, marker, move.target);
        }
    }
    return dft_marker;
}

nfa::Nfa ReluctantReplace::generic_marker_dfa(const std::string& regex, Alphabet* alphabet) {
    return generic_marker_dfa(nfa::builder::create_from_regex(regex), alphabet);
}

nfa::Nfa ReluctantReplace::generic_marker_dfa(nfa::Nfa regex, Alphabet* alphabet) {
    if (!regex.is_deterministic()) { regex = determinize(regex); }

    const utils::OrdVector<Symbol> alphabet_symbols{ alphabet->get_alphabet_symbols() };
    nfa::Nfa dfa_generic_end_marker{};
    dfa_generic_end_marker.initial.insert(0);
    std::map<State, StateSet> labeling{};
    std::unordered_map<StateSet, State> labeling_inv{};
    labeling.emplace(0, *regex.initial.begin());
    labeling_inv.emplace(*regex.initial.begin(), 0);

    std::vector<State> worklist{ 0 };
    while (!worklist.empty()) {
        State source{ worklist.back() };
        worklist.pop_back();
        StateSet& source_label{ labeling[source] };

        if (regex.final.intersects_with(source_label)) {
            const State end_marker_target{ dfa_generic_end_marker.add_state() };
            dfa_generic_end_marker.delta.add(source, EPSILON, end_marker_target);
            process_source(regex, alphabet_symbols, dfa_generic_end_marker, labeling, labeling_inv, end_marker_target,
                           source_label, worklist);
        } else {
            process_source(regex, alphabet_symbols, dfa_generic_end_marker, labeling, labeling_inv, source, source_label,
                           worklist);
        }

    }

    return dfa_generic_end_marker;
}

nfa::Nfa ReluctantReplace::begin_marker_nfa(const std::string& regex, Alphabet* alphabet) {
    return begin_marker_nfa(nfa::builder::create_from_regex(regex), alphabet);
}

nfa::Nfa ReluctantReplace::begin_marker_nfa(nfa::Nfa regex, Alphabet* alphabet) {
    regex = revert(regex);
    nfa::Nfa dfa_generic_end_marker{ generic_marker_dfa(std::move(regex), alphabet) };
    dfa_generic_end_marker = revert(dfa_generic_end_marker);
    std::swap(dfa_generic_end_marker.initial, dfa_generic_end_marker.final);
    return dfa_generic_end_marker;
}

Nft ReluctantReplace::begin_marker_nft(const nfa::Nfa& marker_nfa, Symbol begin_marker) {
    Nft begin_marker_nft{ marker_nft(marker_nfa, begin_marker) };
    const State new_initial{ begin_marker_nft.add_state_with_level(0) };
    for (const State orig_final: begin_marker_nft.final) {
        begin_marker_nft.delta.add(new_initial, EPSILON, orig_final);
    }
    begin_marker_nft.final = begin_marker_nft.initial;
    begin_marker_nft.initial = { new_initial };
    return begin_marker_nft;
}

Nft ReluctantReplace::end_marker_dft(const nfa::Nfa& end_marker_dfa, const Symbol end_marker) {
    return marker_nft(end_marker_dfa, end_marker);
}

nfa::Nfa ReluctantReplace::reluctant_nfa_with_marker(nfa::Nfa nfa, const Symbol marker, Alphabet* alphabet) {
    // Convert to reluctant NFA.
    nfa = mata::strings::reluctant_nfa(nfa);

    // Add marker self-loops to accept begin markers inside the shortest match.
    for (State state{ 0 }; state < nfa.num_of_states(); ++state) {
        nfa.delta.add(state, marker, state);
    }

    // Intersect with NFA to avoid removing the next begin marker which might be used for the next replace.
    // TODO(nft): Could be optimised.
    nfa::Nfa nfa_avoid_removing_next_begin_marker{ 2, { 0 }, { 0 } };
    StatePost& initial{ nfa_avoid_removing_next_begin_marker.delta.mutable_state_post(0) };
    const utils::OrdVector<Symbol> alphabet_symbols{ alphabet->get_alphabet_symbols() };
    for (const Symbol symbol: alphabet_symbols) {
        initial.emplace_back(symbol, 0);
    }
    StatePost& marker_state{ nfa_avoid_removing_next_begin_marker.delta.mutable_state_post(1) };
    nfa_avoid_removing_next_begin_marker.delta.add(0, marker, 1);
    for (const Symbol symbol: alphabet_symbols) {
        marker_state.emplace_back(symbol, 0);
    }
    nfa_avoid_removing_next_begin_marker.delta.add(1, marker, 1);
    // TODO(nft): Leaves a non-terminating begin_marker transitions in a form of a lasso from final states.
    //  These lassos should be removed to further optimize NFT creation.
    return mata::strings::reluctant_nfa(reduce(intersection(nfa, nfa_avoid_removing_next_begin_marker)));
}

Nft ReluctantReplace::reluctant_leftmost_nft(const std::string& regex, Alphabet* alphabet, Symbol begin_marker,
                                         const Word& replacement, ReplaceMode replace_mode) {
    return reluctant_leftmost_nft(nfa::builder::create_from_regex(regex), alphabet, begin_marker, replacement, replace_mode);
}

Nft ReluctantReplace::reluctant_leftmost_nft(nfa::Nfa nfa, Alphabet* alphabet, Symbol begin_marker,
                                         const Word& replacement, ReplaceMode replace_mode) {
    nfa = reluctant_nfa_with_marker(std::move(nfa), begin_marker, alphabet);
    Nft nft_reluctant_leftmost{
        nft::builder::from_nfa_with_levels_zero(nfa, 2, true, { EPSILON }) };
    const size_t regex_num_of_states{ nft_reluctant_leftmost.num_of_states() };
    const utils::OrdVector<Symbol> alphabet_symbols{ alphabet->get_alphabet_symbols() };
    nft_reluctant_leftmost.levels.resize(regex_num_of_states + replacement.size() * 2 + alphabet_symbols.size() + 4);

    // Create self-loop on the new initial state.
    const State initial{ nft_reluctant_leftmost.add_state_with_level(0) };
    nft_reluctant_leftmost.insert_identity(initial, alphabet_symbols.to_vector());

    // Move to replace mode when begin marker is encountered.
    State curr_state{ nft_reluctant_leftmost.num_of_states() };
    nft_reluctant_leftmost.delta.add(initial, begin_marker, curr_state);
    nft_reluctant_leftmost.delta.mutable_state_post(curr_state).push_back(
        SymbolPost{ EPSILON, StateSet{ nft_reluctant_leftmost.initial } }
    );
    nft_reluctant_leftmost.levels[curr_state] = 1;
    ++curr_state;

    // Start outputting replacement by epsilon transition from all regex final states.
    for (const State regex_final: nft_reluctant_leftmost.final) {
        nft_reluctant_leftmost.delta.add(regex_final, EPSILON, curr_state);
    }
    nft_reluctant_leftmost.levels[curr_state] = 1;
    // Output the replacement.
    curr_state = nft_reluctant_leftmost.insert_word_by_parts(curr_state, { {}, replacement });
    State next_state{ nft_reluctant_leftmost.add_state_with_level(0) };
    nft_reluctant_leftmost.delta.add(curr_state, EPSILON, next_state);
    nft_reluctant_leftmost.final.insert(next_state);
    nft_reluctant_leftmost.final.clear();
    switch (replace_mode) {
        case ReplaceMode::All: { // Return to beginning to possibly repeat replacement.
            nft_reluctant_leftmost.insert_word_by_parts(next_state, { { EPSILON }, { EPSILON } }, initial);
            break;
        };
        case ReplaceMode::Single: { // End the replacement mode.
            nft_reluctant_leftmost.levels.resize(nft_reluctant_leftmost.levels.size() + alphabet_symbols.size() + 1);
            const State final{ next_state };
            nft_reluctant_leftmost.final.insert(final);
            nft_reluctant_leftmost.insert_identity(final, alphabet_symbols.to_vector());
            nft_reluctant_leftmost.insert_word_by_parts(final, { { begin_marker }, { EPSILON } }, final);
            break;
        };
        default: {
            throw std::runtime_error("Unimplemented replace mode");
        }
    }

    nft_reluctant_leftmost.initial = { initial };
    nft_reluctant_leftmost.final.insert(initial);

    return nft_reluctant_leftmost;
}

Nft nft::strings::replace_reluctant_literal(const Word& literal, const Word& replacement, Alphabet* alphabet,
                                            strings::ReplaceMode replace_mode, Symbol end_marker) {
    return ReluctantReplace::replace_literal(literal, replacement, alphabet, replace_mode, end_marker);
}

Nft ReluctantReplace::replace_literal_nft(const Word& literal, const Word& replacement, const Alphabet* alphabet,
                                      const Symbol end_marker, ReplaceMode replace_mode) {
    Nft nft{};
    nft.num_of_levels = 2;
    State init_state{ nft.add_state_with_level(0) };
    nft.initial.insert(init_state);
    const std::vector<std::pair<State, Word>> state_word_pairs{
        [&]() {
            std::vector<std::pair<State, Word>> state_word_pairs{};
            state_word_pairs.emplace_back(init_state, Word{});
            const auto literal_begin{ literal.begin() };
            const auto literal_end{ literal.end() };
            for (auto literal_it{ literal_begin }; literal_it < literal_end; ++literal_it) {
                state_word_pairs.emplace_back(nft.add_state_with_level(0), Word{ literal_begin, literal_it + 1 });
            }
            return state_word_pairs;
        }() };
    const utils::OrdVector<Symbol> alphabet_symbols{ alphabet->get_alphabet_symbols() };
    add_generic_literal_transitions(literal, state_word_pairs, nft, alphabet_symbols);
    State final{ nft.add_state_with_level(0) };
    nft.final.insert(final);
    add_replacement_transitions(replacement, end_marker, replace_mode, state_word_pairs, nft, alphabet_symbols);
    add_end_marker_transitions_from_literal_states(end_marker, state_word_pairs, nft);
    return nft;
}

Nft nft::strings::replace_reluctant_single_symbol(Symbol from_symbol, const Word& replacement, mata::Alphabet* alphabet,
                                                  ReplaceMode replace_mode) {
    return ReluctantReplace::replace_symbol(from_symbol, replacement, alphabet, replace_mode);
}

Nft nft::strings::replace_reluctant_single_symbol(Symbol from_symbol, Symbol replacement, mata::Alphabet* alphabet,
                                                  ReplaceMode replace_mode) {
    return ReluctantReplace::replace_symbol(from_symbol, replacement, alphabet, replace_mode);
}

Nft ReluctantReplace::replace_regex(nfa::Nfa regex, const Word& replacement, Alphabet* alphabet,
                                              ReplaceMode replace_mode, Symbol begin_marker) {
    // SMT-LIB theory Strings (Unicode Strings): Semantics for matching empty words in replace regex functions:
    // ; (str.replace_re s r t) is the string obtained by replacing the
    // ; shortest leftmost match of r in s, if any, by t.
    // ; Note that if the language of r contains the empty string,
    // ; the result is to prepend t to s.
    // (str.replace_re String RegLan String String)
    //
    // ; (str.replace_re_all s r t) is the string obtained by replacing,
    // ; left-to right, each shortest *non-empty* match of r in s by t.
    // (str.replace_re_all String RegLan String String)
    if (replace_mode == ReplaceMode::All) {
        // Removing the empty string from the regex.
        regex.unify_initial(true).final.erase(*regex.initial.begin());
    }

    ReluctantReplace reluctant_replace{};
    // TODO(nft): Add optional bool parameter to revert whether to swap initial and final states.
    Nft dft_begin_marker{ reluctant_replace.begin_marker_nft(reluctant_replace.begin_marker_nfa(regex, alphabet), begin_marker) };
    Nft nft_reluctant_replace{
        reluctant_replace.reluctant_leftmost_nft(std::move(regex), alphabet, begin_marker, replacement, replace_mode) };
    return compose(dft_begin_marker, nft_reluctant_replace);
}

Nft ReluctantReplace::replace_literal(const Word& literal, const Word& replacement, Alphabet* alphabet,
                                                ReplaceMode replace_mode, Symbol end_marker) {
    // SMT-LIB theory Strings (Unicode Strings): Semantics for matching empty words in replace literal functions:
    // ; Replace
    // ; (str.replace s t t') is the string obtained by replacing the first
    // ; occurrence of t in s, if any, by t'. Note that if t is empty, the
    // ; result is to prepend t' to s; also, if t does not occur in s then
    // ; the result is s.
    // (str.replace String String String String)
    //
    // ; (str.replace_all s t t’) is s if t is the empty string. Otherwise, it
    // ; is the string obtained from s by replacing all occurrences of t in s
    // ; by t’, starting with the first occurrence and proceeding in
    // ; left-to-right order.
    // (str.replace_all String String String String)
    if (replace_mode == ReplaceMode::All && literal == Word{}) {
        return nft::strings::create_identity(alphabet);
    }

    ReluctantReplace reluctant_replace{};
    Nft nft_end_marker{ [&]() {
        Nft nft_end_marker{ create_identity(alphabet) };
        State middle{ nft_end_marker.add_state_with_level(1) };
        State end_marker_state{ nft_end_marker.add_state_with_level(0) };
        nft_end_marker.delta.add(*nft_end_marker.initial.begin(), EPSILON, middle);
        nft_end_marker.delta.add(middle, end_marker, end_marker_state);
        nft_end_marker.final.clear();
        nft_end_marker.final.insert(end_marker_state);
        return nft_end_marker;
    }() };
    Nft nft_literal_replace{ reluctant_replace.replace_literal_nft(literal, replacement, alphabet, end_marker, replace_mode) };
    return compose(nft_end_marker, nft_literal_replace);
}

Nft ReluctantReplace::replace_symbol(Symbol from_symbol, Symbol replacement, mata::Alphabet* alphabet,
                                     ReplaceMode replace_mode) {
    return create_identity_with_single_symbol_replace(alphabet, from_symbol, replacement, replace_mode);
}

Nft ReluctantReplace::replace_symbol(Symbol from_symbol, const Word& replacement, mata::Alphabet* alphabet,
                                     ReplaceMode replace_mode) {
    return create_identity_with_single_symbol_replace(alphabet, from_symbol, replacement, replace_mode);
}
