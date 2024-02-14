/* nfa-strings.hh -- Operations on NFAs for string solving.
 */

#include "mata/lvlfa/strings.hh"
#include "mata/lvlfa/lvlfa.hh"

//using mata::lvlfa::Lvlfa;
using mata::lvlfa::Level;
using mata::Symbol;
using mata::lvlfa::State;
using mata::nfa::StatePost;
using mata::nfa::SymbolPost;
using namespace mata::lvlfa;

Lvlfa mata::lvlfa::create_identity(mata::Alphabet* alphabet, Level level_cnt) {
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
    Lvlfa nft{ num_of_states, { 0 }, { 0 }, std::move(levels), level_cnt, alphabet };
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

