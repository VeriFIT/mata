/* tests-ord-vector.cc -- tests of OrdVector
 *
 * Copyright (c) 2018 Ondrej Lengal <ondra.lengal@gmail.com>
 *
 * This file is a part of libmata.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <catch2/catch.hpp>

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
    CHECK_THROWS(set.erase(5));
    set.erase(2);
    CHECK(set == OrdVectorT{ 1, 6 });
    set.erase(1);
    set.erase(6);
    CHECK(set.empty());
    set.push_back(3);
    CHECK(set == OrdVectorT{ 3 });
    set.erase(3);
    CHECK(set.empty());
    CHECK_THROWS(set.erase(0));
    set.emplace_back(3);
    set.emplace_back(4);
    CHECK(set == OrdVectorT{ 3, 4 });
    CHECK_THROWS(set.erase(0));
}

TEST_CASE("mata::utils::OrdVector::intersection(}")
{
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
