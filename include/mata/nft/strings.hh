/* nfa-strings.hh -- Operations on NFAs for string solving.
 */

#ifndef MATA_NFT_STRING_SOLVING_HH_
#define MATA_NFT_STRING_SOLVING_HH_

#include "mata/nfa/strings.hh"
#include "nft.hh"

namespace mata::nft::strings {
/**
 * Create identity transducer over the @p alphabet with @p level_cnt levels.
 */
Nft create_identity(mata::Alphabet* alphabet, Level level_cnt = 2);

/**
 * Create identity input/output transducer with 2 levels over the @p alphabet with @p level_cnt levels with single
 *  symbol @p from_symbol replaced with @to_symbol.
 */
Nft create_identity_with_single_replace(mata::Alphabet* alphabet, Symbol from_symbol, Symbol to_symbol);

enum class ReplaceMode {
    Single,
    All,
};

Nft replace_reluctant(
    const std::string& regex,
    const Word& replacement,
    Alphabet* alphabet,
    // TODO(nft): Change into constants?
    ReplaceMode replace_mode,
    Symbol begin_marker = EPSILON - 100
);
Nft replace_reluctant(
    nfa::Nfa regex,
    const Word& replacement,
    Alphabet* alphabet,
    ReplaceMode replace_mode,
    Symbol begin_marker = EPSILON - 100
);

nfa::Nfa end_marker_dfa(nfa::Nfa regex);
Nft marker_nft(const nfa::Nfa& marker_dfa, Symbol marker);

nfa::Nfa generic_marker_dfa(const std::string& regex, Alphabet* alphabet);
nfa::Nfa generic_marker_dfa(nfa::Nfa regex, Alphabet* alphabet);

nfa::Nfa begin_marker_nfa(const std::string& regex, Alphabet* alphabet);
nfa::Nfa begin_marker_nfa(nfa::Nfa regex, Alphabet* alphabet);

Nft begin_marker_nft(const nfa::Nfa& marker_nfa, Symbol begin_marker);
Nft end_marker_dft(const nfa::Nfa& end_marker_dfa, Symbol end_marker);
nfa::Nfa reluctant_nfa_with_marker(nfa::Nfa nfa, Symbol marker, Alphabet* alphabet);

Nft reluctant_leftmost_nft(const std::string& regex, Alphabet* alphabet, Symbol begin_marker, const Word& replacement, ReplaceMode replace_mode);
Nft reluctant_leftmost_nft(nfa::Nfa nfa, Alphabet* alphabet, Symbol begin_marker, const Word& replacement, ReplaceMode replace_mode);
} // Namespace mata::nft::strings.

#endif // MATA_NFT_STRING_SOLVING_HH_.
