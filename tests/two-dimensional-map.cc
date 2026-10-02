/* two-dimensional-map.cc -- tests of TwoDimensionalMap
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/utils/two-dimensional-map.hh"
#include "mata/utils/utils.hh"

using namespace mata::utils;

TEST_CASE("mata::utils::TwoDimensionalMap::large_map == false") {
	SECTION("mata::utils::TwoDimensionalMap::insert() && get()") {
		TwoDimensionalMap<unsigned> map(10, 10);
		map.insert(1, 2, 3);
		map.insert(4, 5, 6);
		map.insert(1, 5, 7);
		map.insert(4, 2, 8);

		CHECK(map.get(1, 2) == 3);
		CHECK(map.get(4, 5) == 6);
		CHECK(map.get(1, 5) == 7);
		CHECK(map.get(4, 2) == 8);
		CHECK(map.get(0, 0) == std::numeric_limits<unsigned>::max());
		CHECK(map.get(5, 5) == std::numeric_limits<unsigned>::max());
	}

	SECTION("mata::utils::TwoDimensionalMap::get_first_inverted()") {
		TwoDimensionalMap<unsigned> map(10, 10);
		map.insert(1, 2, 3);
		map.insert(4, 5, 6);
		map.insert(1, 5, 7);
		map.insert(4, 2, 8);

		CHECK(map.get_first_inverted(3) == 1);
		CHECK(map.get_first_inverted(6) == 4);
		CHECK(map.get_first_inverted(7) == 1);
		CHECK(map.get_first_inverted(8) == 4);
	}

	SECTION("mata::utils::TwoDimensionalMap::get_second_inverted()") {
		TwoDimensionalMap<unsigned> map(10, 10);
		map.insert(1, 2, 3);
		map.insert(4, 5, 6);
		map.insert(1, 5, 7);
		map.insert(4, 2, 8);

		CHECK(map.get_second_inverted(3) == 2);
		CHECK(map.get_second_inverted(6) == 5);
		CHECK(map.get_second_inverted(7) == 5);
		CHECK(map.get_second_inverted(8) == 2);
	}

	SECTION("big") {
		TwoDimensionalMap<unsigned> map(1'000, 1'000);
		for (unsigned i = 0; i < 1'000; ++i) {
			for (unsigned j = 0; j < 1'000; ++j) { map.insert(i, j, i + j); }
		}

		for (unsigned i = 0; i < 1'000; ++i) {
			for (unsigned j = 0; j < 1'000; ++j) { CHECK(map.get(i, j) == i + j); }
		}
	}

	SECTION("missing values in an allocated row") {
		TwoDimensionalMap<unsigned> map(10, 10);
		map.insert(1, 2, 3);

		// The row of the first key 1 is allocated, the row of the first key 0 is not.
		CHECK(map.get(1, 0) == std::numeric_limits<unsigned>::max());
		CHECK(map.get(1, 9) == std::numeric_limits<unsigned>::max());
		CHECK(map.get(0, 2) == std::numeric_limits<unsigned>::max());
	}

	SECTION("zero is a regular value") {
		TwoDimensionalMap<unsigned> map(10, 10);
		map.insert(3, 4, 0);

		CHECK(map.get(3, 4) == 0);
		CHECK(map.get(3, 5) == std::numeric_limits<unsigned>::max());
		CHECK(map.get_first_inverted(0) == 3);
		CHECK(map.get_second_inverted(0) == 4);
	}

	SECTION("inserting a smaller value keeps the larger inverted entries") {
		TwoDimensionalMap<unsigned> map(10, 10);
		map.insert(1, 2, 5);
		map.insert(3, 4, 3);

		CHECK(map.get_first_inverted(5) == 1);
		CHECK(map.get_second_inverted(5) == 2);
		CHECK(map.get_first_inverted(3) == 3);
		CHECK(map.get_second_inverted(3) == 4);
		CHECK(map.get(1, 2) == 5);
		CHECK(map.get(3, 4) == 3);
	}

	SECTION("overwriting a value") {
		TwoDimensionalMap<unsigned> map(10, 10);
		map.insert(1, 2, 3);
		map.insert(1, 2, 4);

		CHECK(map.get(1, 2) == 4);
		CHECK(map.get_first_inverted(4) == 1);
		CHECK(map.get_second_inverted(4) == 2);
	}
}

TEST_CASE("mata::utils::TwoDimensionalMap::large_map == true") {
	SECTION("mata::utils::TwoDimensionalMap::insert() && get()") {
		TwoDimensionalMap<unsigned, true, 10> map(10, 10);
		map.insert(1, 2, 3);
		map.insert(4, 5, 6);
		map.insert(1, 5, 7);
		map.insert(4, 2, 8);

		CHECK(map.get(1, 2) == 3);
		CHECK(map.get(4, 5) == 6);
		CHECK(map.get(1, 5) == 7);
		CHECK(map.get(4, 2) == 8);
		CHECK(map.get(0, 0) == std::numeric_limits<unsigned>::max());
		CHECK(map.get(5, 5) == std::numeric_limits<unsigned>::max());
	}

	SECTION("mata::utils::TwoDimensionalMap::get_first_inverted()") {
		TwoDimensionalMap<unsigned, true, 10> map(10, 10);
		map.insert(1, 2, 3);
		map.insert(4, 5, 6);
		map.insert(1, 5, 7);
		map.insert(4, 2, 8);

		CHECK(map.get_first_inverted(3) == 1);
		CHECK(map.get_first_inverted(6) == 4);
		CHECK(map.get_first_inverted(7) == 1);
		CHECK(map.get_first_inverted(8) == 4);
	}

	SECTION("mata::utils::TwoDimensionalMap::get_second_inverted()") {
		TwoDimensionalMap<unsigned, true, 10> map(10, 10);
		map.insert(1, 2, 3);
		map.insert(4, 5, 6);
		map.insert(1, 5, 7);
		map.insert(4, 2, 8);

		CHECK(map.get_second_inverted(3) == 2);
		CHECK(map.get_second_inverted(6) == 5);
		CHECK(map.get_second_inverted(7) == 5);
		CHECK(map.get_second_inverted(8) == 2);
	}

	SECTION("big") {
		TwoDimensionalMap<unsigned, true, 1'000> map(1'000, 1'000);
		for (unsigned i = 0; i < 1'000; ++i) {
			for (unsigned j = 0; j < 1'000; ++j) { map.insert(i, j, i + j); }
		}

		for (unsigned i = 0; i < 1'000; ++i) {
			for (unsigned j = 0; j < 1'000; ++j) { CHECK(map.get(i, j) == i + j); }
		}
	}

	SECTION("zero is a regular value") {
		TwoDimensionalMap<unsigned, true, 10> map(10, 10);
		map.insert(3, 4, 0);

		CHECK(map.get(3, 4) == 0);
		CHECK(map.get(3, 5) == std::numeric_limits<unsigned>::max());
		CHECK(map.get_first_inverted(0) == 3);
		CHECK(map.get_second_inverted(0) == 4);
	}

	SECTION("inserting a smaller value keeps the larger inverted entries") {
		TwoDimensionalMap<unsigned, true, 10> map(10, 10);
		map.insert(1, 2, 5);
		map.insert(3, 4, 3);

		CHECK(map.get_first_inverted(5) == 1);
		CHECK(map.get_second_inverted(5) == 2);
		CHECK(map.get_first_inverted(3) == 3);
		CHECK(map.get_second_inverted(3) == 4);
		CHECK(map.get(1, 2) == 5);
		CHECK(map.get(3, 4) == 3);
	}
}
