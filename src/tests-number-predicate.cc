//
// Created by Lukáš Holík on 06.12.2022.
//

#include "../3rdparty/catch.hpp"

#include <mata/nfa.hh>

using namespace Mata::Util;
using namespace Mata::Nfa;

TEST_CASE("Mata::Util::NumPredicate") {
    NumPredicate<State> p;
    std::vector<bool> vals = {true, false, true, false};
    for (auto val: vals) {
        if (val) p.watch_elements();
        else p.dont_watch_elements();
        p.add({1, 2, 3, 4, 5});
        State i = 1;
        for (auto q: p) {
            CHECK(q == i);
            i++;
        }
        CHECK(i == 6);
        std::vector<State> v = {1, 2, 3, 4, 5};
        auto elems = p.get_elements();
        CHECK(elems == v);
        CHECK(p.size() == 5);
        p.remove({2, 4});
        CHECK(!p[0]);
        CHECK(p[1]);
        CHECK(!p[2]);
        CHECK(p[3]);
        CHECK(!p[4]);
        CHECK(p[5]);
        CHECK(!p[6]);
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
        p.add(100);
        CHECK(p[100]);
        CHECK(!p[99]);
        CHECK(!p[101]);
        CHECK(p.size() == 1);
        p.clear();
    }
}
