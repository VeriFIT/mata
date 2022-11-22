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

#include "../3rdparty/catch.hpp"

#include <mata/util.hh>
#include <mata/ord-vector.hh>

using namespace Mata::util;
using namespace Mata::Util;

TEST_CASE("Mata::Util::OrdVector::intersection(}")
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
