//
// Created by Lukáš Holík on 06.12.2022.
//

#include "../3rdparty/catch.hpp"

#include "mata/nfa/nfa.hh"

using namespace Mata::Util;
using namespace Mata::Nfa;

TEST_CASE("Mata::Util::SparseSet") {
    SparseSet<State> p;

    SECTION("basic functionality: insert, erase, access, constructor, size, get_elements ") {
        std::vector<State> v = {1, 2, 3, 4, 5};
        p.insert(v);
        CHECK(std::vector<State>(p.begin(),p.end()) == v);
        CHECK(p.size() == 5);
        p.erase({ 2, 4 });
        CHECK(OrdVector<State>(p.begin(),p.end()) == OrdVector<State>({1,3,5}));
        CHECK(OrdVector<State>(p.begin(),p.end()) == OrdVector<State>({1,3,5}));
        CHECK(p.size() == 3);
        p.insert({ 1, 3, 5 });
        CHECK(OrdVector<State>(p.begin(),p.end()) == OrdVector<State>({1,3,5}));
        p.erase({ 1, 2, 3, 4, 5 });
        CHECK(OrdVector<State>(p.begin(),p.end()) == OrdVector<State>({}));
        for (State q = 0; q < 10; q++) CHECK(!p[q]);
        CHECK(p.empty());
    }

    SECTION("iterator ") {
        p.insert({ 1, 2, 3, 4, 5 });
        State i = 1;
        for (auto q: p) {
            CHECK(q == i);
            i++;
        }
        CHECK(i == 6);
    }

    SECTION("accessing stuff outside current domain ") {
        CHECK(!p[100]);
        p.insert(100);
        CHECK(p[100]);
        CHECK(!p[99]);
        CHECK(!p[101]);
        CHECK(p.size() == 1);
    }

    SECTION("complement ") {
        p = {2,4};
        p.complement(5);
        CHECK(OrdVector<State>(p.begin(),p.end()) == OrdVector<State>({0,1,3}));
        p.complement(6);
        CHECK(OrdVector<State>(p.begin(),p.end()) == OrdVector<State>({2,4,5}));

        p = {2,4,6,8};
        p.complement(6);
        CHECK(OrdVector<State>(p.begin(),p.end()) == OrdVector<State>({0,1,3,5}));

        p.complement(3);
        CHECK(OrdVector<State>(p.begin(),p.end()) == OrdVector<State>({2}));
    }

    SECTION("filter") {
        p = {0, 1, 2, 3, 4, 5, 6};
        Mata::BoolVector v = {0, 1, 1};
        auto f = [&v](State x) { return x < v.size() ? v[x] : 0; };
        p.filter(f);
        CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({1, 2}));
        v={1,1,1,1,1};
        p.filter(f);
        CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({1, 2}));
        v={0,0,0,0,0};
        p.filter(f);
        CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({}));
        p = {0, 1, 2, 3, 4, 5, 6};
        v={};
        p.filter(f);
        CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({}));
        p = {};
        v = {0, 1, 1};
        CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({}));
    }

    SECTION("sort") {
        p = {1, 0, 2, 4, 6, 3, 5};
        p.sort();
        CHECK(std::vector<State>(p.begin(),p.end()) == std::vector<State>{0,1,2,3,4,5,6});
    }

    SECTION("rename") {
        p = {1, 0, 4, 2};
        std::vector<State> r = {3, 4, 5, 6, 1};
        auto f = [&r](State x) { return r[x]; };
        p.rename(f);
        CHECK(OrdVector<State>(p.begin(), p.end()) == OrdVector<State>({4,3,1,5}));
    }

    SECTION("max") {
        p = {1, 0, 4, 2};
        CHECK(p.max() == 4);
    }

    SECTION("truncate") {
        p = {1, 0, 2, 4, 6, 3, 5};
        p.erase(4);
        p.erase(6);
        p.erase(5);
        p.truncate();
        CHECK( p.domain_size() == 4);
    }
}
