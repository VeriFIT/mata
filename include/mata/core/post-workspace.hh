/** @file
 * @brief The reusable post buffers and the epsilon-closure options, shared by every transition relation.
 */

#ifndef MATA_CORE_POST_WORKSPACE_HH
#define MATA_CORE_POST_WORKSPACE_HH

#include "mata/config.hh"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace mata {

using State = std::conditional_t<MATA_STATE_BITS == 32, std::uint32_t, std::uint64_t>;

/// @name Options for algorithms and functions in Mata.
///@{
/**
 * @brief Where an epsilon closure is applied around a transition step.
 *
 * The values are bit flags, so @c BeforeAndAfter must be tested with @c closes_before() and @c closes_after()
 *  rather than compared for equality. For @c 0 -ε-> 1 -a-> 2 -ε-> 3 and the step over @c a from @c {0}:
 *
 * | Option | Applied | Result |
 * |---|---|---|
 * | @c None | post(S) | @c {} |
 * | @c Before | post(closure(S)) | @c {2} |
 * | @c After | closure(post(S)) | @c {} |
 * | @c BeforeAndAfter | closure(post(closure(S))) | @c {2,3} |
 */
enum class EpsilonClosureOpt : std::uint8_t {
	None = 1 << 0, ///< No epsilon closure.
	Before = 1 << 1, ///< Epsilon closure before the transition.
	After = 1 << 2, ///< Epsilon closure after the transition.
	BeforeAndAfter = Before | After ///< Epsilon closure before and after the transition.
};

/// Does @p opt ask for an epsilon closure before the transition step?
constexpr bool closes_before(const EpsilonClosureOpt opt) {
	return (static_cast<std::uint8_t>(opt) & static_cast<std::uint8_t>(EpsilonClosureOpt::Before)) != 0;
}

/// Does @p opt ask for an epsilon closure after the transition step?
constexpr bool closes_after(const EpsilonClosureOpt opt) {
	return (static_cast<std::uint8_t>(opt) & static_cast<std::uint8_t>(EpsilonClosureOpt::After)) != 0;
}
///@}

/**
 * @brief Reusable buffers of @c post() and @c epsilon_closure(), so that a loop of steps allocates once.
 *
 * A call writes all of its buffers, so one workspace serves one call at a time. Concurrent calls, even const queries
 *  of the same delta, each need their own; in particular a workspace must never be a function-local @c static.
 */
struct PostWorkspace {
	std::vector<State> sources{}; ///< Epsilon closure of the source states of a step.
	std::vector<State> gathered{}; ///< Targets of a step, or the closure being built.
	std::vector<State> worklist{}; ///< Closure states whose epsilon posts are still unread.
	std::vector<State> visited{}; ///< Open-addressed hash table of the states of the closure being built.
};

} // namespace mata.

#endif // MATA_CORE_POST_WORKSPACE_HH
