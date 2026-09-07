/** @file
 * @brief Member definitions for the templated posts of @c mata::Delta.
 *
 * Included at the end of @c mata/core/delta.hh. The bodies live in a header rather than in
 *  `src/core/delta.cc` because a third party instantiating the posts over its own key or target
 *  types needs them; the in-tree depth-2 instantiation is kept out of every translation unit by the
 *  @c extern template declarations in the header.
 */

#ifndef MATA_CORE_DELTA_TPP
#define MATA_CORE_DELTA_TPP

#include <algorithm>
#include <utility>

namespace mata::posts {

template <typename K, typename N>
SymbolPost<K, N>& SymbolPost<K, N>::operator=(SymbolPost&& rhs) noexcept {
	if (*this != rhs) {
		symbol = rhs.symbol;
		targets = std::move(rhs.targets);
	}
	return *this;
}

template <typename K, typename N> void SymbolPost<K, N>::insert(const Target s) {
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
template <typename K, typename N> void SymbolPost<K, N>::insert(const Nested& states) {
	for (const Target s : states) { insert(s); }
}

} // namespace mata::posts.

#endif // MATA_CORE_DELTA_TPP
