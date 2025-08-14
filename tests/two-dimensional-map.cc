/* two-dimensional-map.cc -- tests of TwoDimensionalMap
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/utils/utils.hh"
#include "mata/utils/two-dimensional-map.hh"

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
        TwoDimensionalMap<unsigned> map(1000, 1000);
        for (unsigned i = 0; i < 1000; ++i) {
            for (unsigned j = 0; j < 1000; ++j) {
                map.insert(i, j, i + j);
            }
        }

        for (unsigned i = 0; i < 1000; ++i) {
            for (unsigned j = 0; j < 1000; ++j) {
                CHECK(map.get(i, j) == i + j);
            }
        }
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
        TwoDimensionalMap<unsigned, true, 1000> map(1000, 1000);
        for (unsigned i = 0; i < 1000; ++i) {
            for (unsigned j = 0; j < 1000; ++j) {
                map.insert(i, j, i + j);
            }
        }

        for (unsigned i = 0; i < 1000; ++i) {
            for (unsigned j = 0; j < 1000; ++j) {
                CHECK(map.get(i, j) == i + j);
            }
        }
    }
}
