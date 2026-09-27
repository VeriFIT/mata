/** @file
 * @brief Member definitions of @c mata::posts::PostEntry.
 *
 * Included at the end of @c mata/core/post.hh, for the same reason as @c mata/core/delta.tpp: a third
 *  party instantiating a post over its own key or target types needs the bodies.
 */

#ifndef MATA_CORE_POST_TPP
#define MATA_CORE_POST_TPP

#include <algorithm>
#include <utility>

namespace mata::posts {

template <typename K, typename N, typename R>
PostEntry<K, N, R>& PostEntry<K, N, R>::operator=(PostEntry&& rhs) noexcept {
	// By address: an entry's operator== compares only the key, so comparing the entries would skip
	//  every assignment between equal keys -- including std::sort moving an entry back into its own
	//  moved-from slot, which lost the entry's targets.
	if (this != &rhs) {
		symbol = rhs.symbol;
		targets = std::move(rhs.targets);
	}
	return *this;
}

template <typename K, typename N, typename R> void PostEntry<K, N, R>::insert(const Target s) {
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
template <typename K, typename N, typename R> void PostEntry<K, N, R>::insert(const Nested& states) {
	for (const Target s : states) { insert(s); }
}

} // namespace mata::posts.

#endif // MATA_CORE_POST_TPP
