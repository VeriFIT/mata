/* nfa-strings.hh -- Operations on NFAs for string solving.
 */

#include <utility>

#include "mata/nft/strings.hh"
#include "mata/parser/re2parser.hh"
#include "mata/nft/nft.hh"
#include "mata/nft/builder.hh"

using mata::nft::Nft;
using mata::nfa::Nfa;
using namespace mata;
using mata::nft::Level;
using mata::Symbol;
using mata::nft::State;
using mata::nfa::StatePost;
using mata::nfa::SymbolPost;
using namespace mata::nft;

namespace {
    /// Add transitions, optionally add @p source to @p dfa_generic_end_marker.final, and update @p labeling and @p labeling_inv functions.
    void process_source(const nfa::Nfa& regex, const Alphabet* alphabet, nfa::Nfa& dfa_generic_end_marker,
                        std::map<State, StateSet>& labeling,
                        std::unordered_map<StateSet, State>& labeling_inv, State source,
                        StateSet& source_label, std::vector<State>& worklist) {
        const State generic_initial_state{ *dfa_generic_end_marker.initial.begin() };
        for (const Symbol symbol: alphabet->get_alphabet_symbols()) {
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

Nft mata::nft::strings::create_identity(mata::Alphabet* alphabet, Level level_cnt) {
    if (level_cnt == 0) { throw std::runtime_error("NFT must have at least one level"); }
    const auto alphabet_symbols{ alphabet->get_alphabet_symbols() };
    const size_t additional_states_per_symbol_num{ level_cnt - 1 };
    const size_t num_of_states{ alphabet_symbols.size() * additional_states_per_symbol_num + 1 };
    Levels levels(num_of_states);
    levels[0] = 0;
    Level level{ 1 };
    for (State state{ 1 }; state < num_of_states; ++state) {
        levels[state] = level;
        const Level new_level{ level + 1 };
        level = new_level < level_cnt ? new_level : 1;
    }
    Nft nft{ num_of_states, { 0 }, { 0 }, std::move(levels), level_cnt, alphabet };
    State state{ 0 };
    State new_state;

    for (const Symbol symbol: alphabet_symbols) {
        level = 0;
        new_state = 0;
        for (; level < additional_states_per_symbol_num; ++level) {
            new_state = state + 1;
            if (level == 0) {
                nft.delta.add(0, symbol, new_state);
            } else {
                nft.delta.add(state, symbol, new_state);
            }
            ++state;
        }
        nft.delta.add(new_state, symbol, 0);
    }
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
            for (const Symbol symbol: alphabet->get_alphabet_symbols()) {
                state_lvl1 = nft.add_state_with_level(1);
                nft.delta.add(after_replace_state, symbol, state_lvl1);
                nft.delta.add(state_lvl1, symbol, after_replace_state);
            }
            nft.final.insert(after_replace_state);
            break;
        }
        default: {
            throw std::runtime_error("Unhandled replace mode.");
        }
    }
    return nft;
}

Nft mata::nft::strings::replace_reluctant(
    const std::string& regex,
    const Word& replacement,
    Alphabet* alphabet,
    ReplaceMode replace_mode,
    Symbol begin_marker
) {
    nfa::Nfa regex_nfa{};
    parser::create_nfa(&regex_nfa, regex);
    return replace_reluctant(std::move(regex_nfa), replacement, alphabet, replace_mode, begin_marker);
}

Nft mata::nft::strings::replace_reluctant(
    nfa::Nfa regex,
    const Word& replacement,
    Alphabet* alphabet,
    ReplaceMode replace_mode,
    Symbol begin_marker
) {
    // TODO(nft): Add optional bool parameter to revert whether to swap initial and final states.
    Nft dft_begin_marker{ begin_marker_nft(begin_marker_nfa(regex, alphabet), begin_marker) };
    Nft nft_reluctant_replace{
        reluctant_leftmost_nft(std::move(regex), alphabet, begin_marker, replacement, replace_mode) };
//    return dft_begin_marker.compose(nft_reluctant_replace);
    return Nft{};
}

nfa::Nfa mata::nft::strings::end_marker_dfa(nfa::Nfa regex) {
    if (!regex.is_deterministic()) {
        regex = determinize(regex);
    }

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

Nft mata::nft::strings::marker_nft(const nfa::Nfa& marker_dfa, Symbol marker) {

    Nft dft_marker{ nft::builder::create_from_nfa(marker_dfa) };
    const size_t dft_marker_num_of_states{ dft_marker.num_of_states() };
    for (State source{ 0 }; source < dft_marker_num_of_states; ++source) {
        StatePost& state_post = dft_marker.delta.mutable_state_post(source);
        for (const Move& move: state_post.moves_epsilons()) {
            const State marker_state{ dft_marker.add_state() };
            dft_marker.levels.resize(marker_state + 1);
            dft_marker.levels[marker_state] = 1;
            SymbolPost& symbol_post{ *state_post.find(move.symbol) };
            symbol_post.targets.erase(move.target);
            symbol_post.targets.insert(marker_state);
            dft_marker.delta.add(marker_state, marker, move.target);
        }
    }
    return dft_marker;
}

nfa::Nfa nft::strings::generic_marker_dfa(const std::string& regex, Alphabet* alphabet) {
    nfa::Nfa nfa{};
    parser::create_nfa(&nfa, regex);
    return generic_marker_dfa(std::move(nfa), alphabet);
}

nfa::Nfa nft::strings::generic_marker_dfa(nfa::Nfa regex, Alphabet* alphabet) {
    if (!regex.is_deterministic()) {
        regex = determinize(regex);
    }

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
            process_source(regex, alphabet, dfa_generic_end_marker, labeling, labeling_inv, end_marker_target,
                           source_label, worklist);
        } else {
            process_source(regex, alphabet, dfa_generic_end_marker, labeling, labeling_inv, source, source_label,
                           worklist);
        }

    }

    return dfa_generic_end_marker;
}

nfa::Nfa nft::strings::begin_marker_nfa(const std::string& regex, Alphabet* alphabet) {
    nfa::Nfa nfa{};
    parser::create_nfa(&nfa, regex);
    return begin_marker_nfa(std::move(nfa), alphabet);
}

nfa::Nfa nft::strings::begin_marker_nfa(nfa::Nfa regex, Alphabet* alphabet) {
    regex = revert(regex);
    nfa::Nfa dfa_generic_end_marker{ generic_marker_dfa(std::move(regex), alphabet) };
    dfa_generic_end_marker = revert(dfa_generic_end_marker);
    std::swap(dfa_generic_end_marker.initial, dfa_generic_end_marker.final);
    return dfa_generic_end_marker;
}

Nft nft::strings::begin_marker_nft(const nfa::Nfa& marker_nfa, Symbol begin_marker) {
    Nft begin_marker_nft{ marker_nft(marker_nfa, begin_marker) };
    const State new_initial{ begin_marker_nft.add_state() };
    for (const State orig_final: begin_marker_nft.final) {
        begin_marker_nft.delta.add(new_initial, EPSILON, orig_final);
    }
    begin_marker_nft.final = begin_marker_nft.initial;
    begin_marker_nft.initial = { new_initial };
    begin_marker_nft.levels.resize(new_initial + 1);
    begin_marker_nft.levels[new_initial] = 0;
    return begin_marker_nft;
}

Nft nft::strings::end_marker_dft(const nfa::Nfa& end_marker_dfa, const Symbol end_marker) {
    return marker_nft(end_marker_dfa, end_marker);
}

nfa::Nfa nft::strings::reluctant_nfa_with_marker(nfa::Nfa nfa, const Symbol marker, Alphabet* alphabet) {
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
        initial.push_back({ symbol, 0 });
    }
    StatePost& marker_state{ nfa_avoid_removing_next_begin_marker.delta.mutable_state_post(1) };
    nfa_avoid_removing_next_begin_marker.delta.add(0, marker, 1);
    for (const Symbol symbol: alphabet_symbols) {
        marker_state.push_back({ symbol, 0 });
    }
    nfa_avoid_removing_next_begin_marker.delta.add(1, marker, 1);
    // TODO(nft): Leaves a non-terminating begin_marker transitions in a form of a lasso from final states.
    //  These lassos should be removed to further optimize NFT creation.
    return mata::strings::reluctant_nfa(reduce(intersection(nfa, nfa_avoid_removing_next_begin_marker)));
}

Nft nft::strings::reluctant_leftmost_nft(const std::string& regex, Alphabet* alphabet, Symbol begin_marker,
                                         const Word& replacement, ReplaceMode replace_mode) {
    nfa::Nfa nfa{};
    parser::create_nfa(&nfa, regex);
    return reluctant_leftmost_nft(std::move(nfa), alphabet, begin_marker, replacement, replace_mode);
}

Nft nft::strings::reluctant_leftmost_nft(nfa::Nfa nfa, Alphabet* alphabet, Symbol begin_marker,
                                         const Word& replacement, ReplaceMode replace_mode) {
    nfa = reluctant_nfa_with_marker(std::move(nfa), begin_marker, alphabet);
    std::ostringstream stream;
    stream << EPSILON;
    Nft nft_reluctant_leftmost{
        nft::builder::create_from_nfa(nfa, 2, { EPSILON }, { EPSILON }) };
    const size_t regex_num_of_states{ nft_reluctant_leftmost.num_of_states() };
    assert(nft_reluctant_leftmost.is_deterministic());
    const utils::OrdVector<Symbol> alphabet_symbols{ alphabet->get_alphabet_symbols() };
    nft_reluctant_leftmost.levels.resize(
        regex_num_of_states + replacement.size() * 2 + alphabet_symbols.size() + 4);
    State curr_state{ regex_num_of_states };
    // Create self-loop on the new initial state.
    const State initial{ regex_num_of_states };
    nft_reluctant_leftmost.levels[initial] = 0;
    ++curr_state;
    StatePost& initial_state_post{ nft_reluctant_leftmost.delta.mutable_state_post(initial) };
    for (const Symbol symbol: alphabet_symbols) {
        initial_state_post.push_back({ symbol, curr_state });
        nft_reluctant_leftmost.delta.add(curr_state, symbol, initial);
        nft_reluctant_leftmost.levels[curr_state] = 1;
        ++curr_state;
    }
    // Move to replace mode when begin marker is encountered.
    initial_state_post.insert({ begin_marker, curr_state });
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
    for (const Symbol symbol: replacement) {
        nft_reluctant_leftmost.delta.add(curr_state, symbol, curr_state + 1);
        nft_reluctant_leftmost.delta.add(curr_state + 1, EPSILON, curr_state + 2);
        nft_reluctant_leftmost.levels[curr_state + 1] = 0;
        nft_reluctant_leftmost.levels[curr_state + 2] = 1;
        curr_state += 2;
    }
    nft_reluctant_leftmost.delta.add(curr_state, EPSILON, curr_state + 1);
    nft_reluctant_leftmost.levels[curr_state + 1] = 0;
    nft_reluctant_leftmost.final.insert(curr_state + 1);
    ++curr_state;
    nft_reluctant_leftmost.final.clear();
    switch (replace_mode) {
        case ReplaceMode::All: {
            nft_reluctant_leftmost.delta.add(curr_state, EPSILON, initial);
            break;
        };
        case ReplaceMode::Single: {
            const State final{ curr_state };
            nft_reluctant_leftmost.final.insert(final);
            ++curr_state;
            for (const Symbol symbol: alphabet_symbols) {
                nft_reluctant_leftmost.delta.add(final, symbol, curr_state);
                nft_reluctant_leftmost.delta.add(curr_state, symbol, final);
                nft_reluctant_leftmost.levels[curr_state] = 1;
                ++curr_state;
            }

            nft_reluctant_leftmost.delta.add(final, begin_marker, curr_state);
            nft_reluctant_leftmost.delta.add(curr_state, EPSILON, final);
            nft_reluctant_leftmost.levels[curr_state] = 1;
            ++curr_state;
            break;
        };
        default: {
            throw std::runtime_error("Unimplemented replace mode");
            break;
        }
    }

    nft_reluctant_leftmost.initial = { initial };
    nft_reluctant_leftmost.final.insert(initial);

    return nft_reluctant_leftmost;
}
