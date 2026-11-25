/** @file
 * @brief Member definitions of @c mata::posts::PostEntry, which live on @c mata::posts::detail::PostEntryCore.
 */

#ifndef MATA_CORE_POST_TPP
#define MATA_CORE_POST_TPP

#include <utility>

namespace mata::posts::detail {

template <typename K, typename N, typename R>
PostEntryCore<K, N, R>& PostEntryCore<K, N, R>::operator=(PostEntryCore&& rhs) noexcept(
	std::is_nothrow_move_assignable_v<N> && std::is_nothrow_move_assignable_v<K>
) {
	// Self-assignment is checked by address: operator== compares only the key, so a by-value check
	//  would also skip assigning an entry with the same key but different targets (std::sort does this).
	if (this != &rhs) {
		targets = std::move(rhs.targets);
		symbol = std::move(rhs.symbol);
	}
	return *this;
}

template <typename K, typename N, typename R>
bool PostEntryCore<K, N, R>::insert(const Target s)
	requires(arity_of<N> == 0)
{
	if (targets.empty() || targets.back() < s) {
		targets.push_back(s);
		return true;
	}
	if constexpr (requires { targets.insert(s).second; }) {
		// The set appends at the back and searches otherwise.
		return targets.insert(s).second;
	} else {
		// The set does not say whether it inserted, so its size does.
		const size_t old_size{targets.size()};
		targets.insert(s);
		return targets.size() != old_size;
	}
}

template <typename K, typename N, typename R>
size_t PostEntryCore<K, N, R>::insert(const std::span<const Target> states)
	requires(arity_of<N> == 0)
{
	const size_t old_size{targets.size()};
	if constexpr (requires { targets.insert(states); }) {
		// A sorted merge in the set itself.
		targets.insert(states);
	} else {
		for (const Target s : states) { insert(s); }
	}
	return targets.size() - old_size;
}

template <typename K, typename N, typename R>
void PostEntryCore<K, N, R>::insert(const Nested& states)
	requires(arity_of<N> == 0)
{
	if constexpr (requires { std::span<const Target>{states.begin(), states.end()}; }) {
		insert(std::span<const Target>{states.begin(), states.end()});
	} else {
		for (const Target s : states) { insert(s); }
	}
}

} // namespace mata::posts::detail.

#endif // MATA_CORE_POST_TPP
