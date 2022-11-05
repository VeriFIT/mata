//
// Created by Lukáš Holík on 29.10.2022.
//

#include "../3rdparty/catch.hpp"

#include <mata/util.hh>
#include <mata/synchronized_iterator.hh>

using namespace Mata::Util;

TEST_CASE("Mata::Util::SynchronizedIterator")
{

    SECTION("synchronized_universal_iterator, basic functionality")
    {
        SynchronizedUniverzalIterator<OrdVector<int>> iu;

        // Basic functionality, position[0] gets emptied first
        OrdVector<int> v1{1, 2, 4};
        OrdVector<int> v2{0, 1, 3, 5};
        OrdVector<int> v3{0, 1, 2, 4};

        iu.push_back(v1);
        iu.push_back(v2);
        iu.push_back(v3);

        REQUIRE(iu.advance());
        auto current = iu.get_current();
        REQUIRE(current.size() == 3);
        REQUIRE(*current[0] == 1);
        REQUIRE(*current[1] == 1);
        REQUIRE(*current[2] == 1);
        REQUIRE(!iu.advance());

        iu.reset();

        // Empty after reset
        REQUIRE(!iu.advance());

        // Basic functionality, position[0] does not get emptied first
        v1 = {1, 2, 3, 4, 5};
        v2 = {0, 1, 3};
        v3 = {1, 2, 3};

        iu.push_back(v1);
        iu.push_back(v2);
        iu.push_back(v3);

        REQUIRE(iu.advance());
        current = iu.get_current();
        REQUIRE(current.size() == 3);
        REQUIRE(*current[0] == 1);
        REQUIRE(*current[1] == 1);
        REQUIRE(*current[2] == 1);
        REQUIRE(iu.advance());
        current = iu.get_current();
        REQUIRE(current.size() == 3);
        REQUIRE(*current[0] == 3);
        REQUIRE(*current[1] == 3);
        REQUIRE(*current[2] == 3);
        REQUIRE(!iu.advance());
    }

    SECTION("synchronized_universal_iterator, corner cases") {

        SynchronizedUniverzalIterator<OrdVector<int>> iu;

        // Empty iterator
        REQUIRE(!iu.advance());
        REQUIRE(!iu.advance());
        auto current = iu.get_current();
        REQUIRE(current.empty());

        OrdVector<int> v1{};
        OrdVector<int> v2{1};
        OrdVector<int> v3{};

        iu.push_back(v1);
        iu.push_back(v2);
        iu.push_back(v3);

        REQUIRE(!iu.advance());

        // Empty after reset
        iu.reset();
        REQUIRE(!iu.advance());
        REQUIRE(!iu.advance());
        current = iu.get_current();
        REQUIRE(current.empty());

        // Only empty vectors
        iu.push_back(v1);

        REQUIRE(!iu.advance());

        iu.push_back(v3);
        iu.reset();

        REQUIRE(!iu.advance());

        // Insert the same vector twice
        OrdVector<int> v4{1,2};
        OrdVector<int> v5{2};

        iu.push_back(v4);
        iu.push_back(v4);
        iu.push_back(v5);

        REQUIRE(iu.advance());
        current = iu.get_current();
        REQUIRE(*current[0]==2);
        REQUIRE(*current[1]==2);
        REQUIRE(*current[2]==2);
        REQUIRE(!iu.advance());
    }

    SECTION("SynchronizedExistentialIterator, basic functionality")
    {
        SynchronizedExistentialIterator<OrdVector<int>> ie;

        // Basic functionality
        OrdVector<int> v1{1, 2};
        OrdVector<int> v2{0, 3};
        OrdVector<int> v3{0, 1, 2, 3};

        ie.push_back(v1);
        ie.push_back(v2);
        ie.push_back(v3);

        int i = 0;
        while(ie.advance()) {
            auto current = ie.get_current();
            REQUIRE(current.size() == 2);
            REQUIRE(*current[0] == i);
            REQUIRE(*current[1] == i);
            i++;
        };
        REQUIRE(i==4);
    }

    SECTION("SynchronizedExistentialIterator, corner cases") {

        SynchronizedExistentialIterator<OrdVector<int>> ie;

        // Empty iterator
        REQUIRE(!ie.advance());
        REQUIRE(!ie.advance());
        auto current = ie.get_current();
        REQUIRE(current.empty());

        // Empty vectors
        OrdVector<int> v1{};
        OrdVector<int> v2{1};
        OrdVector<int> v3{};

        ie.push_back(v1);
        ie.push_back(v2);
        ie.push_back(v3);

        REQUIRE(ie.advance());
        current = ie.get_current();
        REQUIRE(*current[0]==1);
        REQUIRE(current.size()==1);
        REQUIRE(!ie.advance());

        // Empty after reset
        ie.reset();
        REQUIRE(!ie.advance());
        current = ie.get_current();
        REQUIRE(current.empty());
        REQUIRE(!ie.advance());
        current = ie.get_current();
        REQUIRE(current.empty());

        // Only empty vectors
        ie.push_back(v1);

        REQUIRE(!ie.advance());

        ie.push_back(v3);
        ie.reset();

        REQUIRE(!ie.advance());

        // Insert the same vector twice
        OrdVector<int> v4{1,2};
        OrdVector<int> v5{2};

        ie.push_back(v4);
        ie.push_back(v5);
        ie.push_back(v4);

        REQUIRE(ie.advance());
        current = ie.get_current();
        REQUIRE(current.size()==2);
        REQUIRE(*current[0]==1);
        REQUIRE(*current[1]==1);
        REQUIRE(ie.advance());
        current = ie.get_current();
        REQUIRE(current.size()==3);
        REQUIRE(*current[0]==2);
        REQUIRE(*current[1]==2);
        REQUIRE(*current[2]==2);
        REQUIRE(!ie.advance());
    }
}
