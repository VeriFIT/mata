#ifndef MATA_GENERATOR_SUPPORT_HH_
#define MATA_GENERATOR_SUPPORT_HH_

#include <version>

/// Defined to 1 when @c std::generator (C++23 `<generator>`, P2502) is actually available, 0 otherwise. Some
///  standard libraries -- notably Apple's system libc++ shipped with Xcode on macOS -- do not implement it at all,
///  regardless of `-std=c++23`, so code that depends on it must be guarded by this macro instead of assuming C++23
///  support implies it.
#ifdef __cpp_lib_generator
	#define MATA_HAS_GENERATOR_SUPPORT 1
#else
	#define MATA_HAS_GENERATOR_SUPPORT 0
#endif

#endif // MATA_GENERATOR_SUPPORT_HH_
