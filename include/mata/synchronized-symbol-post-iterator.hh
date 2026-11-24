/** @file
 * @brief Synchronized iterator over SymbolPosts.
 *
 * Used by the NFA and NFT algorithms that need the successors of a set of states over each symbol:
 *  subset construction, inclusion checking and their relatives.
 */

#ifndef MATA_SYNCHRONIZED_SYMBOL_POST_ITERATOR_HH
#define MATA_SYNCHRONIZED_SYMBOL_POST_ITERATOR_HH

#include <cstdint>
#include <vector>

#include "mata/relation.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/utils/synchronized-iterator.hh"

namespace mata {

/**
 * @brief Specialization of utils::SynchronizedExistentialIterator for iterating over SymbolPosts.
 */
class SynchronizedExistentialSymbolPostIterator
	: public utils::SynchronizedExistentialIterator<utils::OrdVector<SymbolPost>::const_iterator> {
  public:
	/**
	 * @brief Get union of all targets.
	 */
	StateSet unify_targets() const;

	/**
	 * @brief Synchronize with the given SymbolPost @p sync.
	 *
	 * Alignes the synchronized iterator to the same symbol as @p sync.
	 * @return True iff the synchronized iterator points to the same symbol as @p sync.
	 */
	bool synchronize_with(const SymbolPost& sync);

	/**
	 * @brief Synchronize with the given symbol @p sync_symbol.
	 *
	 * Alignes the synchronized iterator to the same symbol as @p sync_symbol.
	 * @return True iff the synchronized iterator points to the same symbol as @p sync.
	 */
	bool synchronize_with(Symbol sync_symbol);

  private:
	/// Buffers of @c unify_targets(), reused across its calls.
	mutable std::vector<State> unify_buffer_{};
	mutable std::vector<uint64_t> unify_words_{};
}; // class SynchronizedExistentialSymbolPostIterator.

} // namespace mata.

#endif // MATA_SYNCHRONIZED_SYMBOL_POST_ITERATOR_HH
