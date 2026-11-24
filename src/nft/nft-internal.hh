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

/**
 * @brief Whether unwinding @p nft with @p dont_care_symbol_replacements changes it: a transition does not advance
 *  exactly one level, or a DONT_CARE is spelled out (it is kept only when it is its own sole replacement).
 */
bool unwinding_changes(const Nft& nft, const utils::OrdVector<Symbol>& dont_care_symbol_replacements);

/**
 * @brief Lay the levels of @p appended_num_of_states states, taken from @p appended, after the levels of the first
 *  @p num_of_states states in @p levels, as the states of an operand shifted by @p num_of_states.
 *
 * A level vector may miss states past the delta (they are on @c DEFAULT_LEVEL) or hold surplus entries, so both
 *  vectors are cut or padded to their state counts first; appending them as they are misplaces the shifted levels.
 *  @p appended may be @p levels itself.
 */
void append_shifted_levels(Levels& levels, size_t num_of_states, const Levels& appended, size_t appended_num_of_states);

} // namespace mata::nft::internal

#endif // MATA_SRC_NFT_NFT_INTERNAL_HH_
