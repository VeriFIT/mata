/** @file config.cc
 *  @brief Tests of the global symbols the build system generates into 'src/config.cc'.
 *
 * The git hash is written by 'cmake/GetGitSha.cmake' as a build step, so a broken generation step no longer shows
 *  up as a configure error: it would silently leave the unexpanded '@GIT_SHA1@' placeholder, a git error message,
 *  or a hash with the trailing newline of 'git rev-parse' in the public 'mata::g_GIT_SHA1'.
 */

#include "mata/utils/utils.hh"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cctype>

TEST_CASE("mata::g_GIT_SHA1") {
	// Empty is the documented result of building outside a git repository (release tarball, Python sdist).
	if (!mata::g_GIT_SHA1.empty()) {
		// 40 characters is a repository with the default 'sha1' object format, 64 a 'sha256' one
		//  ('git init --object-format=sha256'). Catch2 decomposes at ||, so evaluate it first.
		const bool valid_length{mata::g_GIT_SHA1.size() == 40 || mata::g_GIT_SHA1.size() == 64};
		REQUIRE(valid_length);
		REQUIRE(std::all_of(mata::g_GIT_SHA1.begin(), mata::g_GIT_SHA1.end(), [](const char character) {
			return std::isxdigit(static_cast<unsigned char>(character)) != 0;
		}));
	}
}
