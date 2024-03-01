/* tests-nft-concatenation.cc -- Tests for concatenation of NFAs
 */

#include <catch2/catch.hpp>

#include "mata/nft/nft.hh"
#include "mata/utils/ord-vector.hh"


using namespace mata::nft;
using namespace mata::utils;


TEST_CASE("Mata::nft::compose()") {

    Nft lhs, rhs, expected, result;
    SECTION("levelsl_cnt == 2") {

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

            SECTION("Epsilon perfectly matches.") {
                lhs = Nft(7, { 0 }, { 6 }, { 0, 1, 0, 1, 0, 1, 0 }, 2);
                lhs.delta.add(0, 'e', 1);
                lhs.delta.add(1, 'a', 2);
                lhs.delta.add(2, 'g', 3);
                lhs.delta.add(3, EPSILON, 4);
                lhs.delta.add(4, EPSILON, 5);
                lhs.delta.add(5, 'c', 6);

                rhs = Nft(7, { 0 }, { 6 }, { 0, 1, 0, 1, 0, 1, 0 }, 2);
                rhs.delta.add(0, 'a', 1);
                rhs.delta.add(1, EPSILON, 2);
                rhs.delta.add(2, EPSILON, 3);
                rhs.delta.add(3, 'h', 4);
                rhs.delta.add(4, 'c', 5);
                rhs.delta.add(5, 'j', 6);

                expected = Nft(7, { 0 }, { 6 }, { 0, 1, 0, 1, 0, 1, 0 }, 2);
                expected.delta.add(0, 'e', 1);
                expected.delta.add(1, EPSILON, 2);
                expected.delta.add(2, 'g', 3);
                expected.delta.add(3, 'h', 4);
                expected.delta.add(4, EPSILON, 5);
                expected.delta.add(5, 'j', 6);
                expected.delta.add(2, EPSILON, 7);
                expected.delta.add(7, 'h', 8);
                expected.delta.add(8, 'g', 9);
                expected.delta.add(9, EPSILON, 4);
                expected.delta.add(2, 'g', 10);
                expected.delta.add(10, EPSILON, 11);
                expected.delta.add(11, EPSILON, 12);
                expected.delta.add(12, 'h', 4);

                result = compose(lhs, rhs, 1, 0);

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

    SECTION("lhs.levels_cnt != rhs.levels_cnt") {
        SECTION("lhs.levels_cnt > rhs.levels_cnt") {
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

        SECTION("lhs.levels_cnt < rhs.levels_cnt") {
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

    SECTION("levels_cnt == 4") {

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
}
