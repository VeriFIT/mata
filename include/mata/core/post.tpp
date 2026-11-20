/** @file
 * @brief Member definitions of @c mata::posts::PostEntry, which live on @c mata::posts::detail::PostEntryCore.
 */

#ifndef MATA_CORE_POST_TPP
#define MATA_CORE_POST_TPP

#include <algorithm>
#include <utility>

namespace mata::posts::detail {

template <typename K, typename N, typename R>
PostEntryCore<K, N, R>& PostEntryCore<K, N, R>::operator=(PostEntryCore&& rhs) noexcept {
	// Self-assignment is checked by address: operator== compares only the key, so a by-value check
	//  would also skip assigning an entry with the same key but different targets (std::sort does this).
	if (this != &rhs) {
		symbol = rhs.symbol;
		targets = std::move(rhs.targets);
	}
	return *this;
}

template <typename K, typename N, typename R>
void PostEntryCore<K, N, R>::insert(const Target s)
	requires(arity_of<N> == 0)
{
	if (targets.empty() || targets.back() < s) {
		targets.push_back(s);
		return;
	}
	// Find the place where to put the element (if not present).
	// Insert to OrdVector without the searching of a proper position inside insert(const Key&x).
	if (const auto it = std::ranges::lower_bound(targets, s); it == targets.end() || *it != s) {
		targets.insert(it, s);
	}
}

// TODO: slow! This should be doing merge, not inserting one by one.
template <typename K, typename N, typename R>
void PostEntryCore<K, N, R>::insert(const Nested& states)
	requires(arity_of<N> == 0)
{
	for (const Target s : states) { insert(s); }
}

} // namespace mata::posts::detail.

#endif // MATA_CORE_POST_TPP
