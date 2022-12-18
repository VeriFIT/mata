//
// Created by Lukáš Holík on 06.12.2022.
//

#include "../3rdparty/catch.hpp"

#include <mata/nfa.hh>

using namespace Mata::Util;
using namespace Mata::Nfa;

TEST_CASE("Mata::Util::NumberPredicate") {
    NumberPredicate<State> p;
    p.truncate_domain();
    // to test switching between watching and not watching
    std::vector<bool> vals = {true, false, false, true, true};
    int i = 0;
    for (auto val: vals) {
        i++;
        if (val) p.track_elements();
        else p.dont_track_elements();

        SECTION("basic functionality: add, remove, access, constructor, size, get_elements "+std::to_string(i)) {
            std::vector<State> v = {1, 2, 3, 4, 5};
            p.add(v);
            p.truncate_domain();
            auto elems = p.get_elements();
            CHECK(elems == v);
            CHECK(p.size() == 5);
            p.remove({2, 4});
            p.truncate_domain();
            CHECK(OrdVector<State>(p) == OrdVector<State>({1,3,5}));
            v = {1, 3, 5};
            elems = p.get_elements();
            CHECK(elems == v);
            CHECK(p.size() == 3);
            CHECK(p.size() == 3);
            p.add({1, 3, 5});
            elems = p.get_elements();
            CHECK(elems == v);
            p.remove({1, 2, 3, 4, 5});
            elems = p.get_elements();
            v = {};
            CHECK(elems == v);
            for (State q = 0; q < 10; q++) CHECK(!p[q]);
            CHECK(p.size() == 0);
        }

        SECTION("iterator "+std::to_string(i)) {
            p.add({1, 2, 3, 4, 5});
            State i = 1;
            for (auto q: p) {
                CHECK(q == i);
                i++;
            }
            CHECK(i == 6);
            p.clear();
        }

        SECTION("accessing stuff outside current domain "+std::to_string(i)) {
            CHECK(!p[100]);
            p.add(100);
            CHECK(p[100]);
            CHECK(!p[99]);
            CHECK(!p[101]);
            CHECK(p.size() == 1);
        }

        SECTION("complement "+std::to_string(i)) {
            p = {2,4};
            p.complement(5);
            CHECK(OrdVector<State>(p) == OrdVector<State>({0,1,3}));
            p.complement(6);
            CHECK(OrdVector<State>(p) == OrdVector<State>({2,4,5}));

            p = {2,4,8};
            p.complement(6);
            CHECK(OrdVector<State>(p) == OrdVector<State>({0,1,3,5}));

            p.complement(6);
            CHECK(OrdVector<State>(p) == OrdVector<State>({2,4}));

            p = {};
        }
    }
}
