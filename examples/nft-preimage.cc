#include "mata/alphabet.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/nft/nft.hh"
#include "mata/nfa/nfa.hh"
#include "mata/nft/types.hh"
#include "mata/nfa/builder.hh"

int main() {
    using namespace mata;
    using namespace mata::nft;
    using namespace mata::nfa;
    mata::EnumAlphabet alphabet{ 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'A', 'B', 'C', 'D', 'E', 'F', 'G' };


    Nft nft{}; // By default, has DEFAULT_NUM_OF_LEVELS tapes.
    //Nft nft{ Nft::with_levels(3) };
    State initial{ nft.add_state() }; // Add a new state with level 0.
    nft.initial.insert(initial);
    State final{ nft.add_state_with_level(0) }; // Add a new state with a manually specified level.
    nft.final.insert(final);

    // Transduce one symbol on each tape (= level).
    State next_state{ nft.add_transition(initial, { 'a', 'A' }) };
    // Then transduce a sequence of 'bc' to 'BC'.
    next_state = nft.insert_word(next_state, { 'b', 'B', 'c', 'C' });
    // Then transduce a sequence 'd' to 'DEF' (non-length-preserving operation).
    nft.insert_word_by_parts(next_state, { { 'd' }, { 'D', 'E', 'F' } }, final);
    // Finally, accept the same suffix on all tapes.
    nft.insert_identity(final, &alphabet);
    // NFT nft transduces a sequence of 'abcd' to 'ABCDEF', accepting the same suffix on all tapes.

    // Create NFA from a regex, each character mapping to its UTF-8 value.
    Nfa nfa = nfa::builder::create_from_regex("ABCDEFggg");

    // Compute the pre-image of an NFA (applies NFA to the image tape, i.e., tape 1)
    Nft backward_applied_nft = nft.apply(nfa, 1);
    // Extract a pre-image NFA.
    Nfa nfa_pre_image{ backward_applied_nft.to_nfa_move() };
    // Minimize the result pre-image using hopcroft minimization.
    nfa_pre_image = determinize(remove_epsilon(nfa_pre_image).trim());
    assert(nfa_pre_image.is_deterministic());
    nfa_pre_image = algorithms::minimize_hopcroft(nfa_pre_image);
    std::cout << nfa_pre_image.print_to_dot(true) << "\n";
}
