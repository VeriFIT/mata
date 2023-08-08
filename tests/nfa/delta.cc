// TODO: some header

#include "nfa-util.hh"

#include "mata/alphabet.hh"
#include "mata/nfa/types.hh"
#include "mata/nfa/delta.hh"
#include "mata/nfa/nfa.hh"

#include "../3rdparty/catch.hpp"

using namespace Mata::Nfa;

using Symbol = Mata::Symbol;

TEST_CASE("Mata::Nfa::SymbolPost") {
    CHECK(SymbolPost{ 0, StateSet{} } == SymbolPost{ 0, StateSet{ 0, 1 } });
    CHECK(SymbolPost{ 1, StateSet{} } != SymbolPost{ 0, StateSet{} });
    CHECK(SymbolPost{ 0, StateSet{ 1 } } < SymbolPost{ 1, StateSet{} });
    CHECK(SymbolPost{ 0, StateSet{ 1 } } <= SymbolPost{ 1, StateSet{} });
    CHECK(SymbolPost{ 0, StateSet{ 1 } } <= SymbolPost{ 0, StateSet{} });
    CHECK(SymbolPost{ 1, StateSet{ 0 } } > SymbolPost{ 0, StateSet{ 1 } });
    CHECK(SymbolPost{ 1, StateSet{ 0 } } >= SymbolPost{ 0, StateSet{ 1 } });
    CHECK(SymbolPost{ 1, StateSet{ 0 } } >= SymbolPost{ 0, StateSet{ 1 } });
}

TEST_CASE("Mata::Nfa::Delta::state_post()") {
    Mata::Nfa::Nfa aut{};

    SECTION("Add new states within the limit") {
        aut.add_state(19);
        aut.initial.insert(0);
        aut.initial.insert(1);
        aut.initial.insert(2);
        REQUIRE_NOTHROW(aut.delta.state_post(0));
        REQUIRE_NOTHROW(aut.delta.state_post(1));
        REQUIRE_NOTHROW(aut.delta.state_post(2));
        REQUIRE(aut.delta.state_post(0).empty());
        REQUIRE(aut.delta.state_post(1).empty());
        REQUIRE(aut.delta.state_post(2).empty());

        CHECK(&aut.delta.state_post(4) == &aut.delta[4]);
    }

    SECTION("Add new states over the limit") {
        aut.add_state(1);
        REQUIRE_NOTHROW(aut.initial.insert(0));
        REQUIRE_NOTHROW(aut.initial.insert(1));
        REQUIRE_NOTHROW(aut.delta.state_post(0));
        REQUIRE_NOTHROW(aut.delta.state_post(1));
        REQUIRE_NOTHROW(aut.delta.state_post(2));
        CHECK(aut.delta.state_post(0).empty());
        CHECK(aut.delta.state_post(1).empty());
        CHECK(aut.delta.state_post(2).empty());
    }

    SECTION("Add new states without specifying the number of states") {
        CHECK_NOTHROW(aut.initial.insert(0));
        CHECK_NOTHROW(aut.delta.state_post(2));
        CHECK(aut.delta.state_post(0).empty());
        CHECK(aut.delta.state_post(2).empty());
    }

    SECTION("Add new initial without specifying the number of states with over +1 number") {
        REQUIRE_NOTHROW(aut.initial.insert(25));
        CHECK_NOTHROW(aut.delta.state_post(25));
        CHECK_NOTHROW(aut.delta.state_post(26));
        CHECK(aut.delta.state_post(25).empty());
        CHECK(aut.delta.state_post(26).empty());
    }
}

TEST_CASE("Mata::Nfa::Delta::mutable_post()") {
    Nfa nfa;

    SECTION("Default initialized") {
        CHECK(nfa.delta.num_of_states() == 0);
        CHECK(nfa.delta.mutable_state_post(0).empty());
        CHECK(nfa.delta.num_of_states() == 1);

        CHECK(nfa.delta.mutable_state_post(9).empty());
        CHECK(nfa.delta.num_of_states() == 10);

        CHECK(nfa.delta.mutable_state_post(9).empty());
        CHECK(nfa.delta.num_of_states() == 10);
    }
}
