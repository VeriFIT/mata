/* nfa-strings.hh -- Operations on NFAs for string solving.
 */

#include <utility>

#include "mata/nft/strings.hh"
#include "mata/parser/re2parser.hh"
#include "mata/nft/nft.hh"
#include "mata/nft/builder.hh"

//using mata::nft::Nft;
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

Nft mata::nft::strings::create_identity_with_single_replace(
    mata::Alphabet* alphabet, const Symbol from_symbol, const Symbol to_symbol) {
    Nft nft{ create_identity(alphabet) };
    if (alphabet->empty()) { throw std::runtime_error("Alphabet does not contain symbol being replaced."); }
    auto symbol_post_to_state_with_replace{ nft.delta.mutable_state_post(0).find(from_symbol) };
    const State from_replace_state{ symbol_post_to_state_with_replace->targets.front() };
    nft.delta.mutable_state_post(from_replace_state).front().symbol = to_symbol;
    return nft;
}

Nft mata::nft::strings::replace_reluctant(
    const std::string& regex,
    const std::string& replacement,
    Alphabet* alphabet,
    Symbol begin_marker,
    Symbol end_marker
) {
    nfa::Nfa regex_nfa{};
    parser::create_nfa(&regex_nfa, regex);
    return replace_reluctant(std::move(regex_nfa), replacement, alphabet, begin_marker, end_marker);
}

Nft mata::nft::strings::replace_reluctant(
    nfa::Nfa regex,
    const std::string& replacement,
    Alphabet* alphabet,
    Symbol begin_marker,
    Symbol end_marker
) {
    nfa::Nfa dfa_generic_marker{ generic_end_marker_dfa(std::move(regex), alphabet) };
    Nft dft_generic_end_marker{ end_marker_dft(dfa_generic_marker, end_marker) };
    Nft dft_begin_marker{ begin_marker_nft(dfa_generic_marker, begin_marker) };

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

nfa::Nfa nft::strings::generic_end_marker_dfa(const std::string& regex, Alphabet* alphabet) {
    nfa::Nfa nfa{};
    parser::create_nfa(&nfa, regex);
    return generic_end_marker_dfa(std::move(nfa), alphabet);
}

nfa::Nfa nft::strings::generic_end_marker_dfa(nfa::Nfa regex, Alphabet* alphabet) {
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
    nfa::Nfa dfa_generic_end_marker{ generic_end_marker_dfa(std::move(regex), alphabet) };
    dfa_generic_end_marker = revert(dfa_generic_end_marker);
    std::swap(dfa_generic_end_marker.initial, dfa_generic_end_marker.final);
    return dfa_generic_end_marker;
}

Nft nft::strings::begin_marker_nft(const nfa::Nfa& begin_marker_dfa, Symbol begin_marker) {
    Nft begin_marker_dft{ marker_nft(begin_marker_dfa, begin_marker) };
    const State new_initial{ begin_marker_dft.add_state() };
    for (const State orig_final: begin_marker_dft.final) {
        begin_marker_dft.delta.add(new_initial, EPSILON, orig_final);
    }
    begin_marker_dft.final = begin_marker_dft.initial;
    begin_marker_dft.initial = { new_initial };
    begin_marker_dft.levels.resize(new_initial + 1);
    begin_marker_dft.levels[new_initial] = 0;
    return begin_marker_dft;
}

Nft nft::strings::end_marker_dft(const nfa::Nfa& end_marker_dfa, Symbol end_marker) {
    return marker_nft(end_marker_dfa, end_marker);
}
