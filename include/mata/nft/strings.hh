/* nfa-strings.hh -- Operations on NFAs for string solving.
 */

#ifndef MATA_NFT_STRING_SOLVING_HH_
#define MATA_NFT_STRING_SOLVING_HH_

#include "mata/nfa/strings.hh"
#include "nft.hh"

namespace mata::nft {
/**
 * Create identity transducer over the @p alphabet with @p level_cnt levels.
 */
Nft create_identity(mata::Alphabet* alphabet, Level level_cnt = 2);

/**
 * Create identity input/output transducer with 2 levels over the @p alphabet with @p level_cnt levels with single
 *  symbol @p from_symbol replaced with @to_symbol.
 */
Nft create_identity_with_single_replace(mata::Alphabet* alphabet, Symbol from_symbol, Symbol to_symbol);

} // Namespace mata::nft.

#endif // MATA_NFT_STRING_SOLVING_HH_.
