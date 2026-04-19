/* tests-nft-concatenation.cc -- Tests for concatenation of NFAs
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/nft/nft.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/utils/custom_vector.h"


using namespace mata::nft;
using namespace mata::utils;


TEST_CASE("Mata::nft::compose()") {

    Nft lhs, rhs, expected, result;
    SECTION("levels_cnt == 2") {

        SECTION("Linear structure") {

            SECTION("Epsilon free") {
                lhs = Nft::with_levels({2, { 0, 1, 0, 1, 0, 1, 0 } }, 7, { 0 }, { 6 });
                lhs.delta.add(0, 'e', 1);
                lhs.delta.add(1, 'a', 2);
                lhs.delta.add(2, 'g', 3);
                lhs.delta.add(3, 'b', 4);
                lhs.delta.add(4, 'i', 5);
                lhs.delta.add(5, 'c', 6);

                rhs = Nft::with_levels({2, { 0, 1, 0, 1, 0, 1, 0 } }, 7, { 0 }, { 6 });
                rhs.delta.add(0, 'a', 1);
                rhs.delta.add(1, 'f', 2);
                rhs.delta.add(2, 'b', 3);
                rhs.delta.add(3, 'h', 4);
                rhs.delta.add(4, 'c', 5);
                rhs.delta.add(5, 'j', 6);

                expected = Nft::with_levels({2, { 0, 1, 0, 1, 0, 1, 0 } }, 7, { 0 }, { 6 });
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
                lhs = Nft(9, { 0 }, { 8 }, Levels({ 0, 1, 2, 3, 0, 1, 2, 3, 0 }));
                lhs.delta.add(0, 'i', 1);
                lhs.delta.add(1, 'b', 2);
                lhs.delta.add(2, 'c', 3);
                lhs.delta.add(3, EPSILON, 4);
                lhs.delta.add(4, 'k', 5);
                lhs.delta.add(5, 'f', 6);
                lhs.delta.add(6, 'g', 7);
                lhs.delta.add(7, 'l', 8);

                rhs = Nft(9, { 0 }, { 8 }, Levels({ 0, 1, 2, 3, 0, 1, 2, 3, 0 }));
                rhs.delta.add(0, 'a', 1);
                rhs.delta.add(1, 'i', 2);
                rhs.delta.add(2, EPSILON, 3);
                rhs.delta.add(3, EPSILON, 4);
                rhs.delta.add(4, 'e', 5);
                rhs.delta.add(5, 'k', 6);
                rhs.delta.add(6, 'l', 7);
                rhs.delta.add(7, 'h', 8);

                expected = Nft(9, { 0 }, { 8 }, Levels({ 0, 1, 2, 3, 0, 1, 2, 3, 0 }));
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
                lhs = Nft::with_levels({2, { 0, 1, 0, 0, 1, 1, 0, 0 } }, 8, { 0 }, { 6, 7 });
                lhs.delta.add(0, 'e', 1);
                lhs.delta.add(1, 'a', 2);
                lhs.delta.add(2, 'c', 4);
                lhs.delta.add(4, 'e', 6);
                lhs.delta.add(1, 'b', 3);
                lhs.delta.add(3, 'd', 5);
                lhs.delta.add(5, 'f', 7);

                rhs = Nft::with_levels({2, { 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0 } }, 11, { 0 }, { 8, 9, 10 });
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

                expected = Nft::with_levels({2, { 0, 1, 0, 1, 0 } }, 5, { 0 }, { 4 });
                expected.delta.add(0, 'e', 1);
                expected.delta.add(1, 'd', 2);
                expected.delta.add(2, 'd', 3);
                expected.delta.add(3, 'g', 4);
                expected.delta.add(3, 'h', 4);

                result = compose(lhs, rhs, OrdVector<Level>{ 1 }, OrdVector<Level>{ 0 });

                CHECK(are_equivalent(result, expected));
            }

            SECTION("Epsilon perfectly matches.") {
                lhs = Nft::with_levels({2, { 0, 1, 0, 0, 1, 1, 0, 0 } }, 8, { 0 }, { 6, 7 });
                lhs.delta.add(0, 'e', 1);
                lhs.delta.add(1, 'a', 2);
                lhs.delta.add(2, 'c', 4);
                lhs.delta.add(4, EPSILON, 6);
                lhs.delta.add(1, 'b', 3);
                lhs.delta.add(3, EPSILON, 5);
                lhs.delta.add(5, 'f', 7);

                rhs = Nft::with_levels({2, { 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0 } }, 11, { 0 }, { 8, 9, 10 });
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

                expected = Nft::with_levels({2, { 0, 1, 0, 1, 0 } }, 5, { 0 }, { 4 });
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
            lhs = Nft::with_levels({2, { 0, 1, 0, 1, 0 } }, 5, { 0 }, { 2, 4 });
            lhs.delta.add(0, 'a', 1);
            lhs.delta.add(1, 'b', 2);
            lhs.delta.add(2, 'c', 3);
            lhs.delta.add(3, 'e', 4);
            lhs.delta.add(3, 'd', 2);

            rhs = Nft::with_levels({2, { 0, 1, 0, 1, 0, 1, 0 } }, 7, { 0 }, { 6 });
            rhs.delta.add(0, 'b', 1);
            rhs.delta.add(1, 'x', 2);
            rhs.delta.add(2, 'd', 3);
            rhs.delta.add(3, 'y', 4);
            rhs.delta.add(4, 'f', 4);
            rhs.delta.add(4, 'd', 5);
            rhs.delta.add(5, 'z', 6);

            expected = Nft::with_levels({2, { 0, 1, 0, 1, 0, 1, 0 } }, 7, { 0 }, { 6 });
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
                lhs = Nft::with_levels({2, { 0, 1, 0, 1, 0, 1, 0 } }, 7, { 0 }, { 6 });
                lhs.delta.add(0, 'x', 1);
                lhs.delta.add(1, EPSILON, 2);
                lhs.delta.add(2, 'y', 3);
                lhs.delta.add(3, 'a', 4);
                lhs.delta.add(4, 'x', 5);
                lhs.delta.add(5, 'c', 6);

                rhs = Nft::with_levels({2, { 0, 1, 0, 1, 0 } }, 5, { 0 }, { 4 });
                rhs.delta.add(0, 'a', 1);
                rhs.delta.add(1, 'b', 2);
                rhs.delta.add(2, 'c', 3);
                rhs.delta.add(3, 'd', 4);

                expected = Nft::with_levels({2, { 0, 1, 0, 1, 0, 1, 0 } }, 7, { 0 }, { 6 });
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
                lhs = Nft::with_levels({2, { 0, 1, 0, 1, 0, 1, 0 } }, 7, { 0 }, { 6 });
                lhs.delta.add(0, 'x', 1);
                lhs.delta.add(1, EPSILON, 2);
                lhs.delta.add(2, EPSILON, 3);
                lhs.delta.add(3, 'a', 4);
                lhs.delta.add(4, 'x', 5);
                lhs.delta.add(5, EPSILON, 6);
                rhs = Nft(5, { 0 }, { 4 }, Levels({ 0, 1, 0, 1, 0 }));
                rhs.delta.add(0, 'a', 1);
                rhs.delta.add(1, 'b', 2);
                rhs.delta.add(2, EPSILON, 3);
                rhs.delta.add(3, 'd', 4);

                expected = Nft::with_levels({2, { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1 } }, 13, { 0 }, { 6 });
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

    SECTION("lhs.levels.num_of_levels != rhs.levels.num_of_levels") {
        SECTION("lhs.levels.num_of_levels > rhs.levels.num_of_levels") {
            lhs = Nft::with_levels({5, { 0, 1, 2, 3, 4, 0 } }, 6, { 0 }, { 5 });
            lhs.delta.add(0, 'a', 1);
            lhs.delta.add(1, 'b', 2);
            lhs.delta.add(2, 'c', 3);
            lhs.delta.add(3, 'e', 4);
            lhs.delta.add(4, 'd', 5);

            rhs = Nft::with_levels({3, { 0, 1, 2, 0 } }, 4, { 0 }, { 3 });
            rhs.delta.add(0, 'b', 1);
            rhs.delta.add(1, 'd', 2);
            rhs.delta.add(2, 'f', 3);

            expected = Nft::with_levels({4, { 0, 1, 2, 3, 0 } }, 5, { 0 }, { 4 });
            expected.delta.add(0, 'a', 1);
            expected.delta.add(1, 'c', 2);
            expected.delta.add(2, 'e', 3);
            expected.delta.add(3, 'f', 4);

            result = compose(lhs, rhs, { 1, 4 }, { 0, 1 });

            CHECK(are_equivalent(result, expected));
        }

        SECTION("lhs.levels.num_of_levels < rhs.levels.num_of_levels") {
            lhs = Nft::with_levels({3, { 0, 1, 2, 0 } }, 4, { 0 }, { 3 });
            lhs.delta.add(0, 'b', 1);
            lhs.delta.add(1, 'd', 2);
            lhs.delta.add(2, 'f', 3);

            rhs = Nft::with_levels({5, { 0, 1, 2, 3, 4, 0 } }, 6, { 0 }, { 5 });
            rhs.delta.add(0, 'a', 1);
            rhs.delta.add(1, 'b', 2);
            rhs.delta.add(2, 'c', 3);
            rhs.delta.add(3, 'e', 4);
            rhs.delta.add(4, 'd', 5);

            expected = Nft::with_levels({4, { 0, 1, 2, 3, 0 } }, 5, { 0 }, { 4 });
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
            lhs = Nft::with_levels({4, { 0, 1, 2, 3, 0, 1, 2, 3, 0 } }, 9, { 0 }, { 8 });
            lhs.delta.add(0, 'i', 1);
            lhs.delta.add(1, 'b', 2);
            lhs.delta.add(2, 'c', 3);
            lhs.delta.add(3, 'j', 4);
            lhs.delta.add(4, 'k', 5);
            lhs.delta.add(5, 'f', 6);
            lhs.delta.add(6, 'g', 7);
            lhs.delta.add(7, 'l', 8);

            rhs = Nft::with_levels({4, { 0, 1, 2, 3, 0, 1, 2, 3, 0 } }, 9, { 0 }, { 8 });
            rhs.delta.add(0, 'a', 1);
            rhs.delta.add(1, 'i', 2);
            rhs.delta.add(2, 'j', 3);
            rhs.delta.add(3, 'd', 4);
            rhs.delta.add(4, 'e', 5);
            rhs.delta.add(5, 'k', 6);
            rhs.delta.add(6, 'l', 7);
            rhs.delta.add(7, 'h', 8);

            expected= Nft::with_levels({4, { 0, 1, 2, 3, 0, 1, 2, 3, 0 } }, 9, { 0 }, { 8 });
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
            lhs = Nft::with_levels({4, { 0, 1, 2, 3, 0, 1, 2, 3, 0 } }, 9, { 0 }, { 8 });
            lhs.delta.add(0, 'i', 1);
            lhs.delta.add(1, 'b', 2);
            lhs.delta.add(2, 'c', 3);
            lhs.delta.add(3, EPSILON, 4);
            lhs.delta.add(4, 'k', 5);
            lhs.delta.add(5, 'f', 6);
            lhs.delta.add(6, 'g', 7);
            lhs.delta.add(7, 'l', 8);

            rhs = Nft::with_levels({4, { 0, 1, 2, 3, 0, 1, 2, 3, 0 } }, 9, { 0 }, { 8 });
            rhs.delta.add(0, 'a', 1);
            rhs.delta.add(1, 'i', 2);
            rhs.delta.add(2, EPSILON, 3);
            rhs.delta.add(3, EPSILON, 4);
            rhs.delta.add(4, 'e', 5);
            rhs.delta.add(5, 'k', 6);
            rhs.delta.add(6, 'l', 7);
            rhs.delta.add(7, 'h', 8);

            expected= Nft::with_levels({4, { 0, 1, 2, 3, 0, 1, 2, 3, 0 } }, 9, { 0 }, { 8 });
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
            lhs = Nft::with_levels({4, { 0, 1, 2, 3, 0, 1, 2, 3, 0 } }, 9, { 0 }, { 8 });
            lhs.delta.add(0, EPSILON, 1);
            lhs.delta.add(1, 'c', 2);
            lhs.delta.add(2, 'd', 3);
            lhs.delta.add(3, EPSILON, 4);
            lhs.delta.add(4, 'b', 5);
            lhs.delta.add(5, 'g', 6);
            lhs.delta.add(6, 'h', 7);
            lhs.delta.add(7, 'a', 8);

            rhs = Nft::with_levels({4, { 0, 1, 2, 3, 0, 1, 2, 3, 0 } }, 9, { 0 }, { 4, 8 });
            rhs.delta.add(0, 'e', 1);
            rhs.delta.add(1, 'b', 2);
            rhs.delta.add(2, 'a', 3);
            rhs.delta.add(3, 'f', 4);
            rhs.delta.add(4, 'i', 5);
            rhs.delta.add(5, 'x', 6);
            rhs.delta.add(6, 'y', 7);
            rhs.delta.add(7, 'j', 8);

            expected= Nft::with_levels({4, { 0, 1, 2, 3, 0, 1, 2, 3, 0 } }, 9, { 0 }, { 8 });
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
            lhs = Nft::with_levels({12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0 } }, 13, { 0 }, { 12 });
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

            rhs = Nft::with_levels({13, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0 } }, 14, { 0 }, { 13 });
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

            expected = Nft::with_levels({13, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0 } }, 14, { 0 }, { 13 });
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
            lhs = Nft::with_levels({12, { 0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0 } }, 12, { 0 }, { 11 });
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

            rhs = Nft::with_levels({13, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 0 } }, 11, { 0 }, { 10 });
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

            expected = Nft::with_levels({13, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0 } }, 14, { 0 }, { 13 });
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
            lhs = Nft::with_levels({12, { 0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0 } }, 12, { 0 }, { 11 });
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

            rhs = Nft::with_levels({13, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 0 } }, 11, { 0 }, { 10 });
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

            expected = Nft::with_levels({13, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0 } }, 14, { 0 }, { 13 });
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

TEST_CASE("nft::compose(Nft&, Nft&, Level, Level, ...) - easy cases") {
    SECTION("2 levels x 2 levels") {
        Nft expected_full(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        expected_full.delta.add(0, 'a', 1);
        expected_full.delta.add(1, 'b', 2);
        expected_full.delta.add(2, 'e', 3);
        expected_full.delta.add(3, 'c', 4);
        expected_full.delta.add(4, 'd', 5);
        expected_full.delta.add(5, 'f', 6);
        Nft expected_proj = project_out(expected_full, { 1 }, JumpMode::NoJump);

        Nft lhs(5, { 0 }, { 4 }, Levels({ 0, 1, 0, 1, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);

        Nft rhs(5, { 0 }, { 4 }, Levels({ 0, 1, 0, 1, 0 }));
        rhs.delta.add(0, 'b', 1);
        rhs.delta.add(1, 'e', 2);
        rhs.delta.add(2, 'd', 3);
        rhs.delta.add(3, 'f', 4);

        SECTION("LHS | RHS") {
            Nft result_full = compose(lhs, rhs, 1, 0, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft result_full = compose(rhs, lhs, 0, 1, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 1, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));

        }
    }
    SECTION("2 levels x 1 level") {
        Nft expected_full(5, { 0 }, { 4 }, Levels({ 0, 1, 0, 1, 0 }));
        expected_full.delta.add(0, 'a', 1);
        expected_full.delta.add(1, 'b', 2);
        expected_full.delta.add(2, 'c', 3);
        expected_full.delta.add(3, 'd', 4);
        Nft expected_proj = project_out(expected_full, { 1 }, JumpMode::NoJump);

        Nft lhs(5, { 0 }, { 4 }, Levels({ 0, 1, 0, 1, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);

        Nft rhs(3, { 0 }, { 2 }, Levels(CustomVector<Level>({ 0, 0, 0 })));
        rhs.delta.add(0, 'b', 1);
        rhs.delta.add(1, 'd', 2);

        SECTION("LHS | RHS") {
            Nft result_full = compose(lhs, rhs, 1, 0, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft result_full = compose(rhs, lhs, 0, 1, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 1, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }
    SECTION("1 level x 1 level") {
        Nft expected_full(4, { 0 }, { 3 }, Levels({ 0, 0, 0, 0 }));
        expected_full.delta.add(0, 'a', 1);
        expected_full.delta.add(1, 'b', 2);
        expected_full.delta.add(2, 'c', 3);

        Nft lhs(4, { 0 }, { 3 }, Levels({ 0, 0, 0, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);

        Nft rhs(4, { 0 }, { 3 }, Levels({ 0, 0, 0, 0 }));
        rhs.delta.add(0, 'a', 1);
        rhs.delta.add(1, 'b', 2);
        rhs.delta.add(2, 'c', 3);

        Nft result_full = compose(lhs, rhs, 0, 0, false, JumpMode::NoJump);
        CHECK(result_full.num_of_states() <= expected_full.num_of_states());
        CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
        CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
        result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));
    }
}

TEST_CASE("nft::compose(..., Level, Level, ...) - sliding without epsilon") {
    SECTION("sync level 0 x sync level 0") {
        Nft lhs(5, { 0 }, { 4 }, Levels({ 0, 1, 0, 1, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'd', 3);
        lhs.delta.add(3, 'e', 4);

        Nft rhs(5, { 0 }, { 4 }, Levels({ 0, 1, 0, 1, 0 }));
        rhs.delta.add(0, 'a', 1);
        rhs.delta.add(1, 'c', 2);
        rhs.delta.add(2, 'd', 3);
        rhs.delta.add(3, 'f', 4);

        SECTION("LHS | RHS") {
            Nft expected_full(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, 'b', 2);
            expected_full.delta.add(2, 'c', 3);
            expected_full.delta.add(3, 'd', 4);
            expected_full.delta.add(4, 'e', 5);
            expected_full.delta.add(5, 'f', 6);
            Nft expected_proj = project_out(expected_full, { 0 }, JumpMode::NoJump);

            Nft result_full = compose(lhs, rhs, 0, 0, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 0, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
            expected_full.delta.add(0, 'a', 1);
            expected_full.delta.add(1, 'c', 2);
            expected_full.delta.add(2, 'b', 3);
            expected_full.delta.add(3, 'd', 4);
            expected_full.delta.add(4, 'f', 5);
            expected_full.delta.add(5, 'e', 6);
            Nft expected_proj = project_out(expected_full, { 0 }, JumpMode::NoJump);

            Nft result_full = compose(rhs, lhs, 0, 0, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 0 x sync level 1") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'x', 0);
        lhs.delta.add(2, 'z', 6);

        Nft rhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'a', 2);
        rhs.delta.add(2, 'h', 3);
        rhs.delta.add(3, 'i', 4);
        rhs.delta.add(4, 'd', 5);
        rhs.delta.add(5, 'j', 6);
        rhs.delta.add(2, 'w', 0);
        rhs.delta.add(2, 'y', 6);

        SECTION("LHS | RHS") {
            Nft expected_full(13, { 0 }, { 12 }, Levels({ 0, 1, 2, 3, 4, 4, 0, 1, 2, 3, 4, 4, 0 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 0, 1, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(15, { 0 }, { 12 }, Levels({ 0, 4, 3, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 3, 4 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 1, 0, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 0 x sync level 2") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'x', 0);
        lhs.delta.add(2, 'z', 6);

        Nft rhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'h', 2);
        rhs.delta.add(2, 'a', 3);
        rhs.delta.add(3, 'i', 4);
        rhs.delta.add(4, 'j', 5);
        rhs.delta.add(5, 'd', 6);
        rhs.delta.add(2, 'a', 0);
        rhs.delta.add(2, 'a', 6);

        Nft expected_full(15, { 0 }, { 12 }, Levels({ 0, 1, 2, 3, 4, 3, 4, 0, 1, 2, 3, 4, 0, 4, 3 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 0, 2, true, JumpMode::RepeatSymbol).trim();
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft result_full = compose(rhs, lhs, 2, 0, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 2, 0, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sycn level 1 x sync level 0") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'w', 0);
        lhs.delta.add(2, 'z', 6);

        Nft rhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        rhs.delta.add(0, 'b', 1);
        rhs.delta.add(1, 'g', 2);
        rhs.delta.add(2, 'h', 3);
        rhs.delta.add(3, 'e', 4);
        rhs.delta.add(4, 'i', 5);
        rhs.delta.add(5, 'j', 6);
        rhs.delta.add(2, 'x', 0);
        rhs.delta.add(2, 'y', 6);

        SECTION("LHS | RHS") {
            Nft expected_full(15, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 3, 3, 4 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 0, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }

        SECTION("RHS | LHS") {
            Nft expected_full(13, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 4 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 1, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 1 x sync level 1") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'x', 0);
        lhs.delta.add(2, 'z', 6);

        Nft rhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'b', 2);
        rhs.delta.add(2, 'h', 3);
        rhs.delta.add(3, 'i', 4);
        rhs.delta.add(4, 'e', 5);
        rhs.delta.add(5, 'j', 6);
        rhs.delta.add(2, 'w', 0);
        rhs.delta.add(2, 'y', 6);

        SECTION("LHS | RHS") {
            Nft expected_full(13, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 4 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 1, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(13, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 4 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 1, 1, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 1 x sync level 2") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'x', 0);
        lhs.delta.add(2, 'z', 6);

        Nft rhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'h', 2);
        rhs.delta.add(2, 'b', 3);
        rhs.delta.add(3, 'i', 4);
        rhs.delta.add(4, 'j', 5);
        rhs.delta.add(5, 'e', 6);
        rhs.delta.add(2, 'b', 0);
        rhs.delta.add(2, 'b', 6);

        SECTION("LHS | RHS") {
            Nft expected_full(13, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 4 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 2, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(13, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 4 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 2, 1, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 2 x sync level 0") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);
        lhs.delta.add(2, 'c', 6);

        Nft rhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        rhs.delta.add(0, 'c', 1);
        rhs.delta.add(1, 'g', 2);
        rhs.delta.add(2, 'h', 3);
        rhs.delta.add(3, 'f', 4);
        rhs.delta.add(4, 'i', 5);
        rhs.delta.add(5, 'j', 6);
        rhs.delta.add(2, 'x', 0);
        rhs.delta.add(2, 'y', 6);

        Nft expected_full(15, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 3, 3, 4 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 2, 0, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft result_full = compose(rhs, lhs, 0, 2, false, JumpMode::NoJump).trim();
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 2, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 2 x sync level 1") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);
        lhs.delta.add(2, 'c', 6);

        Nft rhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'c', 2);
        rhs.delta.add(2, 'h', 3);
        rhs.delta.add(3, 'i', 4);
        rhs.delta.add(4, 'f', 5);
        rhs.delta.add(5, 'j', 6);
        rhs.delta.add(2, 'x', 0);
        rhs.delta.add(2, 'y', 6);

        SECTION("LHS | RHS") {
            Nft expected_full(13, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 4 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 2, 1, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(13, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4, 4}));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 1, 2, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 2 x sync level 2") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);
        lhs.delta.add(2, 'c', 6);

        Nft rhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'h', 2);
        rhs.delta.add(2, 'c', 3);
        rhs.delta.add(3, 'i', 4);
        rhs.delta.add(4, 'j', 5);
        rhs.delta.add(5, 'f', 6);
        rhs.delta.add(2, 'c', 0);
        rhs.delta.add(2, 'c', 6);

        SECTION("LHS | RHS") {
            Nft expected_full(11, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }));
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

            Nft expected_proj(9, { 0 }, { 8 }, Levels({ 0, 1, 2, 3, 0, 1, 2, 3, 0 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 2, 2, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }));
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

            Nft expected_proj(9, { 0 }, { 8 }, Levels({ 0, 1, 2, 3, 0, 1, 2, 3, 0 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 2, 2, true, JumpMode::NoJump).trim();
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }
}

TEST_CASE("nft::compose(..., Level, Level, ...) - sliding with epsilon") {
    SECTION("sync level 0 x sync level 0") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, EPSILON, 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);

        Nft rhs(4, { 0 }, { 3 }, Levels({ 0, 1, 2, 0 }));
        rhs.delta.add(0, 'd', 1);
        rhs.delta.add(1, 'g', 2);
        rhs.delta.add(2, 'h', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(13, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 3, 4 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 0, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 0 x sync level 1") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, EPSILON, 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);

        Nft rhs(4, { 0 }, { 3 }, Levels({ 0, 1, 2, 0 }));
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'd', 2);
        rhs.delta.add(2, 'h', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(12, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 0, 1, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 1, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 0 x sync level 2") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, EPSILON, 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);

        Nft rhs(4, { 0 }, { 3 }, Levels({ 0, 1, 2, 0 }));
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'h', 2);
        rhs.delta.add(2, 'd', 3);

        Nft expected_full(11, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 0, 2, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft result_full = compose(rhs, lhs, 2, 0, false, JumpMode::NoJump);
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 2, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 1 x sync level 0") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, EPSILON, 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);

        Nft rhs(4, { 0 }, { 3 }, Levels({ 0, 1, 2, 0 }));
        rhs.delta.add(0, 'e', 1);
        rhs.delta.add(1, 'g', 2);
        rhs.delta.add(2, 'h', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(13, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 3, 4 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 1, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 1 x sync level 1") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, EPSILON, 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);

        Nft rhs(4, { 0 }, { 3 }, Levels({ 0, 1, 2, 0 }));
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'e', 2);
        rhs.delta.add(2, 'h', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(12, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 4 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 1, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 1, 1, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 1 x sync level 2") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, EPSILON, 2);
        lhs.delta.add(2, 'c', 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, 'c', 0);

        Nft rhs(4, { 0 }, { 3 }, Levels({ 0, 1, 2, 0 }));
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'h', 2);
        rhs.delta.add(2, 'e', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(11, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 2, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 2, 1, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 2 x sync level 0") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, EPSILON, 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, EPSILON, 0);

        Nft rhs(4, { 0 }, { 3 }, Levels({ 0, 1, 2, 0 }));
        rhs.delta.add(0, 'f', 1);
        rhs.delta.add(1, 'g', 2);
        rhs.delta.add(2, 'h', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(11, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 2, 0, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }

        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 2, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 2 x sync level 1") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, EPSILON, 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, EPSILON, 0);

        Nft rhs(4, { 0 }, { 3 }, Levels({ 0, 1, 2, 0 }));
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'f', 2);
        rhs.delta.add(2, 'h', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(11, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 2, 1, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 1, 2, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("sync level 2 x sync level 2") {
        Nft lhs(7, { 0 }, { 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, 'b', 2);
        lhs.delta.add(2, EPSILON, 3);
        lhs.delta.add(3, 'd', 4);
        lhs.delta.add(4, 'e', 5);
        lhs.delta.add(5, 'f', 6);
        lhs.delta.add(2, EPSILON, 0);

        Nft rhs(4, { 0 }, { 3 }, Levels({ 0, 1, 2, 0 }));
        rhs.delta.add(0, 'g', 1);
        rhs.delta.add(1, 'h', 2);
        rhs.delta.add(2, 'f', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(11, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 2, 2, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(11, { 0 }, { 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }));
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
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 2, 2, true, JumpMode::NoJump);
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }
}

TEST_CASE("nft::composition(..., Level, Level, ...) - epsilon on both sides") {
    SECTION("3 levels x 3 levels + fast EPSILON on LHS") {
        Nft lhs(4, { 0 }, { 3 }, Levels({ 0, 1, 2, 0 }));
        lhs.delta.add(0, EPSILON, 1);
        lhs.delta.add(0, EPSILON, 3);
        lhs.delta.add(1, 'a', 2);
        lhs.delta.add(2, 'b', 3);

        Nft rhs(4, { 0 }, { 0, 3 }, Levels({ 0, 1, 2, 0 }));
        rhs.delta.add(0, 'c', 1);
        rhs.delta.add(1, EPSILON, 2);
        rhs.delta.add(2, 'd', 3);

        SECTION("LHS | RHS") {
            Nft expected_full(28, { 0 }, { 5, 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4 }));
            expected_full.delta.add(0, EPSILON, 1);
            expected_full.delta.add(1, EPSILON, 2);
            expected_full.delta.add(2, EPSILON, 3);
            expected_full.delta.add(3, EPSILON, 4);
            expected_full.delta.add(4, EPSILON, 5);
            expected_full.delta.add(0, EPSILON, 6);
            expected_full.delta.add(6, EPSILON, 7);
            expected_full.delta.add(7, 'a', 8);
            expected_full.delta.add(8, 'b', 9);
            expected_full.delta.add(9, EPSILON, 5);
            expected_full.delta.add(0, 'c', 11);
            expected_full.delta.add(11, EPSILON, 12);
            expected_full.delta.add(12, EPSILON, 13);
            expected_full.delta.add(13, EPSILON, 14);
            expected_full.delta.add(14, 'd', 15);
            expected_full.delta.add(15, EPSILON, 16);
            expected_full.delta.add(16, EPSILON, 17);
            expected_full.delta.add(17, EPSILON, 18);
            expected_full.delta.add(18, EPSILON, 19);
            expected_full.delta.add(19, EPSILON, 10);
            expected_full.delta.add(5, 'c', 20);
            expected_full.delta.add(20, EPSILON, 21);
            expected_full.delta.add(21, EPSILON, 22);
            expected_full.delta.add(22, EPSILON, 23);
            expected_full.delta.add(23, 'd', 10);
            expected_full.delta.add(15, EPSILON, 24);
            expected_full.delta.add(24, EPSILON, 25);
            expected_full.delta.add(25, 'a', 26);
            expected_full.delta.add(26, 'b', 27);
            expected_full.delta.add(27, EPSILON, 10);
            Nft expected_proj = project_out(expected_full, { 1 }, JumpMode::RepeatSymbol);

            Nft result_full = compose(lhs, rhs, 0, 1, false, JumpMode::NoJump);

            CHECK(result_full.final.size() == expected_full.final.size());
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 0, 1, true, JumpMode::NoJump);
            CHECK(result_proj.final.size() == expected_proj.final.size());
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft expected_full(28, { 0 }, { 5, 10 }, Levels({ 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4 }));
            expected_full.delta.add(0, EPSILON, 1);
            expected_full.delta.add(1, EPSILON, 2);
            expected_full.delta.add(2, EPSILON, 3);
            expected_full.delta.add(3, EPSILON, 4);
            expected_full.delta.add(4, EPSILON, 5);
            expected_full.delta.add(0, EPSILON, 6);
            expected_full.delta.add(6, EPSILON, 7);
            expected_full.delta.add(7, EPSILON, 8);
            expected_full.delta.add(8, 'a', 9);
            expected_full.delta.add(9, 'b', 5);
            expected_full.delta.add(0, 'c', 11);
            expected_full.delta.add(11, EPSILON, 12);
            expected_full.delta.add(12, 'd', 13);
            expected_full.delta.add(13, EPSILON, 14);
            expected_full.delta.add(14, EPSILON, 15);
            expected_full.delta.add(15, EPSILON, 16);
            expected_full.delta.add(16, EPSILON, 17);
            expected_full.delta.add(17, EPSILON, 18);
            expected_full.delta.add(18, EPSILON, 19);
            expected_full.delta.add(19, EPSILON, 10);
            expected_full.delta.add(5, 'c', 20);
            expected_full.delta.add(20, EPSILON, 21);
            expected_full.delta.add(21, 'd', 22);
            expected_full.delta.add(22, EPSILON, 23);
            expected_full.delta.add(23, EPSILON, 10);
            expected_full.delta.add(15, EPSILON, 24);
            expected_full.delta.add(24, EPSILON, 25);
            expected_full.delta.add(25, EPSILON, 26);
            expected_full.delta.add(26, 'a', 27);
            expected_full.delta.add(27, 'b', 10);
            Nft expected_proj = project_out(expected_full, { 1 }, JumpMode::NoJump);

            Nft result_full = compose(rhs, lhs, 1, 0, false, JumpMode::NoJump);
            CHECK(result_full.final.size() == expected_full.final.size());
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 1, 0, true, JumpMode::NoJump);
            CHECK(result_proj.final.size() == expected_proj.final.size());
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("3 levels x 1 level") {
        Nft lhs(4, { 0 }, { 3 }, Levels({ 0, 1, 2, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, EPSILON, 2);
        lhs.delta.add(2, 'b', 3);

        Nft rhs(2, { 0 }, { 1 }, Levels(CustomVector<Level>({ 0, 0 })));
        rhs.delta.add(0, EPSILON, 1);

        Nft expected_full(14, { 0 }, { 3 }, Levels({ 0, 1, 2, 0, 1, 2, 0, 1, 2, 1, 2, 0, 1, 2 }));
        expected_full.delta.add(0, 'a', 1);
        expected_full.delta.add(1, EPSILON, 2);
        expected_full.delta.add(2, 'b', 3);
        expected_full.delta.add(0, EPSILON, 4);
        expected_full.delta.add(4, EPSILON, 5);
        expected_full.delta.add(5, EPSILON, 6);
        expected_full.delta.add(6, 'a', 7);
        expected_full.delta.add(7, EPSILON, 8);
        expected_full.delta.add(8, 'b', 3);
        expected_full.delta.add(0, 'a', 9);
        expected_full.delta.add(9, EPSILON, 10);
        expected_full.delta.add(10, 'b', 11);
        expected_full.delta.add(11, EPSILON, 12);
        expected_full.delta.add(12, EPSILON, 13);
        expected_full.delta.add(13, EPSILON, 3);
        Nft expected_proj = project_out(expected_full, { 1 }, JumpMode::NoJump);

        SECTION("LHS | RHS") {
            Nft result_full = compose(lhs, rhs, 1, 0, false, JumpMode::NoJump);
            CHECK(result_full.final.size() == expected_full.final.size());
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 0, true, JumpMode::NoJump);
            CHECK(result_proj.final.size() == expected_proj.final.size());
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
        SECTION("RHS | LHS") {
            Nft result_full = compose(rhs, lhs, 0, 1, false, JumpMode::NoJump);
            CHECK(result_full.final.size() == expected_full.final.size());
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 1, true, JumpMode::NoJump);
            CHECK(result_proj.final.size() == expected_proj.final.size());
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }

    SECTION("3 levels x 1 level (long)") {
        Nft lhs(7, { 0 }, { 3, 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0 }));
        lhs.delta.add(0, 'a', 1);
        lhs.delta.add(1, EPSILON, 2);
        lhs.delta.add(2, 'b', 3);
        lhs.delta.add(3, 'c', 4);
        lhs.delta.add(4, EPSILON, 5);
        lhs.delta.add(5, 'd', 6);

        Nft rhs(2, { 0 }, { 1 }, Levels(CustomVector<Level>({ 0, 0 })));
        rhs.delta.add(0, EPSILON, 1);

        Nft expected_full(24, { 0 }, { 3, 6 }, Levels({ 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 1, 2, 0, 1, 2, 1, 2, 1, 2, 0, 1, 2 }));
        expected_full.delta.add(0, 'a', 1);
        expected_full.delta.add(1, EPSILON, 2);
        expected_full.delta.add(2, 'b', 3);
        expected_full.delta.add(3, 'c', 4);
        expected_full.delta.add(4, EPSILON, 5);
        expected_full.delta.add(5, 'd', 6);
        expected_full.delta.add(0, EPSILON, 7);
        expected_full.delta.add(7, EPSILON, 8);
        expected_full.delta.add(8, EPSILON, 9);
        expected_full.delta.add(9, 'a', 10);
        expected_full.delta.add(10, EPSILON, 11);
        expected_full.delta.add(11, 'b', 3);
        expected_full.delta.add(3, 'c', 4);
        expected_full.delta.add(4, EPSILON, 5);
        expected_full.delta.add(5, 'd', 6);
        expected_full.delta.add(0, 'a', 12);
        expected_full.delta.add(12, EPSILON, 13);
        expected_full.delta.add(13, 'b', 14);
        expected_full.delta.add(14, EPSILON, 15);
        expected_full.delta.add(15, EPSILON, 16);
        expected_full.delta.add(16, EPSILON, 3);
        expected_full.delta.add(14, 'c', 17);
        expected_full.delta.add(17, EPSILON, 18);
        expected_full.delta.add(18, 'd', 6);
        expected_full.delta.add(14, 'c', 19);
        expected_full.delta.add(19, EPSILON, 20);
        expected_full.delta.add(20, 'd', 21);
        expected_full.delta.add(21, EPSILON, 22);
        expected_full.delta.add(22, EPSILON, 23);
        expected_full.delta.add(23, EPSILON, 6);
        Nft expected_proj = project_out(expected_full, { 1 }, JumpMode::NoJump);

        SECTION("LHS | RHS") {
            Nft result_full = compose(lhs, rhs, 1, 0, false, JumpMode::NoJump);
            CHECK(result_full.final.size() == expected_full.final.size());
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(lhs, rhs, 1, 0, true, JumpMode::NoJump);
            CHECK(result_proj.final.size() == expected_proj.final.size());
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }

        SECTION("RHS | LHS") {
            Nft result_full = compose(rhs, lhs, 0, 1, false, JumpMode::NoJump);
            CHECK(result_full.final.size() == expected_full.final.size());
            CHECK(result_full.num_of_states() <= expected_full.num_of_states());
            CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
            CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
            result_full.trim();
            result_full.remove_epsilon();
            expected_full.trim();
            expected_full.remove_epsilon();
            CHECK(are_equivalent(result_full, expected_full));

            Nft result_proj = compose(rhs, lhs, 0, 1, true, JumpMode::NoJump);
            CHECK(result_proj.final.size() == expected_proj.final.size());
            CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
            CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
            CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
            result_proj.trim();
            result_proj.remove_epsilon();
            expected_proj.trim();
            expected_proj.remove_epsilon();
            CHECK(are_equivalent(result_proj, expected_proj));
        }
    }
}

TEST_CASE("nft::composition(..., Level, Level, ...) - complex") {
    Nft lhs(25, { 0 }, { 12, 14, 19, 24 }, Levels({ 0, 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 0, 0, 0, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0 }));
    lhs.delta.add(0, 'a', 1);
    lhs.delta.add(1, 'b', 2);
    lhs.delta.add(1, 'c', 3);
    lhs.delta.add(2, 'd', 4);
    lhs.delta.add(2, 'e', 5);
    lhs.delta.add(3, 'f', 6);
    lhs.delta.add(4, EPSILON, 7);
    lhs.delta.add(5, 'b', 8);
    lhs.delta.add(5, EPSILON, 9);
    lhs.delta.add(6, 'y', 10);
    lhs.delta.add(7, 'g', 11);
    lhs.delta.add(8, 'h', 12);
    lhs.delta.add(9, 'i', 13);
    lhs.delta.add(10, 'j', 14);
    lhs.delta.add(11, 'k', 15);
    lhs.delta.add(13, 'l', 20);
    lhs.delta.add(15, 'm', 16);
    lhs.delta.add(16, 'o', 17);
    lhs.delta.add(17, 'x', 18);
    lhs.delta.add(18, 'q', 19);
    lhs.delta.add(20, 'n', 21);
    lhs.delta.add(21, 'p', 22);
    lhs.delta.add(22, 'y', 23);
    lhs.delta.add(23, 'r', 24);

    Nft rhs(15, { 0 }, { 11, 12, 13, 14 }, Levels({ 0, 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 0, 0, 0, 0 }));
    rhs.delta.add(0, 's', 1);
    rhs.delta.add(1, 't', 2);
    rhs.delta.add(1, 'u', 3);
    rhs.delta.add(2, 'v', 4);
    rhs.delta.add(3, 'w', 5);
    rhs.delta.add(3, 'x', 6);
    rhs.delta.add(4, 'a', 7);
    rhs.delta.add(5, 'b', 8);
    rhs.delta.add(6, 'x', 9);
    rhs.delta.add(6, 'y', 10);
    rhs.delta.add(7, 'y', 11);
    rhs.delta.add(8, 'z', 12);
    rhs.delta.add(9, 'a', 13);
    rhs.delta.add(10, 'b', 14);

    SECTION("LHS | RHS") {
        Nft expected_full(64, { 0 }, { 12, 22, 44, 63 }, Levels({ 0, 1, 2, 3, 4, 5, 6, 5, 6, 6, 7, 8, 0, 2, 3, 4, 5, 6, 5, 6, 7, 8, 0, 6, 1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2, 3, 4, 5, 6, 5, 6, 6, 7, 8, 0, 3, 4, 5, 6, 7, 8, 0, 1, 2, 3, 4, 5, 6, 5, 6, 6, 7, 8, 0 }));
        expected_full.delta.add(0, 'a', 1);
        expected_full.delta.add(1, 'c', 2);
        expected_full.delta.add(1, 'b', 13);
        expected_full.delta.add(2, 'f', 3);
        expected_full.delta.add(3, 's', 4);
        expected_full.delta.add(4, 't', 5);
        expected_full.delta.add(5, 'v', 6);
        expected_full.delta.add(4, 'u', 7);
        expected_full.delta.add(7, 'w', 8);
        expected_full.delta.add(7, 'x', 9);
        expected_full.delta.add(9, 'y', 10);
        expected_full.delta.add(10, 'j', 11);
        expected_full.delta.add(11, 'b', 12);
        expected_full.delta.add(13, 'e', 14);
        expected_full.delta.add(14, 's', 15);
        expected_full.delta.add(15, 't', 16);
        expected_full.delta.add(16, 'v', 17);
        expected_full.delta.add(15, 'u', 18);
        expected_full.delta.add(18, 'w', 19);
        expected_full.delta.add(19, 'b', 20);
        expected_full.delta.add(20, 'h', 21);
        expected_full.delta.add(21, 'z', 22);
        expected_full.delta.add(18, 'x', 23);
        expected_full.delta.add(0, 'a', 24);
        expected_full.delta.add(24, 'b', 25);
        expected_full.delta.add(25, 'd', 26);
        expected_full.delta.add(26, EPSILON, 27);
        expected_full.delta.add(27, EPSILON, 28);
        expected_full.delta.add(28, EPSILON, 29);
        expected_full.delta.add(29, EPSILON, 30);
        expected_full.delta.add(30, 'g', 31);
        expected_full.delta.add(31, EPSILON, 32);
        expected_full.delta.add(32, 'k', 33);
        expected_full.delta.add(33, 'm', 34);
        expected_full.delta.add(34, 'o', 35);
        expected_full.delta.add(35, 's', 36);
        expected_full.delta.add(36, 't', 37);
        expected_full.delta.add(37, 'v', 38);
        expected_full.delta.add(36, 'u', 39);
        expected_full.delta.add(39, 'w', 40);
        expected_full.delta.add(39, 'x', 41);
        expected_full.delta.add(41, 'x', 42);
        expected_full.delta.add(42, 'q', 43);
        expected_full.delta.add(43, 'a', 44);
        expected_full.delta.add(25, 'e', 45);
        expected_full.delta.add(45, EPSILON, 46);
        expected_full.delta.add(46, EPSILON, 47);
        expected_full.delta.add(47, EPSILON, 48);
        expected_full.delta.add(48, EPSILON, 49);
        expected_full.delta.add(49, 'i', 50);
        expected_full.delta.add(50, EPSILON, 51);
        expected_full.delta.add(51, 'l', 52);
        expected_full.delta.add(52, 'n', 53);
        expected_full.delta.add(53, 'p', 54);
        expected_full.delta.add(54, 's', 55);
        expected_full.delta.add(55, 't', 56);
        expected_full.delta.add(56, 'v', 57);
        expected_full.delta.add(55, 'u', 58);
        expected_full.delta.add(58, 'w', 59);
        expected_full.delta.add(58, 'x', 60);
        expected_full.delta.add(60, 'y', 61);
        expected_full.delta.add(61, 'r', 62);
        expected_full.delta.add(62, 'b', 63);
        Nft expected_proj = project_out(expected_full, { 6 }, JumpMode::NoJump);

        Nft result_full = compose(lhs, rhs, 3, 3, false, JumpMode::NoJump);
        CHECK(result_full.final.size() == expected_full.final.size());
        CHECK(result_full.num_of_states() <= expected_full.num_of_states());
        CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
        CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
        result_full.trim();
        result_full.remove_epsilon();
        expected_full.trim();
        expected_full.remove_epsilon();
        CHECK(are_equivalent(result_full, expected_full));

        Nft result_proj = compose(lhs, rhs, 3, 3, true, JumpMode::NoJump).trim();
        CHECK(result_proj.final.size() == expected_proj.final.size());
        CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
        CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
        CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
        result_proj.trim();
        result_proj.remove_epsilon();
        expected_proj.trim();
        expected_proj.remove_epsilon();
        CHECK(are_equivalent(result_proj, expected_proj));
    }

    SECTION("RHS | LHS") {
        Nft expected_full(77, { 0 }, { 18, 27, 54, 76 }, Levels({ 0, 1, 2, 3, 4, 5, 6, 5, 6, 2, 3, 4, 5, 6, 5, 6, 7, 8, 0, 3, 4, 5, 5, 6, 6, 7, 8, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2, 3, 4, 5, 6, 2, 3, 4, 5, 6, 3, 4, 5, 6, 7, 8, 0, 6, 7, 8, 0, 1, 2, 3, 4, 5, 6, 2, 3, 4, 5, 6, 3, 4, 5, 6, 7, 8, 0 }));
        expected_full.delta.add(0, 's', 1);
        expected_full.delta.add(1, 't', 2);
        expected_full.delta.add(2, 'v', 3);
        expected_full.delta.add(3, 'a', 4);
        expected_full.delta.add(4, 'c', 5);
        expected_full.delta.add(5, 'f', 6);
        expected_full.delta.add(4, 'b', 7);
        expected_full.delta.add(7, 'e', 8);
        expected_full.delta.add(1, 'u', 9);
        expected_full.delta.add(9, 'w', 10);
        expected_full.delta.add(10, 'a', 11);
        expected_full.delta.add(11, 'c', 12);
        expected_full.delta.add(12, 'f', 13);
        expected_full.delta.add(11, 'b', 14);
        expected_full.delta.add(14, 'e', 15);
        expected_full.delta.add(15, 'b', 16);
        expected_full.delta.add(16, 'z', 17);
        expected_full.delta.add(17, 'h', 18);
        expected_full.delta.add(9, 'x', 19);
        expected_full.delta.add(19, 'a', 20);
        expected_full.delta.add(20, 'b', 21);
        expected_full.delta.add(21, 'e', 23);
        expected_full.delta.add(20, 'c', 22);
        expected_full.delta.add(22, 'f', 24);
        expected_full.delta.add(24, 'y', 25);
        expected_full.delta.add(25, 'b', 26);
        expected_full.delta.add(26, 'j', 27);
        expected_full.delta.add(0, EPSILON, 28);
        expected_full.delta.add(28, EPSILON, 29);
        expected_full.delta.add(29, EPSILON, 30);
        expected_full.delta.add(30, 'a', 31);
        expected_full.delta.add(31, 'b', 32);
        expected_full.delta.add(32, 'd', 33);
        expected_full.delta.add(33, EPSILON, 34);
        expected_full.delta.add(34, EPSILON, 35);
        expected_full.delta.add(35, 'g', 36);
        expected_full.delta.add(36, 's', 37);
        expected_full.delta.add(37, 't', 38);
        expected_full.delta.add(38, 'v', 39);
        expected_full.delta.add(39, 'k', 40);
        expected_full.delta.add(40, 'm', 41);
        expected_full.delta.add(41, 'o', 42);
        expected_full.delta.add(37, 'u', 43);
        expected_full.delta.add(43, 'w', 44);
        expected_full.delta.add(44, 'k', 45);
        expected_full.delta.add(45, 'm', 46);
        expected_full.delta.add(46, 'o', 47);
        expected_full.delta.add(43, 'x', 48);
        expected_full.delta.add(48, 'k', 49);
        expected_full.delta.add(49, 'm', 50);
        expected_full.delta.add(50, 'o', 51);
        expected_full.delta.add(51, 'x', 52);
        expected_full.delta.add(52, 'a', 53);
        expected_full.delta.add(53, 'q', 54);
        expected_full.delta.add(32, 'e', 55);
        expected_full.delta.add(55, EPSILON, 56);
        expected_full.delta.add(56, EPSILON, 57);
        expected_full.delta.add(57, 'i', 58);
        expected_full.delta.add(58, 's', 59);
        expected_full.delta.add(59, 't', 60);
        expected_full.delta.add(60, 'v', 61);
        expected_full.delta.add(61, 'l', 62);
        expected_full.delta.add(62, 'n', 63);
        expected_full.delta.add(63, 'p', 64);
        expected_full.delta.add(59, 'u', 65);
        expected_full.delta.add(65, 'w', 66);
        expected_full.delta.add(66, 'l', 67);
        expected_full.delta.add(67, 'n', 68);
        expected_full.delta.add(68, 'p', 69);
        expected_full.delta.add(65, 'x', 70);
        expected_full.delta.add(70, 'l', 71);
        expected_full.delta.add(71, 'n', 72);
        expected_full.delta.add(72, 'p', 73);
        expected_full.delta.add(73, 'y', 74);
        expected_full.delta.add(74, 'b', 75);
        expected_full.delta.add(75, 'r', 76);
        Nft expected_proj = project_out(expected_full, { 6 }, JumpMode::NoJump);

        Nft result_full = compose(rhs, lhs, 3, 3, false, JumpMode::NoJump);
        CHECK(result_full.final.size() == expected_full.final.size());
        CHECK(result_full.num_of_states() <= expected_full.num_of_states());
        CHECK(result_full.levels.num_of_levels == expected_full.levels.num_of_levels);
        CHECK(result_full.delta.num_of_transitions() <= expected_full.delta.num_of_transitions());
        result_full.trim();
        result_full.remove_epsilon();
        expected_full.trim();
        expected_full.remove_epsilon();
        CHECK(are_equivalent(result_full, expected_full));

        Nft result_proj = compose(rhs, lhs, 3, 3, true, JumpMode::NoJump).trim();
        CHECK(result_proj.final.size() == expected_proj.final.size());
        CHECK(result_proj.num_of_states() <= expected_proj.num_of_states());
        CHECK(result_proj.levels.num_of_levels == expected_proj.levels.num_of_levels);
        CHECK(result_proj.delta.num_of_transitions() <= expected_proj.delta.num_of_transitions());
        result_proj.trim();
        result_proj.remove_epsilon();
        expected_proj.trim();
        expected_proj.remove_epsilon();
        CHECK(are_equivalent(result_proj, expected_proj));
    }
}
