#ifndef MATA_GENERATOR_SUPPORT_HH_
#define MATA_GENERATOR_SUPPORT_HH_

#include <version>

/// Defined to 1 when @c std::generator (C++23 `<generator>`, P2502) is actually available, 0 otherwise. Some
///  standard libraries -- notably Apple's system libc++ shipped with Xcode on macOS -- do not implement it at all,
///  regardless of `-std=c++23`, so code that depends on it must be guarded by this macro instead of assuming C++23
///  support implies it.
// A macro, not a 'constexpr' constant: it guards '#if' blocks around the declarations that need 'std::generator'.
#ifdef __cpp_lib_generator
	#define MATA_HAS_GENERATOR_SUPPORT 1
#else
	#define MATA_HAS_GENERATOR_SUPPORT 0
#endif

/// Brackets for definitions of coroutines returning @c std::generator. GCC's `-Wmismatched-new-delete` (part of
///  `-Wall`) fires on every such coroutine built against libstdc++: the promise type allocates its frame with the
///  plain @c ::operator new but deallocates through its own sized @c operator delete, which forwards to
///  `::operator delete(ptr, size)` via a deallocator function pointer stored in the frame. The pairing is correct,
///  the compiler just cannot follow it. Known false positive, GCC PR middle-end/109224, which also fires inside
///  libstdc++'s own test suite.
// Macros, not pragmas written out at each site: '_Pragma' is the only way to make them conditional on the compiler.
#if defined(__GNUC__) && !defined(__clang__)
	#define MATA_GENERATOR_COROUTINE_BEGIN                                                                             \
		_Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wmismatched-new-delete\"")
	#define MATA_GENERATOR_COROUTINE_END _Pragma("GCC diagnostic pop")
#else
	#define MATA_GENERATOR_COROUTINE_BEGIN
	#define MATA_GENERATOR_COROUTINE_END
#endif

#endif // MATA_GENERATOR_SUPPORT_HH_
