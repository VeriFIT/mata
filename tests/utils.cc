/**
 * @file tests/utils.cc
 * @brief Unit tests for mata::utils.
 */

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "mata/utils/utils.hh"

using namespace mata::utils;

TEST_CASE("mata::utils::to_string()") {
	SECTION("a char is printed as the character") {
		CHECK(to_string('a') == "a");
	}

	SECTION("a byte-sized integer is printed as its number") {
		CHECK(to_string(std::uint8_t{97}) == "97");
		CHECK(to_string(std::int8_t{-3}) == "-3");
		CHECK(to_string(std::vector<std::uint8_t>{0, 1, 97, 10}) == "[0, 1, 97, 10]");
	}
}

namespace {
struct Opaque {};
} // namespace

TEST_CASE("mata::utils::format_or_unprintable()") {
	CHECK(format_or_unprintable(42) == "42");
	CHECK(format_or_unprintable('a') == "a");
	CHECK(format_or_unprintable(std::string{"word"}) == "word");
	CHECK(format_or_unprintable(Opaque{}) == "<unprintable>");
}
