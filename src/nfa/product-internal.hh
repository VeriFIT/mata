/** @file
 * @brief Internal product construction with an optional one-sided symbol range; not installed.
 */

#ifndef MATA_SRC_NFA_PRODUCT_INTERNAL_HH_
#define MATA_SRC_NFA_PRODUCT_INTERNAL_HH_

#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>

#include "mata/nfa/nfa.hh"

namespace mata::nfa::internal {

/**
 * @brief Compute a product, synchronizing every symbol when @p first_epsilon is @c std::nullopt.
 *
 * Otherwise, symbols at or above @p first_epsilon move one side only, as in the public product.
 */
Nfa product(
	const Nfa& lhs,
	const Nfa& rhs,
	const std::function<bool(State, State)>& final_condition,
	std::optional<Symbol> first_epsilon,
	std::unordered_map<std::pair<State, State>, State, mata::utils::PairHash<State, State>>* product_map
);

} // namespace mata::nfa::internal

#endif // MATA_SRC_NFA_PRODUCT_INTERNAL_HH_
