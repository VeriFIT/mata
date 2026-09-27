/** @file
 * @brief Contains the @c ArgOf type trait used for parameter passing.
 */

#ifndef MATA_ARG_OF_HH
#define MATA_ARG_OF_HH

#include <type_traits>

namespace mata::utils {

/**
 * @brief How a parameter of type @p X is taken.
 *
 * If a parameter is small and trivially copyable, it is taken by value even if the caller has a reference to it.
 * @note Decided per type rather than per call site.
 * @tparam X The type of the parameter.
 */
template <typename X>
using ArgOf = std::conditional_t<std::is_trivially_copyable_v<X> && sizeof(X) <= 2 * sizeof(void*), const X, const X&>;

} // namespace mata::utils.

#endif // MATA_ARG_OF_HH
