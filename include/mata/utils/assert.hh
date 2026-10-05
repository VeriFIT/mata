/**
 * @file assert.hh
 * @brief Assertion utilities for Mata.
 *
 * This file provides macros for assertions with helpful error messages.
 */

#ifndef MATA_UTILS_ASSERT_HH
#define MATA_UTILS_ASSERT_HH

#include <format>

#ifdef NDEBUG
	#define MATA_ASSERT(cond, ...) ((void) 0)
#else
namespace mata::internal {
[[noreturn]] inline void
	assert_fail(const char* expr, const char* file, int line, const char* func, const std::string& msg) {
	if (msg.empty()) {
		std::fprintf(stderr, "%s:%d: %s: Assertion '%s' failed.\n", file, line, func, expr);
	} else {
		std::fprintf(stderr, "%s:%d: %s: Assertion '%s' failed: %s\n", file, line, func, expr, msg.c_str());
	}
	std::abort();
}
} // namespace mata::internal

	/**
	 * @brief Assert that a condition is true, and if not, print an error message and abort the program.
	 *
	 * @param cond The condition to assert.
	 * @param ... Optional format string and arguments for the error message.
	 *
	 * Usage:
	 * ```cpp
	 * MATA_ASSERT(state < num_states, "state={}, num_states={}", state, num_states);
	 * MATA_ASSERT(state < num_states);
	 * ```
	 * In release builds (when `NDEBUG` is defined), the assertions are disabled and have no effect.
	 */
	#define MATA_ASSERT(cond, ...)                                                                                     \
		{                                                                                                              \
			((cond) ? ((void) 0)                                                                                       \
					: mata::internal::assert_fail(                                                                     \
						  #cond, __FILE__, __LINE__, __func__, std::string(__VA_OPT__(std::format(__VA_ARGS__)))       \
					  ));                                                                                              \
		}
#endif // NDEBUG

/**
 * @brief Assert a condition whose check is not constant time, e.g. a scan of a whole container.
 *
 * Off by default even in Debug builds; enable with the CMake option @c MATA_EXPENSIVE_ASSERTS, which defines
 *  the macro of the same name. Without it a Debug lookup keeps the complexity of a Release lookup instead of
 *  paying an extra linear scan per call.
 */
#ifdef MATA_EXPENSIVE_ASSERTS
	#define MATA_EXPENSIVE_ASSERT(cond, ...) MATA_ASSERT(cond __VA_OPT__(, ) __VA_ARGS__)
#else
	#define MATA_EXPENSIVE_ASSERT(cond, ...) ((void) 0)
#endif

#endif // MATA_UTILS_ASSERT_HH
