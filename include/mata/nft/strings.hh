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

Nft reluctant_replace(
    const std::string& regex,
    const std::string& replacement,
    // TODO: Change into constants?
    Symbol begin_marker = EPSILON - 101,
    Symbol end_marker = EPSILON - 100
);
Nft reluctant_replace(
    mata::nfa::Nfa regex,
    const std::string& replacement,
    Symbol begin_marker = EPSILON - 101,
    Symbol end_marker = EPSILON - 100
);

nfa::Nfa end_marker_dfa(nfa::Nfa regex);
nft::Nft end_marker_dft(const nfa::Nfa& end_marker_dfa, Symbol end_marker);

} // Namespace mata::nft::strings.

#endif // MATA_NFT_STRING_SOLVING_HH_.
