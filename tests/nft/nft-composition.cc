/* tests-nft-concatenation.cc -- Tests for concatenation of NFAs
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/nft/nft.hh"
#include "mata/utils/ord-vector.hh"


// #define SKIP_TESTS

using namespace mata::nft;
using namespace mata::utils;


TEST_CASE("Mata::nft::compose()") {

    Nft lhs, rhs, expected, result;
    SECTION("levels_cnt == 2") {

        SECTION("Linear structure") {

            SECTION("Epsilon free") {
                lhs = Nft(7, { 0 }, { 6 }, { 0, 1, 0, 1, 0, 1, 0 }, 2);
                lhs.delta.add(0, 'e', 1);
                lhs.delta.add(1, 'a', 2);
                lhs.delta.add(2, 'g', 3);
                lhs.delta.add(3, 'b', 4);
                lhs.delta.add(4, 'i', 5);
                lhs.delta.add(5, 'c', 6);

                rhs = Nft(7, { 0 }, { 6 }, { 0, 1, 0, 1, 0, 1, 0 }, 2);
                rhs.delta.add(0, 'a', 1);
                rhs.delta.add(1, 'f', 2);
                rhs.delta.add(2, 'b', 3);
                rhs.delta.add(3, 'h', 4);
                rhs.delta.add(4, 'c', 5);
                rhs.delta.add(5, 'j', 6);

                expected = Nft(7, { 0 }, { 6 }, { 0, 1, 0, 1, 0, 1, 0 }, 2);
                expected.delta.add(0, 'e', 1);
                expected.delta.add(1, 'f', 2);
                expected.delta.add(2, 'g', 3);
                expected.delta.add(3, 'h', 4);
                expected.delta.add(4, 'i', 5);
                expected.delta.add(5, 'j', 6);

                result = compose(lhs, rhs);

                CHECK(are_equivalent(result, expected));
            }

            SECTION("Epsilon perfectly matches") {
                lhs = Nft(9, { 0 }, { 8 }, { 0, 1, 2, 3, 0, 1, 2, 3, 0 }, 4);
                lhs.delta.add(0, 'i', 1);
                lhs.delta.add(1, 'b', 2);
                lhs.delta.add(2, 'c', 3);
                lhs.delta.add(3, EPSILON, 4);
                lhs.delta.add(4, 'k', 5);
                lhs.delta.add(5, 'f', 6);
                lhs.delta.add(6, 'g', 7);
                lhs.delta.add(7, 'l', 8);

                rhs = Nft(9, { 0 }, { 8 }, { 0, 1, 2, 3, 0, 1, 2, 3, 0 }, 4);
                rhs.delta.add(0, 'a', 1);
                rhs.delta.add(1, 'i', 2);
                rhs.delta.add(2, EPSILON, 3);
                rhs.delta.add(3, EPSILON, 4);
                rhs.delta.add(4, 'e', 5);
                rhs.delta.add(5, 'k', 6);
                rhs.delta.add(6, 'l', 7);
                rhs.delta.add(7, 'h', 8);

                expected= Nft(9, { 0 }, { 8 }, { 0, 1, 2, 3, 0, 1, 2, 3, 0 }, 4);
                expected.delta.add(0, 'a', 1);
                expected.delta.add(1, 'b', 2);
                expected.delta.add(2, 'c', 3);
                expected.delta.add(3, EPSILON, 4);
                expected.delta.add(4, 'e', 5);
                expected.delta.add(5, 'f', 6);
                expected.delta.add(6, 'g', 7);
                expected.delta.add(7, 'h', 8);

                result = compose(lhs, rhs, { 0, 3 }, { 1, 2 });

                CHECK(are_equivalent(result, expected));
            }

        }

        SECTION("Branching") {

            SECTION("Epsilon free") {
                lhs = Nft(8, { 0 }, { 6, 7 }, { 0, 1, 0, 0, 1, 1, 0, 0 }, 2);
                lhs.delta.add(0, 'e', 1);
                lhs.delta.add(1, 'a', 2);
                lhs.delta.add(2, 'c', 4);
                lhs.delta.add(4, 'e', 6);
                lhs.delta.add(1, 'b', 3);
                lhs.delta.add(3, 'd', 5);
                lhs.delta.add(5, 'f', 7);

                rhs = Nft(11, { 0 }, { 8, 9, 10 }, { 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0 }, 2);
                rhs.delta.add(0, 'e', 1);
                rhs.delta.add(1, 'a', 3);
                rhs.delta.add(3, 'c', 5);
                rhs.delta.add(5, 'e', 8);
                rhs.delta.add(0, 'b', 2);
                rhs.delta.add(2, 'd', 4);
                rhs.delta.add(4, 'f', 6);
                rhs.delta.add(6, 'g', 9);
                rhs.delta.add(4, 'f', 7);
                rhs.delta.add(7, 'h', 10);

                expected = Nft(5, { 0 }, { 4 }, { 0, 1, 0, 1, 0 }, 2);
                expected.delta.add(0, 'e', 1);
                expected.delta.add(1, 'd', 2);
                expected.delta.add(2, 'd', 3);
                expected.delta.add(3, 'g', 4);
                expected.delta.add(3, 'h', 4);

                result = compose(lhs, rhs, OrdVector<Level>{ 1 }, OrdVector<Level>{ 0 });

                CHECK(are_equivalent(result, expected));
            }

            SECTION("Epsilon perfectly matches.") {
                lhs = Nft(8, { 0 }, { 6, 7 }, { 0, 1, 0, 0, 1, 1, 0, 0 }, 2);
                lhs.delta.add(0, 'e', 1);
                lhs.delta.add(1, 'a', 2);
                lhs.delta.add(2, 'c', 4);
                lhs.delta.add(4, EPSILON, 6);
                lhs.delta.add(1, 'b', 3);
                lhs.delta.add(3, EPSILON, 5);
                lhs.delta.add(5, 'f', 7);

                rhs = Nft(11, { 0 }, { 8, 9, 10 }, { 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0 }, 2);
                rhs.delta.add(0, EPSILON, 1);
                rhs.delta.add(1, 'a', 3);
                rhs.delta.add(3, 'c', 5);
                rhs.delta.add(5, 'e', 8);
                rhs.delta.add(0, 'b', 2);
                rhs.delta.add(2, 'd', 4);
                rhs.delta.add(4, 'f', 6);
                rhs.delta.add(6, 'g', 9);
                rhs.delta.add(4, 'f', 7);
                rhs.delta.add(7, 'h', 10);

                expected = Nft(5, { 0 }, { 4 }, { 0, 1, 0, 1, 0 }, 2);
                expected.delta.add(0, 'e', 1);
                expected.delta.add(1, 'd', 2);
                expected.delta.add(2, EPSILON, 3);
                expected.delta.add(3, 'g', 4);
                expected.delta.add(3, 'h', 4);

                result = compose(lhs, rhs, OrdVector<Level>{ 1 }, OrdVector<Level>{ 0 });

                CHECK(are_equivalent(result, expected));
            }
        }

        SECTION("Cycle") {
            lhs = Nft(5, { 0 }, { 2, 4 }, { 0, 1, 0, 1, 0 }, 2);
            lhs.delta.add(0, 'a', 1);
            lhs.delta.add(1, 'b', 2);
            lhs.delta.add(2, 'c', 3);
            lhs.delta.add(3, 'e', 4);
            lhs.delta.add(3, 'd', 2);

            rhs = Nft(7, { 0 }, { 6 }, { 0, 1, 0, 1, 0, 1, 0 }, 2);
            rhs.delta.add(0, 'b', 1);
            rhs.delta.add(1, 'x', 2);
            rhs.delta.add(2, 'd', 3);
            rhs.delta.add(3, 'y', 4);
            rhs.delta.add(4, 'f', 4);
            rhs.delta.add(4, 'd', 5);
            rhs.delta.add(5, 'z', 6);

            expected = Nft(7, { 0 }, { 6 }, { 0, 1, 0, 1, 0, 1, 0 }, 2);
            expected.delta.add(0, 'a', 1);
            expected.delta.add(1, 'x', 2);
            expected.delta.add(2, 'c', 3);
            expected.delta.add(3, 'y', 4);
            expected.delta.add(4, 'c', 5);
            expected.delta.add(5, 'z', 6);

            result = compose(lhs, rhs);

            CHECK(are_equivalent(result, expected));

        }

        SECTION("Epsilon does not match on synchronization level.") {

            SECTION("Epsilon only on synchronization levels") {
                lhs = Nft(7, { 0 }, { 6 }, { 0, 1, 0, 1, 0, 1, 0 }, 2);
                lhs.delta.add(0, 'x', 1);
                lhs.delta.add(1, EPSILON, 2);
                lhs.delta.add(2, 'y', 3);
                lhs.delta.add(3, 'a', 4);
                lhs.delta.add(4, 'x', 5);
                lhs.delta.add(5, 'c', 6);

                rhs = Nft(5, { 0 }, { 4 }, { 0, 1, 0, 1, 0 }, 2);
                rhs.delta.add(0, 'a', 1);
                rhs.delta.add(1, 'b', 2);
                rhs.delta.add(2, 'c', 3);
                rhs.delta.add(3, 'd', 4);

                expected = Nft(7, { 0 }, { 6 }, { 0, 1, 0, 1, 0, 1, 0 }, 2);
                expected.delta.add(0, 'x', 1);
                expected.delta.add(1, EPSILON, 2);
                expected.delta.add(2, 'y', 3);
                expected.delta.add(3, 'b', 4);
                expected.delta.add(4, 'x', 5);
                expected.delta.add(5, 'd', 6);

                result = compose(lhs, rhs);

                CHECK(are_equivalent(result, expected));

            }

            SECTION("Epsilon is even on non-synchronization levels") {
                lhs = Nft(7, { 0 }, { 6 }, { 0, 1, 0, 1, 0, 1, 0 }, 2);
                lhs.delta.add(0, 'x', 1);
                lhs.delta.add(1, EPSILON, 2);
                lhs.delta.add(2, EPSILON, 3);
                lhs.delta.add(3, 'a', 4);
                lhs.delta.add(4, 'x', 5);
                lhs.delta.add(5, EPSILON, 6);
                rhs = Nft(5, { 0 }, { 4 }, { 0, 1, 0, 1, 0 }, 2);
                rhs.delta.add(0, 'a', 1);
                rhs.delta.add(1, 'b', 2);
                rhs.delta.add(2, EPSILON, 3);
                rhs.delta.add(3, 'd', 4);

                expected = Nft(13, { 0 }, { 6 }, { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1 }, 2);
                expected.delta.add(0, 'x', 1);
                expected.delta.add(1, EPSILON, 2);
                expected.delta.add(2, EPSILON, 3);
                expected.delta.add(3, 'b', 4);
                expected.delta.add(4, 'x', 5);
                expected.delta.add(4, EPSILON, 10);
                expected.delta.add(10, 'd', 11);
                expected.delta.add(11, 'x', 12);
                expected.delta.add(12, EPSILON, 6);
                expected.delta.add(4, 'x', 7);
                expected.delta.add(7, EPSILON, 8);
                expected.delta.add(8, EPSILON, 9);
                expected.delta.add(9, 'd', 6);
                expected.delta.add(5, 'd', 6);

                result = compose(lhs, rhs);

                CHECK(are_equivalent(result, expected));
            }
        }
    }

    SECTION("lhs.num_of_levels != rhs.num_of_levels") {
        SECTION("lhs.num_of_levels > rhs.num_of_levels") {
            lhs = Nft(6, { 0 }, { 5 }, { 0, 1, 2, 3, 4, 0 }, 5);
            lhs.delta.add(0, 'a', 1);
            lhs.delta.add(1, 'b', 2);
            lhs.delta.add(2, 'c', 3);
            lhs.delta.add(3, 'e', 4);
            lhs.delta.add(4, 'd', 5);

            rhs = Nft(4, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);
            rhs.delta.add(0, 'b', 1);
            rhs.delta.add(1, 'd', 2);
            rhs.delta.add(2, 'f', 3);

            expected = Nft(5, { 0 }, { 4 }, { 0, 1, 2, 3, 0 }, 4);
            expected.delta.add(0, 'a', 1);
            expected.delta.add(1, 'c', 2);
            expected.delta.add(2, 'e', 3);
            expected.delta.add(3, 'f', 4);

            result = compose(lhs, rhs, { 1, 4 }, { 0, 1 });

            CHECK(are_equivalent(result, expected));
        }

        SECTION("lhs.num_of_levels < rhs.num_of_levels") {
            lhs = Nft(4, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);
            lhs.delta.add(0, 'b', 1);
            lhs.delta.add(1, 'd', 2);
            lhs.delta.add(2, 'f', 3);

            rhs = Nft(6, { 0 }, { 5 }, { 0, 1, 2, 3, 4, 0 }, 5);
            rhs.delta.add(0, 'a', 1);
            rhs.delta.add(1, 'b', 2);
            rhs.delta.add(2, 'c', 3);
            rhs.delta.add(3, 'e', 4);
            rhs.delta.add(4, 'd', 5);

            expected = Nft(5, { 0 }, { 4 }, { 0, 1, 2, 3, 0 }, 4);
            expected.delta.add(0, 'a', 1);
            expected.delta.add(1, 'c', 2);
            expected.delta.add(2, 'e', 3);
            expected.delta.add(3, 'f', 4);

            result = compose(lhs, rhs, { 0, 1 }, { 1, 4 });

            CHECK(are_equivalent(result, expected));
        }
    }

    SECTION("num_of_levels == 4") {

        SECTION("Epsilon free") {
            lhs = Nft(9, { 0 }, { 8 }, { 0, 1, 2, 3, 0, 1, 2, 3, 0 }, 4);
            lhs.delta.add(0, 'i', 1);
            lhs.delta.add(1, 'b', 2);
            lhs.delta.add(2, 'c', 3);
            lhs.delta.add(3, 'j', 4);
            lhs.delta.add(4, 'k', 5);
            lhs.delta.add(5, 'f', 6);
            lhs.delta.add(6, 'g', 7);
            lhs.delta.add(7, 'l', 8);

            rhs = Nft(9, { 0 }, { 8 }, { 0, 1, 2, 3, 0, 1, 2, 3, 0 }, 4);
            rhs.delta.add(0, 'a', 1);
            rhs.delta.add(1, 'i', 2);
            rhs.delta.add(2, 'j', 3);
            rhs.delta.add(3, 'd', 4);
            rhs.delta.add(4, 'e', 5);
            rhs.delta.add(5, 'k', 6);
            rhs.delta.add(6, 'l', 7);
            rhs.delta.add(7, 'h', 8);

            expected= Nft(9, { 0 }, { 8 }, { 0, 1, 2, 3, 0, 1, 2, 3, 0 }, 4);
            expected.delta.add(0, 'a', 1);
            expected.delta.add(1, 'b', 2);
            expected.delta.add(2, 'c', 3);
            expected.delta.add(3, 'd', 4);
            expected.delta.add(4, 'e', 5);
            expected.delta.add(5, 'f', 6);
            expected.delta.add(6, 'g', 7);
            expected.delta.add(7, 'h', 8);

            result = compose(lhs, rhs, { 0, 3 }, { 1, 2 });

            CHECK(are_equivalent(result, expected));
        }

        SECTION("Epsilon perfectly matches") {
            lhs = Nft(9, { 0 }, { 8 }, { 0, 1, 2, 3, 0, 1, 2, 3, 0 }, 4);
            lhs.delta.add(0, 'i', 1);
            lhs.delta.add(1, 'b', 2);
            lhs.delta.add(2, 'c', 3);
            lhs.delta.add(3, EPSILON, 4);
            lhs.delta.add(4, 'k', 5);
            lhs.delta.add(5, 'f', 6);
            lhs.delta.add(6, 'g', 7);
            lhs.delta.add(7, 'l', 8);

            rhs = Nft(9, { 0 }, { 8 }, { 0, 1, 2, 3, 0, 1, 2, 3, 0 }, 4);
            rhs.delta.add(0, 'a', 1);
            rhs.delta.add(1, 'i', 2);
            rhs.delta.add(2, EPSILON, 3);
            rhs.delta.add(3, EPSILON, 4);
            rhs.delta.add(4, 'e', 5);
            rhs.delta.add(5, 'k', 6);
            rhs.delta.add(6, 'l', 7);
            rhs.delta.add(7, 'h', 8);

            expected= Nft(9, { 0 }, { 8 }, { 0, 1, 2, 3, 0, 1, 2, 3, 0 }, 4);
            expected.delta.add(0, 'a', 1);
            expected.delta.add(1, 'b', 2);
            expected.delta.add(2, 'c', 3);
            expected.delta.add(3, EPSILON, 4);
            expected.delta.add(4, 'e', 5);
            expected.delta.add(5, 'f', 6);
            expected.delta.add(6, 'g', 7);
            expected.delta.add(7, 'h', 8);

            result = compose(lhs, rhs, { 0, 3 }, { 1, 2 });

            CHECK(are_equivalent(result, expected));
        }

        SECTION("Epsilon only on synchronization levels") {
            lhs = Nft(9, { 0 }, { 8 }, { 0, 1, 2, 3, 0, 1, 2, 3, 0 }, 4);
            lhs.delta.add(0, EPSILON, 1);
            lhs.delta.add(1, 'c', 2);
            lhs.delta.add(2, 'd', 3);
            lhs.delta.add(3, EPSILON, 4);
            lhs.delta.add(4, 'b', 5);
            lhs.delta.add(5, 'g', 6);
            lhs.delta.add(6, 'h', 7);
            lhs.delta.add(7, 'a', 8);

            rhs = Nft(9, { 0 }, { 4, 8 }, { 0, 1, 2, 3, 0, 1, 2, 3, 0 }, 4);
            rhs.delta.add(0, 'e', 1);
            rhs.delta.add(1, 'b', 2);
            rhs.delta.add(2, 'a', 3);
            rhs.delta.add(3, 'f', 4);
            rhs.delta.add(4, 'i', 5);
            rhs.delta.add(5, 'x', 6);
            rhs.delta.add(6, 'y', 7);
            rhs.delta.add(7, 'j', 8);

            expected= Nft(9, { 0 }, { 8 }, { 0, 1, 2, 3, 0, 1, 2, 3, 0 }, 4);
            expected.delta.add(0, EPSILON, 1);
            expected.delta.add(1, 'c', 2);
            expected.delta.add(2, 'd', 3);
            expected.delta.add(3, EPSILON, 4);
            expected.delta.add(4, 'e', 5);
            expected.delta.add(5, 'g', 6);
            expected.delta.add(6, 'h', 7);
            expected.delta.add(7, 'f', 8);

            result = compose(lhs, rhs, { 0, 3 }, { 1, 2 });

            CHECK(are_equivalent(result, expected));
        }
    }

    SECTION("level ordering") {
        SECTION("no jump") {
            lhs = Nft(13, { 0 }, { 12 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0 }, 12);
            lhs.delta.add(0, 'u', 1);
            lhs.delta.add(1, 'a', 2);
            lhs.delta.add(2, 'b', 3);
            lhs.delta.add(3, 'v', 4);
            lhs.delta.add(4, 'w', 5);
            lhs.delta.add(5, 'c', 6);
            lhs.delta.add(6, 'x', 7);
            lhs.delta.add(7, 'y', 8);
            lhs.delta.add(8, 'd', 9);
            lhs.delta.add(9, 'e', 10);
            lhs.delta.add(10, 'z', 11);
            lhs.delta.add(11, 'f', 12);

            rhs = Nft(14, { 0 }, { 13 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0 }, 13);
            rhs.delta.add(0, 'g', 1);
            rhs.delta.add(1, 'h', 2);
            rhs.delta.add(2, 'u', 3);
            rhs.delta.add(3, 'i', 4);
            rhs.delta.add(4, 'v', 5);
            rhs.delta.add(5, 'w', 6);
            rhs.delta.add(6, 'j', 7);
            rhs.delta.add(7, 'x', 8);
            rhs.delta.add(8, 'y', 9);
            rhs.delta.add(9, 'k', 10);
            rhs.delta.add(10, 'l', 11);
            rhs.delta.add(11, 'm', 12);
            rhs.delta.add(12, 'z', 13);

            expected = Nft(14, { 0 }, { 13 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0 }, 13);
            expected.delta.add(0, 'g', 1);
            expected.delta.add(1, 'h', 2);
            expected.delta.add(2, 'a', 3);
            expected.delta.add(3, 'b', 4);
            expected.delta.add(4, 'i', 5);
            expected.delta.add(5, 'c', 6);
            expected.delta.add(6, 'j', 7);
            expected.delta.add(7, 'd', 8);
            expected.delta.add(8, 'e', 9);
            expected.delta.add(9, 'k', 10);
            expected.delta.add(10, 'l', 11);
            expected.delta.add(11, 'm', 12);
            expected.delta.add(12, 'f', 13);

            result = compose(lhs, rhs, { 0, 3, 4, 6, 7, 10 }, { 2, 4, 5, 7, 8, 12 });

            CHECK(are_equivalent(result, expected));
        }

        SECTION("long jump - JumpMode::RepeatSymbol") {
            lhs = Nft(12, { 0 }, { 11 }, { 0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0 }, 12);
            lhs.delta.add(0, 'u', 1);
            lhs.delta.add(1, 'a', 2);
            lhs.delta.add(2, 'v', 3);
            lhs.delta.add(3, 'w', 4);
            lhs.delta.add(4, 'c', 5);
            lhs.delta.add(5, 'x', 6);
            lhs.delta.add(6, 'y', 7);
            lhs.delta.add(7, 'd', 8);
            lhs.delta.add(8, 'e', 9);
            lhs.delta.add(9, 'z', 10);
            lhs.delta.add(10, 'f', 11);

            rhs = Nft(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 0 }, 13);
            rhs.delta.add(0, 'g', 1);
            rhs.delta.add(1, 'h', 2);
            rhs.delta.add(2, 'u', 3);
            rhs.delta.add(3, 'i', 4);
            rhs.delta.add(4, 'v', 5);
            rhs.delta.add(5, 'w', 6);
            rhs.delta.add(6, 'j', 7);
            rhs.delta.add(7, 'x', 8);
            rhs.delta.add(8, 'y', 9);
            rhs.delta.add(9, 'z', 10);

            expected = Nft(14, { 0 }, { 13 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0 }, 13);
            expected.delta.add(0, 'g', 1);
            expected.delta.add(1, 'h', 2);
            expected.delta.add(2, 'a', 3);
            expected.delta.add(3, 'a', 4);
            expected.delta.add(4, 'i', 5);
            expected.delta.add(5, 'c', 6);
            expected.delta.add(6, 'j', 7);
            expected.delta.add(7, 'd', 8);
            expected.delta.add(8, 'e', 9);
            expected.delta.add(9, 'y', 10);
            expected.delta.add(10, 'y', 11);
            expected.delta.add(11, 'y', 12);
            expected.delta.add(12, 'f', 13);

            result = compose(lhs, rhs, { 0, 3, 4, 6, 7, 10 }, { 2, 4, 5, 7, 8, 12 }, true, JumpMode::RepeatSymbol);

            CHECK(are_equivalent(result, expected, JumpMode::RepeatSymbol));
        }

        SECTION("long jump - JumpMode::AppendDontCares") {
            lhs = Nft(12, { 0 }, { 11 }, { 0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0 }, 12);
            lhs.delta.add(0, 'u', 1);
            lhs.delta.add(1, 'a', 2);
            lhs.delta.add(2, 'v', 3);
            lhs.delta.add(3, 'w', 4);
            lhs.delta.add(4, 'c', 5);
            lhs.delta.add(5, 'x', 6);
            lhs.delta.add(6, 'y', 7);
            lhs.delta.add(7, 'd', 8);
            lhs.delta.add(8, 'e', 9);
            lhs.delta.add(9, 'z', 10);
            lhs.delta.add(10, 'f', 11);

            rhs = Nft(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 0 }, 13);
            rhs.delta.add(0, 'g', 1);
            rhs.delta.add(1, 'h', 2);
            rhs.delta.add(2, 'u', 3);
            rhs.delta.add(3, 'i', 4);
            rhs.delta.add(4, 'v', 5);
            rhs.delta.add(5, 'w', 6);
            rhs.delta.add(6, 'j', 7);
            rhs.delta.add(7, 'x', 8);
            rhs.delta.add(8, 'y', 9);
            rhs.delta.add(9, 'z', 10);

            expected = Nft(14, { 0 }, { 13 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0 }, 13);
            expected.delta.add(0, 'g', 1);
            expected.delta.add(1, 'h', 2);
            expected.delta.add(2, 'a', 3);
            expected.delta.add(3, DONT_CARE, 4);
            expected.delta.add(4, 'i', 5);
            expected.delta.add(5, 'c', 6);
            expected.delta.add(6, 'j', 7);
            expected.delta.add(7, 'd', 8);
            expected.delta.add(8, 'e', 9);
            expected.delta.add(9, DONT_CARE, 10);
            expected.delta.add(10, DONT_CARE, 11);
            expected.delta.add(11, DONT_CARE, 12);
            expected.delta.add(12, 'f', 13);

            result = compose(lhs, rhs, { 0, 3, 4, 6, 7, 10 }, { 2, 4, 5, 7, 8, 12 }, true, JumpMode::AppendDontCares);

            CHECK(are_equivalent(result, expected, JumpMode::AppendDontCares));
        }
    }
}

#ifndef SKIP_TESTS
TEST_CASE("nft::compose(Nft&, Nft&, Level, Level, ...) - easy cases") {
    SECTION("2 levels x 2 levels") {
        Nft expected_full(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        expected_full.delta.add(0, 'a', 1);
        expected_full.delta.add(1, 'b', 2);
        expected_full.delta.add(2, 'e', 3);
        expected_full.delta.add(3, 'c', 4);
        expected_full.delta.add(4, 'd', 5);
        expected_full.delta.add(5, 'f', 6);
        Nft expected_proj = project_out(expected_full, { 1 }, JumpMode::NoJump);

        Nft lhs(5, { 0 }, { 4 }, { 0, 1, 0, 1, 0 }, 2);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);

        Nft rhs(5, { 0 }, { 4 }, { 0, 1, 0, 1, 0 }, 2);
        rhs.delta.add(0, 'b', 1);
        rhs.delta.add(1, 'e', 2);
        rhs.delta.add(2, 'd', 3);
        rhs.delta.add(3, 'f', 4);

        SECTION("LHS | RHS") {
            Nft result_full = compose(lhs, rhs, 1, 0, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft result_full = compose(rhs, lhs, 0, 1, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 1, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));

        }
    }
    SECTION("2 levels x 1 level") {
        Nft expected_full(5, { 0 }, { 4 }, { 0, 1, 0, 1, 0 }, 2);
        expected_full.delta.add(0, 'a', 1);
        expected_full.delta.add(1, 'b', 2);
        expected_full.delta.add(2, 'c', 3);
        expected_full.delta.add(3, 'd', 4);
        Nft expected_proj = project_out(expected_full, { 1 }, JumpMode::NoJump);

        Nft lhs(5, { 0 }, { 4 }, { 0, 1, 0, 1, 0 }, 2);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);

        Nft rhs(3, { 0 }, { 2 }, { 0, 0, 0 }, 1);
        rhs.delta.add(0, 'b', 1);
        rhs.delta.add(1, 'd', 2);

        SECTION("LHS | RHS") {
            Nft result_full = compose(lhs, rhs, 1, 0, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft result_full = compose(rhs, lhs, 0, 1, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 1, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }
    SECTION("1 level x 1 level") {
        Nft expected_full(4, { 0 }, { 3 }, { 0, 0, 0, 0 }, 1);
        expected_full.delta.add(0, 'a', 1);
        expected_full.delta.add(1, 'b', 2);
        expected_full.delta.add(2, 'c', 3);

        Nft lhs(4, { 0 }, { 3 }, { 0, 0, 0, 0 }, 1);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);

        Nft rhs(4, { 0 }, { 3 }, { 0, 0, 0, 0 }, 1);
        rhs.delta.add(0, 'a', 1);
        rhs.delta.add(1, 'b', 2);
        rhs.delta.add(2, 'c', 3);

        Nft result_full = compose(lhs, rhs, 0, 0, false, JumpMode::NoJump);
        CHECK(result_full.num_of_states() == expected_full.num_of_states());
        CHECK(result_full.num_of_levels == expected_full.num_of_levels);
        CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
        CHECK(are_equivalent(result_full, expected_full));
    }
}
#endif




TEST_CASE("nft::compose(..., Level, Level, ...) - sliding without epsilon") {
#ifndef SKIP_TESTS
    SECTION("sync level 0 x sync level 0") {
        Nft lhs(5, { 0 }, { 4 }, { 0, 1, 0, 1, 0 }, 2);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'd', 3);
        lhs.delta.add(3, 'e', 4);

        Nft rhs(5, { 0 }, { 4 }, { 0, 1, 0, 1, 0 }, 2);
        rhs.delta.add(0, 'a', 1);
        rhs.delta.add(1, 'c', 2);
        rhs.delta.add(2, 'd', 3);
        rhs.delta.add(3, 'f', 4);

        SECTION("LHS | RHS") {
            Nft expected_full(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, 'b', 2);
            expected_full.delta.add(2, 'c', 3);
            expected_full.delta.add(3, 'd', 4);
            expected_full.delta.add(4, 'e', 5);
            expected_full.delta.add(5, 'f', 6);
            Nft expected_proj = project_out(expected_full, { 0 }, JumpMode::NoJump);

            Nft result_full = compose(lhs, rhs, 0, 0, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 0, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, 'c', 2);
            expected_full.delta.add(2, 'b', 3);
            expected_full.delta.add(3, 'd', 4);
            expected_full.delta.add(4, 'f', 5);
            expected_full.delta.add(5, 'e', 6);
            Nft expected_proj = project_out(expected_full, { 0 }, JumpMode::NoJump);

            Nft result_full = compose(rhs, lhs, 0, 0, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 0 x sync level 1") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'x', 0);
        lhs.delta.add(2, 'z', 6);

        Nft rhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'a', 2);
        rhs.delta.add(2, 'h', 3);
        rhs.delta.add(3, 'i', 4);
        rhs.delta.add(4, 'd', 5);
        rhs.delta.add(5, 'j', 6);
        rhs.delta.add(2, 'w', 0);
        rhs.delta.add(2, 'y', 6);

        SECTION("LHS | RHS") {
            Nft expected_full(13, { 0 }, { 12 }, { 0, 1, 2, 3, 4, 4, 0, 1, 2, 3, 4, 4, 0 }, 5);
            expected_full.delta.add(0, 'g', 1);
            expected_full.delta.add(1, 'a', 2);
            expected_full.delta.add(2, 'b', 3);
            expected_full.delta.add(3, 'x', 4);
            expected_full.delta.add(3, 'c', 5);
            expected_full.delta.add(3, 'z', 11);
            expected_full.delta.add(4, 'w', 0);
            expected_full.delta.add(5, 'h', 6);
            expected_full.delta.add(6, 'i', 7);
            expected_full.delta.add(7, 'd', 8);
            expected_full.delta.add(8, 'e', 9);
            expected_full.delta.add(9, 'f', 10);
            expected_full.delta.add(10, 'j', 12);
            expected_full.delta.add(11, 'y', 12);
            Nft expected_proj = project_out(expected_full, { 1 }, JumpMode::NoJump);

            Nft result_full = compose(lhs, rhs, 0, 1, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 0, 1, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(15, { 0 }, { 12 }, { 0, 4, 3, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 3, 4 }, 5);
            expected_full.delta.add(0, 'g', 3);
            expected_full.delta.add(3, 'a', 4);
            expected_full.delta.add(4, 'w', 2);
            expected_full.delta.add(2, 'b', 1);
            expected_full.delta.add(1, 'x', 0);
            expected_full.delta.add(4, 'h', 5);
            expected_full.delta.add(5, 'b', 6);
            expected_full.delta.add(6, 'c', 7);
            expected_full.delta.add(7, 'i', 8);
            expected_full.delta.add(8, 'd', 9);
            expected_full.delta.add(9, 'j', 10);
            expected_full.delta.add(10, 'e', 11);
            expected_full.delta.add(11, 'f', 12);
            expected_full.delta.add(4, 'y', 13);
            expected_full.delta.add(13, 'b', 14);
            expected_full.delta.add(14, 'z', 12);
            Nft expected_proj = project_out(expected_full, { 1 }, JumpMode::NoJump);

            Nft result_full = compose(rhs, lhs, 1, 0, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 1, 0, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 0 x sync level 2") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'x', 0);
        lhs.delta.add(2, 'z', 6);

        Nft rhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'h', 2);
        rhs.delta.add(2, 'a', 3);
        rhs.delta.add(3, 'i', 4);
        rhs.delta.add(4, 'j', 5);
        rhs.delta.add(5, 'd', 6);
        rhs.delta.add(2, 'a', 0);
        rhs.delta.add(2, 'a', 6);

        Nft expected_full(15, { 0 }, { 12 }, { 0, 1, 2, 3, 4, 3, 4, 0, 1, 2, 3, 4, 0, 4, 3 }, 5);
        expected_full.delta.add(0, 'g', 1);
        expected_full.delta.add(1, 'h', 2);
        expected_full.delta.add(2, 'a', 3);
        expected_full.delta.add(3, 'b', 4);
        expected_full.delta.add(4, 'x', 0);
        expected_full.delta.add(2, 'a', 5);
        expected_full.delta.add(5, 'b', 6);
        expected_full.delta.add(6, 'c', 7);
        expected_full.delta.add(7, 'i', 8);
        expected_full.delta.add(8, 'j', 9);
        expected_full.delta.add(9, 'd', 10);
        expected_full.delta.add(10, 'e', 11);
        expected_full.delta.add(11, 'f', 12);
        expected_full.delta.add(2, 'a', 14);
        expected_full.delta.add(14, 'b', 13);
        expected_full.delta.add(13, 'z', 12);
        Nft expected_proj = project_out(expected_full, { 2 }, JumpMode::NoJump);

        SECTION("LHS | RHS") {
            Nft result_full = compose(lhs, rhs, 0, 2, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 0, 2, true, JumpMode::RepeatSymbol).trim();
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft result_full = compose(rhs, lhs, 2, 0, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 2, 0, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sycn level 1 x sync level 0") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'w', 0);
        lhs.delta.add(2, 'z', 6);

        Nft rhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'b', 1);
        rhs.delta.add(1, 'g', 2);
        rhs.delta.add(2, 'h', 3);
        rhs.delta.add(3, 'e', 4);
        rhs.delta.add(4, 'i', 5);
        rhs.delta.add(5, 'j', 6);
        rhs.delta.add(2, 'x', 0);
        rhs.delta.add(2, 'y', 6);

        SECTION("LHS | RHS") {
            Nft expected_full(15, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 3, 3, 4 }, 5);
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, 'b', 2);
            expected_full.delta.add(2, 'c', 3);
            expected_full.delta.add(3, 'g', 4);
            expected_full.delta.add(4, 'h', 5);
            expected_full.delta.add(5, 'd', 6);
            expected_full.delta.add(6, 'e', 7);
            expected_full.delta.add(7, 'f', 8);
            expected_full.delta.add(8, 'i', 9);
            expected_full.delta.add(9, 'j', 10);
            expected_full.delta.add(11, 'y', 10);
            expected_full.delta.add(12, 'g', 11);
            expected_full.delta.add(2, 'z', 12);
            expected_full.delta.add(2, 'w', 13);
            expected_full.delta.add(13, 'g', 14);
            expected_full.delta.add(14, 'x', 0);
            Nft expected_proj = project_out(expected_full, { 1 }, JumpMode::NoJump);

            Nft result_full = compose(lhs, rhs, 1, 0, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 0, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }

        SECTION("RHS | LHS") {
            Nft expected_full(13, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 4 }, 5);
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, 'b', 2);
            expected_full.delta.add(2, 'g', 3);
            expected_full.delta.add(3, 'h', 4);
            expected_full.delta.add(4, 'c', 5);
            expected_full.delta.add(5, 'd', 6);
            expected_full.delta.add(6, 'e', 7);
            expected_full.delta.add(7, 'i', 8);
            expected_full.delta.add(8, 'j', 9);
            expected_full.delta.add(9, 'f', 10);
            expected_full.delta.add(11, 'z', 10);
            expected_full.delta.add(3, 'y', 11);
            expected_full.delta.add(3, 'x', 12);
            expected_full.delta.add(12, 'w', 0);
            Nft expected_proj = project_out(expected_full, { 1 }, JumpMode::NoJump);

            Nft result_full = compose(rhs, lhs, 0, 1, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 1, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 1 x sync level 1") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'x', 0);
        lhs.delta.add(2, 'z', 6);

        Nft rhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'b', 2);
        rhs.delta.add(2, 'h', 3);
        rhs.delta.add(3, 'i', 4);
        rhs.delta.add(4, 'e', 5);
        rhs.delta.add(5, 'j', 6);
        rhs.delta.add(2, 'w', 0);
        rhs.delta.add(2, 'y', 6);

        SECTION("LHS | RHS") {
            Nft expected_full(13, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 4 }, 5);
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, 'g', 2);
            expected_full.delta.add(2, 'b', 3);
            expected_full.delta.add(3, 'c', 4);
            expected_full.delta.add(4, 'h', 5);
            expected_full.delta.add(5, 'd', 6);
            expected_full.delta.add(6, 'i', 7);
            expected_full.delta.add(7, 'e', 8);
            expected_full.delta.add(8, 'f', 9);
            expected_full.delta.add(9, 'j', 10);
            expected_full.delta.add(11, 'y', 10);
            expected_full.delta.add(3, 'x', 12);
            expected_full.delta.add(12, 'w', 0);
            expected_full.delta.add(3, 'z', 11);
            Nft expected_proj = project_out(expected_full, { 2 }, JumpMode::NoJump);

            Nft result_full = compose(lhs, rhs, 1, 1, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 1, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(13, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 4 }, 5);
            expected_full.delta.add(0, 'g', 1);
            expected_full.delta.add(1, 'a', 2);
            expected_full.delta.add(2, 'b', 3);
            expected_full.delta.add(3, 'h', 4);
            expected_full.delta.add(4, 'c', 5);
            expected_full.delta.add(5, 'i', 6);
            expected_full.delta.add(6, 'd', 7);
            expected_full.delta.add(7, 'e', 8);
            expected_full.delta.add(8, 'j', 9);
            expected_full.delta.add(9, 'f', 10);
            expected_full.delta.add(11, 'z', 10);
            expected_full.delta.add(3, 'y', 11);
            expected_full.delta.add(3, 'w', 12);
            expected_full.delta.add(12, 'x', 0);
            Nft expected_proj = project_out(expected_full, { 2 }, JumpMode::NoJump);

            Nft result_full = compose(rhs, lhs, 1, 1, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 1, 1, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 1 x sync level 2") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'x', 0);
        lhs.delta.add(2, 'z', 6);

        Nft rhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'h', 2);
        rhs.delta.add(2, 'b', 3);
        rhs.delta.add(3, 'i', 4);
        rhs.delta.add(4, 'j', 5);
        rhs.delta.add(5, 'e', 6);
        rhs.delta.add(2, 'b', 0);
        rhs.delta.add(2, 'b', 6);

        SECTION("LHS | RHS") {
            Nft expected_full(13, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 4 }, 5);
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, 'g', 2);
            expected_full.delta.add(2, 'h', 3);
            expected_full.delta.add(3, 'b', 4);
            expected_full.delta.add(4, 'c', 5);
            expected_full.delta.add(5, 'd', 6);
            expected_full.delta.add(6, 'i', 7);
            expected_full.delta.add(7, 'j', 8);
            expected_full.delta.add(8, 'e', 9);
            expected_full.delta.add(9, 'f', 10);
            expected_full.delta.add(3, 'b', 12);
            expected_full.delta.add(12, 'x', 0);
            expected_full.delta.add(3, 'b', 11);
            expected_full.delta.add(11, 'z', 10);
            Nft expected_proj = project_out(expected_full, { 3 }, JumpMode::NoJump);

            Nft result_full = compose(lhs, rhs, 1, 2, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 2, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(13, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 4 }, 5);
            expected_full.delta.add(0, 'g', 1);
            expected_full.delta.add(1, 'h', 2);
            expected_full.delta.add(2, 'a', 3);
            expected_full.delta.add(3, 'b', 4);
            expected_full.delta.add(4, 'c', 5);
            expected_full.delta.add(5, 'i', 6);
            expected_full.delta.add(6, 'j', 7);
            expected_full.delta.add(7, 'd', 8);
            expected_full.delta.add(8, 'e', 9);
            expected_full.delta.add(9, 'f', 10);
            expected_full.delta.add(3, 'b', 12);
            expected_full.delta.add(12, 'x', 0);
            expected_full.delta.add(3, 'b', 11);
            expected_full.delta.add(11, 'z', 10);
            Nft expected_proj = project_out(expected_full, { 3 }, JumpMode::NoJump);

            Nft result_full = compose(rhs, lhs, 2, 1, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 2, 1, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 2 x sync level 0") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);
        lhs.delta.add(2, 'c', 6);

        Nft rhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'c', 1);
        rhs.delta.add(1, 'g', 2);
        rhs.delta.add(2, 'h', 3);
        rhs.delta.add(3, 'f', 4);
        rhs.delta.add(4, 'i', 5);
        rhs.delta.add(5, 'j', 6);
        rhs.delta.add(2, 'x', 0);
        rhs.delta.add(2, 'y', 6);

        Nft expected_full(15, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 3, 3, 4 }, 5);
        expected_full.delta.add(0, 'a', 1);
        expected_full.delta.add(1, 'b', 2);
        expected_full.delta.add(2, 'c', 3);
        expected_full.delta.add(3, 'g', 4);
        expected_full.delta.add(4, 'h', 5);
        expected_full.delta.add(5, 'd', 6);
        expected_full.delta.add(6, 'e', 7);
        expected_full.delta.add(7, 'f', 8);
        expected_full.delta.add(8, 'i', 9);
        expected_full.delta.add(9, 'j', 10);
        expected_full.delta.add(2, 'c', 13);
        expected_full.delta.add(13, 'g', 14);
        expected_full.delta.add(14, 'x', 0);
        expected_full.delta.add(2, 'c', 12);
        expected_full.delta.add(12, 'g', 11);
        expected_full.delta.add(11, 'y', 10);
        Nft expected_proj = project_out(expected_full, { 2 }, JumpMode::NoJump);

        SECTION("LHS | RHS") {
            Nft result_full = compose(lhs, rhs, 2, 0, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 2, 0, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft result_full = compose(rhs, lhs, 0, 2, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 2, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 2 x sync level 1") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);
        lhs.delta.add(2, 'c', 6);

        Nft rhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'c', 2);
        rhs.delta.add(2, 'h', 3);
        rhs.delta.add(3, 'i', 4);
        rhs.delta.add(4, 'f', 5);
        rhs.delta.add(5, 'j', 6);
        rhs.delta.add(2, 'x', 0);
        rhs.delta.add(2, 'y', 6);

        SECTION("LHS | RHS") {
            Nft expected_full(13, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 4 }, 5);
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, 'b', 2);
            expected_full.delta.add(2, 'g', 3);
            expected_full.delta.add(3, 'c', 4);
            expected_full.delta.add(4, 'h', 5);
            expected_full.delta.add(5, 'd', 6);
            expected_full.delta.add(6, 'e', 7);
            expected_full.delta.add(7, 'i', 8);
            expected_full.delta.add(8, 'f', 9);
            expected_full.delta.add(9, 'j', 10);
            expected_full.delta.add(3, 'c', 12);
            expected_full.delta.add(12, 'x', 0);
            expected_full.delta.add(3, 'c', 11);
            expected_full.delta.add(11, 'y', 10);
            Nft expected_proj = project_out(expected_full, { 3 }, JumpMode::NoJump);

            Nft result_full = compose(lhs, rhs, 2, 1, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 2, 1, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(13, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 4}, 5);
            expected_full.delta.add(0, 'g', 1);
            expected_full.delta.add(1, 'a', 2);
            expected_full.delta.add(2, 'b', 3);
            expected_full.delta.add(3, 'c', 4);
            expected_full.delta.add(4, 'h', 5);
            expected_full.delta.add(5, 'i', 6);
            expected_full.delta.add(6, 'd', 7);
            expected_full.delta.add(7, 'e', 8);
            expected_full.delta.add(8, 'f', 9);
            expected_full.delta.add(9, 'j', 10);
            expected_full.delta.add(3, 'c', 12);
            expected_full.delta.add(12, 'x', 0);
            expected_full.delta.add(3, 'c', 11);
            expected_full.delta.add(11, 'y', 10);
            Nft expected_proj = project_out(expected_full, { 3 }, JumpMode::NoJump);

            Nft result_full = compose(rhs, lhs, 1, 2, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 1, 2, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 2 x sync level 2") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);
        lhs.delta.add(2, 'c', 6);

        Nft rhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'h', 2);
        rhs.delta.add(2, 'c', 3);
        rhs.delta.add(3, 'i', 4);
        rhs.delta.add(4, 'j', 5);
        rhs.delta.add(5, 'f', 6);
        rhs.delta.add(2, 'c', 0);
        rhs.delta.add(2, 'c', 6);

        SECTION("LHS | RHS") {
            Nft expected_full(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }, 5);
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, 'b', 2);
            expected_full.delta.add(2, 'g', 3);
            expected_full.delta.add(3, 'h', 4);
            expected_full.delta.add(4, 'c', 5);
            expected_full.delta.add(5, 'd', 6);
            expected_full.delta.add(6, 'e', 7);
            expected_full.delta.add(7, 'i', 8);
            expected_full.delta.add(8, 'j', 9);
            expected_full.delta.add(9, 'f', 10);
            expected_full.delta.add(4, 'c', 0);
            expected_full.delta.add(4, 'c', 10);

            Nft expected_proj(9, { 0 }, { 8 }, { 0, 1, 2, 3, 0, 1, 2, 3, 0 }, 4);
            expected_proj.delta.add(0, 'a', 1);
            expected_proj.delta.add(1, 'b', 2);
            expected_proj.delta.add(2, 'g', 3);
            expected_proj.delta.add(3, 'h', 4);
            expected_proj.delta.add(4, 'd', 5);
            expected_proj.delta.add(5, 'e', 6);
            expected_proj.delta.add(6, 'i', 7);
            expected_proj.delta.add(7, 'j', 8);
            expected_proj.delta.add(3, 'h', 8);
            expected_proj.delta.add(3, 'h', 0);

            Nft result_full = compose(lhs, rhs, 2, 2, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 2, 2, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }, 5);
            expected_full.delta.add(0, 'g', 1);
            expected_full.delta.add(1, 'h', 2);
            expected_full.delta.add(2, 'a', 3);
            expected_full.delta.add(3, 'b', 4);
            expected_full.delta.add(4, 'c', 5);
            expected_full.delta.add(5, 'i', 6);
            expected_full.delta.add(6, 'j', 7);
            expected_full.delta.add(7, 'd', 8);
            expected_full.delta.add(8, 'e', 9);
            expected_full.delta.add(9, 'f', 10);
            expected_full.delta.add(4, 'c', 0);
            expected_full.delta.add(4, 'c', 10);

            Nft expected_proj(9, { 0 }, { 8 }, { 0, 1, 2, 3, 0, 1, 2, 3, 0 }, 4);
            expected_proj.delta.add(0, 'g', 1);
            expected_proj.delta.add(1, 'h', 2);
            expected_proj.delta.add(2, 'a', 3);
            expected_proj.delta.add(3, 'b', 4);
            expected_proj.delta.add(4, 'i', 5);
            expected_proj.delta.add(5, 'j', 6);
            expected_proj.delta.add(6, 'd', 7);
            expected_proj.delta.add(7, 'e', 8);
            expected_proj.delta.add(3, 'b', 8);
            expected_proj.delta.add(3, 'b', 0);

            Nft result_full = compose(rhs, lhs, 2, 2, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 2, 2, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }
#endif
}

TEST_CASE("nft::compose(..., Level, Level, ...) - sliding with epsilon") {
#ifndef SKIP_TESTS
    SECTION("sync level 0 x sync level 0") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, EPSILON, 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);

        Nft rhs(4, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'd', 1);
        rhs.delta.add(1, 'g', 2);
        rhs.delta.add(2, 'h', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(13, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 3, 4 }, 5);
            expected_full.delta.add(0, EPSILON, 1);
            expected_full.delta.add(1, 'b', 2);
            expected_full.delta.add(2, 'c', 3);
            expected_full.delta.add(3, EPSILON, 4);
            expected_full.delta.add(4, EPSILON, 5);
            expected_full.delta.add(5, 'd', 6);
            expected_full.delta.add(6, 'e', 7);
            expected_full.delta.add(7, 'f', 8);
            expected_full.delta.add(8, 'g', 9);
            expected_full.delta.add(9, 'h', 10);
            expected_full.delta.add(2, 'c', 11);
            expected_full.delta.add(11, EPSILON, 12);
            expected_full.delta.add(12, EPSILON, 0);
            Nft expected_proj = project_out(expected_full, { 0 }, JumpMode::NoJump);

            Nft result_full = compose(lhs, rhs, 0, 0, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 0, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }, 5);
            expected_full.delta.add(0, EPSILON, 1);
            expected_full.delta.add(1, EPSILON, 2);
            expected_full.delta.add(2, EPSILON, 3);
            expected_full.delta.add(3, 'b', 4);
            expected_full.delta.add(4, 'c', 5);
            expected_full.delta.add(5, 'd', 6);
            expected_full.delta.add(6, 'g', 7);
            expected_full.delta.add(7, 'h', 8);
            expected_full.delta.add(8, 'e', 9);
            expected_full.delta.add(9, 'f', 10);
            expected_full.delta.add(4, 'c', 0);
            Nft expected_proj = project_out(expected_full, { 0 }, JumpMode::NoJump);

            Nft result_full = compose(rhs, lhs, 0, 0, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 0 x sync level 1") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, EPSILON, 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);

        Nft rhs(4, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'd', 2);
        rhs.delta.add(2, 'h', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(12, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4 }, 5);
            expected_full.delta.add(0, EPSILON, 1);
            expected_full.delta.add(1, EPSILON, 2);
            expected_full.delta.add(2, 'b', 3);
            expected_full.delta.add(3, 'c', 4);
            expected_full.delta.add(4, EPSILON, 5);
            expected_full.delta.add(5, 'g', 6);
            expected_full.delta.add(6, 'd', 7);
            expected_full.delta.add(7, 'e', 8);
            expected_full.delta.add(8, 'f', 9);
            expected_full.delta.add(9, 'h', 10);
            expected_full.delta.add(3, 'c', 11);
            expected_full.delta.add(11, EPSILON, 0);
            Nft expected_proj = project_out(expected_full, { 1 }, JumpMode::NoJump);

            Nft result_full = compose(lhs, rhs, 0, 1, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 0, 1, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }, 5);
            expected_full.delta.add(0, EPSILON, 1);
            expected_full.delta.add(1, EPSILON, 2);
            expected_full.delta.add(2, EPSILON, 3);
            expected_full.delta.add(3, 'b', 4);
            expected_full.delta.add(4, 'c', 5);
            expected_full.delta.add(5, 'g', 6);
            expected_full.delta.add(6, 'd', 7);
            expected_full.delta.add(7, 'h', 8);
            expected_full.delta.add(8, 'e', 9);
            expected_full.delta.add(9, 'f', 10);
            expected_full.delta.add(4, 'c', 0);
            Nft expected_proj = project_out(expected_full, { 1 }, JumpMode::NoJump);

            Nft result_full = compose(rhs, lhs, 1, 0, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 1, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 0 x sync level 2") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, EPSILON, 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);

        Nft rhs(4, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'h', 2);
        rhs.delta.add(2, 'd', 3);

        Nft expected_full(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }, 5);
        expected_full.delta.add(0, EPSILON, 1);
        expected_full.delta.add(1, EPSILON, 2);
        expected_full.delta.add(2, EPSILON, 3);
        expected_full.delta.add(3, 'b', 4);
        expected_full.delta.add(4, 'c', 5);
        expected_full.delta.add(5, 'g', 6);
        expected_full.delta.add(6, 'h', 7);
        expected_full.delta.add(7, 'd', 8);
        expected_full.delta.add(8, 'e', 9);
        expected_full.delta.add(9, 'f', 10);
        expected_full.delta.add(4, 'c', 0);
        Nft expected_proj = project_out(expected_full, { 2 }, JumpMode::NoJump);

        SECTION("LHS | RHS") {
            Nft result_full = compose(lhs, rhs, 0, 2, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 0, 2, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft result_full = compose(rhs, lhs, 2, 0, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 2, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 1 x sync level 0") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, EPSILON, 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);

        Nft rhs(4, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'e', 1);
        rhs.delta.add(1, 'g', 2);
        rhs.delta.add(2, 'h', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(13, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 3, 4 }, 5);
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, EPSILON, 2);
            expected_full.delta.add(2, 'c', 3);
            expected_full.delta.add(3, EPSILON, 4);
            expected_full.delta.add(4, EPSILON, 5);
            expected_full.delta.add(5, 'd', 6);
            expected_full.delta.add(6, 'e', 7);
            expected_full.delta.add(7, 'f', 8);
            expected_full.delta.add(8, 'g', 9);
            expected_full.delta.add(9, 'h', 10);
            expected_full.delta.add(2, 'c', 11);
            expected_full.delta.add(11, EPSILON, 12);
            expected_full.delta.add(12, EPSILON, 0);
            Nft expected_proj = project_out(expected_full, { 1 }, JumpMode::NoJump);

            Nft result_full = compose(lhs, rhs, 1, 0, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }, 5);
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, EPSILON, 2);
            expected_full.delta.add(2, EPSILON, 3);
            expected_full.delta.add(3, EPSILON, 4);
            expected_full.delta.add(4, 'c', 5);
            expected_full.delta.add(5, 'd', 6);
            expected_full.delta.add(6, 'e', 7);
            expected_full.delta.add(7, 'g', 8);
            expected_full.delta.add(8, 'h', 9);
            expected_full.delta.add(9, 'f', 10);
            expected_full.delta.add(4, 'c', 0);
            Nft expected_proj = project_out(expected_full, { 1 }, JumpMode::NoJump);

            Nft result_full = compose(rhs, lhs, 0, 1, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 1, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 1 x sync level 1") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, EPSILON, 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);

        Nft rhs(4, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'e', 2);
        rhs.delta.add(2, 'h', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(12, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4 }, 5);
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, EPSILON, 2);
            expected_full.delta.add(2, EPSILON, 3);
            expected_full.delta.add(3, 'c', 4);
            expected_full.delta.add(4, EPSILON, 5);
            expected_full.delta.add(5, 'd', 6);
            expected_full.delta.add(6, 'g', 7);
            expected_full.delta.add(7, 'e', 8);
            expected_full.delta.add(8, 'f', 9);
            expected_full.delta.add(9, 'h', 10);
            expected_full.delta.add(3, 'c', 11);
            expected_full.delta.add(11, EPSILON, 0);
            Nft expected_proj = project_out(expected_full, { 2 }, JumpMode::NoJump);

            Nft result_full = compose(lhs, rhs, 1, 1, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 1, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }, 5);
            expected_full.delta.add(0, EPSILON, 1);
            expected_full.delta.add(1, 'a', 2);
            expected_full.delta.add(2, EPSILON, 3);
            expected_full.delta.add(3, EPSILON, 4);
            expected_full.delta.add(4, 'c', 5);
            expected_full.delta.add(5, 'g', 6);
            expected_full.delta.add(6, 'd', 7);
            expected_full.delta.add(7, 'e', 8);
            expected_full.delta.add(8, 'h', 9);
            expected_full.delta.add(9, 'f', 10);
            expected_full.delta.add(4, 'c', 0);
            Nft expected_proj = project_out(expected_full, { 2 }, JumpMode::NoJump);

            Nft result_full = compose(rhs, lhs, 1, 1, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 1, 1, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 1 x sync level 2") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, EPSILON, 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);

        Nft rhs(4, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'h', 2);
        rhs.delta.add(2, 'e', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }, 5);
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, EPSILON, 2);
            expected_full.delta.add(2, EPSILON, 3);
            expected_full.delta.add(3, EPSILON, 4);
            expected_full.delta.add(4, 'c', 5);
            expected_full.delta.add(5, 'd', 6);
            expected_full.delta.add(6, 'g', 7);
            expected_full.delta.add(7, 'h', 8);
            expected_full.delta.add(8, 'e', 9);
            expected_full.delta.add(9, 'f', 10);
            expected_full.delta.add(4, 'c', 0);
            Nft expected_proj = project_out(expected_full, { 3 }, JumpMode::NoJump);

            Nft result_full = compose(lhs, rhs, 1, 2, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 2, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }, 5);
            expected_full.delta.add(0, EPSILON, 1);
            expected_full.delta.add(1, EPSILON, 2);
            expected_full.delta.add(2, 'a', 3);
            expected_full.delta.add(3, EPSILON, 4);
            expected_full.delta.add(4, 'c', 5);
            expected_full.delta.add(5, 'g', 6);
            expected_full.delta.add(6, 'h', 7);
            expected_full.delta.add(7, 'd', 8);
            expected_full.delta.add(8, 'e', 9);
            expected_full.delta.add(9, 'f', 10);
            expected_full.delta.add(4, 'c', 0);
            Nft expected_proj = project_out(expected_full, { 3 }, JumpMode::NoJump);

            Nft result_full = compose(rhs, lhs, 2, 1, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 2, 1, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 2 x sync level 0") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, EPSILON, 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, EPSILON, 0);

        Nft rhs(4, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'f', 1);
        rhs.delta.add(1, 'g', 2);
        rhs.delta.add(2, 'h', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }, 5);
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, 'b', 2);
            expected_full.delta.add(2, EPSILON, 3);
            expected_full.delta.add(3, EPSILON, 4);
            expected_full.delta.add(4, EPSILON, 5);
            expected_full.delta.add(5, 'd', 6);
            expected_full.delta.add(6, 'e', 7);
            expected_full.delta.add(7, 'f', 8);
            expected_full.delta.add(8, 'g', 9);
            expected_full.delta.add(9, 'h', 10);
            expected_full.delta.add(4, EPSILON, 0);
            Nft expected_proj = project_out(expected_full, { 2 }, JumpMode::NoJump);

            Nft result_full = compose(lhs, rhs, 2, 0, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 2, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }

        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }, 5);
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, 'b', 2);
            expected_full.delta.add(2, EPSILON, 3);
            expected_full.delta.add(3, EPSILON, 4);
            expected_full.delta.add(4, EPSILON, 5);
            expected_full.delta.add(5, 'd', 6);
            expected_full.delta.add(6, 'e', 7);
            expected_full.delta.add(7, 'f', 8);
            expected_full.delta.add(8, 'g', 9);
            expected_full.delta.add(9, 'h', 10);
            expected_full.delta.add(4, EPSILON, 0);
            Nft expected_proj = project_out(expected_full, { 2 }, JumpMode::NoJump);

            Nft result_full = compose(rhs, lhs, 0, 2, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 2, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 2 x sync level 1") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, EPSILON, 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, EPSILON, 0);

        Nft rhs(4, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'f', 2);
        rhs.delta.add(2, 'h', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }, 5);
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, 'b', 2);
            expected_full.delta.add(2, EPSILON, 3);
            expected_full.delta.add(3, EPSILON, 4);
            expected_full.delta.add(4, EPSILON, 5);
            expected_full.delta.add(5, 'd', 6);
            expected_full.delta.add(6, 'e', 7);
            expected_full.delta.add(7, 'g', 8);
            expected_full.delta.add(8, 'f', 9);
            expected_full.delta.add(9, 'h', 10);
            expected_full.delta.add(4, EPSILON, 0);
            Nft expected_proj = project_out(expected_full, { 3 }, JumpMode::NoJump);

            Nft result_full = compose(lhs, rhs, 2, 1, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 2, 1, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }, 5);
            expected_full.delta.add(0, EPSILON, 1);
            expected_full.delta.add(1, 'a', 2);
            expected_full.delta.add(2, 'b', 3);
            expected_full.delta.add(3, EPSILON, 4);
            expected_full.delta.add(4, EPSILON, 5);
            expected_full.delta.add(5, 'g', 6);
            expected_full.delta.add(6, 'd', 7);
            expected_full.delta.add(7, 'e', 8);
            expected_full.delta.add(8, 'f', 9);
            expected_full.delta.add(9, 'h', 10);
            expected_full.delta.add(4, EPSILON, 0);
            Nft expected_proj = project_out(expected_full, { 3 }, JumpMode::NoJump);

            Nft result_full = compose(rhs, lhs, 1, 2, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 1, 2, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }
#endif
    SECTION("sync level 2 x sync level 2") {
        Nft lhs(7, { 0 }, { 6 }, { 0, 1, 2, 0, 1, 2, 0 }, 3);
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, EPSILON, 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, EPSILON, 0);

        Nft rhs(4, { 0 }, { 3 }, { 0, 1, 2, 0 }, 3);
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'h', 2);
        rhs.delta.add(2, 'f', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }, 5);
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, 'b', 2);
            expected_full.delta.add(2, EPSILON, 3);
            expected_full.delta.add(3, EPSILON, 4);
            expected_full.delta.add(4, EPSILON, 5);
            expected_full.delta.add(5, 'd', 6);
            expected_full.delta.add(6, 'e', 7);
            expected_full.delta.add(7, 'g', 8);
            expected_full.delta.add(8, 'h', 9);
            expected_full.delta.add(9, 'f', 10);
            expected_full.delta.add(4, EPSILON, 0);
            Nft expected_proj = project_out(expected_full, { 4 }, JumpMode::NoJump);

            Nft result_full = compose(lhs, rhs, 2, 2, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 2, 2, true, JumpMode::NoJump);
            result_proj.print_to_dot(std::string("result.dot"), true);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }, 5);
            expected_full.delta.add(0, EPSILON, 1);
            expected_full.delta.add(1, EPSILON, 2);
            expected_full.delta.add(2, 'a', 3);
            expected_full.delta.add(3, 'b', 4);
            expected_full.delta.add(4, EPSILON, 5);
            expected_full.delta.add(5, 'g', 6);
            expected_full.delta.add(6, 'h', 7);
            expected_full.delta.add(7, 'd', 8);
            expected_full.delta.add(8, 'e', 9);
            expected_full.delta.add(9, 'f', 10);
            expected_full.delta.add(4, EPSILON, 0);
            Nft expected_proj = project_out(expected_full, { 4 }, JumpMode::NoJump);

            Nft result_full = compose(rhs, lhs, 2, 2, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() == expected_full.num_of_states());
            CHECK(result_full.num_of_levels == expected_full.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() == expected_full.delta.num_of_transitions());
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 2, 2, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() == expected_proj.num_of_states());
            CHECK(result_proj.num_of_levels == expected_proj.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() == expected_proj.delta.num_of_transitions());
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }
}

// TEST_CASE("nft::compose_fast()") {
    //     SECTION("project_out == true") {
        //         SECTION("two levels easy match") {
            //             Nft lhs;
            //             lhs.num_of_levels = 2;
//             lhs.levels = { 0, 1, 0, 1, 0 };
//             lhs.initial.insert(0);
//             lhs.final.insert(4);
//             lhs.delta.add(0, 'x', 1);
//             lhs.delta.add(1, 'a', 2);
//             lhs.delta.add(1, 'b', 2);
//             lhs.delta.add(2, 'y', 3);
//             lhs.delta.add(3, 'c', 4);
//             lhs.delta.add(3, 'd', 4);

//             Nft rhs;
//             rhs.num_of_levels = 2;
//             rhs.levels = { 0, 1, 0, 1, 0 };
//             rhs.initial.insert(0);
//             rhs.final.insert(4);
//             rhs.delta.add(0, 'x', 1);
//             rhs.delta.add(1, 'e', 2);
//             rhs.delta.add(1, 'f', 2);
//             rhs.delta.add(2, 'y', 3);
//             rhs.delta.add(3, 'g', 4);
//             rhs.delta.add(3, 'h', 4);

//             Nft result = compose_fast(lhs, rhs, { 0 }, { 0 }, true, true, JumpMode::NoJump);

//             Nft expected;
//             expected.num_of_levels = 2;
//             expected.levels = { 0, 1, 0, 1, 0 };
//             expected.initial.insert(0);
//             expected.final.insert(4);
//             expected.delta.add(0, 'a', 1);
//             expected.delta.add(0, 'b', 1);
//             expected.delta.add(1, 'e', 2);
//             expected.delta.add(1, 'f', 2);
//             expected.delta.add(2, 'c', 3);
//             expected.delta.add(2, 'd', 3);
//             expected.delta.add(3, 'g', 4);
//             expected.delta.add(3, 'h', 4);

//             CHECK(result.num_of_levels == 2);
//             CHECK(are_equivalent(result, expected));
//         }

//         SECTION("four levels with loop") {
//             Nft lhs;
//             lhs.num_of_levels = 4;
//             lhs.levels = { 0, 1, 2, 3, 0 };
//             lhs.initial.insert(0);
//             lhs.final.insert(4);
//             lhs.delta.add(0, 'x', 1);
//             lhs.delta.add(1, 'a', 2);
//             lhs.delta.add(1, 'b', 2);
//             lhs.delta.add(2, 'y', 3);
//             lhs.delta.add(3, 'c', 4);
//             lhs.delta.add(3, 'd', 4);
//             lhs.delta.add(3, 'e', 0);
//             lhs.delta.add(3, 'f', 0);

//             Nft rhs;
//             rhs.num_of_levels = 4;
//             rhs.levels = { 0, 1, 2, 3, 0 };
//             rhs.initial.insert(0);
//             rhs.final.insert(4);
//             rhs.delta.add(0, 'g', 1);
//             rhs.delta.add(0, 'h', 1);
//             rhs.delta.add(1, 'x', 2);
//             rhs.delta.add(2, 'i', 3);
//             rhs.delta.add(2, 'j', 3);
//             rhs.delta.add(3, 'y', 4);
//             rhs.delta.add(3, 'y', 0);

//             Nft result = compose_fast(lhs, rhs, { 0, 2 }, { 1, 3 }, true, true, JumpMode::NoJump);

//             Nft expected;
//             expected.num_of_levels = 4;
//             expected.levels = { 0, 1, 2, 3, 0 };
//             expected.initial.insert(0);
//             expected.final.insert(4);
//             expected.delta.add(0, 'g', 1);
//             expected.delta.add(0, 'h', 1);
//             expected.delta.add(1, 'a', 2);
//             expected.delta.add(1, 'b', 2);
//             expected.delta.add(2, 'i', 3);
//             expected.delta.add(2, 'j', 3);
//             expected.delta.add(3, 'c', 4);
//             expected.delta.add(3, 'd', 4);
//             expected.delta.add(3, 'e', 0);
//             expected.delta.add(3, 'f', 0);

//             CHECK(result.num_of_levels == 4);
//             CHECK(are_equivalent(result, expected));
//         }

//         SECTION("synchronization of lst two levels with one mismatch") {
//             Nft lhs;
//             lhs.num_of_levels = 3;
//             lhs.levels = { 0, 1, 2 };
//             lhs.initial.insert(0);
//             lhs.final.insert(0);
//             lhs.delta.add(0, 'u', 1);
//             lhs.delta.add(0, 'v', 1);
//             lhs.delta.add(1, 'x', 2);
//             lhs.delta.add(2, 'y', 0);

//             Nft rhs;
//             rhs.num_of_levels = 4;
//             rhs.levels = { 0, 1, 2, 2, 3, 3, 0, 0, 1, 1, 2, 2, 3, 3, 0, 0};
//             rhs.initial.insert(0);
//             rhs.final.insert(7);
//             rhs.final.insert(14);
//             rhs.final.insert(15);
//             rhs.delta.add(0, 'a', 1);
//             rhs.delta.add(0, 'b', 1);
//             rhs.delta.add(1, 'c', 2);
//             rhs.delta.add(1, 'd', 2);
//             rhs.delta.add(1, 'c', 3);
//             rhs.delta.add(1, 'd', 3);
//             rhs.delta.add(2, 'x', 4);
//             rhs.delta.add(4, 'y', 6);
//             rhs.delta.add(6, 'e', 8);
//             rhs.delta.add(8, 'f', 10);
//             rhs.delta.add(10, 'x', 12);
//             rhs.delta.add(12, 'y', 14);
//             rhs.delta.add(3, 'x', 5);
//             rhs.delta.add(5, 'z', 7);
//             rhs.delta.add(7, 'g', 9);
//             rhs.delta.add(9, 'h', 11);
//             rhs.delta.add(11, 'x', 13);
//             rhs.delta.add(13, 'y', 15);

//             Nft result = compose_fast(lhs, rhs, { 1, 2 }, { 2, 3 }, true, true, JumpMode::NoJump);

//             Nft expected;
//             expected.num_of_levels = 3;
//             expected.levels = { 0, 1, 2, 0, 1, 2, 0 };
//             expected.initial.insert(0);
//             expected.final.insert(6);
//             expected.delta.add(0, 'u', 1);
//             expected.delta.add(0, 'v', 1);
//             expected.delta.add(1, 'a', 2);
//             expected.delta.add(1, 'b', 2);
//             expected.delta.add(2, 'c', 3);
//             expected.delta.add(2, 'd', 3);
//             expected.delta.add(3, 'u', 4);
//             expected.delta.add(3, 'v', 4);
//             expected.delta.add(4, 'e', 5);
//             expected.delta.add(5, 'f', 6);

//             CHECK(result.num_of_levels == 3);
//             CHECK(are_equivalent(result, expected));
//         }

//         SECTION("epsilon on synchronization") {
//             Nft lhs;
//             lhs.num_of_levels = 3;
//             lhs.levels = { 0, 1, 2, 0 };
//             lhs.initial.insert(0);
//             lhs.final.insert(3);
//             lhs.delta.add(0, 'u', 1);
//             lhs.delta.add(0, 'v', 1);
//             lhs.delta.add(1, 'x', 2);
//             lhs.delta.add(2, 'y', 3);

//             Nft rhs;
//             rhs.num_of_levels = 3;
//             rhs.levels = { 0, 1, 2, 2, 0, 2, 0, 1, 1, 2, 2, 0, 0 };
//             rhs.initial.insert(0);
//             rhs.final.insert(11);
//             rhs.final.insert(12);
//             rhs.delta.add(0, 'a', 1);
//             rhs.delta.add(0, 'b', 1);
//             rhs.delta.add(1, 'c', 2);
//             rhs.delta.add(1, EPSILON, 3);
//             rhs.delta.add(1, EPSILON, 5);
//             rhs.delta.add(2, EPSILON, 4);
//             rhs.delta.add(3, EPSILON, 4);
//             rhs.delta.add(4, 'e', 7);
//             rhs.delta.add(4, 'f', 7);
//             rhs.delta.add(7, 'x', 9);
//             rhs.delta.add(9, 'y', 11);
//             rhs.delta.add(5, 'd', 6);
//             rhs.delta.add(6, 'g', 8);
//             rhs.delta.add(8, 'x', 10);
//             rhs.delta.add(10, 'y', 12);

//             Nft result = compose_fast(lhs, rhs, { 1, 2 }, { 1, 2 }, true, true, JumpMode::NoJump);

//             Nft expected;
//             expected.num_of_levels = 2;
//             expected.levels = { 0, 1, 0, 1, 0 };
//             expected.initial.insert(0);
//             expected.final.insert(4);
//             expected.delta.add(0, EPSILON, 1);
//             expected.delta.add(1, 'a', 2);
//             expected.delta.add(1, 'b', 2);
//             expected.delta.add(2, 'u', 3);
//             expected.delta.add(2, 'v', 3);
//             expected.delta.add(3, 'e', 4);
//             expected.delta.add(3, 'f', 4);

//             CHECK(result.num_of_levels == 2);
//             CHECK(are_equivalent(result, expected));
//         }
//     }

//     SECTION("project_out == false") {
//         SECTION("two levels easy match") {
//             Nft lhs;
//             lhs.num_of_levels = 2;
//             lhs.levels = { 0, 1, 0, 1, 0 };
//             lhs.initial.insert(0);
//             lhs.final.insert(4);
//             lhs.delta.add(0, 'x', 1);
//             lhs.delta.add(1, 'a', 2);
//             lhs.delta.add(1, 'b', 2);
//             lhs.delta.add(2, 'y', 3);
//             lhs.delta.add(3, 'c', 4);
//             lhs.delta.add(3, 'd', 4);

//             Nft rhs;
//             rhs.num_of_levels = 2;
//             rhs.levels = { 0, 1, 0, 1, 0 };
//             rhs.initial.insert(0);
//             rhs.final.insert(4);
//             rhs.delta.add(0, 'x', 1);
//             rhs.delta.add(1, 'e', 2);
//             rhs.delta.add(1, 'f', 2);
//             rhs.delta.add(2, 'y', 3);
//             rhs.delta.add(3, 'g', 4);
//             rhs.delta.add(3, 'h', 4);

//             Nft result = compose_fast(lhs, rhs, { 0 }, { 0 }, false, true, JumpMode::NoJump);

//             Nft expected;
//             expected.num_of_levels = 3;
//             expected.levels = { 0, 2, 0, 2, 0, 1, 1 };
//             expected.initial.insert(0);
//             expected.final.insert(4);
//             expected.delta.add(0, 'x', 5);
//             expected.delta.add(5, 'a', 1);
//             expected.delta.add(5, 'b', 1);
//             expected.delta.add(1, 'e', 2);
//             expected.delta.add(1, 'f', 2);
//             expected.delta.add(2, 'y', 6);
//             expected.delta.add(6, 'c', 3);
//             expected.delta.add(6, 'd', 3);
//             expected.delta.add(3, 'g', 4);
//             expected.delta.add(3, 'h', 4);

//             CHECK(result.num_of_levels == 3);
//             CHECK(are_equivalent(result, expected));
//         }


//         SECTION("four levels with loop") {
//             Nft lhs;
//             lhs.num_of_levels = 4;
//             lhs.levels = { 0, 1, 2, 3, 0 };
//             lhs.initial.insert(0);
//             lhs.final.insert(4);
//             lhs.delta.add(0, 'x', 1);
//             lhs.delta.add(1, 'a', 2);
//             lhs.delta.add(1, 'b', 2);
//             lhs.delta.add(2, 'y', 3);
//             lhs.delta.add(3, 'c', 4);
//             lhs.delta.add(3, 'd', 4);
//             lhs.delta.add(3, 'e', 0);
//             lhs.delta.add(3, 'f', 0);
//             lhs.print_to_dot(std::string("lhs.dot"), true, true);

//             Nft rhs;
//             rhs.num_of_levels = 4;
//             rhs.levels = { 0, 1, 2, 3, 0 };
//             rhs.initial.insert(0);
//             rhs.final.insert(4);
//             rhs.delta.add(0, 'g', 1);
//             rhs.delta.add(0, 'h', 1);
//             rhs.delta.add(1, 'x', 2);
//             rhs.delta.add(2, 'i', 3);
//             rhs.delta.add(2, 'j', 3);
//             rhs.delta.add(3, 'y', 4);
//             rhs.delta.add(3, 'y', 0);
//             rhs.print_to_dot(std::string("rhs.dot"), true, true);

//             Nft result = compose_fast(lhs, rhs, { 0, 2 }, { 1, 3 }, false, true, JumpMode::NoJump);
//             result.print_to_dot(std::string("result.dot"), true, true);

//             Nft expected;
//             expected.num_of_levels = 6;
//             expected.levels = { 0, 2, 3, 4, 0, 1, 5 };
//             expected.initial.insert(0);
//             expected.final.insert(4);
//             expected.delta.add(0, 'g', 5);
//             expected.delta.add(0, 'h', 5);
//             expected.delta.add(5, 'x', 1);
//             expected.delta.add(1, 'a', 2);
//             expected.delta.add(1, 'b', 2);
//             expected.delta.add(2, 'i', 3);
//             expected.delta.add(2, 'j', 3);
//             expected.delta.add(3, 'y', 6);
//             expected.delta.add(6, 'c', 4);
//             expected.delta.add(6, 'd', 4);
//             expected.delta.add(6, 'e', 0);
//             expected.delta.add(6, 'f', 0);
//             expected.print_to_dot(std::string("expected.dot"), true, true);

//             CHECK(result.num_of_levels == 6);
//             CHECK(are_equivalent(result, expected));
//         }

//         SECTION("synchronization of lst two levels with one mismatch") {
//             Nft lhs;
//             lhs.num_of_levels = 3;
//             lhs.levels = { 0, 1, 2 };
//             lhs.initial.insert(0);
//             lhs.final.insert(0);
//             lhs.delta.add(0, 'u', 1);
//             lhs.delta.add(0, 'v', 1);
//             lhs.delta.add(1, 'x', 2);
//             lhs.delta.add(2, 'y', 0);

//             Nft rhs;
//             rhs.num_of_levels = 4;
//             rhs.levels = { 0, 1, 2, 2, 3, 3, 0, 0, 1, 1, 2, 2, 3, 3, 0, 0};
//             rhs.initial.insert(0);
//             rhs.final.insert(7);
//             rhs.final.insert(14);
//             rhs.final.insert(15);
//             rhs.delta.add(0, 'a', 1);
//             rhs.delta.add(0, 'b', 1);
//             rhs.delta.add(1, 'c', 2);
//             rhs.delta.add(1, 'd', 2);
//             rhs.delta.add(1, 'c', 3);
//             rhs.delta.add(1, 'd', 3);
//             rhs.delta.add(2, 'x', 4);
//             rhs.delta.add(4, 'y', 6);
//             rhs.delta.add(6, 'e', 8);
//             rhs.delta.add(8, 'f', 10);
//             rhs.delta.add(10, 'x', 12);
//             rhs.delta.add(12, 'y', 14);
//             rhs.delta.add(3, 'x', 5);
//             rhs.delta.add(5, 'z', 7);
//             rhs.delta.add(7, 'g', 9);
//             rhs.delta.add(9, 'h', 11);
//             rhs.delta.add(11, 'x', 13);
//             rhs.delta.add(13, 'y', 15);

//             Nft result = compose_fast(lhs, rhs, { 1, 2 }, { 2, 3 }, false, true, JumpMode::NoJump);

//             Nft expected;
//             expected.num_of_levels = 5;
//             expected.levels = { 0, 1, 2, 3, 1, 2, 3, 4, 0, 4, 0 };
//             expected.initial.insert(0);
//             expected.final.insert(10);
//             expected.delta.add(0, 'u', 1);
//             expected.delta.add(0, 'v', 1);
//             expected.delta.add(1, 'a', 2);
//             expected.delta.add(1, 'b', 2);
//             expected.delta.add(2, 'c', 3);
//             expected.delta.add(2, 'd', 3);
//             expected.delta.add(3, 'x', 7);
//             expected.delta.add(7, 'y', 8);
//             expected.delta.add(8, 'u', 4);
//             expected.delta.add(8, 'v', 4);
//             expected.delta.add(4, 'e', 5);
//             expected.delta.add(5, 'f', 6);
//             expected.delta.add(6, 'x', 9);
//             expected.delta.add(9, 'y', 10);

//             CHECK(result.num_of_levels == 5);
//             CHECK(are_equivalent(result, expected));
//         }

//         SECTION("epsilon on synchronization") {
//             Nft lhs;
//             lhs.num_of_levels = 3;
//             lhs.levels = { 0, 1, 2, 0 };
//             lhs.initial.insert(0);
//             lhs.final.insert(3);
//             lhs.delta.add(0, 'u', 1);
//             lhs.delta.add(0, 'v', 1);
//             lhs.delta.add(1, 'x', 2);
//             lhs.delta.add(2, 'y', 3);

//             Nft rhs;
//             rhs.num_of_levels = 3;
//             rhs.levels = { 0, 1, 2, 2, 0, 2, 0, 1, 1, 2, 2, 0, 0 };
//             rhs.initial.insert(0);
//             rhs.final.insert(11);
//             rhs.final.insert(12);
//             rhs.delta.add(0, 'a', 1);
//             rhs.delta.add(0, 'b', 1);
//             rhs.delta.add(1, 'c', 2);
//             rhs.delta.add(1, EPSILON, 3);
//             rhs.delta.add(1, EPSILON, 5);
//             rhs.delta.add(2, EPSILON, 4);
//             rhs.delta.add(3, EPSILON, 4);
//             rhs.delta.add(4, 'e', 7);
//             rhs.delta.add(4, 'f', 7);
//             rhs.delta.add(7, 'x', 9);
//             rhs.delta.add(9, 'y', 11);
//             rhs.delta.add(5, 'd', 6);
//             rhs.delta.add(6, 'g', 8);
//             rhs.delta.add(8, 'x', 10);
//             rhs.delta.add(10, 'y', 12);

//             Nft result = compose_fast(lhs, rhs, { 1, 2 }, { 1, 2 }, false, true, JumpMode::NoJump);

//             Nft expected;
//             expected.num_of_levels = 4;
//             expected.levels = { 0, 1, 2, 1, 2, 3, 0, 3, 0 };
//             expected.initial.insert(0);
//             expected.final.insert(8);
//             expected.delta.add(0, EPSILON, 1);
//             expected.delta.add(1, 'a', 2);
//             expected.delta.add(1, 'b', 2);
//             expected.delta.add(2, EPSILON, 5);
//             expected.delta.add(5, EPSILON, 6);
//             expected.delta.add(6, 'u', 3);
//             expected.delta.add(6, 'v', 3);
//             expected.delta.add(3, 'e', 4);
//             expected.delta.add(3, 'f', 4);
//             expected.delta.add(4, 'x', 7);
//             expected.delta.add(7, 'y', 8);

//             CHECK(result.num_of_levels == 4);
//             CHECK(are_equivalent(result, expected));
//         }
//     }
// }