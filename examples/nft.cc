#include "mata/alphabet.hh"
#include "mata/nfa/algorithms.hh"
#include "mata/nft/nft.hh"
#include <cassert>
#include "mata/nfa/nfa.hh"
#include "mata/nft/types.hh"
#include "mata/nfa/builder.hh"

int main() {
    using namespace mata;
    using namespace mata::nft;
    using namespace mata::nfa;
    mata::EnumAlphabet alphabet{ 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'A', 'B', 'C', 'D', 'E', 'F', 'G' };


    Nft nft{}; // Create an empty NFT. By default, has DEFAULT_NUM_OF_LEVELS levels (= tapes).
    // Or specify the number of levels explicitly:
    //Nft nft{ Nft::with_levels(3) };
    State initial{ nft.add_state() }; // Add a new state with level 0.
    nft.initial.insert(initial);
    State final{ nft.add_state_with_level(0) }; // Add a new state with a manually specified level.
    nft.final.insert(final);
    // Invalid: Initial and final states must have level 0.
    // nft.final.insert(nft.add_state_with_level(1));

    // Transduce one symbol on each tape.
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
    // Hopcroft minimization requires a deterministic NFA as input.
    nfa_pre_image = determinize(remove_epsilon(nfa_pre_image).trim());
    assert(nfa_pre_image.is_deterministic());
    nfa_pre_image = nfa::algorithms::minimize_hopcroft(nfa_pre_image);
    // Print the resulting pre-image NFA in dot format.
    std::cout << "Pre-image NFA:\n";
    std::cout << nfa_pre_image.print_to_dot(true) << "\n";

    // Compute a composition of two NFTs.
    Nft nft2{}; // Create another NFT.
    State initial2{ nft2.add_state() };
    nft2.initial.insert(initial2);
    State final2{ nft2.add_state() };
    nft2.final.insert(final2);
    State next2{ nft2.add_transition(initial2, { 'A', '1' }) };
    next2 = nft2.insert_word(next2, { 'B', '2', 'C', '3' });
    nft2.insert_word_by_parts(next2, { { 'D' }, { '4', '5', '6' } }, final2);
    nft2.insert_identity(final2, &alphabet);
    Nft nft_composed{ mata::nft::compose(nft, nft2) }; // Compose, by default on tape 1 of nft and tape 0 of nft2, giving the composition nft2 after nft.
    // Print the composed NFT in dot format, labeling the states as `state_number:level`.
    std::cout << "Composed NFT:\n";
    nft_composed.print_to_dot(std::cout, true);
    // Or specify the tapes to compose on explicitly: tape 0 of nft and tape 1 of nft2.
    Nft nft_composed_explicit_tapes{ mata::nft::compose(nft, nft2, 0, 1) };
}
