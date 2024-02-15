/* nfa-strings.hh -- Operations on NFAs for string solving.
 */

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

Nft mata::nft::create_identity(mata::Alphabet* alphabet, Level level_cnt) {
    if (level_cnt == 0) { throw std::runtime_error("NFT must have at least one level"); }
    const auto alphabet_symbols{ alphabet->get_alphabet_symbols() };
    const size_t additional_states_per_symbol_num{ level_cnt - 1 };
    const size_t num_of_states{ alphabet_symbols.size() * additional_states_per_symbol_num + 1 };
    std::vector<Level> levels(num_of_states);
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

Nft mata::nft::create_identity_with_single_replace(
    mata::Alphabet *alphabet, const Symbol from_symbol, const Symbol to_symbol) {
    Nft nft{ create_identity(alphabet) };
    if (alphabet->empty()) { throw std::runtime_error("Alphabet does not contain symbol being replaced."); }
    auto symbol_post_to_state_with_replace{ nft.delta.mutable_state_post(0).find(from_symbol) };
    const State from_replace_state{ symbol_post_to_state_with_replace->targets.front() };
    nft.delta.mutable_state_post(from_replace_state).front().symbol = to_symbol;
    return nft;
}

Nft mata::nft::reluctant_replace(
    const std::string& regex,
    const std::string& replacement,
    Symbol begin_marker,
    Symbol end_marker
) {
    nfa::Nfa regex_nfa{};
    parser::create_nfa(&regex_nfa, regex);
    return reluctant_replace(std::move(regex_nfa), replacement);
}

Nft mata::nft::reluctant_replace(
    nfa::Nfa regex,
    const std::string& replacement,
    Symbol begin_marker,
    Symbol end_marker
) {
    regex = end_marker_dfa(std::move(regex));
    Nft dft_end_marker{ end_marker_dft(regex, end_marker) };

    return Nft{};
}

nfa::Nfa mata::nft::end_marker_dfa(nfa::Nfa regex) {
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

nft::Nft mata::nft::end_marker_dft(const nfa::Nfa& end_marker_dfa, const Symbol end_marker) {
    assert(end_marker_dfa.is_deterministic());

    Nft dft_end_marker{ nft::builder::create_from_nfa(end_marker_dfa) };
    const size_t dft_end_marker_num_of_states{ dft_end_marker.num_of_states() };
    for (State source{ 0 }; source < dft_end_marker_num_of_states; ++source) {
        StatePost& state_post = dft_end_marker.delta.mutable_state_post(source);
        for (const Move& move: state_post.moves_epsilons()) {
            const State end_marker_state{ dft_end_marker.add_state() };
            SymbolPost& symbol_post{ *state_post.find(move.symbol) };
            symbol_post.targets.erase(move.target);
            symbol_post.targets.insert(end_marker_state);
            dft_end_marker.delta.add(end_marker_state, end_marker, move.target);
        }
    }
    return dft_end_marker;
}
