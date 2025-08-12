/* tests-ord-vector.cc -- tests of OrdVector
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/utils/utils.hh"
#include "mata/utils/ord-vector.hh"

using namespace mata::utils;

TEST_CASE("mata::utils::OrdVector::erase()") {
    using OrdVectorT = OrdVector<int>;
    OrdVectorT set{ 1, 2, 3, 4, 6 };
    set.erase(3);
    CHECK(set == OrdVectorT{ 1, 2, 4, 6 });
    set.erase(4);
    CHECK(set == OrdVectorT{ 1, 2, 6 });
    CHECK(set.erase(5) == 0);
    set.erase(2);
    CHECK(set == OrdVectorT{ 1, 6 });
    set.erase(1);
    set.erase(6);
    CHECK(set.empty());
    set.push_back(3);
    CHECK(set == OrdVectorT{ 3 });
    set.erase(3);
    CHECK(set.empty());
    CHECK(set.erase(0) == 0);
    set.emplace_back(3);
    set.emplace_back(4);
    CHECK(set == OrdVectorT{ 3, 4 });
    CHECK(set.erase(0) == 0);
}

TEST_CASE("mata::utils::OrdVector::front())") {
    OrdVector<int> vector{ 0, 1, 2, 3 };
    CHECK(vector.front() == 0);
    vector.erase(0);
    const OrdVector<int> vector_const{ vector };
    CHECK(vector_const.front() == 1);
}

TEST_CASE("mata::utils::OrdVector::intersection()") {
    using OrdVectorT = OrdVector<int>;
    OrdVectorT set1{};
    OrdVectorT set2{};

    SECTION("Empty sets")
    {
        REQUIRE(set1.intersection(set2).empty());
    }

    SECTION("Sets of same lengths")
    {
        set1 = {1, 3, 5, 7};
        set2 = {1, 2, 5, 6};

        REQUIRE(set1.intersection(set2) == OrdVectorT{1, 5});
    }

    SECTION("Sets of different lengths")
    {
        set1 = {1, 3, 5, 7};
        set2 = {1, 2, 5, 7, 8};

        REQUIRE(set1.intersection(set2) == OrdVectorT{1, 5, 7});
    }

    SECTION("Empty intersection of non-empty sets")
    {
        set1 = {0, 3, 6};
        set2 = {1, 2, 5, 7, 8};

        REQUIRE(set1.intersection(set2).empty());
    }
}

TEST_CASE("mata::utils::OrdVector::difference()") {
    using OrdVectorT = OrdVector<int>;
    OrdVectorT set1{};
    OrdVectorT set2{};

    SECTION("Empty sets") {
        CHECK(set1.difference(set2).empty());
    }

    SECTION("Empty rhs set") {
        set1 = { 1, 2, 3 };
        CHECK(set1.difference(set2) == set1);
    }

    SECTION("Empty lhs set") {
        set2 = { 1, 2, 3 };
        CHECK(set1.difference(set2).empty());
    }

    SECTION("filled sets") {
        set1 = { 1, 2, 3 };
        set2 = { 1, 2, 3 };
        CHECK(set1.difference(set2).empty());

        set1 = { 1, 2, 3 };
        set2 = { 1, 3 };
        CHECK(set1.difference(set2) == mata::utils::OrdVector<int>{ 2 });

        set1 = { 1, 3 };
        set2 = { 1, 2, 3 };
        CHECK(set1.difference(set2).empty());

        set1 = { 1, 2, 3 };
        set2 = { 3 };
        CHECK(set1.difference(set2) == mata::utils::OrdVector<int>{ 1, 2 });
    }
}

TEST_CASE("mata::utils::OrdVector::min()") {
    SECTION("Empty vector") {
        OrdVector<int> vec;
        CHECK_THROWS_AS(vec.min(), std::out_of_range);
    }

    SECTION("One element") {
        OrdVector<int> vec{ 42 };
        CHECK(vec.min() == 42);
    }

    SECTION("Multiple elements") {
        OrdVector<int> vec{ 3, 2, 4, 2, 5, 99 };
        CHECK(vec.min() == 2);
    }
}

TEST_CASE("mata::utils::OrdVector::max()") {
    SECTION("Empty vector") {
        OrdVector<int> vec;
        CHECK_THROWS_AS(vec.max(), std::out_of_range);
    }

    SECTION("One element") {
        OrdVector<int> vec{ 42 };
        CHECK(vec.max() == 42);
    }

    SECTION("Multiple elements") {
        OrdVector<int> vec{ 3, 2, 4, 2, 5, 99 };
        CHECK(vec.max() == 99);
    }
}

TEST_CASE("mata::utils::OrdVector::at()") {
    SECTION("Empty vector") {
        OrdVector<int> vec;
        CHECK_THROWS_AS(vec.at(0), std::out_of_range);
    }

    SECTION("One element") {
        OrdVector<int> vec{ 42 };
        CHECK(vec.at(0) == 42);
    }

    SECTION("Multiple elements") {
        OrdVector<int> vec{ 3, 2, 4, 2, 5, 99 };
        CHECK(vec.at(0) == 2);
        CHECK(vec.at(1) == 3);
        CHECK(vec.at(2) == 4);
        CHECK(vec.at(3) == 5);
        CHECK(vec.at(4) == 99);
        CHECK_THROWS_AS(vec.at(5), std::out_of_range);
    }
}
