/** @file
 * @brief Helpers shared between the NFT sources; not installed.
 */

#ifndef MATA_SRC_NFT_NFT_INTERNAL_HH_
#define MATA_SRC_NFT_NFT_INTERNAL_HH_

#include "mata/nft/nft.hh"

namespace mata::nft::internal {

/**
 * @brief Throw @c std::invalid_argument when the level metadata of @p nft is incomplete or out of range.
 *
 * Operations that interpret transitions through the level vector (jump unwinding, inclusion, equivalence)
 *  must not silently treat a transducer with missing or malformed levels as a flat NFA; that produced
 *  wrong answers without any error (#791).
 *
 * @param operation Name of the operation, used in the exception message.
 */
void validate_nft_levels(const Nft& nft, const char* operation);

} // namespace mata::nft::internal

#endif // MATA_SRC_NFT_NFT_INTERNAL_HH_
